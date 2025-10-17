#pragma once

#include "instruction.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ng::orgasm {

// Function definition
struct Function {
  std::string name;
  std::vector<ParamDef> params;
  std::vector<Instruction> instructions;
  std::unordered_map<std::string, int> labels; // label name -> instruction address
};

// Import definition
struct Import {
  std::string symbol_name;
  std::string module_name;
};

// Export definition
struct Export {
  std::string symbol_name;
};

// Start block (entry point)
struct StartBlock {
  std::vector<Instruction> instructions;
  std::unordered_map<std::string, int> labels; // label name -> instruction address
};

// Module definition
struct Module {
  std::string name;

  // Symbol table
  std::vector<std::string> symbols;

  // Data sections
  std::vector<ConstDef> constants;
  std::vector<StringDef> strings;
  std::vector<ArrayDef> arrays;
  std::vector<VarDef> variables;

  // Imports and exports
  std::vector<Import> imports;
  std::vector<Export> exports;

  // Functions
  std::vector<Function> functions;

  // Start block
  std::unique_ptr<StartBlock> start_block;

  Module() = default;
  explicit Module(std::string n) : name(std::move(n)) {}
};

} // namespace ng::orgasm
