
#include <typecheck/typecheck.hpp>
#include <debug.hpp>
#include <token.hpp>
namespace NG::typecheck
{
    using namespace NG::ast;

    constexpr inline bool isIntegralType(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return c >= code(typeinfo_tag::SIGNED) && c < code(typeinfo_tag::FLOATING_POINT);
    }
    constexpr inline bool isSigned(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(typeinfo_tag::SIGNED);
    }
    constexpr inline bool isPrimitive(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return c >= code(typeinfo_tag::PRIMITIVES) && c < code(typeinfo_tag::COLLECTION_TYPE);
    }
    constexpr inline bool isFloatingPoint(typeinfo_tag tag) noexcept
    {
        auto c = code(tag);
        return (c & 0xF0) == code(typeinfo_tag::FLOATING_POINT);
    }

    struct TypeChecker : DummyVisitor
    {
        Map<Str, CheckingRef<TypeInfo>> type_index{};

        Map<Str, CheckingRef<TypeInfo>> locals{};

        CheckingRef<TypeInfo> result;

        Vec<CheckingRef<TypeInfo>> contextRequirement;

        TypeChecker(Map<Str, CheckingRef<TypeInfo>> locals, Vec<CheckingRef<TypeInfo>> contextRequirement = {})
            : locals(locals), contextRequirement(contextRequirement)
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
            for (auto stmt : module->statements)
            {
                stmt->accept(this);
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
                auto localType = checker.result;
                // for funbody type checking
                if (checker.result->tag() == typeinfo_tag::PARAM_WITH_DEFAULT_VALUE)
                {
                    auto &paramWithDefault = static_cast<ParamWithDefaultValueType &>(*checker.result);
                    localType = paramWithDefault.paramType;
                }
                checker.locals.insert_or_assign(param->paramName, localType);
            }
            CheckingRef<TypeInfo> returnType;
            checker.contextRequirement = paramTypes;
            if (funDef->returnType)
            {
                funDef->returnType->accept(&checker);
                returnType = checker.result;
            }
            else
            {
                // No annotation provided: assume unit to keep type-checking total.
                returnType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
            }
            // todo: check function definition body to ensure return type corrects
            auto funType = makecheck<FunctionType>(returnType, paramTypes);
            if (funDef->body)
            {
                funDef->body->accept(&checker);
                auto bodyReturnType = checker.result;
                if (!bodyReturnType)
                {
                    bodyReturnType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
                }
                if (!returnType->match(*bodyReturnType))
                {
                    throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " + returnType->repr());
                }
            }
            if (!funDef->funName.empty())
            {
                locals.insert_or_assign(funDef->funName, funType);
            }
            result = funType;
        }

        void visit(SimpleStatement *simpleStatement) override
        {
            TypeChecker checker{locals};
            simpleStatement->expression->accept(&checker);
        }

        void visit(CompoundStatement *compoundStatement) override
        {
            TypeChecker checker{locals, contextRequirement};
            CheckingRef<TypeInfo> returnType = nullptr;
            for (auto stmt : compoundStatement->statements)
            {
                checker.result = nullptr;
                stmt->accept(&checker);
                if (checker.result)
                {
                    if (returnType)
                    {
                        if (!returnType->match(*checker.result))
                        {
                            if (checker.result->match(*returnType))
                            {
                                returnType = checker.result;
                            }
                            else
                            {
                                throw TypeCheckingException("Mismatched return types in compound statement: " +
                                                            returnType->repr() + ", " + checker.result->repr());
                            }
                        }
                    }
                    else
                    {
                        returnType = checker.result;
                    }
                }
            }
            result = returnType;
        }

        void visit(ReturnStatement *returnStatement) override
        {
            if (returnStatement->expression)
            {
                TypeChecker checker{locals};
                returnStatement->expression->accept(&checker);
                result = checker.result;
            }
            else
            {
                result = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
            }
        }

        void visit(NextStatement *nextStatement) override
        {
            if (nextStatement->expressions.size() != contextRequirement.size())
            {
                throw TypeCheckingException("Next statement argument count mismatch: " +
                                            std::to_string(nextStatement->expressions.size()) + " to " +
                                            std::to_string(contextRequirement.size()));
            }
            TypeChecker checker{locals};
            for (size_t i = 0; i < nextStatement->expressions.size(); ++i)
            {
                nextStatement->expressions[i]->accept(&checker);
                auto exprType = checker.result;
                auto reqType = contextRequirement[i];
                if (!reqType->match(*exprType))
                {
                    throw TypeCheckingException("Next statement argument type mismatch: " +
                                                exprType->repr() + " to " + reqType->repr());
                }
            }
        }

