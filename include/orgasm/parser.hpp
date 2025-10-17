#pragma once

#include "lexer.hpp"
#include "module.hpp"
#include <memory>
#include <optional>

namespace ng::orgasm {

class Parser {
public:
  explicit Parser(std::string_view source);

  std::unique_ptr<Module> parse_module();

private:
  Lexer lexer_;
  Token current_token_;

  void advance();
  bool match(TokenType type);
  bool expect(TokenType type, const std::string &error_msg);
  void consume(TokenType type, const std::string &error_msg);

  // Parsing methods
  void parse_symbols(Module &module);
  void parse_import(Module &module);
  void parse_export(Module &module);
  void parse_const(Module &module);
  void parse_str(Module &module);
  void parse_array(Module &module);
  void parse_val(Module &module);
  void parse_function(Module &module);
  void parse_function_params(Module &module);
  void parse_endfun(Module &module);
  void parse_start(Module &module);

  std::vector<Instruction> parse_instruction_block();
  Instruction parse_instruction();
  PrimitiveType parse_type(const std::string &type_str);
  Operand parse_operand();
  Value parse_value(PrimitiveType type);

  [[noreturn]] void error(const std::string &message);
};

} // namespace ng::orgasm
