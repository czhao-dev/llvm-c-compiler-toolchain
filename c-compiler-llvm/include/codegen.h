#pragma once

#include "ast.h"

#include <string>

namespace minic {

std::string codegenStatus();
std::string emitLLVMIR(const ProgramNode &program, const std::string &moduleName = "minic_module",
                       int optLevel = 0);
void compileToNative(const ProgramNode &program, const std::string &outputPath,
                     const std::string &moduleName = "minic_module", int optLevel = 0);

} // namespace minic
