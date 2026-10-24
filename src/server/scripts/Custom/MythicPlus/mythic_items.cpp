/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "Chat.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "MythicItemPolicy.h"
#include "MythicPlus.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "ScriptMgr.h"

// Mythic+ items: every row of mythic_plus_item gets 17 generated templates, Mythic 0 to Mythic 16, built in memory
// from the real item at start-up (MythicItemPolicy.h). The cache and the vault hand out Mythic 0; crests raise the
// rank at the Keystone Master, keeping enchants, gems and binding. A Blacksmith's Prismatic Socket Kit and
// Valorstones add prismatic sockets as a per-item enchant (MythicValorPolicy.h), so no extra templates are needed.
namespace MythicPlus
{
namespace
{
struct RankedItem
{
    uint32 entry;
    uint16 map;
};

// By rank-id index
std::vector<RankedItem> rankedItems;
std::unordered_map<uint16, std::vector<uint32>> indexesByMap;

ItemTemplate MakeRank(ItemTemplate const& base, uint32 itemId, uint8 rank)
{
    ItemTemplate item = base;
    int const from = int(base.ItemLevel);
    int const to = int(ItemLevelForRank(rank));
    item.ItemId = itemId;
    item.ItemLevel = uint32(to);
    item.Quality = ITEM_QUALITY_EPIC;
    item.Bonding = BIND_WHEN_PICKED_UP;
    item.Flags = ItemFlags(item.Flags & ~ITEM_FLAG_HEROIC_TOOLTIP);
    item.Description = RankDescription(rank);
    for (uint32 i = 0; i < item.StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        item.ItemStat[i].ItemStatValue = Rescale(base.ItemStat[i].ItemStatValue, StatGrowth, from, to);
    item.Armor = uint32(Rescale(int32(base.Armor), ArmorGrowth, from, to));
    item.Block = uint32(Rescale(int32(base.Block), StatGrowth, from, to));
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        item.Damage[i].DamageMin = RescaleDamage(base.Damage[i].DamageMin, from, to);
        item.Damage[i].DamageMax = RescaleDamage(base.Damage[i].DamageMax, from, to);
    }
    return item;
}

void BuildRankTemplates()
{
    rankedItems.clear();
    indexesByMap.clear();
    QueryResult result = WorldDatabase.Query("SELECT idx, entry, map FROM mythic_plus_item ORDER BY idx");
    if (!result)
    {
        LOG_WARN("server.loading", ">> Mythic+: mythic_plus_item is empty, so Mythic+ caches drop nothing.");
        return;
    }
    uint32 built = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 const index = fields[0].Get<uint32>();
        uint32 const entry = fields[1].Get<uint32>();
        uint16 const map = fields[2].Get<uint16>();
        ItemTemplate const* base = sObjectMgr->GetItemTemplate(entry);
        if (index >= MaxItemIndex || index != rankedItems.size() || !base)
        {
            LOG_ERROR("sql.sql", "Mythic+: mythic_plus_item row {} (item {}) is out of order, past {} or not an item; "
                                 "Mythic+ items stop there.", index, entry, MaxItemIndex);
            break;
        }
        rankedItems.push_back({entry, map});
        indexesByMap[map].push_back(index);
        for (uint8 rank = 0; rank <= MaxRank; ++rank)
        {
            if (!sObjectMgr->AddRuntimeItemTemplate(MakeRank(*base, RankId(index, rank), rank)))
                LOG_ERROR("sql.sql", "Mythic+: item {} already exists, so Mythic {} of item {} can't use it.",
                          RankId(index, rank), rank, entry);
            else
                ++built;
        }
    } while (result->NextRow());
    LOG_INFO("server.loading", ">> Mythic+: built {} item ranks for {} items.", built, rankedItems.size());
}

// The armor type a class collects, like Legion's loot
uint32 PreferredArmor(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        default:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

bool Suits(Player* player, ItemTemplate const* item)
{
    if (!item || player->CanUseItem(item) != EQUIP_ERR_OK)
        return false;
    bool const bodyArmor = item->Class == ITEM_CLASS_ARMOR && item->InventoryType != INVTYPE_CLOAK &&
                           item->SubClass >= ITEM_SUBCLASS_ARMOR_CLOTH && item->SubClass <= ITEM_SUBCLASS_ARMOR_PLATE;
    return !bodyArmor || item->SubClass == PreferredArmor(player);
}

void MailItem(Player* player, Item* item)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->SaveToDB(trans);
    MailDraft("Mythic+ reward", "Your bags were full, so your Mythic+ item came by mail.")
        .AddItem(item)
        .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
    CharacterDatabase.CommitTransaction(trans);
}

struct ItemState
{
    std::array<std::array<uint32, 3>, MAX_ENCHANTMENT_SLOT> enchants{};
    int32 randomProperty = 0;
    uint32 durability = 0;
};

ItemState SaveState(Item* item)
{
    ItemState state;
    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
    {
        EnchantmentSlot const enchant = EnchantmentSlot(slot);
        state.enchants[slot] = {item->GetEnchantmentId(enchant), item->GetEnchantmentDuration(enchant),
                                item->GetEnchantmentCharges(enchant)};
    }
    state.randomProperty = item->GetItemRandomPropertyId();
    state.durability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    return state;
}

uint8 OwnSockets(ItemTemplate const* proto)
{
    uint8 sockets = 0;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        if (proto->Socket[i].Color)
            ++sockets;
    return sockets;
}

bool TakesSocket(Item* item)
{
    return CanAddSocket(item->GetTemplate()->InventoryType, OwnSockets(item->GetTemplate()),
                        item->GetPrismaticSocketCount());
}

void RestoreState(Item* item, ItemState const& state)
{
    if (state.randomProperty)
        item->SetItemRandomProperties(state.randomProperty);
    // Every slot, so added prismatic sockets, their gems and the socket bonus survive an upgrade
    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
        if (state.enchants[slot][0])
            item->SetEnchantment(EnchantmentSlot(slot), state.enchants[slot][0], state.enchants[slot][1],
                                 state.enchants[slot][2]);
    item->SetBinding(true);
    item->SetUInt32Value(ITEM_FIELD_DURABILITY,
                         std::min(state.durability, item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY)));
}
}

