
#include "common.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <visitor.hpp>
#include <debug.hpp>
#include <ast.hpp>
#include <filesystem>
#include <utility>
#include <regex>

namespace fs = std::filesystem;

namespace NG::parsing
{
    using namespace NG;
    using namespace NG::ast;

    const std::regex IMPORT_DECL_PATTERN{"^[A-Za-z_][A-Za-z_\\-0-9\\.]+$"};

    class ParserImpl
    {
        ParseState state;

        static auto unexpected(ParseState &state, std::list<TokenType> types = {})
        {
            return std::unexpected(state.error(std::string{"Unexpected token "} + state->repr + " at " + std::to_string(state->position.line) + ":" + std::to_string(state->position.col), std::move(types)));
        }

    public:
        explicit ParserImpl(ParseState &state)
            : state(state) {}

        auto parse(const Str &fileName) -> ParseResult<ASTRef<ASTNode>>
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
                    auto funDefResult = funDef();
                    if (!funDefResult)
                    {
                        return std::unexpected(funDefResult.error());
                    }
                    if (exported)
                    {
                        mod->exports.push_back((*funDefResult)->funName);
                    }
                    current_mod->definitions.push_back(std::move(*funDefResult));
                    break;
                }
                case TokenType::KEYWORD_VAL:
                {
                    auto valDefResult = valDef();
                    if (!valDefResult)
                    {
                        return std::unexpected(valDefResult.error());
                    }
                    if (exported)
                    {
                        mod->exports.push_back((*valDefResult)->name());
                    }
                    current_mod->definitions.push_back(std::move(*valDefResult));
                    break;
                }
                case TokenType::KEYWORD_TYPE:
                {
                    auto typeDefResult = typeDef();
                    if (!typeDefResult)
                    {
                        return std::unexpected(typeDefResult.error());
                    }
                    if (exported)
                    {
                        mod->exports.push_back((*typeDefResult)->name());
                    }
                    current_mod->definitions.push_back(std::move(*typeDefResult));
                    break;
                }
                case TokenType::KEYWORD_MODULE:
                {
                    if (moduleDeclared)
                    {
                        return std::unexpected(state.error("Redeclare a module"));
                    }
                    moduleDeclared = true;
                    auto subModuleResult = moduleDecl(mod);
                    if (!subModuleResult)
                    {
                        return std::unexpected(subModuleResult.error());
                    }
                    break;
                }
                // todo: export an import.
                case TokenType::KEYWORD_IMPORT:
                {
                    auto importDeclResult = importDecl();
                    if (!importDeclResult)
                    {
                        return std::unexpected(importDeclResult.error());
                    }
                    current_mod->imports.push_back(std::move(*importDeclResult));
                    break;
                }
                    // case TokenType::KEYWORD_IF:
                case TokenType::KEYWORD_CASE:
                case TokenType::KEYWORD_LOOP:
                case TokenType::KEYWORD_COLLECT:
                case TokenType::KEYWORD_UNIT:
                case TokenType::KEYWORD_NEXT:
                default:
                {
                    if (exported)
                    {
                        return std::unexpected(state.error("Invalid export: only definitions can be exported"));
                    }
                    auto statementResult = statement();
                    if (!statementResult)
                    {
                        return std::unexpected(statementResult.error());
                    }
                    current_mod->statements.push_back(std::move(*statementResult));
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

        auto accept(TokenType type) -> ParseResult<void>
        {
            if (!expect(type))
            {
                return unexpected(state, {type});
            }
            state.next();
            return {};
        }

        auto funDef() -> ParseResult<ASTRef<FunctionDef>>
        {
            if (auto result = accept(TokenType::KEYWORD_FUN); !result)
            {
                return std::unexpected(result.error());
            }
            if (expect(TokenType::ID))
            {
                auto def = makeast<FunctionDef>();
                def->funName = state->repr;
                if (auto result = accept(TokenType::ID); !result)
                {
                    return std::unexpected(result.error());
                }

                auto paramsResult = funParams();
                if (!paramsResult)
                {
                    return std::unexpected(paramsResult.error());
                }
                def->params = std::move(*paramsResult);

                if (expect(TokenType::OPERATOR) && state->operatorType == Operators::ASSIGN)
                {
                    accept(TokenType::OPERATOR);
                    if (expect(TokenType::KEYWORD_NATIVE))
                    {
                        accept(TokenType::KEYWORD_NATIVE);
                        def->native = true;
                        if (auto result = accept(TokenType::SEMICOLON); !result)
                        {
                            return std::unexpected(result.error());
                        }
                        return def;
                    }
                    else
                    {
                        auto expressionBody = expression();
                        if (!expressionBody)
                        {
                            return std::unexpected(expressionBody.error());
                        }
                        auto body = makeast<ReturnStatement>();
                        body->expression = *expressionBody;
                        def->body = body;
                        if (auto result = accept(TokenType::SEMICOLON); !result)
                        {
                            return std::unexpected(result.error());
                        }
                        return def;
                    }
                }

                auto bodyResult = statement();
                if (!bodyResult)
                {
                    return std::unexpected(bodyResult.error());
                }
                def->body = std::move(*bodyResult);

                return def;
            }
            return std::unexpected(state.error("Expected function name", {TokenType::ID}));
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
        auto importDecl() -> ParseResult<ASTRef<ImportDecl>> // NOLINT(readability-function-cognitive-complexity)
        {
            if (auto result = accept(TokenType::KEYWORD_IMPORT); !result)
            {
                return std::unexpected(result.error());
            }
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
                    return std::unexpected(state.error("Invalid module path when import."));
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
                if (auto result = accept(TokenType::ID); !result)
                {
                    return std::unexpected(result.error());
                }
                imp->alias = alias;
            }

            // symbol import
            if (expect(TokenType::LEFT_PAREN))
            {
                if (auto result = accept(TokenType::LEFT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }

                while (expect(TokenType::ID))
                {
                    imp->imports.push_back(state->repr);
                    if (auto result = accept(TokenType::ID); !result)
                    {
                        return std::unexpected(result.error());
                    }
                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    if (auto result = accept(TokenType::COMMA); !result)
                    {
                        return std::unexpected(result.error());
                    }
                }
                if (imp->imports.empty() && expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES)
                {
                    if (auto result = accept(TokenType::OPERATOR); !result)
                    {
                        return std::unexpected(result.error());
                    }
                    imp->imports.emplace_back("*");
                }
                if (auto result = accept(TokenType::RIGHT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }
            }
            else
            {
                if (expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES)
                {
                    if (auto result = accept(TokenType::OPERATOR); !result)
                    {
                        return std::unexpected(result.error());
                    }
                    imp->imports.emplace_back("*");
                }
            }
            if (imp->imports.empty() && imp->alias.empty())
            {
                imp->alias = imp->module;
            }

            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }

            return imp;
        }

        auto valDef() -> ParseResult<ASTRef<ValDef>>
        {
            auto valDefStmtResult = valDefStatement();
            if (!valDefStmtResult)
            {
                return std::unexpected(valDefStmtResult.error());
            }

            return makeast<ValDef>(std::move(*valDefStmtResult));
        }

        auto propertyDef() -> ParseResult<ASTRef<PropertyDef>>
        {
            if (auto result = accept(TokenType::KEYWORD_PROPERTY); !result)
            {
                return std::unexpected(result.error());
            }

            auto nameResult = idExpression();
            if (!nameResult)
            {
                return std::unexpected(nameResult.error());
            }

            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }
            return makeast<PropertyDef>((*nameResult)->repr());
        }

        auto typeDef() -> ParseResult<ASTRef<TypeDef>>
        {
            ASTRef<TypeDef> typeDef = makeast<TypeDef>();

            if (auto result = accept(TokenType::KEYWORD_TYPE); !result)
            {
                return std::unexpected(result.error());
            }

            auto typeNameResult = idExpression();
            if (!typeNameResult)
            {
                return std::unexpected(typeNameResult.error());
            }
            typeDef->typeName = (*typeNameResult)->repr();

            if (auto result = accept(TokenType::LEFT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }
            while (!expect(TokenType::RIGHT_CURLY))
            {
                if (expect(TokenType::KEYWORD_PROPERTY))
                {
                    auto propertyDefResult = propertyDef();
                    if (!propertyDefResult)
                    {
                        return std::unexpected(propertyDefResult.error());
                    }
                    typeDef->properties.push_back(std::move(*propertyDefResult));
                }
                else if (expect(TokenType::KEYWORD_FUN))
                {
                    auto funDefResult = funDef();
                    if (!funDefResult)
                    {
                        return std::unexpected(funDefResult.error());
                    }
                    typeDef->memberFunctions.push_back(std::move(*funDefResult));
                }
            }
            if (auto result = accept(TokenType::RIGHT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }

            return typeDef;
        }

        auto moduleDecl(ASTRef<Module> mod) -> ParseResult<ASTRef<Module>>
        {
            if (auto result = accept(TokenType::KEYWORD_MODULE); !result)
            {
                return std::unexpected(result.error());
            }
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
                    return std::unexpected(state.error("Invalid module name for module: " + moduleName + ", expected: " + mod->name));
                }
            }
            if (expect(TokenType::KEYWORD_EXPORTS))
            {
                if (auto result = accept(TokenType::KEYWORD_EXPORTS); !result)
                {
                    return std::unexpected(result.error());
                }
                if (state->type == TokenType::OPERATOR && state->operatorType == Operators::TIMES)
                {
                    if (auto result = accept(TokenType::OPERATOR); !result)
                    {
                        return std::unexpected(result.error());
                    }
                    mod->exports.emplace_back("*");
                }
                else
                {
                    auto exportListResult = exportList();
                    if (!exportListResult)
                    {
                        return std::unexpected(exportListResult.error());
                    }
                    mod->exports = std::move(*exportListResult);
                }
            }
            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }
            return mod;
        }

