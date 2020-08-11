#include "ast.hpp"
#include <token.hpp>
#include "ropto.hpp"
#include <type_traits>

namespace ropto {
    using namespace NG::AST;

    template<class T, typename = std::enable_if_t<std::is_base_of<ASTNode, T>::value>>
    void write(T* value, byte_stream &stream) {
        write(value->astNodeType(), stream);
    }
} // namespace ropto

namespace NG::AST {
    using namespace ropto;

    class ASTSerializer : public virtual DefaultDummyAstVisitor {

    public:
        byte_stream stream;

        ASTSerializer() : stream{} {}

        void visit(Module *mod) override {
            stream << mod << mod->name;
            stream << mod->definitions.size();
            for (auto def : mod->definitions) {
                def->accept(this);
            }
            stream << mod->statements.size();
            for (const auto &item : mod->statements) {
                item->accept(this);
            }
        }

        void visit(SimpleStatement *simpleStmt) override {
            stream << simpleStmt;
            simpleStmt->expression->accept(this);
        }

        void visit(ReturnStatement *returnStmt) override {
            stream << returnStmt;
            returnStmt->expression->accept(this);
        }

        void visit(CompoundStatement *compoundStmt) override {
            stream << compoundStmt << compoundStmt->statements.size();
            for (auto stmt : compoundStmt->statements) {
                stmt->accept(this);
            }
        }

        void visit(IfStatement *ifStmt) override {
            stream << ifStmt;
            ifStmt->testing->accept(this);
            ifStmt->consequence->accept(this);
            stream << (ifStmt->alternative != nullptr);
            if (ifStmt->alternative != nullptr) {
                ifStmt->alternative->accept(this);
            }
        }

        void visit(ValDefStatement *valDef) override {
            stream << valDef << valDef->name;
            valDef->value->accept(this);
        }

        void visit(Param *param) override {
            stream << param << param->paramName << param->type << param->annotatedType;
        }

        void visit(FunctionDef *funDef) override {
            stream << funDef << funDef->funName;
            stream << funDef->params.size();
            for (auto param : funDef->params) {
                param->accept(this);
            }
            funDef->body->accept(this);
        }

        void visit(ValDef *valDef) override {
            stream << valDef;
            valDef->body->accept(this);
        }

        void visit(IdExpression *idExpr) override {
            stream << idExpr << idExpr->id;
        }

        void visit(FunCallExpression *funCallExpr) override {
            stream << funCallExpr;
            funCallExpr->primaryExpression->accept(this);
            stream << funCallExpr->arguments.size();
            for (auto arg : funCallExpr->arguments) {
                arg->accept(this);
            }
        }

        void visit(IdAccessorExpression *idAccExpr) override {
            stream << idAccExpr;
            idAccExpr->primaryExpression->accept(this);
            idAccExpr->accessor->accept(this);
        }

        void visit(BinaryExpression *binExpr) override {
            stream << binExpr;
            auto &&optr = binExpr->optr;
            stream << optr->type << optr->operatorType << optr->repr;
            binExpr->left->accept(this);
            binExpr->right->accept(this);
        }

        void visit(AssignmentExpression *assignmentExpr) override {
            stream << assignmentExpr << assignmentExpr->name;
            assignmentExpr->value->accept(this);
        }

        void visit(IntegerValue *intVal) override {
            stream << intVal << intVal->value;
        }

        void visit(StringValue *strVal) override {
            stream << strVal << strVal->value;
        }

        void visit(BooleanValue *boolVal) override {
            stream << boolVal << boolVal->value;
        }

        void visit(ArrayLiteral *array) override {
            stream << array;
            stream << array->elements.size();
            for (const auto &element : array->elements) {
                element->accept(this);
            }
        }

        ~ASTSerializer() override = default;
    };

    std::vector<uint8_t> serialize_ast(ASTRef<ASTNode> node) {
        ASTSerializer serializer;
        serializer.stream << ASTNodeType::NODE;
        node->accept(&serializer);
        return serializer.stream.iterate();
    }

    class ASTDeserializer {
        byte_stream stream;

        void withSize(const std::function<void(std::size_t)> &fn) {
            std::size_t size;
            stream >> size;
            for (auto i = 0; i < size; i++) {
                fn(i);
            }
        }

        static void assertASTNodeType(ASTNodeType expected, ASTNodeType actual) {
            if (expected != actual) {
                throw ParseException("Unexpected ast node type");
            }
        }

    public:
        explicit ASTDeserializer(std::vector<uint8_t> &bytes) : stream{bytes} {}

        ASTRef<ASTNode> deserialize() {
            ASTNodeType type;
            stream >> type;

            if (static_cast<uint32_t>(type) != 0xdeadbeef) {
                throw ParseException("Unknown magic number");
            }
            return expect<Module>();
        }

        template<class T, typename = std::enable_if_t<std::is_base_of<ASTNode, T>::value>>
        ASTRef<T> expect() {
            ASTRef<T> ref = nullptr;
            ASTNodeType type;
            stream >> type;
            return dynamic_cast<ASTRef<T>>(expectNode(ref, type));
        }

