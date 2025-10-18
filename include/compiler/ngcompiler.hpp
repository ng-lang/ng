#pragma once

#include <ast.hpp>
#include <visitor.hpp>
#include <orgasm/module.hpp>
#include <orgasm/instruction.hpp>
#include <orgasm/types.hpp>
#include <typecheck/typecheck.hpp>
#include <typecheck/typeinfo.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

namespace NG::compiler {

using namespace NG::ast;
using namespace NG::typecheck;
using namespace ng::orgasm;

/**
 * @brief Compiler from NG AST to ORGASM Level-2 assembly
 * 
 * This class implements a visitor pattern to traverse the NG AST
 * and generate ORGASM Level-2 assembly instructions.
 */
class NGCompiler : public DummyVisitor {
public:
    /**
     * @brief Construct a new NGCompiler
     * 
     * @param typeIndex Type information from type checking
     */
    explicit NGCompiler(TypeIndex typeIndex);

    /**
     * @brief Compile an AST node to ORGASM module
     * 
     * @param ast The AST node to compile
     * @return std::unique_ptr<ng::orgasm::Module> The compiled ORGASM module
     */
    std::unique_ptr<ng::orgasm::Module> compile(ASTRef<ASTNode> ast);

    // Visitor methods
    void visit(CompileUnit *compileUnit) override;
    void visit(Module *mod) override;
    void visit(FunctionDef *funDef) override;
    void visit(ValDef *valDef) override;
    void visit(ValDefStatement *valDef) override;
    void visit(ValueBindingStatement *valBind) override;
    void visit(CompoundStatement *compoundStmt) override;
    void visit(SimpleStatement *simpleStmt) override;
    void visit(ReturnStatement *returnStmt) override;
    void visit(IfStatement *ifStmt) override;
    void visit(LoopStatement *loopStatement) override;
    void visit(NextStatement *nextStatement) override;
    void visit(AssignmentExpression *assignmentExpr) override;
    void visit(BinaryExpression *binExpr) override;
    void visit(UnaryExpression *unoExpr) override;
    void visit(FunCallExpression *funCallExpr) override;
    void visit(IdExpression *idExpr) override;
    void visit(IdAccessorExpression *idAccExpr) override;
    void visit(IndexAccessorExpression *index) override;
    void visit(IndexAssignmentExpression *index) override;
    void visit(IntegralValue<int8_t> *intVal) override;
    void visit(IntegralValue<uint8_t> *intVal) override;
    void visit(IntegralValue<int16_t> *intVal) override;
    void visit(IntegralValue<uint16_t> *intVal) override;
    void visit(IntegralValue<int32_t> *intVal) override;
    void visit(IntegralValue<uint32_t> *intVal) override;
    void visit(IntegralValue<int64_t> *intVal) override;
    void visit(IntegralValue<uint64_t> *intVal) override;
    void visit(FloatingPointValue<float> *floatVal) override;
    void visit(FloatingPointValue<double> *floatVal) override;
    void visit(StringValue *strVal) override;
    void visit(BooleanValue *boolVal) override;
    void visit(ArrayLiteral *array) override;
    void visit(TupleLiteral *tuple) override;
    void visit(UnitLiteral *unit) override;
    void visit(ImportDecl *importDecl) override;

private:
    // Type information
    TypeIndex typeIndex_;
    
    // Current module being compiled
    std::unique_ptr<ng::orgasm::Module> module_;
    
    // Current function being compiled
    ng::orgasm::Function* currentFunction_;
    
    // Symbol tracking
    std::unordered_map<std::string, int> symbolIndexMap_;
    std::vector<std::string> symbols_;
    
    // Variable tracking (maps variable name to index in .val section)
    std::unordered_map<std::string, int> variableIndexMap_;
    
    // Constant tracking (maps value to index)
    std::unordered_map<std::string, int> constIndexMap_;
    std::unordered_map<std::string, int> stringIndexMap_;
    
    // Import tracking (maps import name to index)
    std::unordered_map<std::string, int> importIndexMap_;
    
    // Function tracking (maps function name to index)
    std::unordered_map<std::string, int> functionIndexMap_;
    
    // Label counter for generating unique labels
    int labelCounter_;
    
    // Helper methods
    
    /**
     * @brief Get the ORGASM type for a given TypeInfo
     */
    ng::orgasm::PrimitiveType getOrgasmType(CheckingRef<TypeInfo> typeInfo);
    
    /**
     * @brief Add a symbol to the symbol table
     */
    int addSymbol(const std::string& name);
    
    /**
     * @brief Add a constant to the constant pool
     */
    int addConstant(ng::orgasm::PrimitiveType type, const ng::orgasm::Value& value);
    
    /**
     * @brief Add a string to the string pool
     */
    int addString(const std::string& value);
    
    /**
     * @brief Add a variable to the variable table
     */
    int addVariable(const std::string& name, ng::orgasm::PrimitiveType type);
    
    /**
     * @brief Add an import declaration
     */
    int addImport(const std::string& symbol, const std::string& module);
    
    /**
     * @brief Get or create a function index
     */
    int getFunctionIndex(const std::string& name);
    
    /**
     * @brief Generate a unique label name
     */
    std::string generateLabel(const std::string& prefix);
    
    /**
     * @brief Emit an instruction to the current function
     */
    void emitInstruction(OpCode opcode, ng::orgasm::PrimitiveType type = ng::orgasm::PrimitiveType::UNIT);
    
    /**
     * @brief Emit an instruction with operands
     */
    void emitInstruction(OpCode opcode, ng::orgasm::PrimitiveType type, const std::vector<Operand>& operands);
    
    /**
     * @brief Get the current instruction address
     */
    int getCurrentAddress();
    
    /**
     * @brief Create a label at the current address
     */
    void createLabel(const std::string& name);
};

} // namespace NG::compiler