        auto exportList() -> ParseResult<Vec<Str>>
        {
            bool withParen = false;

            Vec<Str> exports{};

            if (expect(TokenType::LEFT_PAREN))
            {
                withParen = true;
                if (auto result = accept(TokenType::LEFT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }
            }

            while (expect(TokenType::ID))
            {
                auto &&symbol = state->repr;
                exports.push_back(symbol);
                if (auto result = accept(TokenType::ID); !result)
                {
                    return std::unexpected(result.error());
                }
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                if (auto result = accept(TokenType::COMMA); !result)
                {
                    return std::unexpected(result.error());
                }
            }
            if (withParen)
            {
                if (auto result = accept(TokenType::RIGHT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }
            }

            return exports;
        }

        auto funParams() -> ParseResult<Vec<ASTRef<Param>>>
        {
            Vec<ASTRef<Param>> params{};
            if (expect(TokenType::LEFT_PAREN))
            {
                if (auto result = accept(TokenType::LEFT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }

                while (expect(TokenType::ID))
                {
                    const Str &name = state->repr;
                    ASTRef<Param> param = nullptr;
                    if (auto result = accept(TokenType::ID); !result)
                    {
                        return std::unexpected(result.error());
                    }
                    if (expect(TokenType::COLON) && accept(TokenType::COLON))
                    {

                        auto annoResult = typeAnnotation();
                        if (!annoResult)
                        {
                            return std::unexpected(annoResult.error());
                        }

                        param = makeast<Param>(name, annoResult.value());
                        params.push_back(param);
                    }
                    else
                    {
                        param = makeast<Param>(name);
                        params.push_back(param);
                    }
                    if (expect(TokenType::OPERATOR) && state->operatorType == Operators::ASSIGN)
                    {
                        accept(TokenType::OPERATOR);
                        auto value = expression();
                        if (!value)
                        {
                            return std::unexpected(value.error());
                        }
                        param->value = value.value();
                    }

                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    if (auto result = accept(TokenType::COMMA); !result)
                    {
                        return std::unexpected(result.error());
                    }
                }

                if (auto result = accept(TokenType::RIGHT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }
            }
            return params;
        }

        auto statement() -> ParseResult<ASTRef<Statement>>
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
            default:
                return simpleStatement();
            }
        }

        auto simpleStatement() -> ParseResult<ASTRef<SimpleStatement>>
        {
            auto exprResult = expression();
            if (!exprResult)
            {
                return std::unexpected(exprResult.error());
            }

            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }
            auto stmt = makeast<SimpleStatement>();
            stmt->expression = std::move(*exprResult);

            return stmt;
        }

        auto typeAnnotation() -> ParseResult<ASTRef<TypeAnnotation>>
        {
            TokenType maybeBuiltin = state->type;
            if (code(TokenType::KEYWORD_INT) <= code(maybeBuiltin) &&
                code(TokenType::KEYWORD_F128) >= code(maybeBuiltin))
            {
                ASTRef<TypeAnnotation> anno = makeast<TypeAnnotation>(state->repr);
                size_t builtin_type_code = code(maybeBuiltin) - code(TokenType::KEYWORD_INT) + code(TypeAnnotationType::BUILTIN_INT);
                anno->type = from_code<TypeAnnotationType>(builtin_type_code);
                accept(maybeBuiltin); // NOLINT(*-unused-return-value)
                return anno;
            }
            if (maybeBuiltin == TokenType::ID)
            {
                ASTRef<TypeAnnotation> anno = makeast<TypeAnnotation>(state->repr);
                accept(TokenType::ID); // NOLINT(*-unused-return-value)
                anno->type = TypeAnnotationType::CUSTOMIZED;
                return anno;
            }
            return std::unexpected(state.error("Unknown type annotation"));
        }

        auto valDefStatement() -> ParseResult<ASTRef<ValDefStatement>>
        {
            if (auto result = accept(TokenType::KEYWORD_VAL); !result)
            {
                return std::unexpected(result.error());
            }
            auto name = state->repr;
            std::optional<ASTRef<TypeAnnotation>> anno{};
            if (auto result = accept(TokenType::ID); !result)
            {
                return std::unexpected(result.error());
            }
            if (expect(TokenType::COLON) && accept(TokenType::COLON))
            {
                auto annoResult = typeAnnotation();
                if (!annoResult)
                {
                    return std::unexpected(annoResult.error());
                }
                anno = annoResult.value();
            }
            if (state->operatorType != Operators::ASSIGN)
            {
                return unexpected(state);
            }
            if (auto result = accept(TokenType::OPERATOR); !result)
            {
                return std::unexpected(result.error()); // Assignment operator
            }
            auto valueResult = expression();
            if (!valueResult)
            {
                return std::unexpected(valueResult.error());
            }
            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }
            auto def = makeast<ValDefStatement>(name);
            def->value = std::move(*valueResult);
            def->typeAnnotation.swap(anno);
            return def;
        }

