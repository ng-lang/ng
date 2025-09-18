
#include "common.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <visitor.hpp>
#include <debug.hpp>
#include <ast.hpp>
#include <filesystem>
#include <utility>
#include <regex>
#include <functional>

namespace fs = std::filesystem;

namespace NG::parsing
{
    using namespace NG;
    using namespace NG::ast;

    const std::regex IMPORT_DECL_PATTERN{"^[A-Za-z_][A-Za-z_\\-0-9\\.]+$"};

    static const Set<TokenType> unary_operators{
        TokenType::NOT,
        TokenType::MINUS,
        TokenType::QUERY,
    };

    [[nodiscard]] inline auto isUnaryOperator(TokenType optr) -> bool
    {
        return unary_operators.contains(optr);
    }

    class ParserImpl
    {
        ParseState state;

        [[noreturn]]
        void unexpected(Str message)
        {
            if (message.empty())
            {
                message = std::string{"Unexpected token "} + state->repr;
            }
            throw ParseException(message + " at " + std::to_string(state->position.line) + ":" + std::to_string(state->position.col));
        }

    public:
        explicit ParserImpl(ParseState &state)
            : state(state) {}

        auto parse(const Str &fileName) -> ASTRef<ASTNode>
        {
            // file as default module

            auto compileUnit = makeast<CompileUnit>();

            compileUnit->fileName = fileName;
            fs::path filePath{fileName};
            if (fs::exists(filePath))
            {
                compileUnit->path = filePath.parent_path().string();
            }

            Str fileWithoutPath{filePath.filename()};
            Str moudleName = fileWithoutPath;
            if (moudleName.ends_with(".ng"))
            {
                moudleName = fileWithoutPath.substr(0, fileWithoutPath.size() - 3);
            }

            auto mod = makeast<Module>();
            mod->name = moudleName;
            ASTRef<Module> current_mod = mod;
            compileUnit->module = mod;
            bool moduleDeclared = false;

            while (!state.eof())
            {
                bool exported = false;
                if (expect(TokenType::KEYWORD_EXPORT))
                {
                    exported = true;
                    accept(TokenType::KEYWORD_EXPORT);
                    // todo: export single identifiers
                }
                switch (state->type)
                {
                case TokenType::KEYWORD_FUN:
                {
                    auto fn = funDef();
                    if (exported)
                    {
                        mod->exports.push_back(fn->funName);
                    }
                    current_mod->definitions.push_back(std::move(fn));
                    break;
                }
                case TokenType::KEYWORD_VAL:
                {
                    auto value = valDef();
                    if (exported)
                    {
                        mod->exports.push_back(value->name());
                    }
                    current_mod->definitions.push_back(std::move(value));
                    break;
                }
                case TokenType::KEYWORD_TYPE:
                {
                    auto type = typeDef();
                    if (exported)
                    {
                        mod->exports.push_back(type->name());
                    }
                    current_mod->definitions.push_back(std::move(type));
                    break;
                }
                case TokenType::KEYWORD_MODULE:
                {
                    if (moduleDeclared)
                    {
                        unexpected("Redeclare a module");
                    }
                    moduleDeclared = true;
                    moduleDecl(mod);
                    break;
                }
                // todo: export an import.
                case TokenType::KEYWORD_IMPORT:
                {
                    current_mod->imports.push_back(importDecl());
                    break;
                }
                    // case TokenType::KEYWORD_IF:
                case TokenType::KEYWORD_CASE:
                case TokenType::KEYWORD_LOOP:
                case TokenType::KEYWORD_COLLECT:
                case TokenType::KEYWORD_NEXT:
                default:
                {
                    if (exported)
                    {
                        unexpected("Invalid export: only definitions can be exported");
                    }
                    current_mod->statements.push_back(statement());
                }
                }
            }
            return compileUnit;
        }

    private:
        auto expect(TokenType type) -> bool
        {
            return !state.eof() && state->type == type;
        }

        void accept(TokenType type)
        {
            if (!expect(type))
            {
                return unexpected("Unexpected token " + state->repr);
            }
            state.next();
        }

