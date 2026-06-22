#include "macro_expander.h"

#include <deque>
#include <stdexcept>

#include "pp_tokenizer.h"

namespace pp {

namespace {

// Belt-and-suspenders guard against a hide-set bookkeeping bug. Correct
// input can never actually hit this: every macro expansion adds its own
// name to the hide set of every token it produces, so a token's hide set
// can grow at most |macro table| times along any expansion chain before it
// necessarily contains every macro name that could still match it — the
// chain provably terminates. See docs/SPEC.md for the worked examples.
constexpr std::size_t kExpansionBudget = 200000;

} // namespace

std::vector<PPToken> expandMacros(const std::vector<PPToken> &tokens, const MacroTable &macros) {
    std::deque<PPToken> work(tokens.begin(), tokens.end());
    std::vector<PPToken> output;
    std::size_t budget = kExpansionBudget;

    while (!work.empty()) {
        if (budget-- == 0) {
            throw std::logic_error("macro expansion exceeded token budget (possible hide-set bug)");
        }

        PPToken tok = std::move(work.front());
        work.pop_front();

        if (tok.kind != PPTokenKind::Identifier) {
            output.push_back(std::move(tok));
            continue;
        }
        if (tok.hideSet.count(tok.text) != 0) {
            // Painted blue: this identifier is the macro currently being
            // expanded somewhere up the chain — leave it as literal text.
            output.push_back(std::move(tok));
            continue;
        }
        const MacroTable::MacroDef *def = macros.lookup(tok.text);
        if (def == nullptr) {
            output.push_back(std::move(tok));
            continue;
        }

        HideSet newHideSet = tok.hideSet;
        newHideSet.insert(tok.text);

        std::vector<PPToken> replacement;
        replacement.reserve(def->replacement.size());
        for (const PPToken &rt : def->replacement) {
            PPToken copy = rt;
            copy.hideSet.insert(newHideSet.begin(), newHideSet.end());
            replacement.push_back(std::move(copy));
        }

        // Rescan the replacement before the tokens that originally followed it.
        work.insert(work.begin(), replacement.begin(), replacement.end());
    }

    return output;
}

std::string expandText(const std::string &text, const MacroTable &macros) {
    PPTokenizer tokenizer(text);
    std::vector<PPToken> expanded = expandMacros(tokenizer.tokenize(), macros);

    std::string result;
    for (const PPToken &t : expanded) {
        result += t.text;
    }
    return result;
}

} // namespace pp
