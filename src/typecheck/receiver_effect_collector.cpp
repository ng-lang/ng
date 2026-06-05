
#include <typecheck/receiver_effect_collector.hpp>
#include <ast.hpp>
#include <token.hpp>

namespace NG::typecheck
{
    auto staticPlaceKey(const ast::Expression *expr) -> std::optional<Str>
    {
        if (auto id = dynamic_cast<const ast::IdExpression *>(expr))
        {
            return id->id;
        }
        if (auto idAcc = dynamic_cast<const ast::IdAccessorExpression *>(expr))
        {
            if (!idAcc->arguments.empty())
            {
                return std::nullopt;
            }
            auto primary = staticPlaceKey(idAcc->primaryExpression.get());
            if (!primary.has_value())
            {
                return std::nullopt;
            }
            return *primary + "." + idAcc->accessor->repr();
        }
        if (auto index = dynamic_cast<const ast::IndexAccessorExpression *>(expr))
        {
            auto primary = staticPlaceKey(index->primary.get());
            if (!primary.has_value())
            {
                return std::nullopt;
            }
            if (auto intLit = dynamic_cast<const ast::IntegralValue<int32_t> *>(index->accessor.get()))
            {
                return *primary + "[" + std::to_string(intLit->value) + "]";
            }
            return std::nullopt;
        }
        if (auto index = dynamic_cast<const ast::IndexAssignmentExpression *>(expr))
        {
            auto primary = staticPlaceKey(index->primary.get());
            if (!primary.has_value())
            {
                return std::nullopt;
            }
            if (auto intLit = dynamic_cast<const ast::IntegralValue<int32_t> *>(index->accessor.get()))
            {
                return *primary + "[" + std::to_string(intLit->value) + "]";
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    auto relativeReceiverPlace(const ast::Expression *expr, const Str &receiverName) -> std::optional<Str>
    {
        auto place = staticPlaceKey(expr);
        if (!place.has_value())
        {
            return std::nullopt;
        }
        if (place->starts_with(receiverName + "."))
        {
            return place->substr(receiverName.size() + 1);
        }
        if (*place == receiverName)
        {
            return "";
        }
        return std::nullopt;
    }

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
        for (auto &b : stmt->bindings) if (b.target) b.target->accept(this);
        if (stmt->loopBody) stmt->loopBody->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::NextStatement *stmt)
    {
        for (auto &e : stmt->expressions) e->accept(this);
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
        if (expr->optr && expr->optr->type == TokenType::KEYWORD_MOVE)
        {
            recordMove(expr->operand.get());
        }
        else if (expr->operand)
        {
            expr->operand->accept(this);
        }
    }

    void ReceiverEffectCollector::visit(ast::BinaryExpression *expr)
    {
        if (expr->left) expr->left->accept(this);
        if (expr->right) expr->right->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::AssignmentExpression *expr)
    {
        recordWrite(expr->target.get());
        if (expr->value) expr->value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::IndexAssignmentExpression *expr)
    {
        recordWrite(expr);
        if (expr->value) expr->value->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::IdExpression *expr) { recordRead(expr); }

    void ReceiverEffectCollector::visit(ast::IdAccessorExpression *expr)
    {
        if (expr->primaryExpression) expr->primaryExpression->accept(this);
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::IndexAccessorExpression *expr)
    {
        if (expr->primary) expr->primary->accept(this);
        if (expr->accessor) expr->accessor->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::FunCallExpression *expr)
    {
        if (expr->primaryExpression) expr->primaryExpression->accept(this);
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::QualifiedTraitCallExpression *expr)
    {
        if (expr->receiver) expr->receiver->accept(this);
        for (auto &arg : expr->arguments) arg->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::NewObjectExpression *expr)
    {
        for (auto &[_, v] : expr->properties) v->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::ArrayLiteral *expr)
    {
        for (auto &e : expr->elements) e->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::TupleLiteral *expr)
    {
        for (auto &e : expr->elements) e->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::SpreadExpression *expr)
    {
        if (expr->expression) expr->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::CastExpression *expr)
    {
        if (expr->expression) expr->expression->accept(this);
    }

    void ReceiverEffectCollector::visit(ast::SwitchStatement *stmt)
    {
        if (stmt->scrutinee) stmt->scrutinee->accept(this);
        for (auto &c : stmt->cases)
        {
            if (c.body) c.body->accept(this);
        }
    }
} // namespace NG::typecheck