        auto funDef() -> ASTRef<FunctionDef>
        {
            accept(TokenType::KEYWORD_FUN);
            if (expect(TokenType::ID))
            {
                auto def = makeast<FunctionDef>();
                def->funName = state->repr;
                accept(TokenType::ID);

                def->params = std::move(funParams());

                if (expect(TokenType::SINGLE_ARROW))
                {
                    accept(TokenType::SINGLE_ARROW);
                    def->returnType = std::move(typeAnnotation());
                }

                if (expect(TokenType::BIND))
                {
                    accept(TokenType::BIND);
                    if (expect(TokenType::KEYWORD_NATIVE))
                    {
                        accept(TokenType::KEYWORD_NATIVE);
                        def->native = true;
                        if (def->returnType == nullptr)
                        {
                            unexpected("Native function '" + def->funName + "' must declare a return type.");
                        }
                        accept(TokenType::SEMICOLON);
                        return def;
                    }
                    else
                    {
                        auto expressionBody = expression();
                        auto body = makeast<ReturnStatement>();
                        body->expression = expressionBody;
                        def->body = body;
                        accept(TokenType::SEMICOLON);
                        return def;
                    }
                }
                def->body = std::move(statement());
                return def;
            }
            unexpected("Expected function name");
        }

        /**
         * syntax:
         *   import a.b.c;
         *   import "a"."b"."c";
         *   import "abc" c;
         *   import abcdef c;
         *   import a.b.c (a, b, c);
         *   import a.b.c (*);
         *   import a.b.c abc (a, b, c);
         */
        auto importDecl() -> ASTRef<ImportDecl> // NOLINT(readability-function-cognitive-complexity)
        {
            accept(TokenType::KEYWORD_IMPORT);
            auto imp = makeast<ImportDecl>();

            Vec<Str> modulePath{};

            while (expect(TokenType::ID) || expect(TokenType::STRING))
            {
                Str moduleSegment = state->repr;
                accept(state->type);
                if (std::regex_match(moduleSegment, IMPORT_DECL_PATTERN))
                {
                    modulePath.push_back(moduleSegment);
                    // always use latest
                    imp->module = moduleSegment;
                }
                else
                {
                    unexpected("Invalid module path when import.");
                }
                if (expect(TokenType::DOT))
                {
                    accept(TokenType::DOT);
                    continue;
                }
                else
                {
                    imp->modulePath = modulePath;
                    break;
                }
            }

            // alias import
            if (expect(TokenType::ID))
            {
                Str alias = state->repr;
                accept(TokenType::ID);
                imp->alias = alias;
            }

            // symbol import
            if (expect(TokenType::LEFT_PAREN))
            {
                accept(TokenType::LEFT_PAREN);

                while (expect(TokenType::ID))
                {
                    imp->imports.push_back(state->repr);
                    accept(TokenType::ID);
                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    accept(TokenType::COMMA);
                }
                if (imp->imports.empty() && expect(TokenType::TIMES))
                {
                    accept(TokenType::TIMES);
                    imp->imports.emplace_back("*");
                }
                accept(TokenType::RIGHT_PAREN);
            }
            else
            {
                if (expect(TokenType::TIMES))
                {
                    accept(TokenType::TIMES);
                    imp->imports.emplace_back("*");
                }
            }
            if (imp->imports.empty() && imp->alias.empty())
            {
                imp->alias = imp->module;
            }
            accept(TokenType::SEMICOLON);

            return imp;
        }

        auto valDef() -> ASTRef<ValDef>
        {
            return makeast<ValDef>(valDefStatement());
        }

        auto propertyDef() -> ASTRef<PropertyDef>
        {
            accept(TokenType::KEYWORD_PROPERTY);

            auto name = idExpression();

            accept(TokenType::SEMICOLON);
            return makeast<PropertyDef>((name)->repr());
        }

        auto typeDef() -> ASTRef<TypeDef>
        {
            ASTRef<TypeDef> typeDef = makeast<TypeDef>();

            accept(TokenType::KEYWORD_TYPE);

            auto typeName = idExpression();
            typeDef->typeName = typeName->repr();

            accept(TokenType::LEFT_CURLY);

            while (!expect(TokenType::RIGHT_CURLY))
            {
                if (expect(TokenType::KEYWORD_PROPERTY))
                {
                    typeDef->properties.push_back(propertyDef());
                }
                else if (expect(TokenType::KEYWORD_FUN))
                {
                    typeDef->memberFunctions.push_back(funDef());
                }
            }
            accept(TokenType::RIGHT_CURLY);
            return typeDef;
        }

