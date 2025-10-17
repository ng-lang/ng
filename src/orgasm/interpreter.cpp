#include "orgasm/interpreter.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ng::orgasm {

Interpreter::Interpreter(std::shared_ptr<Module> module)
    : module_(std::move(module)) {}

void Interpreter::push(const Value &val) { stack_.push(val); }

Value Interpreter::pop() {
  if (stack_.empty()) {
    throw std::runtime_error("Stack underflow");
  }
  Value val = stack_.top();
  stack_.pop();
  return val;
}

Value Interpreter::peek() const {
  if (stack_.empty()) {
    throw std::runtime_error("Stack underflow");
  }
  return stack_.top();
}

void Interpreter::execute() {
  if (!module_->start_block) {
    throw std::runtime_error("No start block in module");
  }

  execute_instructions(module_->start_block->instructions,
                       module_->start_block->labels);
}

Value Interpreter::call_function(int func_index,
                                  const std::vector<Value> &args) {
  if (func_index < 0 || func_index >= module_->functions.size()) {
    throw std::runtime_error("Invalid function index");
  }

  const Function &func = module_->functions[func_index];

  // Check parameter count
  if (args.size() != func.params.size()) {
    throw std::runtime_error("Argument count mismatch");
  }

  // Execute function
  execute_instructions(func.instructions, func.labels, &args);

  // Return value should be on stack
  if (!stack_.empty()) {
    return pop();
  }

  return Value(); // Unit value
}

void Interpreter::register_import(const std::string &symbol,
                                   NativeFunction func) {
  imports_[symbol] = std::move(func);
}

void Interpreter::execute_instructions(
    const std::vector<Instruction> &instructions,
    const std::unordered_map<std::string, int> &labels,
    const std::vector<Value> *params) {
  int pc = 0;

  while (pc < instructions.size()) {
    const Instruction &instr = instructions[pc];
    execute_instruction(instr, labels, pc, params);
    pc++;
  }
}