bool IsMythicItem(uint32 itemId)
{
    std::optional<RankRef> const ref = DecodeRankId(itemId);
    return ref && ref->index < rankedItems.size();
}

uint32 RollItem(Player* player, uint16 map)
{
    return RollItem(player, std::vector<uint16>{map});
}

uint32 RollItem(Player* player, std::vector<uint16> const& maps)
{
    std::vector<uint32> suitable;
    for (uint16 map : maps)
    {
        auto const itr = indexesByMap.find(map);
        if (itr == indexesByMap.end())
            continue;
        for (uint32 index : itr->second)
            if (Suits(player, sObjectMgr->GetItemTemplate(RankId(index, 0))))
                suitable.push_back(RankId(index, 0));
    }
    return suitable.empty() ? 0 : Acore::Containers::SelectRandomContainerElement(suitable);
}

void GiveItem(Player* player, uint32 itemId)
{
    ItemPosCountVec dest;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1) == EQUIP_ERR_OK)
    {
        if (Item* item = player->StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId)))
            player->SendNewItem(item, 1, true, false, true);
        return;
    }
    if (Item* item = Item::CreateItem(itemId, 1, player))
        MailItem(player, item);
}

std::string UpgradeItem(Player* player, uint8 bag, uint8 slot)
{
    Item* item = player->GetItemByPos(bag, slot);
    std::optional<RankRef> const ref = item ? DecodeRankId(item->GetEntry()) : std::nullopt;
    if (!ref || ref->index >= rankedItems.size())
        return "Only Mythic+ items can be upgraded.";
    if (ref->rank >= MaxRank)
        return "This item is fully upgraded.";
    if (player->IsInCombat())
        return "You can't upgrade items in combat.";
    if (item->IsInTrade() || item->IsNotEmptyBag())
        return "That item can't be upgraded right now.";
    uint8 const rank = uint8(ref->rank + 1);
    uint32 const nextId = RankId(ref->index, rank);
    if (!sObjectMgr->GetItemTemplate(nextId))
        return "That item has no higher rank on this server.";
    CrestWallet wallet = Crests(player);
    if (PayForRank(wallet, rank) == Crest::None)
        return "Mythic " + std::to_string(rank) + " costs " + std::to_string(CrestsPerRank) + " " +
               CrestName(CrestForRank(rank)) + " Crests, or better crests.";

    bool const equipped = Player::IsEquipmentPos(bag, slot);
    ItemState const state = SaveState(item);
    Item* upgraded = Item::CreateItem(nextId, 1, player);
    if (!upgraded)
        return "That item can't be upgraded right now.";
    player->DestroyItem(bag, slot, true);
    RestoreState(upgraded, state);
    PayCrests(player, rank);

    if (equipped)
    {
        uint16 dest;
        if (player->CanEquipItem(slot, dest, upgraded, false) == EQUIP_ERR_OK)
        {
            player->EquipItem(dest, upgraded, true);
            return {};
        }
    }
    else
    {
        ItemPosCountVec dest;
        if (player->CanStoreItem(bag, slot, dest, upgraded, false) == EQUIP_ERR_OK)
        {
            player->StoreItem(dest, upgraded, true);
            return {};
        }
    }
    ItemPosCountVec dest;
    if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, upgraded, false) == EQUIP_ERR_OK)
        player->StoreItem(dest, upgraded, true);
    else
        MailItem(player, upgraded);
    return {};
}