        auto moduleDecl(ASTRef<Module> mod) -> ASTRef<Module>
        {
            accept(TokenType::KEYWORD_MODULE);
            if (expect(TokenType::ID))
            {
                auto moduleName = state->repr;
                accept(TokenType::ID);
                if (mod->name == moduleName)
                {
                }
                else if (mod->name == "[noname]" || mod->name == "[interpreter]")
                {
                    mod->name = moduleName;
                }
                else
                {
                    unexpected("Invalid module name for module: " + moduleName + ", expected: " + mod->name);
                }
            }
            if (expect(TokenType::KEYWORD_EXPORTS))
            {
                accept(TokenType::KEYWORD_EXPORTS);
                if (state->type == TokenType::TIMES)
                {
                    accept(TokenType::TIMES);
                    mod->exports.emplace_back("*");
                }
                else
                {
                    mod->exports = std::move(exportList());
                }
            }
            accept(TokenType::SEMICOLON);
            return mod;
        }

        auto exportList() -> Vec<Str>
        {
            bool withParen = false;

            Vec<Str> exports{};

            if (expect(TokenType::LEFT_PAREN))
            {
                withParen = true;
                accept(TokenType::LEFT_PAREN);
            }

            while (expect(TokenType::ID))
            {
                auto &&symbol = state->repr;
                exports.push_back(symbol);
                accept(TokenType::ID);
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                accept(TokenType::COMMA);
            }
            if (withParen)
            {
                accept(TokenType::RIGHT_PAREN);
            }

            return exports;
        }

        auto funParams() -> Vec<ASTRef<Param>>
        {
            Vec<ASTRef<Param>> params{};
            if (expect(TokenType::LEFT_PAREN))
            {
                accept(TokenType::LEFT_PAREN);

                while (expect(TokenType::ID))
                {
                    const Str &name = state->repr;
                    ASTRef<Param> param = makeast<Param>(name);
                    accept(TokenType::ID);
                    if (expect(TokenType::COLON))
                    {
                        accept(TokenType::COLON);
                        auto anno = typeAnnotation();

                        param = makeast<Param>(name, anno);
                    }
                    params.push_back(param);
                    if (expect(TokenType::BIND))
                    {
                        accept(TokenType::BIND);
                        param->value = expression();
                    }

                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    accept(TokenType::COMMA);
                }

                accept(TokenType::RIGHT_PAREN);
            }
            return params;
        }

        auto statement() -> ASTRef<Statement>
        {
            switch (state->type)
            {
            case TokenType::LEFT_CURLY:
                return compoundStatement();
            case TokenType::KEYWORD_RETURN:
                return returnStatement();
            case TokenType::DUAL_ARROW:
                return arrowReturn();
            case TokenType::KEYWORD_IF:
                return ifStatement();
            case TokenType::KEYWORD_VAL:
                return valDefStatement();
            case TokenType::KEYWORD_LOOP:
                return loopStatement();
            case TokenType::KEYWORD_NEXT:
                return nextStatement();
            case TokenType::SEMICOLON:
                accept(TokenType::SEMICOLON);
                return makeast<EmptyStatement>();
            default:
                return simpleStatement();
            }
        }

        auto simpleStatement() -> ASTRef<SimpleStatement>
        {
            auto expr = expression();
            accept(TokenType::SEMICOLON);
            auto stmt = makeast<SimpleStatement>();
            stmt->expression = std::move(expr);
            return stmt;
        }

