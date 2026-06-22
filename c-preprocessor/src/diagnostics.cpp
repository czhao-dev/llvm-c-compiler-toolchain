#include "diagnostics.h"

namespace pp {

namespace {

std::string format(const std::string &file, int line, const std::string &message) {
    if (line > 0) {
        return file + ":" + std::to_string(line) + ": error: " + message;
    }
    return file + ": error: " + message;
}

} // namespace

PreprocessorError::PreprocessorError(std::string file, int line, std::string message)
    : file_(std::move(file)), line_(line), message_(std::move(message)),
      formatted_(format(file_, line_, message_)) {}

const char *PreprocessorError::what() const noexcept { return formatted_.c_str(); }

} // namespace pp
