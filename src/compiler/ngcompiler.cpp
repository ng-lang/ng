#include <compiler/ngcompiler.hpp>
#include <token.hpp>
#include <stdexcept>
#include <sstream>

namespace NG::compiler {

NGCompiler::NGCompiler(TypeIndex typeIndex)
    : typeIndex_(std::move(typeIndex))
    , currentFunction_(nullptr)
    , labelCounter_(0)
{
}

std::unique_ptr<ng::orgasm::Module> NGCompiler::compile(ASTRef<ASTNode> ast) {
    // Initialize a new module
    module_ = std::make_unique<ng::orgasm::Module>();
    module_->name = "default";
    
    // Visit the AST
    ast->accept(this);
    
    return std::move(module_);
}

void NGCompiler::visit(CompileUnit *compileUnit) {
    if (compileUnit->module) {
        compileUnit->module->accept(this);
    }
}

void NGCompiler::visit(NG::ast::Module *mod) {
    // Set module name if available
    if (!mod->name.empty()) {
        module_->name = mod->name;
    }
    
    // Process imports
    for (auto& importDecl : mod->imports) {
        importDecl->accept(this);
    }
    
    // Create start block if we have any module-level code
    bool needsStartBlock = !mod->statements.empty();
    
    // Check if any definitions are value definitions (which need start block)
    for (auto& def : mod->definitions) {
        if (def->astNodeType() == ASTNodeType::VAL_DEFINITION) {
            needsStartBlock = true;
            break;
        }
    }
    
    if (needsStartBlock) {
        module_->start_block = std::make_unique<ng::orgasm::StartBlock>();
        currentFunction_ = nullptr; // Indicate we're in start block
    }
    
    // Process definitions
    for (auto& def : mod->definitions) {
        def->accept(this);
    }
    
    // Process module-level statements
    for (auto& stmt : mod->statements) {
        stmt->accept(this);
    }
    
    // Add return at the end of start block if it exists
    if (module_->start_block) {
        int addr = getCurrentAddress();
        ng::orgasm::Instruction returnInstr(addr, OpCode::RETURN);
        module_->start_block->instructions.push_back(returnInstr);
    }
    
    // Set symbols list
    module_->symbols = symbols_;
}

void NGCompiler::visit(FunctionDef *funDef) {
    // Create a new function
    ng::orgasm::Function func;
    func.name = funDef->funName;
    
    // Add function to symbol table
    int funcIdx = addSymbol(funDef->funName);
    functionIndexMap_[funDef->funName] = static_cast<int>(module_->functions.size());
    
    // Save current function context
    ng::orgasm::Function* prevFunction = currentFunction_;
    currentFunction_ = &func;
    
    // Process parameters
    for (auto& param : funDef->params) {
        ng::orgasm::ParamDef paramDef;
        paramDef.name = param->paramName;
        
        // Get parameter type from type index
        auto paramTypeIt = typeIndex_.find(param->paramName);
        if (paramTypeIt != typeIndex_.end()) {
            paramDef.type = getOrgasmType(paramTypeIt->second);
        } else {
            paramDef.type = ng::orgasm::PrimitiveType::I32; // Default
        }
        
        func.params.push_back(paramDef);
        addSymbol(param->paramName);
    }
    
    // Process function body
    if (funDef->body) {
        funDef->body->accept(this);
    }
    
    // Add return at the end if not already present
    if (func.instructions.empty() || func.instructions.back().opcode != OpCode::RETURN) {
        int addr = getCurrentAddress();
        ng::orgasm::Instruction returnInstr(addr, OpCode::RETURN);
        func.instructions.push_back(returnInstr);
    }
    
    // Add function to module
    module_->functions.push_back(func);
    currentFunction_ = prevFunction;
}

void NGCompiler::visit(ValDef *valDef) {
    // ValDef is a wrapper around ValDefStatement or ValueBindingStatement
    // The actual compilation is handled by visiting the body
    if (valDef->body) {
        valDef->body->accept(this);
    }
}

void NGCompiler::visit(ValDefStatement *valDef) {
    // Get the type of the variable
    auto typeIt = typeIndex_.find(valDef->name);
    ng::orgasm::PrimitiveType varType = ng::orgasm::PrimitiveType::I32; // Default
    if (typeIt != typeIndex_.end()) {
        varType = getOrgasmType(typeIt->second);
    }
    
    // Add symbol
    addSymbol(valDef->name);
    
    // Add variable
    int varIdx = addVariable(valDef->name, varType);
    
    // Compile the value expression
    if (valDef->value) {
        valDef->value->accept(this);
        
        // Store the result
        emitInstruction(OpCode::STORE_VALUE, varType, {ng::orgasm::ValueOperand{varIdx}});
    }
}

void NGCompiler::visit(ValueBindingStatement *valBind) {
    // Compile the value expression
    if (valBind->value) {
        valBind->value->accept(this);
    }
    
    // TODO: Implement proper tuple/array unpacking
    // Handle bindings - for now just add symbols without unpacking logic
    for (auto& binding : valBind->bindings) {
        addSymbol(binding->name);
        // Full implementation would:
        // 1. Check binding type (TUPLE_UNPACK, ARRAY_UNPACK, etc.)
        // 2. Extract each value from the tuple/array
        // 3. Store to individual variables
    }
}

void NGCompiler::visit(CompoundStatement *compoundStmt) {
    for (auto& stmt : compoundStmt->statements) {
        stmt->accept(this);
    }
}

void NGCompiler::visit(SimpleStatement *simpleStmt) {
    if (simpleStmt->expression) {
        simpleStmt->expression->accept(this);
        // The expression value will be left on the stack
        // In ORGASM, we don't need an explicit IGNORE instruction
    }
}

void NGCompiler::visit(ReturnStatement *returnStmt) {
    if (returnStmt->expression) {
        returnStmt->expression->accept(this);
    }
    emitInstruction(OpCode::RETURN);
}

void NGCompiler::visit(IfStatement *ifStmt) {
    // Compile the condition
    if (ifStmt->testing) {
        ifStmt->testing->accept(this);
    }
    
    // Generate labels
    std::string trueLabel = generateLabel("if_true");
    std::string falseLabel = generateLabel("if_false");
    std::string endLabel = generateLabel("if_end");
    
    // Emit branch instruction
    int addr = getCurrentAddress();
    ng::orgasm::Instruction brInstr(addr, OpCode::BR);
    brInstr.operands.push_back(ng::orgasm::LabelOperand{trueLabel});
    brInstr.operands.push_back(ng::orgasm::LabelOperand{falseLabel});
    if (currentFunction_) {
        currentFunction_->instructions.push_back(brInstr);
    } else {
        module_->start_block->instructions.push_back(brInstr);
    }
    
    // True branch
    createLabel(trueLabel);
    if (ifStmt->consequence) {
        ifStmt->consequence->accept(this);
    }
    
    // Jump to end
    addr = getCurrentAddress();
    ng::orgasm::Instruction gotoEnd(addr, OpCode::GOTO);
    gotoEnd.operands.push_back(ng::orgasm::LabelOperand{endLabel});
    if (currentFunction_) {
        currentFunction_->instructions.push_back(gotoEnd);
    } else {
        module_->start_block->instructions.push_back(gotoEnd);
    }
    
    // False branch
    createLabel(falseLabel);
    if (ifStmt->alternative) {
        ifStmt->alternative->accept(this);
    }
    
    // End label
    createLabel(endLabel);
}

void NGCompiler::visit(LoopStatement *loopStatement) {
    // Generate labels
    std::string startLabel = generateLabel("loop_start");
    std::string endLabel = generateLabel("loop_end");
    
    createLabel(startLabel);
    
    // Compile loop body
    if (loopStatement->loopBody) {
        loopStatement->loopBody->accept(this);
    }
    
    // Jump back to start
    int addr = getCurrentAddress();
    ng::orgasm::Instruction gotoStart(addr, OpCode::GOTO);
    gotoStart.operands.push_back(ng::orgasm::LabelOperand{startLabel});
    if (currentFunction_) {
        currentFunction_->instructions.push_back(gotoStart);
    } else {
        module_->start_block->instructions.push_back(gotoStart);
    }
    
    createLabel(endLabel);
}

void NGCompiler::visit(NextStatement *nextStatement) {
    // Next statement - break out of the current loop
    // In a full implementation, this would need to track loop labels
    // For now, we just add a comment indicating this is a simplified implementation
    // TODO: Implement proper loop break with label tracking
}

void NGCompiler::visit(AssignmentExpression *assignmentExpr) {
    // Compile the value
    if (assignmentExpr->value) {
        assignmentExpr->value->accept(this);
    }
    
    // Get target variable
    if (assignmentExpr->target && assignmentExpr->target->astNodeType() == ASTNodeType::ID_EXPRESSION) {
        auto* idExpr = static_cast<IdExpression*>(assignmentExpr->target.get());
        std::string varName = idExpr->id;
        
        auto varIt = variableIndexMap_.find(varName);
        if (varIt != variableIndexMap_.end()) {
            auto typeIt = typeIndex_.find(varName);
            ng::orgasm::PrimitiveType varType = ng::orgasm::PrimitiveType::I32;
            if (typeIt != typeIndex_.end()) {
                varType = getOrgasmType(typeIt->second);
            }
            emitInstruction(OpCode::STORE_VALUE, varType, {ng::orgasm::ValueOperand{varIt->second}});
        }
    }
}

void NGCompiler::visit(BinaryExpression *binExpr) {
    // Compile left operand
    if (binExpr->left) {
        binExpr->left->accept(this);
    }
    
    // Compile right operand
    if (binExpr->right) {
        binExpr->right->accept(this);
    }
    
    // Get the operation type (default to I32)
    ng::orgasm::PrimitiveType opType = ng::orgasm::PrimitiveType::I32;
    
    // Emit the operation
    if (binExpr->optr) {
        switch (binExpr->optr->type) {
            case TokenType::PLUS:
                emitInstruction(OpCode::ADD, opType);
                break;
            case TokenType::MINUS:
                emitInstruction(OpCode::SUBTRACT, opType);
                break;
            case TokenType::TIMES:
                emitInstruction(OpCode::MULTIPLY, opType);
                break;
            case TokenType::DIVIDE:
                emitInstruction(OpCode::DIVIDE, opType);
                break;
            case TokenType::GT:
                emitInstruction(OpCode::GT, opType);
                break;
            case TokenType::LT:
                emitInstruction(OpCode::LT, opType);
                break;
            case TokenType::GE:
                emitInstruction(OpCode::GE, opType);
                break;
            case TokenType::LE:
                emitInstruction(OpCode::LE, opType);
                break;
            case TokenType::EQUAL:
                emitInstruction(OpCode::EQ, opType);
                break;
            case TokenType::NOT_EQUAL:
                emitInstruction(OpCode::NE, opType);
                break;
            default:
                // Unsupported operation
                break;
        }
    }
}

void NGCompiler::visit(UnaryExpression *unoExpr) {
    // Compile operand
    if (unoExpr->operand) {
        unoExpr->operand->accept(this);
    }
    
    // Handle unary operations
    if (unoExpr->optr) {
        switch (unoExpr->optr->type) {
            case TokenType::MINUS:
                // Negate by multiplying by -1
                {
                    int constIdx = addConstant(ng::orgasm::PrimitiveType::I32, ng::orgasm::Value(-1));
                    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::I32, {ng::orgasm::ConstOperand{constIdx}});
                    emitInstruction(OpCode::MULTIPLY, ng::orgasm::PrimitiveType::I32);
                }
                break;
            case TokenType::NOT:
                emitInstruction(OpCode::NOT, ng::orgasm::PrimitiveType::BOOL);
                break;
            default:
                break;
        }
    }
}

