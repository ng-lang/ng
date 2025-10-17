#include "orgasm/parser.hpp"
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ng::orgasm {

Parser::Parser(std::string_view source)
    : lexer_(source), current_token_(lexer_.next_token()) {}

void Parser::advance() { current_token_ = lexer_.next_token(); }

bool Parser::match(TokenType type) { return current_token_.type == type; }

bool Parser::expect(TokenType type, const std::string &error_msg) {
  if (!match(type)) {
    error(error_msg);
  }
  return true;
}

void Parser::consume(TokenType type, const std::string &error_msg) {
  expect(type, error_msg);
  advance();
}

[[noreturn]] void Parser::error(const std::string &message) {
  std::ostringstream oss;
  oss << "Parse error at line " << current_token_.line << ", column "
      << current_token_.column << ": " << message;
  throw std::runtime_error(oss.str());
}

PrimitiveType Parser::parse_type(const std::string &type_str) {
  static const std::unordered_map<std::string, PrimitiveType> type_map = {
      {"i8", PrimitiveType::I8},
      {"i16", PrimitiveType::I16},
      {"i32", PrimitiveType::I32},
      {"i64", PrimitiveType::I64},
      {"i128", PrimitiveType::I128},
      {"u8", PrimitiveType::U8},
      {"u16", PrimitiveType::U16},
      {"u32", PrimitiveType::U32},
      {"u64", PrimitiveType::U64},
      {"u128", PrimitiveType::U128},
      {"f16", PrimitiveType::F16},
      {"f32", PrimitiveType::F32},
      {"f64", PrimitiveType::F64},
      {"f128", PrimitiveType::F128},
      {"bool", PrimitiveType::BOOL},
      {"char", PrimitiveType::CHAR},
      {"unit", PrimitiveType::UNIT},
      {"addr", PrimitiveType::ADDR},
      {"str", PrimitiveType::ADDR},
      {"v128", PrimitiveType::V128},
      {"v256", PrimitiveType::V256},
      {"v512", PrimitiveType::V512},
      {"atomic.i32", PrimitiveType::ATOMIC_I32},
      {"atomic.i64", PrimitiveType::ATOMIC_I64},
      {"atomic.addr", PrimitiveType::ATOMIC_ADDR},
  };

  auto it = type_map.find(type_str);
  if (it == type_map.end()) {
    error("Unknown type: " + type_str);
  }
  return it->second;
}

Value Parser::parse_value(PrimitiveType type) {
  Value val;
  val.type = type;

  if (match(TokenType::NUMBER)) {
    std::string num_str = current_token_.value;
    advance();

    switch (type) {
    case PrimitiveType::I8:
      val.data = static_cast<int8_t>(std::stoi(num_str));
      break;
    case PrimitiveType::I16:
      val.data = static_cast<int16_t>(std::stoi(num_str));
      break;
    case PrimitiveType::I32:
      val.data = std::stoi(num_str);
      break;
    case PrimitiveType::I64:
      val.data = std::stoll(num_str);
      break;
    case PrimitiveType::U8:
      val.data = static_cast<uint8_t>(std::stoul(num_str));
      break;
    case PrimitiveType::U16:
      val.data = static_cast<uint16_t>(std::stoul(num_str));
      break;
    case PrimitiveType::U32:
      val.data = std::stoul(num_str);
      break;
    case PrimitiveType::U64:
      val.data = std::stoull(num_str);
      break;
    case PrimitiveType::ADDR:
      // For addr type, treat number as a pointer value
      val.data = reinterpret_cast<void *>(std::stoull(num_str));
      break;
    default:
      error("Invalid type for integer value");
    }
  } else if (match(TokenType::FLOAT_NUMBER)) {
    std::string num_str = current_token_.value;
    advance();

    switch (type) {
    case PrimitiveType::F32:
      val.data = std::stof(num_str);
      break;
    case PrimitiveType::F64:
      val.data = std::stod(num_str);
      break;
    default:
      error("Invalid type for float value");
    }
  } else if (match(TokenType::IDENTIFIER)) {
    // Handle boolean literals
    if (current_token_.value == "true") {
      val.type = PrimitiveType::BOOL;
      val.data = true;
      advance();
    } else if (current_token_.value == "false") {
      val.type = PrimitiveType::BOOL;
      val.data = false;
      advance();
    } else {
      error("Unexpected identifier in value: " + current_token_.value);
    }
  } else {
    error("Expected value");
  }

  return val;
}

std::unique_ptr<Module> Parser::parse_module() {
  consume(TokenType::DOT, "Expected '.'");
  consume(TokenType::MODULE, "Expected 'module'");

  expect(TokenType::IDENTIFIER, "Expected module name");
  std::string module_name = current_token_.value;
  advance();

  auto module = std::make_unique<Module>(module_name);

  // Parse module contents
  while (!match(TokenType::DOT) ||
         (lexer_.peek_token().type != TokenType::ENDMODULE &&
          lexer_.peek_token().type != TokenType::EOF_TOKEN)) {
    if (match(TokenType::EOF_TOKEN)) {
      error("Unexpected end of file in module");
    }

    if (!match(TokenType::DOT)) {
      advance();
      continue;
    }

    advance(); // consume '.'

    if (match(TokenType::SYMBOLS)) {
      parse_symbols(*module);
    } else if (match(TokenType::IMPORT)) {
      parse_import(*module);
    } else if (match(TokenType::EXPORT)) {
      parse_export(*module);
    } else if (match(TokenType::CONST)) {
      parse_const(*module);
    } else if (match(TokenType::STR)) {
      parse_str(*module);
    } else if (match(TokenType::ARRAY)) {
      parse_array(*module);
    } else if (match(TokenType::VAL)) {
      parse_val(*module);
    } else if (match(TokenType::FUN)) {
      parse_function(*module);
    } else if (match(TokenType::START)) {
      parse_start(*module);
    } else if (match(TokenType::ENDMODULE)) {
      break;
    } else {
      error("Unexpected directive: " + current_token_.value);
    }
  }

  if (match(TokenType::DOT)) {
    advance();
  }
  if (match(TokenType::ENDMODULE)) {
    advance();
  }

  return module;
}

void Parser::parse_symbols(Module &module) {
  consume(TokenType::SYMBOLS, "Expected 'symbols'");
  consume(TokenType::LBRACKET, "Expected '['");

  while (!match(TokenType::RBRACKET) && !match(TokenType::EOF_TOKEN)) {
    expect(TokenType::IDENTIFIER, "Expected symbol name");
    module.symbols.push_back(current_token_.value);
    advance();

    if (match(TokenType::COMMA)) {
      advance();
    }
  }

  consume(TokenType::RBRACKET, "Expected ']'");
}

void Parser::parse_import(Module &module) {
  consume(TokenType::IMPORT, "Expected 'import'");

  expect(TokenType::IDENTIFIER, "Expected symbol name");
  std::string symbol = current_token_.value;
  advance();

  consume(TokenType::COMMA, "Expected ','");

  expect(TokenType::IDENTIFIER, "Expected module name");
  std::string mod_name = current_token_.value;
  advance();

  module.imports.push_back({symbol, mod_name});
}

void Parser::parse_export(Module &module) {
  consume(TokenType::EXPORT, "Expected 'export'");

  expect(TokenType::IDENTIFIER, "Expected symbol name");
  module.exports.push_back({current_token_.value});
  advance();
}

void Parser::parse_const(Module &module) {
  consume(TokenType::CONST, "Expected 'const'");

  expect(TokenType::IDENTIFIER, "Expected type");
  PrimitiveType type = parse_type(current_token_.value);
  advance();

  Value val = parse_value(type);

  ConstDef const_def;
  const_def.type = type;
  const_def.value = val;

  module.constants.push_back(const_def);

  // Skip optional comment
  if (match(TokenType::COMMENT)) {
    advance();
  }
}

void Parser::parse_str(Module &module) {
  consume(TokenType::STR, "Expected 'str'");
  consume(TokenType::LBRACKET, "Expected '['");

  // Read all tokens until ] and concatenate them as a string
  std::string str_value;
  while (!match(TokenType::RBRACKET) && !match(TokenType::EOF_TOKEN)) {
    if (!str_value.empty()) {
      str_value += " ";
    }
    str_value += current_token_.value;
    advance();
  }

  consume(TokenType::RBRACKET, "Expected ']'");

  StringDef str_def;
  str_def.value = str_value;
  module.strings.push_back(str_def);
}

void Parser::parse_array(Module &module) {
  consume(TokenType::ARRAY, "Expected 'array'");

  expect(TokenType::IDENTIFIER, "Expected element type");
  PrimitiveType elem_type = parse_type(current_token_.value);
  advance();

  consume(TokenType::LBRACKET, "Expected '['");

  ArrayDef array_def;
  array_def.element_type = elem_type;

  while (!match(TokenType::RBRACKET) && !match(TokenType::EOF_TOKEN)) {
    Value val = parse_value(elem_type);
    array_def.elements.push_back(val);

    if (match(TokenType::COMMA)) {
      advance();
    }
  }

  consume(TokenType::RBRACKET, "Expected ']'");

  module.arrays.push_back(array_def);
}

void Parser::parse_val(Module &module) {
  consume(TokenType::VAL, "Expected 'val'");

  expect(TokenType::IDENTIFIER, "Expected type");
  PrimitiveType type = parse_type(current_token_.value);
  advance();

  Value val = parse_value(type);

  VarDef var_def;
  var_def.type = type;
  var_def.initial_value = val;

  module.variables.push_back(var_def);

  // Skip optional comment
  if (match(TokenType::COMMENT)) {
    advance();
  }
}

void Parser::parse_function(Module &module) {
  consume(TokenType::FUN, "Expected 'fun'");

  expect(TokenType::IDENTIFIER, "Expected function name");
  Function func;
  func.name = current_token_.value;
  advance();

  // Parse parameters
  if (match(TokenType::DOT)) {
    advance();
    consume(TokenType::PARAM, "Expected 'param'");
    consume(TokenType::LBRACKET, "Expected '['");

    while (!match(TokenType::RBRACKET) && !match(TokenType::EOF_TOKEN)) {
      expect(TokenType::IDENTIFIER, "Expected parameter name");
      std::string param_name = current_token_.value;
      advance();

      consume(TokenType::COLON, "Expected ':'");

      expect(TokenType::IDENTIFIER, "Expected parameter type");
      PrimitiveType param_type = parse_type(current_token_.value);
      advance();

      func.params.push_back({param_name, param_type});

      if (match(TokenType::COMMA)) {
        advance();
      }
    }

    consume(TokenType::RBRACKET, "Expected ']'");
  }

  // Parse instructions
  func.instructions = parse_instruction_block();

  // Parse endfun
  if (match(TokenType::DOT)) {
    advance();
  }
  if (match(TokenType::ENDFUN)) {
    advance();
    // Optional function name
    if (match(TokenType::IDENTIFIER)) {
      advance();
    }
  }

  module.functions.push_back(std::move(func));
}

void Parser::parse_start(Module &module) {
  consume(TokenType::START, "Expected 'start'");

  module.start_block = std::make_unique<StartBlock>();
  module.start_block->instructions = parse_instruction_block();

  // Parse optional .end
  if (match(TokenType::DOT)) {
    advance();
    if (match(TokenType::END)) {
      advance();
    }
  }
}

std::vector<Instruction> Parser::parse_instruction_block() {
  std::vector<Instruction> instructions;

  while (!match(TokenType::DOT) && !match(TokenType::EOF_TOKEN)) {
    // Check if next token is a directive or end marker
    if (match(TokenType::NUMBER)) {
      // This is an instruction address
      int addr = std::stoi(current_token_.value);
      advance();

      if (match(TokenType::COLON)) {
        advance();
      }

      // Parse instruction
      Instruction instr = parse_instruction();
      instr.address = addr;
      instructions.push_back(instr);
    } else if (match(TokenType::DOT)) {
      // Next section or end
      break;
    } else {
      advance();
    }
  }

  return instructions;
}

Instruction Parser::parse_instruction() {
  // This is a simplified instruction parser
  // In a full implementation, we would parse all instruction types

  expect(TokenType::IDENTIFIER, "Expected instruction");
  std::string instr_name = current_token_.value;
  advance();

  // Parse instruction based on name
  // For now, return a placeholder
  Instruction instr(0, OpCode::IGNORE);

  // Parse operands (simplified)
  while (!match(TokenType::NUMBER) && !match(TokenType::DOT) &&
         !match(TokenType::EOF_TOKEN) && !match(TokenType::COMMENT)) {
    if (match(TokenType::IDENTIFIER) || match(TokenType::NUMBER) ||
        match(TokenType::STRING_LITERAL)) {
      // Simplified operand parsing
      advance();
    } else {
      break;
    }
  }

  return instr;
}

Operand Parser::parse_operand() {
  // Simplified operand parsing
  if (match(TokenType::NUMBER)) {
    int val = std::stoi(current_token_.value);
    advance();
    return val;
  }

  error("Expected operand");
}

} // namespace ng::orgasm
