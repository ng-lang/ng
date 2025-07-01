
#include "common.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <ast.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace NG::parsing {
    using namespace NG;
    using namespace NG::ast;

    class ParserImpl {
        ParseState &state;

        auto unexpected(ParseState &state) {
            return std::unexpected(std::string{"Unexpected token "} + state->repr);
        }

    public:
        explicit ParserImpl(ParseState &state)
                : state(state) {
        }


        ParseResult<ASTRef<ASTNode>> parse(const Str &fileName) {
            // file as default module
            auto mod = makeast<Module>();

            auto compileUnit = makeast<CompileUnit>();

            compileUnit->fileName = fileName;
            fs::path filePath {fileName};
            if (fs::exists(filePath)) {
                compileUnit->path = filePath.parent_path().string();
            }
            ASTRef<Module> current_mod = mod;
            compileUnit->modules.push_back(std::move(mod));

            while (!state.eof()) {
                switch (state->type) {
                    case TokenType::KEYWORD_FUN: {
                        auto funDefResult = funDef();
                        if (!funDefResult) return std::unexpected(funDefResult.error());
                        current_mod->definitions.push_back(std::move(*funDefResult));
                        break;
                    }
                    case TokenType::KEYWORD_VAL: {
                        auto valDefResult = valDef();
                        if (!valDefResult) return std::unexpected(valDefResult.error());
                        current_mod->definitions.push_back(std::move(*valDefResult));
                        break;
                    }
                    case TokenType::KEYWORD_TYPE: {
                        auto typeDefResult = typeDef();
                        if (!typeDefResult) return std::unexpected(typeDefResult.error());
                        current_mod->definitions.push_back(std::move(*typeDefResult));
                        break;
                    }
                    case TokenType::KEYWORD_SIG:
                    case TokenType::KEYWORD_CONS:
                    case TokenType::KEYWORD_MODULE: {
                        auto subModuleResult = moduleDecl();
                        if (!subModuleResult) return std::unexpected(subModuleResult.error());
                        current_mod = subModuleResult.value();
                        compileUnit->modules.push_back(std::move(*subModuleResult));
                        break;
                    }
                    case TokenType::KEYWORD_EXPORT:
                    case TokenType::KEYWORD_IMPORT: {
                        auto importDeclResult = importDecl();
                        if (!importDeclResult) return std::unexpected(importDeclResult.error());
                        current_mod->imports.push_back(std::move(*importDeclResult));
                        break;
                    }
                        // case TokenType::KEYWORD_IF:
                    case TokenType::KEYWORD_CASE:
                    case TokenType::KEYWORD_LOOP:
                    case TokenType::KEYWORD_COLLECT:
                    case TokenType::KEYWORD_UNIT:
                    default: {
                        auto statementResult = statement();
                        if (!statementResult) return std::unexpected(statementResult.error());
                        current_mod->statements.push_back(std::move(*statementResult));
                    }
                }
            }
            return compileUnit;
        }

    private:
        bool expect(TokenType type) {
            return !state.eof() && state->type == type;
        }

        ParseResult<void> accept(TokenType type) {
            if (!expect(type))
                return unexpected(state);
            state.next();
            return {};
        }

        ParseResult<ASTRef<FunctionDef>> funDef() {
            if(auto result = accept(TokenType::KEYWORD_FUN); !result) return std::unexpected(result.error());
            if (expect(TokenType::ID)) {
                auto def = makeast<FunctionDef>();
                def->funName = state->repr;
                if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());

                auto paramsResult = funParams();
                if(!paramsResult) return std::unexpected(paramsResult.error());
                def->params = std::move(*paramsResult);

                auto bodyResult = statement();
                if(!bodyResult) return std::unexpected(bodyResult.error());
                def->body = std::move(*bodyResult);

                return def;
            }
            return std::unexpected("Expected function name");
        }

        ParseResult<ASTRef<ImportDecl>> importDecl() {
            if(auto result = accept(TokenType::KEYWORD_IMPORT); !result) return std::unexpected(result.error());
            auto imp = makeast<ImportDecl>();

            // direct import
            if (expect(TokenType::ID)) {
                Str module = state->repr;
                if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
                imp->module = module;
                imp->alias = module;

                if (!expect(TokenType::SEMICOLON)) {
                    return unexpected(state);
                }
            }

            // alias import
            if (expect(TokenType::STRING)) {
                auto stringValueResult = stringValue();
                if(!stringValueResult) return std::unexpected(stringValueResult.error());
                imp->module = (*stringValueResult)->value;

                if (expect(TokenType::ID)) {
                    Str alias = state->repr;
                    if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
                    imp->alias = alias;
                } else {
                    imp->alias = imp->module;
                }
            }

            // symbol import
            if (expect(TokenType::LEFT_PAREN)) {
                if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());

                while (expect(TokenType::ID)) {
                    imp->imports.push_back(state->repr);
                    if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
                }
                if (imp->imports.empty() && expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES) {
                    if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error());
                    imp->imports.push_back("*");
                }
                if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());
            } else {
                if (expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES) {
                    if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error());
                    imp->imports.push_back("*");
                }
            }

            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());

            return imp;
        }

        ParseResult<ASTRef<ValDef>> valDef() {
            auto valDefStmtResult = valDefStatement();
            if(!valDefStmtResult) return std::unexpected(valDefStmtResult.error());

            return makeast<ValDef>(std::move(*valDefStmtResult));
        }

        ParseResult<ASTRef<PropertyDef>> propertyDef() {
            if(auto result = accept(TokenType::KEYWORD_PROPERTY); !result) return std::unexpected(result.error());

            auto nameResult = idExpression();
            if(!nameResult) return std::unexpected(nameResult.error());

            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());
            return makeast<PropertyDef>((*nameResult)->repr());
        }

        ParseResult<ASTRef<TypeDef>> typeDef() {
            ASTRef<TypeDef> typeDef = makeast<TypeDef>();

            if(auto result = accept(TokenType::KEYWORD_TYPE); !result) return std::unexpected(result.error());

            auto typeNameResult = idExpression();
            if(!typeNameResult) return std::unexpected(typeNameResult.error());
            typeDef->typeName = (*typeNameResult)->repr();

            if(auto result = accept(TokenType::LEFT_CURLY); !result) return std::unexpected(result.error());
            while (!expect(TokenType::RIGHT_CURLY)) {
                if (expect(TokenType::KEYWORD_PROPERTY)) {
                    auto propertyDefResult = propertyDef();
                    if(!propertyDefResult) return std::unexpected(propertyDefResult.error());
                    typeDef->properties.push_back(std::move(*propertyDefResult));
                } else if (expect(TokenType::KEYWORD_FUN)) {
                    auto funDefResult = funDef();
                    if(!funDefResult) return std::unexpected(funDefResult.error());
                    typeDef->memberFunctions.push_back(std::move(*funDefResult));
                }
            }
            if(auto result = accept(TokenType::RIGHT_CURLY); !result) return std::unexpected(result.error());

            return typeDef;
        }

        ParseResult<ASTRef<Module>> moduleDecl() {
            auto mod = makeast<Module>();
            Str moduleName{};
            if(auto result = accept(TokenType::KEYWORD_MODULE); !result) return std::unexpected(result.error());
            while (expect(TokenType::ID) || expect(TokenType::DOT)) {
                if (!moduleName.empty() && !expect(TokenType::DOT)) {
                    moduleName += ".";
                }
                if (expect(TokenType::ID)) {
                    moduleName += state->repr;
                }
                state.next();
            }
            if (expect(TokenType::KEYWORD_EXPORTS)) {
                if(auto result = accept(TokenType::KEYWORD_EXPORTS); !result) return std::unexpected(result.error());
                if (state->type == TokenType::OPERATOR && state->operatorType == Operators::TIMES) {
                    if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error());
                    mod->exports.push_back("*");
                } else {
                    auto exportListResult = exportList();
                    if(!exportListResult) return std::unexpected(exportListResult.error());
                    mod->exports = std::move(*exportListResult);
                }
            }
            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());
            if (mod->name == "default") {
                mod->name = moduleName;
            } else {
                mod->name += ".";
                mod->name += moduleName;
            }
            return mod;
        }

        ParseResult<Vec<Str>> exportList() {
            bool withParen = false;

            Vec<Str> exports{};

            if (expect(TokenType::LEFT_PAREN)) {
                withParen = true;
                if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());
            }

            while (expect(TokenType::ID)) {
                auto &&symbol = state->repr;
                exports.push_back(symbol);
                if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
            }
            if (withParen) {
                if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());
            }

            return exports;
        }

        ParseResult<Vec<ASTRef<Param>>> funParams() {
            Vec<ASTRef<Param>> params{};
            if (expect(TokenType::LEFT_PAREN)) {
                if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());

                while (expect(TokenType::ID)) {
                    const Str &name = state->repr;
                    if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
                    if (expect(TokenType::COLON)) {
                        if(auto result = accept(TokenType::COLON); !result) return std::unexpected(result.error());
                        const Str &type = state->repr;
                        if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());

                        params.push_back(makeast<Param>(name, type));
                    } else {
                        params.push_back(makeast<Param>(name));
                    }

                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
                }

                if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());
            }
            return params;
        }

        ParseResult<ASTRef<Statement>> statement() {
            switch (state->type) {
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
                default:
                    return simpleStatement();
            }
        }

        ParseResult<ASTRef<SimpleStatement>> simpleStatement() {
            auto exprResult = expression();
            if(!exprResult) return std::unexpected(exprResult.error());

            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());
            auto stmt = makeast<SimpleStatement>();
            stmt->expression = std::move(*exprResult);

            return stmt;
        }

        ParseResult<ASTRef<ValDefStatement>> valDefStatement() {
            if(auto result = accept(TokenType::KEYWORD_VAL); !result) return std::unexpected(result.error());
            auto name = state->repr;
            if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
            if (state->operatorType != Operators::ASSIGN) {
                return unexpected(state);
            }
            if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error()); // Assignment operator
            auto valueResult = expression();
            if(!valueResult) return std::unexpected(valueResult.error());
            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());
            auto def = makeast<ValDefStatement>(name);
            def->value = std::move(*valueResult);
            return def;
        }

        ParseResult<ASTRef<IfStatement>> ifStatement() {

            auto ifstmt = makeast<IfStatement>();
            if(auto result = accept(TokenType::KEYWORD_IF); !result) return std::unexpected(result.error());
            if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());

            auto testingResult = expression();
            if(!testingResult) return std::unexpected(testingResult.error());
            ifstmt->testing = std::move(*testingResult);
            if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());
            auto consequenceResult = statement();
            if(!consequenceResult) return std::unexpected(consequenceResult.error());
            ifstmt->consequence = std::move(*consequenceResult);

            if (expect(TokenType::KEYWORD_ELSE)) {
                if(auto result = accept(TokenType::KEYWORD_ELSE); !result) return std::unexpected(result.error());
                auto alternativeResult = statement();
                if(!alternativeResult) return std::unexpected(alternativeResult.error());
                ifstmt->alternative = std::move(*alternativeResult);
            }

            return ifstmt;
        }

        ParseResult<ASTRef<CompoundStatement>> compoundStatement() {
            if(auto result = accept(TokenType::LEFT_CURLY); !result) return std::unexpected(result.error());
            auto stmt = makeast<CompoundStatement>();
            while (!expect(TokenType::RIGHT_CURLY)) {
                auto statementResult = statement();
                if(!statementResult) return std::unexpected(statementResult.error());
                stmt->statements.push_back(std::move(*statementResult));
            }
            if(auto result = accept(TokenType::RIGHT_CURLY); !result) return std::unexpected(result.error());

            return stmt;
        }

        ParseResult<ASTRef<ReturnStatement>> returnBy(TokenType type) {
            if(auto result = accept(type); !result) return std::unexpected(result.error());
            auto exprResult = expression();
            if(!exprResult) return std::unexpected(exprResult.error());
            auto ret = makeast<ReturnStatement>();
            ret->expression = std::move(*exprResult);
            if(auto result = accept(TokenType::SEMICOLON); !result) return std::unexpected(result.error());
            return ret;
        }

        ParseResult<ASTRef<ReturnStatement>> returnStatement() {
            return returnBy(TokenType::KEYWORD_RETURN);
        }

        ParseResult<ASTRef<ReturnStatement>> arrowReturn() {
            return returnBy(TokenType::DUAL_ARROW);
        }

        ParseResult<ASTRef<Expression>> expression() {
            auto exprResult = primaryExpression();
            if(!exprResult) return std::unexpected(exprResult.error());
            auto expr = std::move(*exprResult);

            while (!expectExpressionTerminator() || expect(TokenType::OPERATOR)) {
                if (expect(TokenType::LEFT_PAREN)) {
                    auto funCallResult = funCallExpression(std::move(expr));
                    if(!funCallResult) return std::unexpected(funCallResult.error());
                    expr = std::move(*funCallResult);
                } else if (expect(TokenType::DOT)) {
                    auto idAccResult = idAccessorExpression(std::move(expr));
                    if(!idAccResult) return std::unexpected(idAccResult.error());
                    expr = std::move(*idAccResult);
                } else if (expect(TokenType::OPERATOR)) {
                    auto binExprResult = binaryExpression(std::move(expr));
                    if(!binExprResult) return std::unexpected(binExprResult.error());
                    expr = std::move(*binExprResult);
                } else if (expect(TokenType::LEFT_SQUARE)) {
                    auto indexAccResult = indexAccessorExpression(std::move(expr));
                    if(!indexAccResult) return std::unexpected(indexAccResult.error());
                    expr = std::move(*indexAccResult);
                }
            }

            return expr;
        }

        bool expectExpressionTerminator() {
            return expect(TokenType::COLON) ||        // :
                   expect(TokenType::COMMA) ||        // ,
                   expect(TokenType::DUAL_ARROW) ||   // =>
                   expect(TokenType::SINGLE_ARROW) || // ->
                   expect(TokenType::RIGHT_PAREN) ||  // )
                   expect(TokenType::RIGHT_CURLY) ||  // }
                   expect(TokenType::RIGHT_SQUARE) || // ]
                   expect(TokenType::SEMICOLON) ||    // ;
                   expect(TokenType::OPERATOR) ||
                   state.eof();
        }

        ParseResult<ASTRef<BinaryExpression>> binaryExpression(ASTRef<Expression> expr) {
            auto &&token = state.current();
            if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error());
            auto binexpr = makeast<BinaryExpression>();
            binexpr->optr = new Token{token};
            binexpr->left = std::move(expr);
            auto rightResult = expression();
            if(!rightResult) return std::unexpected(rightResult.error());
            binexpr->right = std::move(*rightResult);
            return binexpr;
        }

        ParseResult<ASTRef<FunCallExpression>> funCallExpression(ASTRef<Expression> primaryExpression) {
            if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());
            Vec<ASTRef<Expression>> args{};
            while (!expect(TokenType::RIGHT_PAREN)) {
                auto argResult = expression();
                if(!argResult) return std::unexpected(argResult.error());
                args.push_back(std::move(*argResult));
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
            }
            if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());
            auto funcall = makeast<FunCallExpression>();
            funcall->primaryExpression = std::move(primaryExpression);
            funcall->arguments = std::move(args);
            return funcall;
        }

        ParseResult<ASTRef<IdAccessorExpression>> idAccessorExpression(ASTRef<Expression> expr) {
            if(auto result = accept(TokenType::DOT); !result) return std::unexpected(result.error());
            auto idacc = makeast<IdAccessorExpression>();
            idacc->primaryExpression = std::move(expr);

            if (expect(TokenType::ID)) {
                auto idExprResult = idExpression();
                if(!idExprResult) return std::unexpected(idExprResult.error());
                idacc->accessor = std::move(*idExprResult);
            } else {
                return unexpected(state);
            }

            if (expect(TokenType::LEFT_PAREN)) {
                if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());

                Vec<ASTRef<Expression>> args{};

                while (!expect(TokenType::RIGHT_PAREN)) {
                    auto exprResult = expression();
                    if(!exprResult) return std::unexpected(exprResult.error());
                    args.push_back(std::move(*exprResult));
                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
                }

                if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());

                idacc->arguments = std::move(args);
            }
            return idacc;
        }

        ParseResult<ASTRef<Expression>> indexAccessorExpression(ASTRef<Expression> primary) {
            if(auto result = accept(TokenType::LEFT_SQUARE); !result) return std::unexpected(result.error());

            auto accessorResult = expression();
            if(!accessorResult) return std::unexpected(accessorResult.error());

            if(auto result = accept(TokenType::RIGHT_SQUARE); !result) return std::unexpected(result.error());
            if (expect(TokenType::OPERATOR) && state->operatorType == Operators::ASSIGN) {
                if(auto result = accept(TokenType::OPERATOR); !result) return std::unexpected(result.error());
                auto valueResult = expression();
                if(!valueResult) return std::unexpected(valueResult.error());
                return makeast<IndexAssignmentExpression>(std::move(primary), std::move(*accessorResult), std::move(*valueResult));
            }
            return makeast<IndexAccessorExpression>(std::move(primary), std::move(*accessorResult));
        }

        ParseResult<ASTRef<NewObjectExpression>> newObjectExpression() {
            ASTRef<NewObjectExpression> newObj = makeast<NewObjectExpression>();

            if(auto result = accept(TokenType::KEYWORD_NEW); !result) return std::unexpected(result.error());

            auto typeNameResult = idExpression();
            if(!typeNameResult) return std::unexpected(typeNameResult.error());
            newObj->typeName = (*typeNameResult)->repr();

            if(auto result = accept(TokenType::LEFT_CURLY); !result) return std::unexpected(result.error());

            while (!expect(TokenType::RIGHT_CURLY)) {
                auto propertyNameResult = idExpression();
                if(!propertyNameResult) return std::unexpected(propertyNameResult.error());
                if(auto result = accept(TokenType::COLON); !result) return std::unexpected(result.error());
                auto exprResult = expression();
                if(!exprResult) return std::unexpected(exprResult.error());

                newObj->properties[(*propertyNameResult)->repr()] = std::move(*exprResult);
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
            }

            if(auto result = accept(TokenType::RIGHT_CURLY); !result) return std::unexpected(result.error());

            return newObj;
        }

        ParseResult<ASTRef<Expression>> primaryExpression() {
            if (expect(TokenType::LEFT_PAREN)) {
                if(auto result = accept(TokenType::LEFT_PAREN); !result) return std::unexpected(result.error());
                auto expr = expression();
                if (expect(TokenType::COMMA)) {
                    // TODO: TupleLiteral!
                }
                if(auto result = accept(TokenType::RIGHT_PAREN); !result) return std::unexpected(result.error());

                return expr;
            } else if (expect(TokenType::ID)) {
                return idExpression();
            } else if (expect(TokenType::NUMBER)) {
                return numberLiteral();
            } else if (expect(TokenType::STRING)) {
                return stringValue();
            } else if (expect(TokenType::KEYWORD_TRUE)) {
                auto &val = state->type;
                if(auto result = accept(TokenType::KEYWORD_TRUE); !result) return std::unexpected(result.error());

                return makeast<BooleanValue>(true);
            } else if (expect(TokenType::KEYWORD_FALSE)) {
                auto &val = state->type;
                if(auto result = accept(TokenType::KEYWORD_FALSE); !result) return std::unexpected(result.error());

                return makeast<BooleanValue>(false);
            } else if (expect(TokenType::LEFT_SQUARE)) {
                return arrayLiteral();
            } else if (expect(TokenType::KEYWORD_NEW)) {
                return newObjectExpression();
            }
            return std::unexpected("Unexpected primary expression");
        }

        ParseResult<ASTRef<StringValue>> stringValue() {
            auto &str = state->repr;
            if(auto result = accept(TokenType::STRING); !result) return std::unexpected(result.error());
            return makeast<StringValue>(str);
        }

        ParseResult<ASTRef<Expression>> numberLiteral() {
            auto integer = state->repr;
            if(auto result = accept(TokenType::NUMBER); !result) return std::unexpected(result.error());
            return makeast<IntegerValue>(std::stoi(integer));
        }

        ParseResult<ASTRef<IdExpression>> idExpression() {
            auto id = state->repr;
            if(auto result = accept(TokenType::ID); !result) return std::unexpected(result.error());
            return makeast<IdExpression>(id);
        }

        ParseResult<ASTRef<Expression>> arrayLiteral() {
            if(auto result = accept(TokenType::LEFT_SQUARE); !result) return std::unexpected(result.error());

            Vec<ASTRef<Expression>> elements{};

            while (!expect(TokenType::RIGHT_SQUARE)) {
                auto elemResult = expression();
                if(!elemResult) return std::unexpected(elemResult.error());
                elements.push_back(std::move(*elemResult));
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                if(auto result = accept(TokenType::COMMA); !result) return std::unexpected(result.error());
            }
            if(auto result = accept(TokenType::RIGHT_SQUARE); !result) return std::unexpected(result.error());

            return makeast<ArrayLiteral>(std::move(elements));
        }

    };


    ParseResult<ASTRef<ASTNode>> Parser::parse(const Str &fileName) {
        return ParserImpl(state).parse(fileName);
    }

} // namespace NG