void Interpreter::execute_instruction(
    const Instruction &instr, const std::unordered_map<std::string, int> &labels,
    int &pc, const std::vector<Value> *params) {
  switch (instr.opcode) {
  case OpCode::LOAD_CONST: {
    auto operand = std::get<ConstOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= module_->constants.size()) {
      throw std::runtime_error("Invalid constant index");
    }
    push(module_->constants[operand.index].value);
    break;
  }

  case OpCode::LOAD_VALUE: {
    auto operand = std::get<ValueOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= module_->variables.size()) {
      throw std::runtime_error("Invalid variable index");
    }
    push(module_->variables[operand.index].initial_value);
    break;
  }

  case OpCode::STORE_VALUE: {
    auto operand = std::get<ValueOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= module_->variables.size()) {
      throw std::runtime_error("Invalid variable index");
    }
    Value val = pop();
    module_->variables[operand.index].initial_value = val;
    break;
  }

  case OpCode::LOAD_PARAM: {
    if (!params) {
      throw std::runtime_error("No parameters available");
    }
    auto operand = std::get<ParamOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= params->size()) {
      throw std::runtime_error("Invalid parameter index");
    }
    push((*params)[operand.index]);
    break;
  }

  case OpCode::LOAD_STR: {
    auto operand = std::get<StringOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= module_->strings.size()) {
      throw std::runtime_error("Invalid string index");
    }
    Value val(module_->strings[operand.index].value);
    push(val);
    break;
  }

  case OpCode::PUSH_PARAM: {
    Value val = pop();
    call_stack_.push_back(val);
    break;
  }

  case OpCode::CALL: {
    auto operand = std::get<FunctionOperand>(instr.operands[0]);
    std::vector<Value> args = call_stack_;
    call_stack_.clear();

    Value result = call_function(operand.index, args);
    push(result);
    break;
  }

  case OpCode::CALL_IMPORT: {
    auto operand = std::get<ImportOperand>(instr.operands[0]);
    if (operand.index < 0 || operand.index >= module_->imports.size()) {
      throw std::runtime_error("Invalid import index");
    }

    const Import &import = module_->imports[operand.index];
    auto it = imports_.find(import.symbol_name);
    if (it == imports_.end()) {
      throw std::runtime_error("Import not registered: " + import.symbol_name);
    }

    std::vector<Value> args = call_stack_;
    call_stack_.clear();

    Value result = it->second(args);
    if (result.type != PrimitiveType::UNIT) {
      push(result);
    }
    break;
  }

  case OpCode::RETURN: {
    // Return is handled by stopping execution
    pc = INT_MAX - 1; // Exit loop
    break;
  }

  case OpCode::ADD: {
    Value b = pop();
    Value a = pop();
    push(add(a, b));
    break;
  }

  case OpCode::SUBTRACT: {
    Value b = pop();
    Value a = pop();
    push(subtract(a, b));
    break;
  }

  case OpCode::MULTIPLY: {
    Value b = pop();
    Value a = pop();
    push(multiply(a, b));
    break;
  }

  case OpCode::DIVIDE: {
    Value b = pop();
    Value a = pop();
    push(divide(a, b));
    break;
  }

  case OpCode::GT: {
    Value b = pop();
    Value a = pop();
    push(gt(a, b));
    break;
  }

  case OpCode::LT: {
    Value b = pop();
    Value a = pop();
    push(lt(a, b));
    break;
  }

  case OpCode::EQ: {
    Value b = pop();
    Value a = pop();
    push(eq(a, b));
    break;
  }

  case OpCode::NE: {
    Value b = pop();
    Value a = pop();
    push(ne(a, b));
    break;
  }

  case OpCode::BR: {
    // Conditional branch
    Value cond = pop();
    bool condition = std::get<bool>(cond.data);

    auto label_true = std::get<LabelOperand>(instr.operands[0]);
    auto label_false = std::get<LabelOperand>(instr.operands[1]);

    if (condition) {
      auto it = labels.find(label_true.name);
      if (it != labels.end()) {
        pc = it->second - 1; // -1 because pc++ happens after
      }
    } else {
      auto it = labels.find(label_false.name);
      if (it != labels.end()) {
        pc = it->second - 1;
      }
    }
    break;
  }

  case OpCode::GOTO: {
    auto operand = std::get<AddressOperand>(instr.operands[0]);
    pc = operand.address - 1; // -1 because pc++ happens after
    break;
  }

  case OpCode::CAST: {
    Value val = pop();
    push(cast_value(val, instr.type));
    break;
  }

  case OpCode::TUPLE_CREATE: {
    // Size is on stack
    Value size_val = pop();
    int size = std::get<int32_t>(size_val.data);

    // Allocate memory for tuple
    void *tuple_mem = malloc(size);
    if (!tuple_mem) {
      throw std::runtime_error("Failed to allocate tuple memory");
    }
    memset(tuple_mem, 0, size);

    Value tuple_addr(tuple_mem);
    push(tuple_addr);
    break;
  }

  case OpCode::TUPLE_GET: {
    auto offset_op = std::get<OffsetOperand>(instr.operands[0]);
    Value tuple_addr = pop();
    void *addr = std::get<void *>(tuple_addr.data);

    // Read value at offset based on type
    char *ptr = static_cast<char *>(addr) + offset_op.offset;
    Value result;
    result.type = instr.type;

    switch (instr.type) {
    case PrimitiveType::I32:
      result.data = *reinterpret_cast<int32_t *>(ptr);
      break;
    case PrimitiveType::I64:
      result.data = *reinterpret_cast<int64_t *>(ptr);
      break;
    case PrimitiveType::F32:
      result.data = *reinterpret_cast<float *>(ptr);
      break;
    case PrimitiveType::F64:
      result.data = *reinterpret_cast<double *>(ptr);
      break;
    case PrimitiveType::BOOL:
      result.data = *reinterpret_cast<bool *>(ptr);
      break;
    case PrimitiveType::ADDR:
      result.data = *reinterpret_cast<void **>(ptr);
      break;
    default:
      throw std::runtime_error("Unsupported type for tuple_get");
    }

    push(result);
    break;
  }

  case OpCode::TUPLE_SET: {
    auto offset_op = std::get<OffsetOperand>(instr.operands[0]);
    Value value = pop();
    Value tuple_addr = pop();
    void *addr = std::get<void *>(tuple_addr.data);

    // Write value at offset based on type
    char *ptr = static_cast<char *>(addr) + offset_op.offset;

    switch (instr.type) {
    case PrimitiveType::I32:
      *reinterpret_cast<int32_t *>(ptr) = std::get<int32_t>(value.data);
      break;
    case PrimitiveType::I64:
      *reinterpret_cast<int64_t *>(ptr) = std::get<int64_t>(value.data);
      break;
    case PrimitiveType::F32:
      *reinterpret_cast<float *>(ptr) = std::get<float>(value.data);
      break;
    case PrimitiveType::F64:
      *reinterpret_cast<double *>(ptr) = std::get<double>(value.data);
      break;
    case PrimitiveType::BOOL:
      *reinterpret_cast<bool *>(ptr) = std::get<bool>(value.data);
      break;
    case PrimitiveType::ADDR:
      *reinterpret_cast<void **>(ptr) = std::get<void *>(value.data);
      break;
    default:
      throw std::runtime_error("Unsupported type for tuple_set");
    }
    break;
  }

  default:
    throw std::runtime_error("Unimplemented instruction");
  }
}