void NGCompiler::visit(FunCallExpression *funCallExpr) {
    // Compile arguments (in order)
    for (auto& arg : funCallExpr->arguments) {
        arg->accept(this);
        emitInstruction(OpCode::PUSH_PARAM);
    }
    
    // Get the function to call
    if (funCallExpr->primaryExpression && 
        funCallExpr->primaryExpression->astNodeType() == ASTNodeType::ID_EXPRESSION) {
        auto* idExpr = static_cast<IdExpression*>(funCallExpr->primaryExpression.get());
        std::string funcName = idExpr->id;
        
        // Determine call type (let interpreter handle return value)
        ng::orgasm::PrimitiveType callType = ng::orgasm::PrimitiveType::I32; // Default assumption
        
        // Check if it's already registered as an import
        auto importIt = importIndexMap_.find(funcName);
        if (importIt != importIndexMap_.end()) {
            emitInstruction(OpCode::CALL_IMPORT, callType, {ng::orgasm::ImportOperand{importIt->second}});
            return;
        }
        
        // Check if it's a defined function
        auto funcIt = functionIndexMap_.find(funcName);
        if (funcIt != functionIndexMap_.end()) {
            emitInstruction(OpCode::CALL, callType, {ng::orgasm::FunctionOperand{funcIt->second}});
            return;
        }
        
        // If not found, assume it's an external import from "core" module
        int importIdx = addImport(funcName, "core");
        emitInstruction(OpCode::CALL_IMPORT, callType, {ng::orgasm::ImportOperand{importIdx}});
    }
}