        auto ifStatement() -> ParseResult<ASTRef<IfStatement>>
        {

            auto ifstmt = makeast<IfStatement>();
            if (auto result = accept(TokenType::KEYWORD_IF); !result)
            {
                return std::unexpected(result.error());
            }
            if (auto result = accept(TokenType::LEFT_PAREN); !result)
            {
                return std::unexpected(result.error());
            }

            auto testingResult = expression();
            if (!testingResult)
            {
                return std::unexpected(testingResult.error());
            }
            ifstmt->testing = std::move(*testingResult);
            if (auto result = accept(TokenType::RIGHT_PAREN); !result)
            {
                return std::unexpected(result.error());
            }
            auto consequenceResult = statement();
            if (!consequenceResult)
            {
                return std::unexpected(consequenceResult.error());
            }
            ifstmt->consequence = std::move(*consequenceResult);

            if (expect(TokenType::KEYWORD_ELSE))
            {
                if (auto result = accept(TokenType::KEYWORD_ELSE); !result)
                {
                    return std::unexpected(result.error());
                }
                auto alternativeResult = statement();
                if (!alternativeResult)
                {
                    return std::unexpected(alternativeResult.error());
                }
                ifstmt->alternative = std::move(*alternativeResult);
            }

            return ifstmt;
        }

