#pragma once

#include <ast.hpp>
#include <orgasm/module.hpp>
#include <visitor.hpp>

namespace NG::orgasm
{
    /**
     * @brief A compiler that translates NG AST to ORGASM bytecode.
     */
    class Compiler : public ast::DummyVisitor
    {
      public:
        explicit Compiler(Vec<Str> modulePaths = {}, Vec<Str> nativeFunctions = {})
            : modulePaths(std::move(modulePaths)), nativeFnNames(nativeFunctions.begin(), nativeFunctions.end()) {}

        /**
         * @brief Compiles a compile unit to a bytecode module.
         *
         * @param compileUnit The compile unit to compile.
         * @return The compiled bytecode module.
         */
        auto compile(ast::ASTRef<ast::CompileUnit> compileUnit) -> BytecodeModule;

        void visit(ast::Module *mod) override;
        void visit(ast::FunctionDef *funDef) override;
        void visit(ast::FunCallExpression *funCallExpr) override;
        void visit(ast::IdAccessorExpression *idAccExpr) override;
        void visit(ast::IndexAccessorExpression *idxAccExpr) override;
        void visit(ast::IndexAssignmentExpression *idxAssignExpr) override;
        void visit(ast::CompoundStatement *compoundStmt) override;
        void visit(ast::SimpleStatement *simpleStmt) override;
        void visit(ast::LoopStatement *loopStmt) override;
        void visit(ast::NextStatement *nextStmt) override;
        void visit(ast::TypeOfExpression *typeofExpr) override;
        void visit(ast::TypeCheckingExpression *typeCheck) override;
        void visit(ast::SpreadExpression *spreadExpr) override;
        void visit(ast::ValueBindingStatement *valBind) override;
        void visit(ast::ValDefStatement *valDefStmt) override;
        void visit(ast::NewObjectExpression *newObj) override;
        void visit(ast::Binding *binding) override;
        void visit(ast::TypeDef *typeDef) override;
        void visit(ast::ImplDef *implDef) override;
        void visit(ast::TypeAliasDef *typeAliasDef) override;
        void visit(ast::NewTypeDef *newTypeDef) override;
        void visit(ast::CastExpression *castExpr) override;
        void visit(ast::TaggedUnionDef *taggedUnionDef) override;
        void visit(ast::SwitchStatement *switchStmt) override;
        void visit(ast::ImportDecl *importDecl) override;
        void visit(ast::ReturnStatement *returnStmt) override;
        void visit(ast::BinaryExpression *binExpr) override;
        void visit(ast::AssignmentExpression *assignExpr) override;
        void visit(ast::UnaryExpression *unaryExpr) override;
        void visit(ast::IfStatement *ifStmt) override;
        void visit(ast::UnitLiteral *unitLit) override;
        
        void visit(ast::IntegralValue<int8_t> *intVal) override;
        void visit(ast::IntegralValue<uint8_t> *intVal) override;
        void visit(ast::IntegralValue<int16_t> *intVal) override;
        void visit(ast::IntegralValue<uint16_t> *intVal) override;
        void visit(ast::IntegralValue<int32_t> *intVal) override;
        void visit(ast::IntegralValue<uint32_t> *intVal) override;
        void visit(ast::IntegralValue<int64_t> *intVal) override;
        void visit(ast::IntegralValue<uint64_t> *intVal) override;

        void visit(ast::FloatingPointValue<float> *floatVal) override;
        void visit(ast::FloatingPointValue<double> *floatVal) override;

        void visit(ast::StringValue *strVal) override;
        void visit(ast::BooleanValue *boolVal) override;
        void visit(ast::ArrayLiteral *arrayLit) override;
        void visit(ast::TupleLiteral *tupleLit) override;
        void visit(ast::IdExpression *idExpr) override;

      private:
        BytecodeModule module;
        Function *current_function = nullptr;
        struct LoopInfo {
            size_t startIp;
            Vec<int32_t> bindingSlots;
        };
        struct ImportedSymbol {
            Str moduleName;
            int32_t importIndex;
        };
        Map<Str, int32_t> locals;
        Map<Str, int32_t> globals;
        Map<Str, ImportedSymbol> imported_symbols;
        Map<Str, ast::FunctionDef*> functionDefs;
        Vec<LoopInfo> loop_stack;
        Vec<Str> modulePaths;
        Set<Str> nativeFnNames;
        Str current_type_name;  // Current type being compiled (for member functions)
        bool last_emit_was_return = false;

        // Tagged union tracking: variant name -> (union type name, variant index)
        struct VariantInfo {
            Str unionName;
            int32_t variantIndex;
            Vec<Str> payloadFields;
        };
        Map<Str, VariantInfo> variant_map;

        void emit(OpCode op);
        void emit_u8(uint8_t val);
        void emit_u16(uint16_t val);
        void emit_reference(ast::ASTRef<ast::Expression> expr);
        void emit_move_place(ast::ASTRef<ast::Expression> expr);

        // Find the field index of a property in the current type. Returns -1 if not found.
        auto find_field_index(const Str &propertyName) const -> int32_t;
        void emit_i32(int32_t val);
        void emit_i64(int64_t val);
        void emit_f32(float val);
        void emit_f64(double val);
        void patch_i32(size_t offset, int32_t val);

        // Compile-time constant expression evaluation for `const if`
        bool evaluate_const_bool(ast::ASTRef<ast::Expression> expr);
    };
} // namespace NG::orgasm