void NGCompiler::visit(IdExpression *idExpr) {
    std::string varName = idExpr->id;
    
    // Check if it's a variable
    auto varIt = variableIndexMap_.find(varName);
    if (varIt != variableIndexMap_.end()) {
        auto typeIt = typeIndex_.find(varName);
        ng::orgasm::PrimitiveType varType = ng::orgasm::PrimitiveType::I32;
        if (typeIt != typeIndex_.end()) {
            varType = getOrgasmType(typeIt->second);
        }
        emitInstruction(OpCode::LOAD_VALUE, varType, {ng::orgasm::ValueOperand{varIt->second}});
    }
    // Check if it's a parameter
    else if (currentFunction_) {
        for (size_t i = 0; i < currentFunction_->params.size(); ++i) {
            if (currentFunction_->params[i].name == varName) {
                emitInstruction(OpCode::LOAD_PARAM, currentFunction_->params[i].type, 
                              {ng::orgasm::ParamOperand{static_cast<int>(i)}});
                return;
            }
        }
    }
}

void NGCompiler::visit(IdAccessorExpression *idAccExpr) {
    // TODO: Implement field/property access
    // This requires:
    // 1. Loading the primary expression (object)
    // 2. Loading the field offset or calling a getter method
    // 3. Using LOAD_SYMBOL or similar instruction
    // For now, only load the primary expression
    if (idAccExpr->primaryExpression) {
        idAccExpr->primaryExpression->accept(this);
    }
}

