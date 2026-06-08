#include <typecheck/receiver_effect_collector.hpp>
#include <typecheck/typecheck_utils.hpp>
#include <token.hpp>

namespace NG::typecheck
{
    void ReceiverEffectCollector::recordRead(ast::Expression *expr)
    {
        if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
        {
            effects.push_back(PlaceEffect{PlaceEffectKind::Read, *place});
        }
    }

    void ReceiverEffectCollector::recordWrite(ast::Expression *expr)
    {
        if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
        {
            effects.push_back(PlaceEffect{PlaceEffectKind::Write, *place});
        }
    }

    void ReceiverEffectCollector::recordMove(ast::Expression *expr)
    {
        if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
        {
            effects.push_back(PlaceEffect{PlaceEffectKind::Move, *place});
        }
    }

    void ReceiverEffectCollector::visit(ast::SimpleStatement *stmt)
    {
        if (stmt->expression) stmt->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::CompoundStatement *stmt)
    {
        for (auto &child : stmt->statements) child->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::ReturnStatement *stmt)
    {
        if (stmt->expression) stmt->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::IfStatement *stmt)
    {
        if (stmt->testing) stmt->testing->accept(this);
        if (stmt->consequence) stmt->consequence->accept(this);
        if (stmt->alternative) stmt->alternative->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::LoopStatement *stmt)
    {
        for (auto &binding : stmt->bindings)
        {
            if (binding.target) binding.target->accept(this);
        }
        if (stmt->loopBody) stmt->loopBody->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::NextStatement *stmt)
    {
        for (auto &expr : stmt->expressions) expr->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::ValDefStatement *stmt)
    {
        if (stmt->value) stmt->value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::ValueBindingStatement *stmt)
    {
        if (stmt->value) stmt->value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::UnaryExpression *expr)
    {
        if (!expr->optr) return;
        if (expr->optr->type == TokenType::KEYWORD_MOVE)
        {
            recordMove(expr->operand.get());
            return;
        }
        if (expr->optr->type == TokenType::KEYWORD_REF || expr->optr->type == TokenType::AMPERSAND)
        {
            if (relativeReceiverPlace(expr->operand.get(), receiverName).has_value())
            {
                unknown = true;
            }
            return;
        }
        if (expr->operand) expr->operand->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::BinaryExpression *expr)
    {
        if (expr->left) expr->left->accept(this);
        if (expr->right) expr->right->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::AssignmentExpression *expr)
    {
        if (expr->value) expr->value->accept(this);
        recordWrite(expr->target.get());
    }

    void ReceiverEffectCollector::visit(ast::IndexAssignmentExpression *expr)
    {
        if (expr->value) expr->value->accept(this);
        recordWrite(expr);
    }

    void ReceiverEffectCollector::visit(ast::IdExpression *expr) { recordRead(expr); }

    void ReceiverEffectCollector::visit(ast::IdAccessorExpression *expr)
    {
        const bool receiverPlace = relativeReceiverPlace(expr, receiverName).has_value();
        recordRead(expr);
        if (!receiverPlace && expr->primaryExpression) expr->primaryExpression->accept(this);
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::IndexAccessorExpression *expr)
    {
        const bool receiverPlace = relativeReceiverPlace(expr, receiverName).has_value();
        recordRead(expr);
        if (!receiverPlace && expr->primary) expr->primary->accept(this);
        if (expr->accessor) expr->accessor->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::FunCallExpression *expr)
    {
        if (relativeReceiverPlace(expr->primaryExpression.get(), receiverName).has_value())
        {
            unknown = true;
        }
        else if (expr->primaryExpression)
        {
            expr->primaryExpression->accept(this);
        }
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::QualifiedTraitCallExpression *expr)
    {
        if (expr->receiver && relativeReceiverPlace(expr->receiver.get(), receiverName).has_value())
        {
            unknown = true;
        }
        if (expr->receiver) expr->receiver->accept(this);
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::NewObjectExpression *expr)
    {
        for (auto &[_, value] : expr->properties) value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::ArrayLiteral *expr)
    {
        for (auto &element : expr->elements) element->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::TupleLiteral *expr)
    {
        for (auto &element : expr->elements) element->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::SpreadExpression *expr)
    {
        if (expr->expression) expr->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::TypeCheckingExpression *expr)
    {
        if (expr->value) expr->value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::CastExpression *expr)
    {
        if (expr->expression) expr->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::SwitchStatement *stmt)
    {
        if (stmt->scrutinee) stmt->scrutinee->accept(this);
        for (auto &caseBlock : stmt->cases)
        {
            if (caseBlock.body) caseBlock.body->accept(this);
        }
    }
} // namespace NG::typecheck