        void visit(IfStatement *ifStatement) override
        {
            TypeChecker checker{locals, contextRequirement};
            ifStatement->testing->accept(&checker);
            auto condType = checker.result;
            if (!condType || condType->tag() != typeinfo_tag::BOOL)
            {
                throw TypeCheckingException("Condition expression must be boolean: " + ifStatement->testing->repr());
            }
            CheckingRef<TypeInfo> returnType = nullptr;
            if (ifStatement->consequence)
            {
                ifStatement->consequence->accept(&checker);
                returnType = checker.result;
                result = returnType;
            }
            if (ifStatement->alternative)
            {
                ifStatement->alternative->accept(&checker);
                auto consequenceType = checker.result;
                if (returnType && consequenceType)
                {
                    if (returnType->match(*consequenceType))
                    {
                        result = returnType;
                    }
                    else if (consequenceType->match(*returnType))
                    {
                        result = consequenceType;
                    }
                    else
                    {
                        throw TypeCheckingException(
                            "Mismatched return types in if-else branches: " +
                            returnType->repr() + ", " + consequenceType->repr());
                    }
                }
                else if (consequenceType)
                {
                    result = consequenceType;
                }
            }
        }

        void visit(LoopStatement *loopStatement) override
        {
            TypeChecker checker{locals};
            Vec<CheckingRef<TypeInfo>> paramTypes;
            for (auto binding : loopStatement->bindings)
            {
                binding.target->accept(&checker);
                auto bindingType = checker.result;
                if (binding.annotation)
                {
                    binding.annotation->accept(&checker);
                    auto annoType = checker.result;
                    if (!annoType->match(*bindingType))
                    {
                        throw TypeCheckingException(
                            "Loop Binding Type Mismatch: " +
                            bindingType->repr() +
                            " to " +
                            annoType->repr());
                    }
                    bindingType = annoType;
                }
                // add to local scope
                checker.locals.insert_or_assign(binding.name, bindingType);
                paramTypes.push_back(bindingType);
            }
            checker.contextRequirement = paramTypes;
            loopStatement->loopBody->accept(&checker);
            result = checker.result;
        }