Value Interpreter::add(const Value &a, const Value &b) {
  Value result;
  result.type = a.type;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) + std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) + std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) + std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) + std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for addition");
  }

  return result;
}

Value Interpreter::subtract(const Value &a, const Value &b) {
  Value result;
  result.type = a.type;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) - std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) - std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) - std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) - std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for subtraction");
  }

  return result;
}

Value Interpreter::multiply(const Value &a, const Value &b) {
  Value result;
  result.type = a.type;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) * std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) * std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) * std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) * std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for multiplication");
  }

  return result;
}

Value Interpreter::divide(const Value &a, const Value &b) {
  Value result;
  result.type = a.type;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) / std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) / std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) / std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) / std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for division");
  }

  return result;
}

Value Interpreter::gt(const Value &a, const Value &b) {
  Value result;
  result.type = PrimitiveType::BOOL;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) > std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) > std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) > std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) > std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for comparison");
  }

  return result;
}

Value Interpreter::lt(const Value &a, const Value &b) {
  Value result;
  result.type = PrimitiveType::BOOL;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) < std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) < std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) < std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) < std::get<double>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for comparison");
  }

  return result;
}

Value Interpreter::eq(const Value &a, const Value &b) {
  Value result;
  result.type = PrimitiveType::BOOL;

  switch (a.type) {
  case PrimitiveType::I32:
    result.data = std::get<int32_t>(a.data) == std::get<int32_t>(b.data);
    break;
  case PrimitiveType::I64:
    result.data = std::get<int64_t>(a.data) == std::get<int64_t>(b.data);
    break;
  case PrimitiveType::F32:
    result.data = std::get<float>(a.data) == std::get<float>(b.data);
    break;
  case PrimitiveType::F64:
    result.data = std::get<double>(a.data) == std::get<double>(b.data);
    break;
  case PrimitiveType::BOOL:
    result.data = std::get<bool>(a.data) == std::get<bool>(b.data);
    break;
  default:
    throw std::runtime_error("Unsupported type for equality");
  }

  return result;
}

Value Interpreter::ne(const Value &a, const Value &b) {
  Value result = eq(a, b);
  result.data = !std::get<bool>(result.data);
  return result;
}

Value Interpreter::cast_value(const Value &val, PrimitiveType target_type) {
  Value result;
  result.type = target_type;

  // Simple cast implementation - handle common cases
  if (val.type == PrimitiveType::I32 && target_type == PrimitiveType::F64) {
    result.data = static_cast<double>(std::get<int32_t>(val.data));
  } else if (val.type == PrimitiveType::F64 && target_type == PrimitiveType::I32) {
    result.data = static_cast<int32_t>(std::get<double>(val.data));
  } else {
    // For now, just copy the value
    result = val;
    result.type = target_type;
  }

  return result;
}

} // namespace ng::orgasm
