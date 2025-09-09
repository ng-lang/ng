
#include <typecheck/typecheck.hpp>
#include <debug.hpp>
#include <token.hpp>
namespace NG::typecheck
{

    struct TypeChecker : DummyVisitor
    {
        Map<Str, CheckingRef<TypeInfo>> type_index{};

        Map<Str, CheckingRef<TypeInfo>> locals{};

        CheckingRef<TypeInfo> result;

        TypeChecker(Map<Str, CheckingRef<TypeInfo>> locals)
            : locals(locals)
        {
        }

        void visit(CompileUnit *compileUnit) override
        {
            compileUnit->module->accept(this);
        }

        void visit(Module *module) override
        {
            for (auto def : module->definitions)
            {
                def->accept(this);
            }
            type_index.merge(locals);
        }

        void visit(ValDef *valDef) override
        {
            valDef->body->accept(this);
        }

        void visit(ValDefStatement *valDefStatement) override
        {
            TypeChecker checker{locals};
            valDefStatement->value->accept(&checker);
            auto valType = checker.result;
            debug_log("Val type", valType->repr());
            if (valDefStatement->typeAnnotation)
            {
                (*valDefStatement->typeAnnotation)->accept(&checker);
                auto annoType = checker.result;
                debug_log("AnnoType type", annoType->repr());

                if (annoType && annoType->match(*valType))
                {
                    locals.insert_or_assign(valDefStatement->name,
                                            annoType);
                }
                else
                {
                    throw TypeCheckingException("Type Mismatch: " + valType->repr() + " to " + annoType->repr());
                }
            }
            else
            {
                locals.insert_or_assign(valDefStatement->name,
                                        valType);
            }
        }

        void visit(StringValue *value) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::STRING);
        }

        void visit(BooleanValue *value) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::BOOL);
        }
        void visit(IntegralValue<int8_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::I8);
        }
        void visit(IntegralValue<uint8_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::U8);
        }
        void visit(IntegralValue<int16_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::I16);
        }
        void visit(IntegralValue<uint16_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::U16);
        }
        void visit(IntegralValue<int32_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::I32);
        }
        void visit(IntegralValue<uint32_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::U32);
        }
        void visit(IntegralValue<int64_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::I64);
        }
        void visit(IntegralValue<uint64_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::U64);
        }
        // void visit(FloatingPointValue<float16_t> *floatVal) override {}
        void visit(FloatingPointValue<float /* float32_t */> *floatVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::F32);
        }
        void visit(FloatingPointValue<double /* float64_t */> *floatVal) override
        {
            result = makecheck<PrimitiveType>(primitive_tag::F64);
        }
        // void AstVisitor::visit(FloatingPointValue<float128_t> *floatVal) override {}

        void visit(BinaryExpression *expression) override
        {
            TypeChecker checker{locals};
            expression->left->accept(&checker);
            auto leftType = checker.result;
            expression->right->accept(&checker);
            auto rightType = checker.result;
            if (leftType->tag() == typeinfo_tag::PRIMITIVE)
            {
                PrimitiveType &leftPrimitive = static_cast<PrimitiveType &>(*leftType);
                switch (expression->optr->operatorType)
                {
                case Operators::MODULUS:
                case Operators::LSHIFT:
                case Operators::RSHIFT:
                    if (code(leftPrimitive.primitive()) < code(primitive_tag::SIGNED) ||
                        code(leftPrimitive.primitive()) > code(primitive_tag::FLOATING_POINT))
                    {
                        throw TypeCheckingException("Invalid type for modulus: " + leftPrimitive.repr());
                    }
                    if (expression->optr->operatorType != Operators::MODULUS)
                    {
                        result = leftType;
                        return;
                    }
                case Operators::PLUS:
                case Operators::MINUS:
                case Operators::TIMES:
                case Operators::DIVIDE:
                    if (leftPrimitive.match(*rightType))
                    {
                        result = leftType;
                    }
                    else if (rightType->match(leftPrimitive))
                    {
                        result = rightType;
                    }
                    else
                    {
                        throw TypeCheckingException("Mismatch type on arithmetic operation: " +
                                                    leftPrimitive.repr() + ", " + rightType->repr());
                    }
                    return;
                case Operators::EQUAL:
                case Operators::NOT_EQUAL:
                case Operators::GE:
                case Operators::GT:
                case Operators::LE:
                case Operators::LT:
                    if (leftPrimitive.match(*rightType) || rightType->match(leftPrimitive))
                    {
                        result = makecheck<PrimitiveType>(primitive_tag::BOOL);
                    }
                    else
                    {
                        throw TypeCheckingException("Mismatch type on comparison operators: " +
                                                    leftPrimitive.repr() + ", " + rightType->repr());
                    }
                    return;
                default:
                    throw TypeCheckingException("Unsupported operator for primitive types");
                }
            }
        }

        void visit(TypeAnnotation *annotation) override
        {
            auto typecode = code(annotation->type);
            if (typecode > code(TypeAnnotationType::BUILTIN) && typecode < code(TypeAnnotationType::CUSTOMIZED))
            {
                result = PrimitiveType::from(annotation->type);
            }
            else
            {
                result = locals.at(annotation->name);
            }
        }
    };

    TypeIndex type_check(ASTRef<ASTNode> ast)
    {
        TypeChecker checker{{}};
        ast->accept(&checker);

        return checker.type_index;
    }
}