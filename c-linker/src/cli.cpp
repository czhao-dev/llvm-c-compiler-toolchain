#include "cli.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "CLI11.hpp"
#include "diagnostic.h"
#include "elf_writer.h"
#include "linker.h"

namespace clnk {

namespace {

const char *kUsage =
    "usage: c-link <input.o>... -o <output> [--entry <symbol>] [--base-text <addr>] [--base-data <addr>]\n"
    "\n"
    "A static linker for ELF64 x86-64 relocatable object files.\n"
    "\n"
    "  -o, --output <path>    output executable path (required)\n"
    "      --entry <symbol>   entry point symbol (default: _start)\n"
    "      --base-text <addr> load address of the first .text byte (default: 0x401000)\n"
    "      --base-data <addr> load address of the first .data byte (default: next page after .text)\n"
    "  -h, --help              show this message\n";

std::uint64_t parseAddress(const std::string &flag, const std::string &value) {
    try {
        std::size_t consumed = 0;
        std::uint64_t parsed = std::stoull(value, &consumed, 0);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw CliError(flag + " requires a numeric address (decimal or 0x-prefixed hex), got '" + value + "'");
    }
}

} // namespace

LinkArgs parseArgs(const std::vector<std::string> &args) {
    CLI::App app{"c-link"};
    // Numeric address parsing (decimal or 0x-prefixed hex) and positional-
    // input/unknown-flag handling stay hand-written on top of CLI11's
    // tokenizing so behavior (and the fact every error here maps to exit
    // code 2 via CliError) is unchanged; CLI11 natively handles both
    // `--flag value` and `--flag=value` forms for every option below.
    app.allow_extras(true);
    app.set_help_flag(); // disable CLI11's own auto --help; -h/--help
                          // below just sets a bool instead of throwing.

    LinkArgs result;
    app.add_flag("-h,--help", result.showHelp);
    app.add_option("-o,--output", result.output);
    app.add_option("--entry", result.entry);

    std::string textBaseValue;
    std::string dataBaseValue;
    app.add_option("--base-text", textBaseValue);
    app.add_option("--base-data", dataBaseValue);

    try {
        std::vector<std::string> reversed(args.rbegin(), args.rend());
        app.parse(reversed);
    } catch (const CLI::ParseError &e) {
        throw CliError(e.what());
    }

    if (result.showHelp) {
        return result;
    }

    if (!textBaseValue.empty()) result.textBase = parseAddress("--base-text", textBaseValue);
    if (!dataBaseValue.empty()) result.dataBase = parseAddress("--base-data", dataBaseValue);

    for (const std::string &arg : app.remaining()) {
        if (!arg.empty() && arg[0] == '-' && arg != "-") {
            throw CliError("unknown flag: " + arg);
        }
        result.inputs.push_back(arg);
    }

    if (result.inputs.empty()) {
        throw CliError("no input files");
    }
    if (result.output.empty()) {
        throw CliError("missing required -o/--output");
    }
    if (result.textBase == 0 || result.textBase % kPageSize != 0) {
        throw CliError("--base-text must be a nonzero multiple of 0x1000");
    }
    if (result.dataBase != 0 && result.dataBase % kPageSize != 0) {
        throw CliError("--base-data must be a multiple of 0x1000");
    }
    return result;
}

int runLink(const LinkArgs &args) {
    LinkOptions options;
    options.entrySymbol = args.entry;
    options.textBase = args.textBase;
    options.dataBase = args.dataBase;
    for (const std::string &input : args.inputs) {
        options.inputPaths.emplace_back(input);
    }
    options.outputPath = args.output;

    LinkResult result = link(options);
    if (!result.ok) {
        for (const Diagnostic &diagnostic : result.diagnostics) {
            std::cerr << toString(diagnostic) << "\n";
        }
        return 1;
    }

    std::vector<std::byte> bytes = writeElfExecutable(result.image);
    std::ofstream out(args.output, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    std::filesystem::permissions(args.output,
                                  std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                                      std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                      std::filesystem::perms::others_exec);
    return 0;
}

int run(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    try {
        LinkArgs parsed = parseArgs(args);
        if (parsed.showHelp) {
            std::cout << kUsage;
            return 0;
        }
        for (const std::string &input : parsed.inputs) {
            if (!std::filesystem::exists(input)) {
                throw CliError("input file not found: " + input);
            }
        }
        return runLink(parsed);
    } catch (const CliError &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}

} // namespace clnk
