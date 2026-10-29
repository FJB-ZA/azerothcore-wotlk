/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef _SUBCLASS_BALANCE_POLICY_H
#define _SUBCLASS_BALANCE_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// The sub-class balance harness (solo-wow): a path is a different way to play, not extra power. Bots of the plain
// class and of the path fight the same scenarios at the same level, talents and gear; a path ships only when its
// damage and survivability land within the budget of the plain class's, and its duels against the plain class are
// even.
namespace SubclassBalance
{
inline constexpr double BudgetPct = 5.0;        // the path's output and damage taken, against the plain class's
inline constexpr uint32_t MinFights = 3;        // fights of each side before a scenario can pass
inline constexpr double NoisePct = BudgetPct / 2; // a side whose fights spread more than this can't be judged
inline constexpr uint32_t MinDuels = 10;
inline constexpr double DuelLow = 0.40;         // the path's share of duels won, inside these
inline constexpr double DuelHigh = 0.60;

enum class Scenario : uint8_t { Dummy, Pack, Duel };

constexpr char const* ScenarioName(Scenario scenario)
{
    switch (scenario)
    {
        case Scenario::Dummy: return "single target";
        case Scenario::Pack: return "5-mob pack";
        case Scenario::Duel: return "duel";
    }
    return "?";
}

// One bot's fight: what it and its pets and guardians dealt, what it took, and for how long
struct Fight
{
    uint64_t damage = 0;     // the bot's own, and its pets' and guardians'
    uint64_t petDamage = 0;  // the part of damage its pets and guardians dealt
    uint64_t taken = 0;
    uint32_t deaths = 0;
    uint32_t ms = 0;
};

constexpr double PerSecond(uint64_t amount, uint32_t ms) { return ms ? amount * 1000.0 / ms : 0.0; }

// What a side's fights add up to
struct Side
{
    uint32_t fights = 0;
    double dps = 0;         // the mean of its fights'
    double petShare = 0;    // of all its damage
    double takenPerSecond = 0;
    uint32_t deaths = 0;
    double spreadPct = 0;   // its fights' damage per second, the standard deviation as a share of the mean
};

inline Side Summarize(std::vector<Fight> const& fights)
{
    Side side;
    side.fights = uint32_t(fights.size());
    if (fights.empty())
        return side;
    uint64_t damage = 0, pet = 0, taken = 0, ms = 0;
    double sum = 0, squares = 0;
    for (Fight const& fight : fights)
    {
        double const dps = PerSecond(fight.damage, fight.ms);
        sum += dps;
        squares += dps * dps;
        damage += fight.damage;
        pet += fight.petDamage;
        taken += fight.taken;
        ms += fight.ms;
        side.deaths += fight.deaths;
    }
    double const n = double(fights.size());
    side.dps = sum / n;
    side.petShare = damage ? double(pet) / double(damage) : 0.0;
    side.takenPerSecond = ms ? taken * 1000.0 / double(ms) : 0.0;
    double const variance = std::max(0.0, squares / n - side.dps * side.dps);
    side.spreadPct = side.dps > 0 ? std::sqrt(variance) / side.dps * 100.0 : 0.0;
    return side;
}

// The path's value against the plain class's, in percent (+ when the path's is higher)
constexpr double DiffPct(double path, double plain) { return plain > 0 ? (path - plain) / plain * 100.0 : 0.0; }

enum class Verdict : uint8_t { Pass, Fail, Inconclusive };

constexpr char const* VerdictName(Verdict verdict)
{
    switch (verdict)
    {
        case Verdict::Pass: return "pass";
        case Verdict::Fail: return "FAIL";
        case Verdict::Inconclusive: return "inconclusive";
    }
    return "?";
}

struct Comparison
{
    Side plain, path;
    double dpsDiffPct = 0;
    double takenDiffPct = 0;
    Verdict verdict = Verdict::Inconclusive;
    std::string why;  // what failed or kept it from a verdict, empty on a pass
};

// A damage scenario (the dummy or the pack): both sides' damage and damage taken within the budget. Too few fights,
// a side that never dealt damage, or fights spread wider than half the budget give no verdict: run it again.
inline Comparison Compare(std::vector<Fight> const& plainFights, std::vector<Fight> const& pathFights)
{
    Comparison c;
    c.plain = Summarize(plainFights);
    c.path = Summarize(pathFights);
    c.dpsDiffPct = DiffPct(c.path.dps, c.plain.dps);
    c.takenDiffPct = DiffPct(c.path.takenPerSecond, c.plain.takenPerSecond);
    if (c.plain.fights < MinFights || c.path.fights < MinFights)
        c.why = "fewer than " + std::to_string(MinFights) + " fights a side";
    else if (c.plain.dps <= 0 || c.path.dps <= 0)
        c.why = "a side dealt no damage";
    else if (c.plain.spreadPct > NoisePct || c.path.spreadPct > NoisePct)
        c.why = "fights too uneven to judge";
    if (!c.why.empty())
        return c;
    if (std::fabs(c.dpsDiffPct) > BudgetPct)
        c.why = "damage outside the budget";
    else if (c.plain.takenPerSecond > 0 && std::fabs(c.takenDiffPct) > BudgetPct)
        c.why = "damage taken outside the budget";
    else if (c.path.deaths > c.plain.deaths)
        c.why = "the path died more";
    c.verdict = c.why.empty() ? Verdict::Pass : Verdict::Fail;
    return c;
}

// Duels between the two: the path's wins of those decided, even within DuelLow..DuelHigh
struct DuelResult
{
    uint32_t pathWins = 0, plainWins = 0, draws = 0;
    Verdict verdict = Verdict::Inconclusive;
    std::string why;
};

constexpr double WinShare(uint32_t wins, uint32_t losses) { return wins + losses ? double(wins) / (wins + losses) : 0.0; }

inline DuelResult JudgeDuels(uint32_t pathWins, uint32_t plainWins, uint32_t draws)
{
    DuelResult result{pathWins, plainWins, draws, Verdict::Inconclusive, {}};
    if (pathWins + plainWins < MinDuels)
    {
        result.why = "fewer than " + std::to_string(MinDuels) + " duels decided";
        return result;
    }
    double const share = WinShare(pathWins, plainWins);
    if (share < DuelLow || share > DuelHigh)
        result.why = share > DuelHigh ? "the path wins too often" : "the path loses too often";
    result.verdict = result.why.empty() ? Verdict::Pass : Verdict::Fail;
    return result;
}

// One row of a path's balance.md
struct Row
{
    std::string date;   // YYYY-MM-DD
    uint8_t level = 0;
    std::string spec;   // the talent build both sides used
    Scenario scenario = Scenario::Dummy;
};

inline std::string TableHeader()
{
    return "| Date | Level | Spec | Scenario | Plain | Path | Difference | Damage taken | Pet share | Verdict |\n"
           "|---|---|---|---|---|---|---|---|---|---|\n";
}

inline std::string Signed(double pct)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+.1f%%", pct);
    return buffer;
}