        void visit(ValDefStatement *valDefStatement) override
        {
            TypeChecker checker{locals};
            valDefStatement->value->accept(&checker);
            auto valType = checker.result;
            if (valDefStatement->typeAnnotation)
            {
                valDefStatement->typeAnnotation->accept(&checker);
                auto annoType = checker.result;

                if (annoType && annoType->match(*valType))
                {
                    locals.insert_or_assign(valDefStatement->name,
                                            annoType);
                }
                else
                {
                    throw TypeCheckingException("Value Define Type Mismatch: " + valType->repr() + " to " + annoType->repr());
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
            result = makecheck<PrimitiveType>(typeinfo_tag::STRING);
        }

        void visit(BooleanValue *value) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
        }
        void visit(IntegralValue<int8_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::I8);
        }
        void visit(IntegralValue<uint8_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::U8);
        }
        void visit(IntegralValue<int16_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::I16);
        }
        void visit(IntegralValue<uint16_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::U16);
        }
        void visit(IntegralValue<int32_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::I32);
        }
        void visit(IntegralValue<uint32_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::U32);
        }
        void visit(IntegralValue<int64_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::I64);
        }
        void visit(IntegralValue<uint64_t> *intVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::U64);
        }
        // void visit(FloatingPointValue<float16_t> *floatVal) override {}
        void visit(FloatingPointValue<float /* float32_t */> *floatVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::F32);
        }
        void visit(FloatingPointValue<double /* float64_t */> *floatVal) override
        {
            result = makecheck<PrimitiveType>(typeinfo_tag::F64);
        }
        // void AstVisitor::visit(FloatingPointValue<float128_t> *floatVal) override {}

        void visit(UnaryExpression *unoExpr) override
        {
            TypeChecker checker{locals};
            unoExpr->operand->accept(&checker);
            auto operandType = checker.result;
            switch (unoExpr->optr->type)
            {
            case TokenType::MINUS:
            {
                if (isPrimitive(operandType->tag()))
                {
                    PrimitiveType &primitive = static_cast<PrimitiveType &>(*operandType);
                    if (isSigned(primitive.tag()) || isFloatingPoint(primitive.tag()))
                    {
                        result = operandType;
                        return;
                    }
                }

                throw TypeCheckingException("Invalid operand type for negate operation.");
            }
            case TokenType::NOT:
            {
                result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
                return;
            }
            case TokenType::QUERY:
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
            if (isPrimitive(leftType->tag()))
            {
                PrimitiveType &leftPrimitive = static_cast<PrimitiveType &>(*leftType);
                switch (expression->optr->type)
                {
                case TokenType::MODULUS:
                case TokenType::LSHIFT:
                case TokenType::RSHIFT:
                    if (!isIntegralType(leftPrimitive.tag()))
                    {
                        throw TypeCheckingException("Invalid type for modulus: " + leftPrimitive.repr());
                    }
                    if (expression->optr->type != TokenType::MODULUS)
                    {
                        result = leftType;
                        return;
                    }
                    [[fallthrough]];
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::TIMES:
                case TokenType::DIVIDE:
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
                case TokenType::EQUAL:
                case TokenType::NOT_EQUAL:
                case TokenType::GE:
                case TokenType::GT:
                case TokenType::LE:
                case TokenType::LT:
                    if (leftPrimitive.match(*rightType) || rightType->match(leftPrimitive))
                    {
                        result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
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
            else if (leftType->tag() == typeinfo_tag::ARRAY)
            {
                ArrayType &arrayType = static_cast<ArrayType &>(*leftType);
                switch (expression->optr->type)
                {
                case TokenType::LSHIFT: // push to array
                    if (arrayType.elementType->match(*rightType))
                    {
                        result = leftType;
                        return;
                    }
                    else
                    {
                        throw TypeCheckingException("Invalid element type for array push: " + rightType->repr());
                    }
                // // TBD: Array comparison
                // case TokenType::EQUAL:
                // case TokenType::NOT_EQUAL:
                //     if (rightType->tag() == typeinfo_tag::ARRAY)
                //     {
                //         ArrayType &rightArrayType = static_cast<ArrayType &>(*rightType);
                //         if (arrayType.elementType->match(*rightArrayType.elementType) ||
                //             rightArrayType.elementType->match(*arrayType.elementType))
                //         {
                //             result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
                //             return;
                //         }
                //     }
                //     throw TypeCheckingException("Mismatch type on array comparison: " +
                //                                 leftType->repr() + ", " + rightType->repr());
                default:
                    throw TypeCheckingException("Unsupported operator for array types");
                }
            }
            else
            {
                throw TypeCheckingException("Unsupported type for binary expression: " + leftType->repr());
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
            if (typecode > code(TypeAnnotationType::BUILTIN) && typecode < code(TypeAnnotationType::END_OF_BUILTIN))
            {
                result = PrimitiveType::from(annotation->type);
                return;
            }
            else if (annotation->type == TypeAnnotationType::ARRAY)
            {
                if (annotation->arguments.size() == 1)
                {
                    auto arg = annotation->arguments[0];
                    TypeChecker checker{locals};
                    arg->accept(&checker);
                    auto argType = checker.result;
                    if (argType)
                    {
                        result = makecheck<ArrayType>(argType);
                        return;
                    }
                    throw TypeCheckingException("Unknown element type for array");
                }
                else
                {
                    throw TypeCheckingException("Array type expects exactly 1 type argument");
                }
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

        void visit(ArrayLiteral *arrayLit) override
        {
            if (arrayLit->elements.empty())
            {
                result = makecheck<ArrayType>(makecheck<Untyped>());
                return;
            }
            TypeChecker checker{locals};
            arrayLit->elements[0]->accept(&checker);
            auto elemType = checker.result;
            for (size_t i = 1; i < arrayLit->elements.size(); ++i)
            {
                arrayLit->elements[i]->accept(&checker);
                auto nextType = checker.result;
                if (!elemType->match(*nextType))
                {
                    if (nextType->match(*elemType))
                    {
                        elemType = nextType;
                    }
                    else
                    {
                        throw TypeCheckingException("Mismatched element type in array literal: " +
                                                    elemType->repr() + ", " + nextType->repr());
                    }
                }
            }
            result = makecheck<ArrayType>(elemType);
        }

        void visit(IndexAccessorExpression *indexAccess) override
        {
            TypeChecker checker{locals};
            indexAccess->primary->accept(&checker);
            auto primaryType = checker.result;
            if (!primaryType)
            {
                throw TypeCheckingException("Invalid index accessor expression: " + indexAccess->primary->repr());
            }
            if (primaryType->tag() != typeinfo_tag::ARRAY)
            {
                throw TypeCheckingException("Index accessor on non-array type: " + primaryType->repr());
            }
            indexAccess->accessor->accept(&checker);
            auto indexType = checker.result;
            if (!indexType || !isIntegralType(indexType->tag()))
            {
                throw TypeCheckingException("Invalid index type for array: " + indexAccess->accessor->repr());
            }
            ArrayType &arrayType = static_cast<ArrayType &>(*primaryType);
            result = arrayType.elementType;
        }

        void visit(IndexAssignmentExpression *indexAssign) override
        {
            TypeChecker checker{locals};
            indexAssign->primary->accept(&checker);
            auto primaryType = checker.result;
            if (!primaryType)
            {
                throw TypeCheckingException("Invalid index assignment expression: " + indexAssign->primary->repr());
            }
            if (primaryType->tag() != typeinfo_tag::ARRAY)
            {
                throw TypeCheckingException("Index assignment on non-array type: " + primaryType->repr());
            }
            indexAssign->accessor->accept(&checker);
            auto indexType = checker.result;
            if (!indexType || !isIntegralType(indexType->tag()))
            {
                throw TypeCheckingException("Invalid index type for array: " + indexAssign->accessor->repr());
            }
            indexAssign->value->accept(&checker);
            auto valueType = checker.result;
            ArrayType &arrayType = static_cast<ArrayType &>(*primaryType);
            if (!arrayType.elementType->match(*valueType))
            {
                throw TypeCheckingException("Invalid value type for array assignment: " + valueType->repr());
            }
            result = arrayType.elementType;
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