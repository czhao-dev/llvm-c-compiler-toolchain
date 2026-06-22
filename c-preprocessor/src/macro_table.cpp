#include "macro_table.h"

namespace pp {

void MacroTable::define(MacroDef def) {
    std::string name = def.name;
    macros_[name] = std::move(def);
}

void MacroTable::undefine(const std::string &name) { macros_.erase(name); }

const MacroTable::MacroDef *MacroTable::lookup(const std::string &name) const {
    auto it = macros_.find(name);
    if (it == macros_.end()) return nullptr;
    return &it->second;
}

bool MacroTable::isDefined(const std::string &name) const { return macros_.count(name) != 0; }

} // namespace pp
