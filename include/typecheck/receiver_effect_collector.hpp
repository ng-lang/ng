#pragma once

#include <typecheck/typeinfo.hpp>
#include <visitor.hpp>
#include <ast.hpp>

namespace NG::typecheck
{
    // Compute a static place key for an expression (e.g. "self.field[0]")
    auto staticPlaceKey(const ast::Expression *expr) -> std::optional<Str>;

    /**
     * @brief Collects read/write/move effects on a receiver's fields.
     *
     * Used by TypeChecker to determine which fields of 'self' are
     * read, written, or moved within a function body.
     */
    struct ReceiverEffectCollector : ast::DummyVisitor
    {
        Str receiverName;
        Vec<PlaceEffect> effects;
        bool unknown = false;

        explicit ReceiverEffectCollector(Str receiverName) : receiverName(std::move(receiverName)) {}

        void recordRead(ast::Expression *expr);
        void recordWrite(ast::Expression *expr);
        void recordMove(ast::Expression *expr);

        void visit(ast::SimpleStatement *stmt) override;
        void visit(ast::CompoundStatement *stmt) override;
        void visit(ast::ReturnStatement *stmt) override;
        void visit(ast::IfStatement *stmt) override;
        void visit(ast::LoopStatement *stmt) override;
        void visit(ast::NextStatement *stmt) override;
        void visit(ast::ValDefStatement *stmt) override;
        void visit(ast::ValueBindingStatement *stmt) override;
        void visit(ast::UnaryExpression *expr) override;
        void visit(ast::BinaryExpression *expr) override;
        void visit(ast::AssignmentExpression *expr) override;
        void visit(ast::IndexAssignmentExpression *expr) override;
        void visit(ast::IdExpression *expr) override;
        void visit(ast::IdAccessorExpression *expr) override;
        void visit(ast::IndexAccessorExpression *expr) override;
        void visit(ast::FunCallExpression *expr) override;
        void visit(ast::QualifiedTraitCallExpression *expr) override;
        void visit(ast::NewObjectExpression *expr) override;
        void visit(ast::ArrayLiteral *expr) override;
        void visit(ast::TupleLiteral *expr) override;
        void visit(ast::SpreadExpression *expr) override;
        void visit(ast::CastExpression *expr) override;
        void visit(ast::SwitchStatement *stmt) override;
    };
} // namespace NG::typecheck
