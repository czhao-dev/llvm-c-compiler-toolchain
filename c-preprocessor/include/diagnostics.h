#ifndef PP_DIAGNOSTICS_H
#define PP_DIAGNOSTICS_H

#include <exception>
#include <string>

namespace pp {

// The single exception type for every recoverable preprocessing failure:
// unterminated comments/literals, malformed or unsupported directives,
// include-resolution failures, circular includes. Formats itself as
// "file:line: error: message", or "file: error: message" when line == 0
// (the sentinel for "no specific line").
class PreprocessorError : public std::exception {
public:
    PreprocessorError(std::string file, int line, std::string message);

    const char *what() const noexcept override;

    const std::string &file() const { return file_; }
    int line() const { return line_; }
    const std::string &message() const { return message_; }

private:
    std::string file_;
    int line_;
    std::string message_;
    std::string formatted_;
};

} // namespace pp

#endif // PP_DIAGNOSTICS_H