        auto loopStatement() -> ParseResult<ASTRef<LoopStatement>>
        {
            if (auto result = accept(TokenType::KEYWORD_LOOP); !result)
            {
                return std::unexpected(result.error());
            }
            auto loopStmt = makeast<LoopStatement>();
            while (expect(TokenType::ID))
            {
                auto identifier = state->repr;
                accept(TokenType::ID);
                auto loopBindingType = LoopBindingType::LOOP_ASSIGN;
                switch (state->type)
                {
                case TokenType::OPERATOR:
                    if (state->operatorType == Operators::ASSIGN)
                    {
                        accept(TokenType::OPERATOR);
                        loopBindingType = LoopBindingType::LOOP_ASSIGN;
                    }
                    else
                    {
                        return std::unexpected(state.error("Unexpected loop binding"));
                    }
                    break;
                case TokenType::KEYWORD_IN:
                    accept(TokenType::KEYWORD_IN);
                    loopBindingType = LoopBindingType::LOOP_IN;
                    break;
                default:
                    return std::unexpected(state.error("Unexpected loop binding"));
                }
                auto bindingTarget = expression();

                if (bindingTarget)
                {
                    loopStmt->bindings.emplace_back(LoopBinding{
                        .name = identifier,
                        .type = loopBindingType,
                        .target = bindingTarget.value()});
                }
                else
                {
                    return std::unexpected(bindingTarget.error());
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

            auto body = statement();
            if (body)
            {
                loopStmt->loopBody = *body;
            }
            else
            {
                return std::unexpected(body.error());
            }
            return loopStmt;
        }

        auto nextStatement() -> ParseResult<ASTRef<NextStatement>>
        {
            if (auto result = accept(TokenType::KEYWORD_NEXT); !result)
            {
                return std::unexpected(result.error());
            }
            auto nextStmt = makeast<NextStatement>();

            while (!expect(TokenType::SEMICOLON))
            {
                auto expr = expression();
                if (expr)
                {
                    nextStmt->expressions.push_back(expr.value());
                }
                else
                {
                    return std::unexpected(expr.error());
                }
                if (!expect(TokenType::SEMICOLON))
                {
                    if (auto result = accept(TokenType::COMMA); !result)
                    {
                        return std::unexpected(result.error());
                    }
                }
            }

            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }

            return nextStmt;
        }

        auto compoundStatement() -> ParseResult<ASTRef<CompoundStatement>>
        {
            if (auto result = accept(TokenType::LEFT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }
            auto stmt = makeast<CompoundStatement>();
            while (!expect(TokenType::RIGHT_CURLY))
            {
                auto statementResult = statement();
                if (!statementResult)
                {
                    return std::unexpected(statementResult.error());
                }
                stmt->statements.push_back(std::move(*statementResult));
            }
            if (auto result = accept(TokenType::RIGHT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }

            return stmt;
        }

        auto returnBy(TokenType type) -> ParseResult<ASTRef<ReturnStatement>>
        {
            if (auto result = accept(type); !result)
            {
                return std::unexpected(result.error());
            }
            auto exprResult = expression();
            if (!exprResult)
            {
                return std::unexpected(exprResult.error());
            }
            auto ret = makeast<ReturnStatement>();
            ret->expression = std::move(*exprResult);
            if (auto result = accept(TokenType::SEMICOLON); !result)
            {
                return std::unexpected(result.error());
            }
            return ret;
        }

        auto returnStatement() -> ParseResult<ASTRef<ReturnStatement>>
        {
            return returnBy(TokenType::KEYWORD_RETURN);
        }

        auto arrowReturn() -> ParseResult<ASTRef<ReturnStatement>>
        {
            return returnBy(TokenType::DUAL_ARROW);
        }

        auto typeCheckExpr(ASTRef<Expression> expr) -> ParseResult<ASTRef<TypeCheckingExpression>>
        {
            if (auto result = accept(TokenType::KEYWORD_IS); !result)
            {
                return std::unexpected(result.error());
            }
            auto type = expression();
            if (!type)
            {
                return std::unexpected(type.error());
            }
            if (!dynamic_ast_cast<IdExpression>(*type) && !dynamic_ast_cast<IdAccessorExpression>(*type))
            {
                std::unexpected("Unexpected expression " + (*type)->repr());
            }

            return makeast<TypeCheckingExpression>(expr, *type);
        }

        auto expression() -> ParseResult<ASTRef<Expression>>
        {
            auto exprResult = primaryExpression();
            if (!exprResult)
            {
                return std::unexpected(exprResult.error());
            }
            auto expr = std::move(*exprResult);

            while (!expectExpressionTerminator() || expect(TokenType::OPERATOR))
            {
                if (expect(TokenType::LEFT_PAREN))
                {
                    auto funCallResult = funCallExpression(std::move(expr));
                    if (!funCallResult)
                    {
                        return std::unexpected(funCallResult.error());
                    }
                    expr = std::move(*funCallResult);
                }
                else if (expect(TokenType::DOT))
                {
                    auto idAccResult = idAccessorExpression(std::move(expr));
                    if (!idAccResult)
                    {
                        return std::unexpected(idAccResult.error());
                    }
                    expr = std::move(*idAccResult);
                }
                else if (expect(TokenType::KEYWORD_IS))
                {
                    auto typeCheckResult = typeCheckExpr(expr);
                    if (!typeCheckResult)
                    {
                        return std::unexpected(typeCheckResult.error());
                    }
                    expr = *typeCheckResult;
                }
                else if (expect(TokenType::OPERATOR))
                {
                    if (state->operatorType == Operators::ASSIGN)
                    {
                        auto assignExpr = assignmentExpression(std::move(expr));
                        if (!assignExpr)
                        {
                            return std::unexpected(assignExpr.error());
                        }
                        expr = std::move(*assignExpr);
                    }
                    else
                    {
                        auto binExprResult = binaryExpression(std::move(expr));
                        if (!binExprResult)
                        {
                            return std::unexpected(binExprResult.error());
                        }
                        expr = std::move(*binExprResult);
                    }
                }
                else if (expect(TokenType::LEFT_SQUARE))
                {
                    auto indexAccResult = indexAccessorExpression(std::move(expr));
                    if (!indexAccResult)
                    {
                        return std::unexpected(indexAccResult.error());
                    }
                    expr = std::move(*indexAccResult);
                }
            }

            return expr;
        }

        auto assignmentExpression(ASTRef<Expression> ref) -> ParseResult<ASTRef<AssignmentExpression>>
        {
            auto idExpr = dynamic_ast_cast<IdExpression>(ref);
            auto idAccessor = dynamic_ast_cast<IdAccessorExpression>(ref);
            if (!idExpr && !idAccessor)
            {
                return std::unexpected(state.error("Unexpected expression, expect identifier to assign"));
            }

            if (state->operatorType != Operators::ASSIGN)
            {
                return std::unexpected(state.error("Only assignment operator can assign"));
            }

            if (auto result = accept(TokenType::OPERATOR); !result)
            {
                return std::unexpected(result.error());
            }
            auto result = expression();
            if (!result)
            {
                return std::unexpected(result.error());
            }

            auto assignmentExpr = makeast<AssignmentExpression>(ref);

            assignmentExpr->value = *result;

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
                   expect(TokenType::OPERATOR) ||
                   state.eof();
        }

        auto binaryExpression(ASTRef<Expression> expr) -> ParseResult<ASTRef<BinaryExpression>>
        {
            auto &&token = state.current();
            if (auto result = accept(TokenType::OPERATOR); !result)
            {
                return std::unexpected(result.error());
            }
            auto binexpr = makeast<BinaryExpression>();
            binexpr->optr = std::make_shared<Token>(token);
            binexpr->left = std::move(expr);
            auto rightResult = expression();
            if (!rightResult)
            {
                return std::unexpected(rightResult.error());
            }
            binexpr->right = std::move(*rightResult);
            return binexpr;
        }

        auto funCallExpression(ASTRef<Expression> primaryExpression) -> ParseResult<ASTRef<FunCallExpression>>
        {
            if (auto result = accept(TokenType::LEFT_PAREN); !result)
            {
                return std::unexpected(result.error());
            }
            Vec<ASTRef<Expression>> args{};
            while (!expect(TokenType::RIGHT_PAREN))
            {
                auto argResult = expression();
                if (!argResult)
                {
                    return std::unexpected(argResult.error());
                }
                args.push_back(std::move(*argResult));
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                if (auto result = accept(TokenType::COMMA); !result)
                {
                    return std::unexpected(result.error());
                }
            }
            if (auto result = accept(TokenType::RIGHT_PAREN); !result)
            {
                return std::unexpected(result.error());
            }
            auto funcall = makeast<FunCallExpression>();
            funcall->primaryExpression = std::move(primaryExpression);
            funcall->arguments = std::move(args);
            return funcall;
        }

        auto idAccessorExpression(ASTRef<Expression> expr) -> ParseResult<ASTRef<IdAccessorExpression>>
        {
            if (auto result = accept(TokenType::DOT); !result)
            {
                return std::unexpected(result.error());
            }
            auto idacc = makeast<IdAccessorExpression>();
            idacc->primaryExpression = std::move(expr);

            if (expect(TokenType::ID))
            {
                auto idExprResult = idExpression();
                if (!idExprResult)
                {
                    return std::unexpected(idExprResult.error());
                }
                idacc->accessor = std::move(*idExprResult);
            }
            else
            {
                return unexpected(state);
            }

            if (expect(TokenType::LEFT_PAREN))
            {
                if (auto result = accept(TokenType::LEFT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }

                Vec<ASTRef<Expression>> args{};

                while (!expect(TokenType::RIGHT_PAREN))
                {
                    auto exprResult = expression();
                    if (!exprResult)
                    {
                        return std::unexpected(exprResult.error());
                    }
                    args.push_back(std::move(*exprResult));
                    if (!expect(TokenType::COMMA))
                    {
                        break;
                    }
                    if (auto result = accept(TokenType::COMMA); !result)
                    {
                        return std::unexpected(result.error());
                    }
                }

                if (auto result = accept(TokenType::RIGHT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }

                idacc->arguments = std::move(args);
            }
            return idacc;
        }

        auto indexAccessorExpression(ASTRef<Expression> primary) -> ParseResult<ASTRef<Expression>>
        {
            if (auto result = accept(TokenType::LEFT_SQUARE); !result)
            {
                return std::unexpected(result.error());
            }

            auto accessorResult = expression();
            if (!accessorResult)
            {
                return std::unexpected(accessorResult.error());
            }

            if (auto result = accept(TokenType::RIGHT_SQUARE); !result)
            {
                return std::unexpected(result.error());
            }
            if (expect(TokenType::OPERATOR) && state->operatorType == Operators::ASSIGN)
            {
                if (auto result = accept(TokenType::OPERATOR); !result)
                {
                    return std::unexpected(result.error());
                }
                auto valueResult = expression();
                if (!valueResult)
                {
                    return std::unexpected(valueResult.error());
                }
                return makeast<IndexAssignmentExpression>(std::move(primary), std::move(*accessorResult), std::move(*valueResult));
            }
            return makeast<IndexAccessorExpression>(std::move(primary), std::move(*accessorResult));
        }

        auto newObjectExpression() -> ParseResult<ASTRef<NewObjectExpression>>
        {
            ASTRef<NewObjectExpression> newObj = makeast<NewObjectExpression>();

            if (auto result = accept(TokenType::KEYWORD_NEW); !result)
            {
                return std::unexpected(result.error());
            }

            auto typeNameResult = idExpression();
            if (!typeNameResult)
            {
                return std::unexpected(typeNameResult.error());
            }
            newObj->typeName = (*typeNameResult)->repr();

            if (auto result = accept(TokenType::LEFT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }

            while (!expect(TokenType::RIGHT_CURLY))
            {
                auto propertyNameResult = idExpression();
                if (!propertyNameResult)
                {
                    return std::unexpected(propertyNameResult.error());
                }
                if (auto result = accept(TokenType::COLON); !result)
                {
                    return std::unexpected(result.error());
                }
                auto exprResult = expression();
                if (!exprResult)
                {
                    return std::unexpected(exprResult.error());
                }

                newObj->properties[(*propertyNameResult)->repr()] = std::move(*exprResult);
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                if (auto result = accept(TokenType::COMMA); !result)
                {
                    return std::unexpected(result.error());
                }
            }

            if (auto result = accept(TokenType::RIGHT_CURLY); !result)
            {
                return std::unexpected(result.error());
            }

            return newObj;
        }

        auto primaryExpression() -> ParseResult<ASTRef<Expression>>
        {
            if (expect(TokenType::LEFT_PAREN))
            {
                if (auto result = accept(TokenType::LEFT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }
                auto expr = expression();
                if (expect(TokenType::COMMA))
                {
                    // TODO: TupleLiteral!
                }
                if (auto result = accept(TokenType::RIGHT_PAREN); !result)
                {
                    return std::unexpected(result.error());
                }

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
                if (auto result = accept(TokenType::KEYWORD_TRUE); !result)
                {
                    return std::unexpected(result.error());
                }

                return makeast<BooleanValue>(true);
            }
            if (expect(TokenType::KEYWORD_FALSE))
            {
                if (auto result = accept(TokenType::KEYWORD_FALSE); !result)
                {
                    return std::unexpected(result.error());
                }
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
            return std::unexpected(state.error("Unexpected primary annotation"));
        }

        auto stringValue() -> ParseResult<ASTRef<StringValue>>
        {
            const auto &str = state->repr;
            if (auto result = accept(TokenType::STRING); !result)
            {
                return std::unexpected(result.error());
            }
            return makeast<StringValue>(str);
        }

        auto numberLiteral() -> ParseResult<ASTRef<Expression>>
        {
            auto integer = state->repr;
            switch (state->type)
            {
            case TokenType::NUMBER:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int32_t>>(std::stoi(integer));
            case TokenType::INTEGRAL:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
            case TokenType::NUMBER_I8:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int8_t>>(static_cast<int8_t>(std::stoi(integer)));
            case TokenType::NUMBER_U8:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<uint8_t>>(static_cast<uint8_t>(std::stoi(integer)));
            case TokenType::NUMBER_I16:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int16_t>>(static_cast<int16_t>(std::stoi(integer)));
            case TokenType::NUMBER_U16:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<uint16_t>>(static_cast<uint16_t>(std::stoi(integer)));
            case TokenType::NUMBER_I32:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int32_t>>(static_cast<int32_t>(std::stoi(integer)));
            case TokenType::NUMBER_U32:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<uint32_t>>(static_cast<uint32_t>(std::stoul(integer)));
            case TokenType::NUMBER_I64:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<int64_t>>(static_cast<int64_t>(std::stoll(integer)));
            case TokenType::NUMBER_U64:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<IntegralValue<uint64_t>>(static_cast<uint64_t>(std::stoull(integer)));
            case TokenType::FLOATING_POINT:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<FloatingPointValue<float>>(std::stof(integer));
            case TokenType::NUMBER_F16:
                return std::unexpected(state.error("Float16 not supported"));
            //     accept(state->type); // NOLINT(*-unused-return-value)
            //     return makeast<FloatingPointValue<float16_t>>(static_cast<float16_t>(std::stof(integer)));
            case TokenType::NUMBER_F32:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<FloatingPointValue<float>>(std::stof(integer));
            case TokenType::NUMBER_F64:
                accept(state->type); // NOLINT(*-unused-return-value)
                return makeast<FloatingPointValue<double>>(static_cast<uint64_t>(std::stod(integer)));
            case TokenType::NUMBER_F128:
                return std::unexpected(state.error("Float128 not supported"));
            //     accept(state->type); // NOLINT(*-unused-return-value)
            //     return makeast<FloatingPointValue<float128_t>>(static_cast<float128_t>(std::stold(integer)));
            default:
                return std::unexpected(state.error("Invalid number literal"));
            }
        }

        auto idExpression() -> ParseResult<ASTRef<IdExpression>>
        {
            auto identifier = state->repr;
            if (auto result = accept(TokenType::ID); !result)
            {
                return std::unexpected(result.error());
            }
            return makeast<IdExpression>(identifier);
        }

        auto arrayLiteral() -> ParseResult<ASTRef<Expression>>
        {
            if (auto result = accept(TokenType::LEFT_SQUARE); !result)
            {
                return std::unexpected(result.error());
            }

            Vec<ASTRef<Expression>> elements{};

            while (!expect(TokenType::RIGHT_SQUARE))
            {
                auto elemResult = expression();
                if (!elemResult)
                {
                    return std::unexpected(elemResult.error());
                }
                elements.push_back(std::move(*elemResult));
                if (!expect(TokenType::COMMA))
                {
                    break;
                }
                if (auto result = accept(TokenType::COMMA); !result)
                {
                    return std::unexpected(result.error());
                }
            }
            if (auto result = accept(TokenType::RIGHT_SQUARE); !result)
            {
                return std::unexpected(result.error());
            }

            return makeast<ArrayLiteral>(std::move(elements));
        }
    };

    auto Parser::parse(const Str &fileName) -> ParseResult<ASTRef<ASTNode>>
    {
        return ParserImpl(state).parse(fileName);
    }

} // namespace NG
