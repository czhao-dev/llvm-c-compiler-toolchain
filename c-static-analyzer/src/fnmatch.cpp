#include "fnmatch.h"

#include <utility>
#include <vector>

namespace sa {

namespace {

enum class AtomKind { Lit, Any, Star, Set };

struct Atom {
    AtomKind kind;
    char lit = '\0';
    bool negate = false;
    std::vector<std::pair<char, char>> ranges;
};

std::vector<Atom> parsePattern(const std::string &pattern) {
    std::vector<Atom> atoms;
    std::size_t i = 0;
    while (i < pattern.size()) {
        char c = pattern[i];
        if (c == '*') {
            atoms.push_back(Atom{AtomKind::Star});
            ++i;
        } else if (c == '?') {
            atoms.push_back(Atom{AtomKind::Any});
            ++i;
        } else if (c == '[') {
            std::size_t j = i + 1;
            bool negate = false;
            if (j < pattern.size() && pattern[j] == '!') {
                negate = true;
                ++j;
            }
            std::size_t setStart = j;
            // A ']' immediately after '['/'[!' is a literal ']' inside the
            // set, not the closing bracket.
            if (j < pattern.size() && pattern[j] == ']') ++j;
            while (j < pattern.size() && pattern[j] != ']') ++j;

            if (j >= pattern.size()) {
                // No closing ']' anywhere: '[' is just a literal character.
                atoms.push_back(Atom{AtomKind::Lit, '['});
                ++i;
            } else {
                std::vector<std::pair<char, char>> ranges;
                std::size_t k = setStart;
                while (k < j) {
                    if (k + 2 < j && pattern[k + 1] == '-') {
                        ranges.emplace_back(pattern[k], pattern[k + 2]);
                        k += 3;
                    } else {
                        ranges.emplace_back(pattern[k], pattern[k]);
                        k += 1;
                    }
                }
                Atom atom;
                atom.kind = AtomKind::Set;
                atom.negate = negate;
                atom.ranges = std::move(ranges);
                atoms.push_back(std::move(atom));
                i = j + 1;
            }
        } else {
            atoms.push_back(Atom{AtomKind::Lit, c});
            ++i;
        }
    }
    return atoms;
}

bool matchesFrom(const std::string &name, std::size_t nameIdx, const std::vector<Atom> &pattern,
                  std::size_t patIdx) {
    if (patIdx >= pattern.size()) return nameIdx >= name.size();

    const Atom &atom = pattern[patIdx];
    switch (atom.kind) {
        case AtomKind::Star:
            for (std::size_t i = nameIdx; i <= name.size(); ++i) {
                if (matchesFrom(name, i, pattern, patIdx + 1)) return true;
            }
            return false;
        case AtomKind::Any:
            return nameIdx < name.size() && matchesFrom(name, nameIdx + 1, pattern, patIdx + 1);
        case AtomKind::Lit:
            return nameIdx < name.size() && name[nameIdx] == atom.lit &&
                   matchesFrom(name, nameIdx + 1, pattern, patIdx + 1);
        case AtomKind::Set: {
            if (nameIdx >= name.size()) return false;
            char ch = name[nameIdx];
            bool inSet = false;
            for (const auto &range : atom.ranges) {
                if (ch >= range.first && ch <= range.second) {
                    inSet = true;
                    break;
                }
            }
            if (inSet == atom.negate) return false;
            return matchesFrom(name, nameIdx + 1, pattern, patIdx + 1);
        }
    }
    return false; // unreachable
}

} // namespace

bool fnmatch(const std::string &name, const std::string &pattern) {
    return matchesFrom(name, 0, parsePattern(pattern), 0);
}

} // namespace sa