void NGCompiler::visit(IndexAccessorExpression *index) {
    // TODO: Implement array/collection indexing
    // This requires emitting appropriate indexing instructions after loading
    // both the array and the index
    // For now, just load both values onto the stack
    if (index->primary) {
        index->primary->accept(this);
    }
    if (index->accessor) {
        index->accessor->accept(this);
    }
    // TODO: Add indexing instruction here
}

void NGCompiler::visit(IndexAssignmentExpression *index) {
    // TODO: Implement array/collection index assignment
    // This requires:
    // 1. Loading the array
    // 2. Loading the index  
    // 3. Loading the value
    // 4. Emitting an assignment instruction
    // For now, only compile the value expression
    if (index->value) {
        index->value->accept(this);
    }
}

// Integral value visitors
void NGCompiler::visit(IntegralValue<int8_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::I8, ng::orgasm::Value(static_cast<int32_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::I8, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<uint8_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::U8, ng::orgasm::Value(static_cast<int32_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::U8, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<int16_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::I16, ng::orgasm::Value(static_cast<int32_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::I16, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<uint16_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::U16, ng::orgasm::Value(static_cast<int32_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::U16, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<int32_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::I32, ng::orgasm::Value(intVal->value));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::I32, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<uint32_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::U32, ng::orgasm::Value(static_cast<uint32_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::U32, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<int64_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::I64, ng::orgasm::Value(intVal->value));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::I64, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(IntegralValue<uint64_t> *intVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::U64, ng::orgasm::Value(static_cast<uint64_t>(intVal->value)));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::U64, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(FloatingPointValue<float> *floatVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::F32, ng::orgasm::Value(floatVal->value));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::F32, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(FloatingPointValue<double> *floatVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::F64, ng::orgasm::Value(floatVal->value));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::F64, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(StringValue *strVal) {
    int strIdx = addString(strVal->value);
    emitInstruction(OpCode::LOAD_STR, ng::orgasm::PrimitiveType::ADDR, {ng::orgasm::StringOperand{strIdx}});
}

void NGCompiler::visit(BooleanValue *boolVal) {
    int constIdx = addConstant(ng::orgasm::PrimitiveType::BOOL, ng::orgasm::Value(boolVal->value));
    emitInstruction(OpCode::LOAD_CONST, ng::orgasm::PrimitiveType::BOOL, {ng::orgasm::ConstOperand{constIdx}});
}

void NGCompiler::visit(ArrayLiteral *array) {
    // TODO: Implement array literal compilation
    // This requires:
    // 1. Allocating array memory with LOAD_ARRAY
    // 2. Populating elements
    // 3. Pushing array address onto stack
    // For now, arrays are not supported in the initial implementation
}

void NGCompiler::visit(TupleLiteral *tuple) {
    // TODO: Implement tuple literal compilation
    // This requires:
    // 1. Calculating tuple size and layout
    // 2. Creating tuple with TUPLE_CREATE
    // 3. Setting each element with TUPLE_SET
    // For now, tuples are not supported in the initial implementation
}

void NGCompiler::visit(UnitLiteral *unit) {
    // Unit literal - no code generation needed
}

void NGCompiler::visit(ImportDecl *importDecl) {
    // Add imports to the module
    for (const auto& importName : importDecl->imports) {
        addImport(importName, importDecl->module);
    }
}

// Helper methods

ng::orgasm::PrimitiveType NGCompiler::getOrgasmType(CheckingRef<TypeInfo> typeInfo) {
    if (!typeInfo) {
        return ng::orgasm::PrimitiveType::UNIT;
    }
    
    switch (typeInfo->tag()) {
        case typeinfo_tag::I8:
            return ng::orgasm::PrimitiveType::I8;
        case typeinfo_tag::I16:
            return ng::orgasm::PrimitiveType::I16;
        case typeinfo_tag::I32:
            return ng::orgasm::PrimitiveType::I32;
        case typeinfo_tag::I64:
            return ng::orgasm::PrimitiveType::I64;
        case typeinfo_tag::U8:
            return ng::orgasm::PrimitiveType::U8;
        case typeinfo_tag::U16:
            return ng::orgasm::PrimitiveType::U16;
        case typeinfo_tag::U32:
            return ng::orgasm::PrimitiveType::U32;
        case typeinfo_tag::U64:
            return ng::orgasm::PrimitiveType::U64;
        case typeinfo_tag::F16:
            return ng::orgasm::PrimitiveType::F16;
        case typeinfo_tag::F32:
            return ng::orgasm::PrimitiveType::F32;
        case typeinfo_tag::F64:
            return ng::orgasm::PrimitiveType::F64;
        case typeinfo_tag::F128:
            return ng::orgasm::PrimitiveType::F128;
        case typeinfo_tag::BOOL:
            return ng::orgasm::PrimitiveType::BOOL;
        case typeinfo_tag::STRING:
            return ng::orgasm::PrimitiveType::ADDR;
        case typeinfo_tag::UNIT:
            return ng::orgasm::PrimitiveType::UNIT;
        default:
            return ng::orgasm::PrimitiveType::I32; // Default fallback
    }
}

int NGCompiler::addSymbol(const std::string& name) {
    auto it = symbolIndexMap_.find(name);
    if (it != symbolIndexMap_.end()) {
        return it->second;
    }
    
    int idx = static_cast<int>(symbols_.size());
    symbols_.push_back(name);
    symbolIndexMap_[name] = idx;
    return idx;
}

int NGCompiler::addConstant(ng::orgasm::PrimitiveType type, const ng::orgasm::Value& value) {
    // Create a key for the constant
    std::ostringstream key;
    key << static_cast<int>(type) << ":";
    
    // Add value to key (simplified)
    if (std::holds_alternative<int32_t>(value.data)) {
        key << std::get<int32_t>(value.data);
    } else if (std::holds_alternative<int64_t>(value.data)) {
        key << std::get<int64_t>(value.data);
    } else if (std::holds_alternative<bool>(value.data)) {
        key << std::get<bool>(value.data);
    }
    
    auto it = constIndexMap_.find(key.str());
    if (it != constIndexMap_.end()) {
        return it->second;
    }
    
    int idx = static_cast<int>(module_->constants.size());
    ng::orgasm::ConstDef constDef;
    constDef.type = type;
    constDef.value = value;
    module_->constants.push_back(constDef);
    constIndexMap_[key.str()] = idx;
    return idx;
}

int NGCompiler::addString(const std::string& value) {
    auto it = stringIndexMap_.find(value);
    if (it != stringIndexMap_.end()) {
        return it->second;
    }
    
    int idx = static_cast<int>(module_->strings.size());
    ng::orgasm::StringDef strDef;
    strDef.value = value;
    module_->strings.push_back(strDef);
    stringIndexMap_[value] = idx;
    return idx;
}

int NGCompiler::addVariable(const std::string& name, ng::orgasm::PrimitiveType type) {
    auto it = variableIndexMap_.find(name);
    if (it != variableIndexMap_.end()) {
        return it->second;
    }
    
    int idx = static_cast<int>(module_->variables.size());
    ng::orgasm::VarDef varDef;
    varDef.type = type;
    
    // Initialize with appropriate default value based on type
    switch (type) {
        case ng::orgasm::PrimitiveType::I8:
        case ng::orgasm::PrimitiveType::I16:
        case ng::orgasm::PrimitiveType::I32:
            varDef.initial_value = ng::orgasm::Value(static_cast<int32_t>(0));
            break;
        case ng::orgasm::PrimitiveType::I64:
            varDef.initial_value = ng::orgasm::Value(static_cast<int64_t>(0));
            break;
        case ng::orgasm::PrimitiveType::U8:
        case ng::orgasm::PrimitiveType::U16:
        case ng::orgasm::PrimitiveType::U32:
            varDef.initial_value = ng::orgasm::Value(static_cast<uint32_t>(0));
            break;
        case ng::orgasm::PrimitiveType::U64:
            varDef.initial_value = ng::orgasm::Value(static_cast<uint64_t>(0));
            break;
        case ng::orgasm::PrimitiveType::F32:
            varDef.initial_value = ng::orgasm::Value(static_cast<float>(0.0));
            break;
        case ng::orgasm::PrimitiveType::F64:
            varDef.initial_value = ng::orgasm::Value(static_cast<double>(0.0));
            break;
        case ng::orgasm::PrimitiveType::BOOL:
            varDef.initial_value = ng::orgasm::Value(false);
            break;
        default:
            varDef.initial_value = ng::orgasm::Value(); // Unit
            break;
    }
    
    module_->variables.push_back(varDef);
    variableIndexMap_[name] = idx;
    return idx;
}

int NGCompiler::addImport(const std::string& symbol, const std::string& moduleName) {
    auto it = importIndexMap_.find(symbol);
    if (it != importIndexMap_.end()) {
        return it->second;
    }
    
    int idx = static_cast<int>(module_->imports.size());
    ng::orgasm::Import import;
    import.symbol_name = symbol;
    import.module_name = moduleName;
    module_->imports.push_back(import);
    importIndexMap_[symbol] = idx;
    addSymbol(symbol);
    return idx;
}

int NGCompiler::getFunctionIndex(const std::string& name) {
    auto it = functionIndexMap_.find(name);
    if (it != functionIndexMap_.end()) {
        return it->second;
    }
    
    // Function not yet defined
    // In a two-pass compiler, this would be resolved in the second pass
    // For now, return 0 as a placeholder (assuming function is defined later)
    // TODO: Implement forward declaration support or two-pass compilation
    return 0;
}

std::string NGCompiler::generateLabel(const std::string& prefix) {
    return prefix + "_" + std::to_string(labelCounter_++);
}

void NGCompiler::emitInstruction(OpCode opcode, ng::orgasm::PrimitiveType type) {
    int addr = getCurrentAddress();
    ng::orgasm::Instruction instr(addr, opcode, type);
    
    if (currentFunction_) {
        currentFunction_->instructions.push_back(instr);
    } else if (module_->start_block) {
        module_->start_block->instructions.push_back(instr);
    }
}

void NGCompiler::emitInstruction(OpCode opcode, ng::orgasm::PrimitiveType type, const std::vector<ng::orgasm::Operand>& operands) {
    int addr = getCurrentAddress();
    ng::orgasm::Instruction instr(addr, opcode, type);
    instr.operands = operands;
    
    if (currentFunction_) {
        currentFunction_->instructions.push_back(instr);
    } else if (module_->start_block) {
        module_->start_block->instructions.push_back(instr);
    }
}

int NGCompiler::getCurrentAddress() {
    if (currentFunction_) {
        return static_cast<int>(currentFunction_->instructions.size());
    } else if (module_->start_block) {
        return static_cast<int>(module_->start_block->instructions.size());
    }
    return 0;
}

void NGCompiler::createLabel(const std::string& name) {
    int addr = getCurrentAddress();
    
    if (currentFunction_) {
        currentFunction_->labels[name] = addr;
    } else if (module_->start_block) {
        module_->start_block->labels[name] = addr;
    }
}

} // namespace NG::compiler