        auto typeAnnotation() -> ASTRef<TypeAnnotation>
        {
            TokenType maybeBuiltin = state->type;
            if (code(TokenType::KEYWORD_INT) <= code(maybeBuiltin) &&
                code(TokenType::KEYWORD_F128) >= code(maybeBuiltin))
            {
                ASTRef<TypeAnnotation> anno = makeast<TypeAnnotation>(state->repr);
                size_t builtin_type_code = code(maybeBuiltin) - code(TokenType::KEYWORD_INT) + code(TypeAnnotationType::BUILTIN_INT);
                anno->type = from_code<TypeAnnotationType>(builtin_type_code);
                accept(maybeBuiltin);
                return anno;
            }
            if (maybeBuiltin == TokenType::KEYWORD_UNIT)
            {
                ASTRef<TypeAnnotation> anno = makeast<TypeAnnotation>(state->repr);
                anno->type = TypeAnnotationType::BUILTIN_UNIT;
                accept(maybeBuiltin);
                return anno;
            }
            if (maybeBuiltin == TokenType::LEFT_SQUARE)
            {
                auto array = makeast<TypeAnnotation>("array");
                accept(TokenType::LEFT_SQUARE);
                array->type = TypeAnnotationType::ARRAY;
                auto argumentRst = typeAnnotation();
                array->arguments.push_back(argumentRst);
                accept(TokenType::RIGHT_SQUARE);
                return array;
            }
            if (maybeBuiltin == TokenType::ID)
            {
                ASTRef<TypeAnnotation> anno = makeast<TypeAnnotation>(state->repr);
                accept(TokenType::ID);
                anno->type = TypeAnnotationType::CUSTOMIZED;
                return anno;
            }
            unexpected("Unknown type annotation");
        }

        auto valDefStatement() -> ASTRef<ValDefStatement>
        {

            accept(TokenType::KEYWORD_VAL);
            auto name = state->repr;
            ASTRef<TypeAnnotation> anno{};
            accept(TokenType::ID);
            if (expect(TokenType::COLON))

            {
                accept(TokenType::COLON);
                anno = typeAnnotation();
            }
            if (!expect(TokenType::BIND))
            {
                unexpected("Unexpected token " + state->repr + ", expect bind operator `=`.");
            }
            accept(TokenType::BIND);
            auto value = expression();
            accept(TokenType::SEMICOLON);
            auto def = makeast<ValDefStatement>(name);
            def->value = std::move(value);
            def->typeAnnotation = std::move(anno);
            return def;
        }

        auto ifStatement() -> ASTRef<IfStatement>
        {

            auto ifstmt = makeast<IfStatement>();
            accept(TokenType::KEYWORD_IF);
            accept(TokenType::LEFT_PAREN);

            auto testing = expression();
            ifstmt->testing = std::move(testing);
            accept(TokenType::RIGHT_PAREN);
            ;
            ifstmt->consequence = std::move(statement());

            if (expect(TokenType::KEYWORD_ELSE))
            {
                accept(TokenType::KEYWORD_ELSE);
                ifstmt->alternative = std::move(statement());
            }

            return ifstmt;
        }

        auto loopStatement() -> ASTRef<LoopStatement>
        {
            accept(TokenType::KEYWORD_LOOP);
            auto loopStmt = makeast<LoopStatement>();
            while (expect(TokenType::ID))
            {
                auto identifier = state->repr;
                accept(TokenType::ID);
                auto loopBindingType = LoopBindingType::LOOP_ASSIGN;
                ASTRef<TypeAnnotation> loopBindingAnnotation = nullptr;
                ASTRef<Expression> bindingTarget{makeast<IdExpression>(identifier)};
                if (state->type == TokenType::COLON)
                {
                    accept(TokenType::COLON);
                    loopBindingAnnotation = typeAnnotation();
                }
                if (state->type == TokenType::BIND)
                {
                    accept(TokenType::BIND);
                    loopBindingType = LoopBindingType::LOOP_ASSIGN;
                    bindingTarget = expression(); // NOLINT(*-unused-variable)
                }
                if (bindingTarget)
                {
                    loopStmt->bindings.emplace_back(LoopBinding{
                        .name = identifier,
                        .type = loopBindingType,
                        .target = bindingTarget,
                        .annotation = loopBindingAnnotation,
                    });
                }
                else
                {
                    unexpected("Invalid loop binding target for " + identifier);
                }
                if (expect(TokenType::COMMA))
                {
                    accept(TokenType::COMMA);
                    continue;
                }
                else
                {
                    break;
                }
            }
            loopStmt->loopBody = std::move(statement());
            return loopStmt;
        }

        auto nextStatement() -> ASTRef<NextStatement>
        {
            accept(TokenType::KEYWORD_NEXT);
            auto nextStmt = makeast<NextStatement>();

            while (!expect(TokenType::SEMICOLON))
            {
                auto expr = expression();
                if (expr)
                {
                    nextStmt->expressions.push_back(expr);
                }
                if (!expect(TokenType::SEMICOLON))
                {
                    accept(TokenType::COMMA);
                }
            }
            accept(TokenType::SEMICOLON);

            return nextStmt;
        }

