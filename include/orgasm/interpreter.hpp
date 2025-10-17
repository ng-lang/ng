#pragma once

#include "module.hpp"
#include <functional>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace ng::orgasm {

// VM execution context
class Interpreter {
public:
  explicit Interpreter(std::shared_ptr<Module> module);

  // Execute the module's start block
  void execute();

  // Execute a specific function by index
  Value call_function(int func_index, const std::vector<Value> &args);

  // Register a native function handler
  using NativeFunction = std::function<Value(const std::vector<Value> &)>;
  void register_import(const std::string &symbol, NativeFunction func);

private:
  std::shared_ptr<Module> module_;
  std::stack<Value> stack_;
  std::vector<Value> call_stack_; // For parameter passing
  std::unordered_map<std::string, NativeFunction> imports_;

  // Execute a sequence of instructions
  void execute_instructions(const std::vector<Instruction> &instructions,
                            const std::unordered_map<std::string, int> &labels,
                            const std::vector<Value> *params = nullptr);

  // Execute a single instruction
  void execute_instruction(const Instruction &instr,
                           const std::unordered_map<std::string, int> &labels,
                           int &pc, const std::vector<Value> *params);

  // Stack operations
  void push(const Value &val);
  Value pop();
  Value peek() const;

  // Type conversion
  Value cast_value(const Value &val, PrimitiveType target_type);

  // Arithmetic operations
  Value add(const Value &a, const Value &b);
  Value subtract(const Value &a, const Value &b);
  Value multiply(const Value &a, const Value &b);
  Value divide(const Value &a, const Value &b);

  // Comparison operations
  Value gt(const Value &a, const Value &b);
  Value lt(const Value &a, const Value &b);
  Value eq(const Value &a, const Value &b);
  Value ne(const Value &a, const Value &b);

  // Helper to get value by type
  template <typename T> T get_value(const Value &val) const;
};

} // namespace ng::orgasm