std::string UpgradeCostText(uint32 itemId)
{
    std::optional<RankRef> const ref = DecodeRankId(itemId);
    if (!ref || ref->rank >= MaxRank)
        return {};
    uint8 const rank = uint8(ref->rank + 1);
    return "Mythic " + std::to_string(rank) + " (item level " + std::to_string(ItemLevelForRank(rank)) + "): " +
           std::to_string(CrestsPerRank) + " " + CrestName(CrestForRank(rank)) + " Crests";
}

std::string AddSocket(Player* player, uint8 bag, uint8 slot)
{
    Item* item = player->GetItemByPos(bag, slot);
    if (!item || !IsMythicItem(item->GetEntry()))
        return "Only Mythic+ items take sockets here.";
    if (player->IsInCombat())
        return "You can't add sockets in combat.";
    if (item->IsInTrade())
        return "That item can't take a socket right now.";
    if (!TakesSocket(item))
        return "This item already has the sockets its Icecrown Citadel 25 equal has.";
    uint8 const prismatic = item->GetPrismaticSocketCount();
    uint32 const price = SocketPrice(prismatic);
    if (!player->HasItemCount(SocketKitItem, 1))
        return "A socket needs a Prismatic Socket Kit. Blacksmiths make them, and the auction house sells them.";
    if (!SpendValorstones(player, price))
        return "The next socket costs " + std::to_string(price) + " Valorstones and a Prismatic Socket Kit.";
    player->DestroyItemCount(SocketKitItem, 1, true);
    player->ApplyEnchantment(item, PRISMATIC_ENCHANTMENT_SLOT, false);
    item->SetEnchantment(PRISMATIC_ENCHANTMENT_SLOT, PrismaticSocketEnchants[prismatic + 1], 0, 0, player->GetGUID());
    player->ApplyEnchantment(item, PRISMATIC_ENCHANTMENT_SLOT, true);
    ChatHandler(player->GetSession())
        .SendSysMessage(item->GetTemplate()->Name1 + " now has " + std::to_string(prismatic + 1) +
                        " prismatic socket" + (prismatic ? "s." : "."));
    return {};
}

std::string SocketCostText(Item* item)
{
    if (!item || !IsMythicItem(item->GetEntry()) || !TakesSocket(item))
        return {};
    return "a prismatic socket: a Prismatic Socket Kit and " +
           std::to_string(SocketPrice(item->GetPrismaticSocketCount())) + " Valorstones";
}

std::string BuyCrests(Player* player, Crest crest)
{
    std::string const name = std::string(CrestName(crest)) + " Crests";
    if (!CanBuyCrest(crest, BestKeyLevel(player)))
        return "Finish a +" + std::to_string(KeyLevelForCrest(crest)) + " keystone to buy " + name + ".";
    uint32 const earned = EarnedThisWeek(player)[uint8(crest) - 1];
    if (!BundleFitsWeek(earned))
        return "You can earn only " + std::to_string(CappedAward(CrestBundle, earned)) + " more " + name +
               " this week.";
    uint32 const price = CrestBundlePrice(crest);
    if (!SpendValorstones(player, price))
        return std::to_string(CrestBundle) + " " + name + " cost " + std::to_string(price) + " Valorstones.";
    AwardCrests(player, crest, CrestBundle);
    return {};
}

std::string OpenVault(Player* player)
{
    std::optional<VaultReward> const reward = PendingVault(player);
    if (!reward)
        return "The vault is empty. Finish a keystone this week to fill next week's.";
    uint32 const item = RollItem(player, reward->maps);
    if (!item)
        return "The vault holds nothing for your class from last week's dungeons.";
    ClaimVault(player, reward->week);
    GiveItem(player, item);
    return {};
}

void SpendCrestsOnGear(Player* player)
{
    // Every upgrade raises one rank, so this bounds the loop well above what a week's crests buy
    for (uint32 upgrades = 0; upgrades < uint32(EQUIPMENT_SLOT_END) * MaxRank; ++upgrades)
    {
        CrestWallet const wallet = Crests(player);
        uint8 pick = EQUIPMENT_SLOT_END;
        uint8 pickRank = MaxRank;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            std::optional<RankRef> const ref = item ? DecodeRankId(item->GetEntry()) : std::nullopt;
            if (ref && ref->index < rankedItems.size() && ref->rank < pickRank &&
                CanAfford(wallet, uint8(ref->rank + 1)))
            {
                pick = slot;
                pickRank = ref->rank;
            }
        }
        if (pick == EQUIPMENT_SLOT_END || !UpgradeItem(player, INVENTORY_SLOT_BAG_0, pick).empty())
            return;
    }
}
}

using namespace MythicPlus;

class MythicItemsWorldScript : public WorldScript
{
public:
    MythicItemsWorldScript() : WorldScript("MythicItemsWorldScript", {WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED}) { }

    // After the item templates load and before anyone logs in with a Mythic+ item
    void OnBeforeWorldInitialized() override { BuildRankTemplates(); }
};

void AddSC_mythic_items()
{
    new MythicItemsWorldScript();
}
