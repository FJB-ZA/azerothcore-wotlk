// Lettered client patches (patch-A.MPQ to patch-Z.MPQ), which the 3.3.5 client loads after the numbered ones.

#ifndef _LETTERED_PATCHES_H
#define _LETTERED_PATCHES_H

#include <string>
#include <vector>

namespace LetteredPatches
{
inline constexpr char First = 'A';
inline constexpr char Last = 'Z';

// The client's spelling first, then Project Reforged's (.mpq); the others only matter on case-sensitive file systems.
inline std::vector<std::string> Candidates(char letter)
{
    char const upper = (letter >= 'a' && letter <= 'z') ? static_cast<char>(letter - 'a' + 'A') : letter;
    char const lower = static_cast<char>(upper - 'A' + 'a');
    std::string const prefix = "patch-";
    return { prefix + upper + ".MPQ", prefix + upper + ".mpq", prefix + lower + ".MPQ", prefix + lower + ".mpq" };
}

// Existing patches under dataDir in load order; opened in this order, a later letter overrides an earlier one.
template <typename Exists>
std::vector<std::string> Find(std::string const& dataDir, Exists exists)
{
    std::vector<std::string> found;
    for (char letter = First; letter <= Last; ++letter)
    {
        for (std::string const& name : Candidates(letter))
        {
            if (exists(dataDir + name))
            {
                found.push_back(dataDir + name);
                break;
            }
        }
    }
    return found;
}
}

#endif