        ASTRef<Module> expectNode(ASTRef<Module> def, ASTNodeType type) {
            assertASTNodeType(ASTNodeType::MODULE, type);
            NG::Str moduleName;
            stream >> moduleName;

            auto mod = makeast<Module>(moduleName);
            withSize([&](std::size_t) {
                mod->definitions.push_back(expect<Definition>());
            });
            withSize([&](std::size_t) {
                mod->statements.push_back(expect<Statement>());
            });
            return mod;
        }

        ASTRef<Definition> expectNode(ASTRef<Definition> def, ASTNodeType type) {
            switch (type) {
                case ASTNodeType::FUN_DEFINITION: {
                    auto funDef = makeast<FunctionDef>();
                    stream >> funDef->funName;

                    withSize([&](std::size_t) {
                        funDef->params.push_back(expect<Param>());
                    });
                    funDef->body = expect<Statement>();
                    return funDef;
                }
                case ASTNodeType::VAL_DEFINITION: {
                    auto defStmt = expect<ValDefStatement>();
                    auto valDef = makeast<ValDef>(defStmt);

                    return valDef;
                }

                default:
                    throw ParseException("Unknown AST node type.");
            }
        }

        ASTRef<Expression> expectNode(ASTRef<Expression> expr, ASTNodeType type) {
            switch (type) {
                case ASTNodeType::ID_EXPRESSION: {
                    NG::Str id;
                    stream >> id;
                    return makeast<IdExpression>(id);
                }
                case ASTNodeType::BINARY_EXPRESSION: {
                    Token token;
                    stream >> token.type >> token.operatorType >> token.repr;
                    auto binexpr = makeast<BinaryExpression>();
                    binexpr->optr = new Token{token};
                    binexpr->left = expect<Expression>();
                    binexpr->right = expect<Expression>();
                    return binexpr;
                }
                case ASTNodeType::FUN_CALL_EXPRESSION: {
                    auto funCall = makeast<FunCallExpression>();
                    funCall->primaryExpression = expect<Expression>();
                    withSize([&](std::size_t) {
                        funCall->arguments.push_back(expect<Expression>());
                    });
                    return funCall;
                }
                case ASTNodeType::ID_ACCESSOR_EXPRESSION: {
                    auto idAcc = makeast<IdAccessorExpression>();
                    idAcc->primaryExpression = expect<Expression>();
                    idAcc->accessor = expect<Expression>();
                    return idAcc;
                }
                case ASTNodeType::ASSIGNMENT_EXPRESSION: {
                    NG::Str name;
                    stream >> name;
                    auto assign = makeast<AssignmentExpression>(name);
                    assign->value = expect<Expression>();
                    return assign;
                }
                case ASTNodeType::INTEGER_VALUE: {
                    int val;
                    stream >> val;
                    return makeast<IntegerValue>(val);
                }
                case ASTNodeType::STRING_VALUE: {
                    Str stringVal;
                    stream >> stringVal;
                    return makeast<StringValue>(stringVal);
                }
                case ASTNodeType::BOOLEAN_VALUE: {
                    bool boolVal;
                    stream >> boolVal;
                    return makeast<BooleanValue>(boolVal);
                }
                case ASTNodeType::ARRAY_LITERAL: {
                    Vec<ASTRef<Expression>> vec {};
                    withSize([&](std::size_t) {
                        vec.push_back(expect<Expression>());
                    });
                    return makeast<ArrayLiteral>(vec);
                }
                default:
                    break;
            }
            return nullptr;
        }

        ASTRef<Param> expectNode(ASTRef<Param> expr, ASTNodeType type) {
            assertASTNodeType(ASTNodeType::PARAM, type);
            NG::Str paramName;
            ParamType paramType;
            NG::Str annotatedType;
            stream >> paramName >> paramType >> annotatedType;
            return makeast<Param>(paramName, annotatedType, paramType);
        }

        ASTRef<Statement> expectNode(ASTRef<Statement> expr, ASTNodeType type) {
            switch (type) {
                case ASTNodeType::SIMPLE_STATEMENT: {
                    auto stmt = makeast<SimpleStatement>();
                    stmt->expression = expect<Expression>();

                    return stmt;
                }
                case ASTNodeType::RETURN_STATEMENT: {
                    auto stmt = makeast<ReturnStatement>();
                    stmt->expression = expect<Expression>();

                    return stmt;
                }
                case ASTNodeType::COMPOUND_STATEMENT: {
                    auto stmt = makeast<CompoundStatement>();
                    withSize([&](std::size_t) {
                        stmt->statements.push_back(expect<Statement>());
                    });
                    return stmt;
                }
                case ASTNodeType::IF_STATEMENT: {
                    auto stmt = makeast<IfStatement>();
                    stmt->testing = expect<Expression>();
                    stmt->consequence = expect<Statement>();
                    bool hasElseBranch;
                    stream >> hasElseBranch;

                    if (hasElseBranch)
                        stmt->alternative = expect<Statement>();
                    return stmt;
                }
                case ASTNodeType::VAL_DEF_STATEMENT: {
                    NG::Str valName;
                    stream >> valName;
                    auto valDef = makeast<ValDefStatement>(valName);
                    valDef->value = expect<Expression>();
                    return valDef;
                }
                default:
                    throw ParseException("Unknown ast node type, expect a statement");
            }
        }
    };

    ASTRef<ASTNode> deserialize_ast(std::vector<uint8_t> &bytes) {
        return ASTDeserializer{bytes}.deserialize();
    }
} // namespace NG::AST