        auto compoundStatement() -> ASTRef<CompoundStatement>
        {
            accept(TokenType::LEFT_CURLY);
            auto stmt = makeast<CompoundStatement>();
            while (!expect(TokenType::RIGHT_CURLY))
            {
                stmt->statements.push_back(std::move(statement()));
            }
            accept(TokenType::RIGHT_CURLY);

            return stmt;
        }

        auto returnBy(TokenType type) -> ASTRef<ReturnStatement>
        {
            accept(type);
            if (expect(TokenType::SEMICOLON))
            {
                accept(TokenType::SEMICOLON);
                return makeast<ReturnStatement>();
            }
            auto ret = makeast<ReturnStatement>();
            ret->expression = std::move(expression());
            accept(TokenType::SEMICOLON);
            return ret;
        }

        auto returnStatement() -> ASTRef<ReturnStatement>
        {
            return returnBy(TokenType::KEYWORD_RETURN);
        }

        auto arrowReturn() -> ASTRef<ReturnStatement>
        {
            return returnBy(TokenType::DUAL_ARROW);
        }

        auto typeCheckExpr(ASTRef<Expression> expr) -> ASTRef<TypeCheckingExpression>
        {
            accept(TokenType::KEYWORD_IS);
            auto type = expression();
            if (!dynamic_ast_cast<IdExpression>(type) && !dynamic_ast_cast<IdAccessorExpression>(type))
            {
                unexpected("Unexpected expression " + type->repr());
            }

            return makeast<TypeCheckingExpression>(expr, type);
        }

        auto expression() -> ASTRef<Expression>
        {
            auto expr = std::move(primaryExpression());

            while (!expectExpressionTerminator() || is_operator(state->type))
            {
                if (expect(TokenType::LEFT_PAREN))
                {
                    expr = std::move(funCallExpression(std::move(expr)));
                }
                else if (expect(TokenType::DOT))
                {
                    expr = std::move(idAccessorExpression(std::move(expr)));
                }
                else if (expect(TokenType::KEYWORD_IS))
                {
                    expr = std::move(typeCheckExpr(std::move(expr)));
                }
                else if (expect(TokenType::ASSIGN_EQUAL))
                {
                    expr = std::move(assignmentExpression(std::move(expr)));
                }
                else if (is_operator(state->type))
                {
                    expr = std::move(binaryExpression(std::move(expr)));
                }

                else if (expect(TokenType::LEFT_SQUARE))
                {
                    expr = std::move(indexAccessorExpression(std::move(expr)));
                }
            }

            return expr;
        }

        auto assignmentExpression(ASTRef<Expression> ref) -> ASTRef<AssignmentExpression>
        {
            auto idExpr = dynamic_ast_cast<IdExpression>(ref);
            auto idAccessor = dynamic_ast_cast<IdAccessorExpression>(ref);
            if (!idExpr && !idAccessor)
            {
                unexpected("Unexpected expression, expect identifier to assign");
            }

            if (!expect(TokenType::ASSIGN_EQUAL))
            {
                unexpected("Only assignment operator can assign");
            }

            accept(TokenType::ASSIGN_EQUAL);
            auto assignmentExpr = makeast<AssignmentExpression>(ref);
            assignmentExpr->value = std::move(expression());

            return assignmentExpr;
        }

        auto expectExpressionTerminator() -> bool
        {
            return expect(TokenType::COLON) ||        // :
                   expect(TokenType::COMMA) ||        // ,
                   expect(TokenType::DUAL_ARROW) ||   // =>
                   expect(TokenType::SINGLE_ARROW) || // ->
                   expect(TokenType::RIGHT_PAREN) ||  // )
                   expect(TokenType::RIGHT_CURLY) ||  // }
                   expect(TokenType::RIGHT_SQUARE) || // ]
                   expect(TokenType::SEMICOLON) ||    // ;
                   expect(TokenType::LEFT_CURLY) ||   // {
                   is_operator(state->type) ||
                   state.eof();
        }

