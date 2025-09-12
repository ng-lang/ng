
#include <typecheck/typecheck.hpp>
#include <debug.hpp>
#include <token.hpp>
namespace NG::typecheck
{
    using namespace NG::ast;

    constexpr inline bool isIntegralType(primitive_tag tag) noexcept
    {
        auto c = code(tag);
        return c >= code(primitive_tag::SIGNED) && c < code(FLOATING_POINT);
    }
    constexpr inline bool isSigned(primitive_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(primitive_tag::SIGNED);
    }
    constexpr inline bool isFloatingPoint(primitive_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(primitive_tag::FLOATING_POINT);
    }

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

        void visit(FunctionDef *funDef) override
        {
            TypeChecker checker{locals};
            Vec<CheckingRef<TypeInfo>> paramTypes;
            for (auto param : funDef->params)
            {
                param->accept(&checker);
                paramTypes.push_back(checker.result);
            }
            CheckingRef<TypeInfo> returnType;
            if (funDef->returnType)
            {
                funDef->returnType->accept(&checker);
                returnType = checker.result;
            }
            else
            {
                // No annotation provided: assume unit to keep type-checking total.
                returnType = makecheck<PrimitiveType>(primitive_tag::UNIT);
            }
            // todo: check function definition body to ensure return type corrects
            auto funType = makecheck<FunctionType>(returnType, paramTypes);

            if (!funDef->funName.empty())
            {
                locals.insert_or_assign(funDef->funName, funType);
            }
            result = funType;
        }

        void visit(ValDefStatement *valDefStatement) override
        {
            TypeChecker checker{locals};
            valDefStatement->value->accept(&checker);
            auto valType = checker.result;
            if (valDefStatement->typeAnnotation)
            {
                (*valDefStatement->typeAnnotation)->accept(&checker);
                auto annoType = checker.result;

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

        void visit(UnaryExpression *unoExpr) override
        {
            TypeChecker checker{locals};
            unoExpr->operand->accept(&checker);
            auto operandType = checker.result;
            switch (unoExpr->optr->operatorType)
            {
            case Operators::MINUS:
            {
                if (operandType->tag() == typeinfo_tag::PRIMITIVE)
                {
                    PrimitiveType &primitive = static_cast<PrimitiveType &>(*operandType);
                    if (isSigned(primitive.primitive()) || isFloatingPoint(primitive.primitive()))
                    {
                        result = operandType;
                        return;
                    }
                }

                throw TypeCheckingException("Invalid operand type for negate operation.");
            }
            case Operators::NOT:
            {
                result = makecheck<PrimitiveType>(primitive_tag::BOOL);
                return;
            }
            case Operators::QUERY:
            {
                throw TypeCheckingException("Not supported operator QUERY (?).");
            }
            default:
                throw TypeCheckingException("Unsupported unary operator.");
            }
        }

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
                    if (!isIntegralType(leftPrimitive.primitive()))
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

        void visit(Param *param) override
        {
            if (param->type == ParamType::Simple || param->annotatedType)
            {
                result = makecheck<Untyped>();
            }
            TypeChecker checker{locals};
            if (param->annotatedType)
            {
                param->annotatedType->accept(&checker);
                if (!checker.result)
                {
                    throw TypeCheckingException("Unknown type annotation for parameter: " + param->paramName);
                }
                result = checker.result;
            }
            if (param->value)
            {
                param->value->accept(&checker);
                auto valueType = checker.result;
                if (valueType)
                {
                    if (result->tag() != typeinfo_tag::UNTYPED)
                    {
                        if (!result->match(*valueType))
                        {
                            throw TypeCheckingException("Invalid default value for type: " + result->repr());
                        }
                        result = makecheck<ParamWithDefaultValueType>(result);
                    }
                    else
                    {
                        result = makecheck<ParamWithDefaultValueType>(valueType);
                    }
                }
                else
                {
                    throw TypeCheckingException("Unexpected default expression type for parameter: " + param->paramName);
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
                auto it = locals.find(annotation->name);
                if (it != locals.end())
                {
                    result = it->second;
                }
                else
                {
                    throw TypeCheckingException("Unknown type: " + annotation->name);
                }
            }
        }

        void visit(IdExpression *id) override
        {
            auto it = locals.find(id->id);
            if (it != locals.end())
            {
                result = it->second;
            }
            else
            {
                throw TypeCheckingException("Unknown type for object: " + id->id);
            }
        }

        void visit(FunCallExpression *funCall) override
        {
            TypeChecker checker{locals};
            funCall->primaryExpression->accept(&checker);
            auto primaryType = checker.result;
            if (!primaryType)
            {
                throw TypeCheckingException("Invalid function call expression: " + funCall->primaryExpression->repr());
            }

            auto funcType = dynamic_cast<FunctionType *>(&(*primaryType));

            if (!funcType)
            {
                throw TypeCheckingException("Invalid function type: " + primaryType->repr());
            }

            Vec<CheckingRef<TypeInfo>> argumentTypes;
            for (auto arg : funCall->arguments)
            {
                arg->accept(&checker);
                argumentTypes.push_back(checker.result);
            }

            if (!funcType->applyWith(argumentTypes))
            {
                throw TypeCheckingException("Invalid argument types for function: " + funcType->repr());
            }
            result = funcType->returnType;
        }
    };

    TypeIndex type_check(ASTRef<ASTNode> ast)
    {
        TypeChecker checker{{}};
        ast->accept(&checker);

        return checker.type_index;
    }
}