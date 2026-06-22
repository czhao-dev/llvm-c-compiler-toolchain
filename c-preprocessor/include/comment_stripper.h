#ifndef PP_COMMENT_STRIPPER_H
#define PP_COMMENT_STRIPPER_H

#include <string>

namespace pp {

// Strips // line comments and /* */ block comments from `source`.
// Comment-start sequences inside "..."/'...' literals are ignored (not
// treated as comments), and quote characters inside comments are ignored
// (not treated as literal delimiters). A multi-line block comment is
// replaced by the same number of '\n' characters it contained, so line
// numbers in the result line up exactly with the input:
//   std::count(result.begin(), result.end(), '\n')
//     == std::count(source.begin(), source.end(), '\n')
//
// Throws PreprocessorError (attributed to `displayFilename` and the line
// the comment/literal started on) for an unterminated block comment or an
// unterminated string/character literal.
std::string stripComments(const std::string &source, const std::string &displayFilename);

} // namespace pp

#endif // PP_COMMENT_STRIPPER_H
