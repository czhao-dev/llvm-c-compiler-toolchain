#ifndef SA_FNMATCH_H
#define SA_FNMATCH_H

#include <string>

namespace sa {

// Port of Python's `fnmatch.fnmatch` glob semantics: `*`, `?`, `[seq]`,
// `[!seq]`. Case-sensitive throughout (POSIX `os.path.normcase` is a
// no-op, so no case-folding is done). A full, anchored match against the
// whole of `name` — not a substring search.
bool fnmatch(const std::string &name, const std::string &pattern);

} // namespace sa

#endif // SA_FNMATCH_H