inline std::string Fixed(double value, char const* format)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

inline std::string VerdictCell(Verdict verdict, std::string const& why)
{
    return why.empty() ? VerdictName(verdict) : std::string(VerdictName(verdict)) + ": " + why;
}

inline std::string TableRow(Row const& row, Comparison const& c)
{
    return "| " + row.date + " | " + std::to_string(row.level) + " | " + row.spec + " | " + ScenarioName(row.scenario) +
           " | " + Fixed(c.plain.dps, "%.1f") + " DPS | " + Fixed(c.path.dps, "%.1f") + " DPS | " +
           Signed(c.dpsDiffPct) + " | " + Signed(c.takenDiffPct) + " | " + Fixed(c.plain.petShare * 100, "%.0f%%") +
           " / " + Fixed(c.path.petShare * 100, "%.0f%%") + " | " + VerdictCell(c.verdict, c.why) + " |\n";
}

inline std::string TableRow(Row const& row, DuelResult const& d)
{
    return "| " + row.date + " | " + std::to_string(row.level) + " | " + row.spec + " | " + ScenarioName(row.scenario) +
           " | " + std::to_string(d.plainWins) + " won | " + std::to_string(d.pathWins) + " won | " +
           Fixed(WinShare(d.pathWins, d.plainWins) * 100, "%.0f%%") + " path | " + std::to_string(d.draws) +
           " draws | | " + VerdictCell(d.verdict, d.why) + " |\n";
}
}

#endif