        auto binaryExpression(ASTRef<Expression> expr) -> ASTRef<BinaryExpression>
        {
            auto &&token = state.current();
            if (state->type == TokenType::BIND)
            {
                unexpected("Invalid use of binding operator `=` in expression.");
            }
            accept(state->type);
            auto binexpr = makeast<BinaryExpression>();
            binexpr->optr = std::make_shared<Token>(token);
            binexpr->left = std::move(expr);
            binexpr->right = std::move(expression());
            return binexpr;
        }

        auto funCallExpression(ASTRef<Expression> primaryExpression) -> ASTRef<FunCallExpression>
        {
            accept(TokenType::LEFT_PAREN);
            Vec<ASTRef<Expression>> args{};
            while (!expect(TokenType::RIGHT_PAREN))
            {
                args.push_back(std::move(expression()));
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_PAREN);
            auto funcall = makeast<FunCallExpression>();
            funcall->primaryExpression = std::move(primaryExpression);
            funcall->arguments = std::move(args);
            return funcall;
        }

        auto idAccessorExpression(ASTRef<Expression> expr) -> ASTRef<IdAccessorExpression>
        {
            accept(TokenType::DOT);
            auto idacc = makeast<IdAccessorExpression>();
            idacc->primaryExpression = std::move(expr);

            if (expect(TokenType::ID))
            {
                idacc->accessor = std::move(idExpression());
            }
            else
            {
                unexpected("Expect identifier after '.'");
            }

            if (expect(TokenType::LEFT_PAREN))
            {
                accept(TokenType::LEFT_PAREN);

                Vec<ASTRef<Expression>> args{};

                while (!expect(TokenType::RIGHT_PAREN))
                {
                    args.push_back(std::move(expression()));
                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    accept(TokenType::COMMA);
                }
                accept(TokenType::RIGHT_PAREN);

                idacc->arguments = std::move(args);
            }
            return idacc;
        }

        auto indexAccessorExpression(ASTRef<Expression> primary) -> ASTRef<Expression>
        {
            accept(TokenType::LEFT_SQUARE);

            auto accessor = expression();
            accept(TokenType::RIGHT_SQUARE);

            if (expect(TokenType::ASSIGN_EQUAL))
            {
                accept(TokenType::ASSIGN_EQUAL);
                auto value = expression();
                return makeast<IndexAssignmentExpression>(std::move(primary), std::move(accessor), std::move(value));
            }
            return makeast<IndexAccessorExpression>(std::move(primary), std::move(accessor));
        }

        auto newObjectExpression() -> ASTRef<NewObjectExpression>
        {
            ASTRef<NewObjectExpression> newObj = makeast<NewObjectExpression>();

            accept(TokenType::KEYWORD_NEW);

            auto typeName = idExpression();
            newObj->typeName = typeName->repr();

            accept(TokenType::LEFT_CURLY);

            while (!expect(TokenType::RIGHT_CURLY))
            {
                auto propertyName = idExpression();
                accept(TokenType::COLON);
                newObj->properties[propertyName->repr()] = std::move(expression());
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_CURLY);

            return newObj;
        }

        auto primaryExpression() -> ASTRef<Expression>
        {
            if (expect(TokenType::LEFT_PAREN))
            {
                accept(TokenType::LEFT_PAREN);
                auto expr = expression();
                if (expect(TokenType::COMMA))
                {
                    // TODO: TupleLiteral!
                }
                accept(TokenType::RIGHT_PAREN);

                return expr;
            }
            if (expect(TokenType::ID))
            {
                return idExpression();
            }
            if (expect(TokenType::NUMBER) ||
                (code(state->type) >= code(TokenType::NUMBER) &&
                 code(state->type) <= code(TokenType::NUMBER_F128)))
            {
                return numberLiteral();
            }
            if (expect(TokenType::STRING))
            {
                return stringValue();
            }
            if (expect(TokenType::KEYWORD_TRUE))
            {
                accept(TokenType::KEYWORD_TRUE);

                return makeast<BooleanValue>(true);
            }
            if (expect(TokenType::KEYWORD_FALSE))
            {
                accept(TokenType::KEYWORD_FALSE);
                return makeast<BooleanValue>(false);
            }
            if (expect(TokenType::LEFT_SQUARE))
            {
                return arrayLiteral();
            }
            if (expect(TokenType::KEYWORD_NEW))
            {
                return newObjectExpression();
            }
            if (is_operator(state->type))
            {
                if (isUnaryOperator(state->type))
                {
                    return unaryExpression();
                }
                else
                {
                    unexpected("Unexpected operator as unary operator");
                }
            }
            unexpected("Unexpected primary expression: " + state->repr);
        }

        auto unaryExpression() -> ASTRef<UnaryExpression>
        {
            auto optrToken = state.current();

            switch (optrToken.type)
            {
            case TokenType::NOT:
                [[fallthrough]];
            case TokenType::MINUS:
                [[fallthrough]];
            case TokenType::QUERY:
            {

                accept(optrToken.type);
                auto expr = makeast<UnaryExpression>();
                expr->optr = std::make_shared<Token>(optrToken);
                expr->operand = std::move(expression());
                return expr;
            }
            default:
                unexpected("Invalid unary operator.");
            }
        }

        auto stringValue() -> ASTRef<StringValue>
        {
            const auto &str = state->repr;
            accept(TokenType::STRING);
            return makeast<StringValue>(str);
        }

        auto numberLiteral() -> ASTRef<Expression>
        {
            auto integer = state->repr;
            switch (state->type)
            {
            case TokenType::NUMBER:
                accept(state->type);
                return makeast<IntegralValue<int32_t>>(std::stoi(integer));
            case TokenType::INTEGRAL:
                accept(state->type);
                return makeast<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
            case TokenType::NUMBER_I8:
                accept(state->type);
                return makeast<IntegralValue<int8_t>>(static_cast<int8_t>(std::stoi(integer)));
            case TokenType::NUMBER_U8:
                accept(state->type);
                return makeast<IntegralValue<uint8_t>>(static_cast<uint8_t>(std::stoi(integer)));
            case TokenType::NUMBER_I16:
                accept(state->type);
                return makeast<IntegralValue<int16_t>>(static_cast<int16_t>(std::stoi(integer)));
            case TokenType::NUMBER_U16:
                accept(state->type);
                return makeast<IntegralValue<uint16_t>>(static_cast<uint16_t>(std::stoi(integer)));
            case TokenType::NUMBER_I32:
                accept(state->type);
                return makeast<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
            case TokenType::NUMBER_U32:
                accept(state->type);
                return makeast<IntegralValue<uint32_t>>(static_cast<uint32_t>(std::stoul(integer)));
            case TokenType::NUMBER_I64:
                accept(state->type);
                return makeast<IntegralValue<int64_t>>(static_cast<int64_t>(std::stoll(integer)));
            case TokenType::NUMBER_U64:
                accept(state->type);
                return makeast<IntegralValue<uint64_t>>(static_cast<uint64_t>(std::stoull(integer)));
            case TokenType::FLOATING_POINT:
                accept(state->type);
                return makeast<FloatingPointValue<float>>(std::stof(integer));
            case TokenType::NUMBER_F16:
                unexpected("Float16 not supported");
            //     accept(state->type);
            //     return makeast<FloatingPointValue<float16_t>>(static_cast<float16_t>(std::stof(integer)));
            case TokenType::NUMBER_F32:
                accept(state->type);
                return makeast<FloatingPointValue<float>>(std::stof(integer));
            case TokenType::NUMBER_F64:
                accept(state->type);
                return makeast<FloatingPointValue<double>>((std::stod(integer)));
            case TokenType::NUMBER_F128:
                unexpected("Float128 not supported");
            //     accept(state->type);
            //     return makeast<FloatingPointValue<float128_t>>(static_cast<float128_t>(std::stold(integer)));
            default:
                unexpected("Invalid number literal");
            }
        }

        auto idExpression() -> ASTRef<IdExpression>
        {
            auto identifier = state->repr;
            accept(TokenType::ID);
            return makeast<IdExpression>(identifier);
        }

        auto arrayLiteral() -> ASTRef<Expression>
        {
            accept(TokenType::LEFT_SQUARE);

            Vec<ASTRef<Expression>> elements{};

            while (!expect(TokenType::RIGHT_SQUARE))
            {
                elements.push_back(std::move(expression()));
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_SQUARE);

            return makeast<ArrayLiteral>(std::move(elements));
        }
    };

    auto Parser::parse(const Str &fileName) -> ASTRef<ASTNode>
    {
        return ParserImpl(state).parse(fileName);
    }

} // namespace NG
