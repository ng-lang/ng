
#include "common.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <ast.hpp>

namespace NG::parsing {
    using namespace NG;
    using namespace NG::ast;

    class ParserImpl {
        ParseState &state;

        void unexpected(ParseState &state) {
            throw ParseException(std::string{"Unexpected token "} + state->repr);
        }

    public:
        explicit ParserImpl(ParseState &state)
                : state(state) {
        }


        ASTRef<ASTNode> parse(const Str &fileName) {
            // file as default module
            auto mod = makeast<Module>();

            auto compileUnit = makeast<CompileUnit>();

            compileUnit->fileName = fileName;
            compileUnit->modules.push_back(mod);

            while (!state.eof()) {
                switch (state->type) {
                    case TokenType::KEYWORD_FUN:
                        mod->definitions.push_back(funDef());
                        break;
                    case TokenType::KEYWORD_VAL:
                        mod->definitions.push_back(valDef());
                        break;
                    case TokenType::KEYWORD_TYPE:
                        mod->definitions.push_back(typeDef());
                        break;
                    case TokenType::KEYWORD_SIG:
                    case TokenType::KEYWORD_CONS:
                    case TokenType::KEYWORD_MODULE: {
                        ASTRef<Module> subModule = moduleDecl();
                        compileUnit->modules.push_back(subModule);
                        mod = subModule;
                        break;
                    }
                    case TokenType::KEYWORD_EXPORT:
                    case TokenType::KEYWORD_IMPORT:
                        mod->imports.push_back(importDecl());
                        break;
                        // case TokenType::KEYWORD_IF:
                    case TokenType::KEYWORD_CASE:
                    case TokenType::KEYWORD_LOOP:
                    case TokenType::KEYWORD_COLLECT:
                    case TokenType::KEYWORD_UNIT:
                    default:
                        mod->statements.push_back(statement());
                }
            }
            return compileUnit;
        }

    private:
        bool expect(TokenType type) {
            return !state.eof() && state->type == type;
        }

        void accept(TokenType type) {
            if (!expect(type))
                return unexpected(state);
            state.next();
        }

        ASTRef<FunctionDef> funDef() {
            accept(TokenType::KEYWORD_FUN);
            if (expect(TokenType::ID)) {
                auto def = makeast<FunctionDef>();
                def->funName = state->repr;
                accept(TokenType::ID);

                auto &&params = funParams();
                def->params.insert(def->params.begin(), params.begin(), params.end());

                def->body = statement();

                return def;
            }
            return nullptr;
        }

        ASTRef<ImportDecl> importDecl() {
            accept(TokenType::KEYWORD_IMPORT);
            auto imp = makeast<ImportDecl>();

            // direct import
            if (expect(TokenType::ID)) {
                Str module = state->repr;
                accept(TokenType::ID);
                imp->module = module;
                imp->alias = module;

                if (!expect(TokenType::SEMICOLON)) {
                    unexpected(state);
                }
            }

            // alias import
            if (expect(TokenType::STRING)) {
                Str module = stringValue()->value;
                imp->module = module;

                if (expect(TokenType::ID)) {
                    Str alias = state->repr;
                    accept(TokenType::ID);
                    imp->alias = alias;
                } else {
                    imp->alias = module;
                }
            }

            // symbol import
            if (expect(TokenType::LEFT_PAREN)) {
                accept(TokenType::LEFT_PAREN);

                while (expect(TokenType::ID)) {
                    imp->imports.push_back(state->repr);
                    accept(TokenType::ID);
                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    accept(TokenType::COMMA);
                }
                if (imp->imports.empty() && expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES) {
                    accept(TokenType::OPERATOR);
                    imp->imports.push_back("*");
                }
                accept(TokenType::RIGHT_PAREN);
            } else {
                if (expect(TokenType::OPERATOR) && state->operatorType == Operators::TIMES) {
                    accept(TokenType::OPERATOR);
                    imp->imports.push_back("*");
                }
            }

            accept(TokenType::SEMICOLON);

            return imp;
        }

        ASTRef<ValDef> valDef() {
            auto valDefStmt = valDefStatement();

            return makeast<ValDef>(valDefStmt);
        }

        ASTRef<PropertyDef> propertyDef() {
            accept(TokenType::KEYWORD_PROPERTY);

            const Str &name = idExpression()->repr();

            accept(TokenType::SEMICOLON);
            return makeast<PropertyDef>(name);
        }

        ASTRef<TypeDef> typeDef() {
            ASTRef<TypeDef> typeDef = makeast<TypeDef>();

            accept(TokenType::KEYWORD_TYPE);

            typeDef->typeName = idExpression()->repr();

            accept(TokenType::LEFT_CURLY);
            while (!expect(TokenType::RIGHT_CURLY)) {
                if (expect(TokenType::KEYWORD_PROPERTY)) {
                    typeDef->properties.push_back(propertyDef());
                } else if (expect(TokenType::KEYWORD_FUN)) {
                    typeDef->memberFunctions.push_back(funDef());
                }
            }
            accept(TokenType::RIGHT_CURLY);

            return typeDef;
        }

        ASTRef<Module> moduleDecl() {
            auto mod = makeast<Module>();
            Str moduleName{};
            accept(TokenType::KEYWORD_MODULE);
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
                accept(TokenType::KEYWORD_EXPORTS);
                if (state->type == TokenType::OPERATOR && state->operatorType == Operators::TIMES) {
                    accept(TokenType::OPERATOR);
                    mod->exports.push_back("*");
                } else {
                    mod->exports = exportList();
                }
            }
            accept(TokenType::SEMICOLON);
            if (mod->name == "default") {
                mod->name = moduleName;
            } else {
                mod->name += ".";
                mod->name += moduleName;
            }
            return mod;
        }

        Vec<Str> exportList() {
            bool withParen = false;

            Vec<Str> exports{};

            if (expect(TokenType::LEFT_PAREN)) {
                withParen = true;
                accept(TokenType::LEFT_PAREN);
            }

            while (expect(TokenType::ID)) {
                auto &&symbol = state->repr;
                exports.push_back(symbol);
                accept(TokenType::ID);
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                accept(TokenType::COMMA);
            }
            if (withParen) {
                accept(TokenType::RIGHT_PAREN);
            }

            return exports;
        }

        Vec<ASTRef<Param>> funParams() {
            Vec<ASTRef<Param>> params{};
            if (expect(TokenType::LEFT_PAREN)) {
                accept(TokenType::LEFT_PAREN);

                while (expect(TokenType::ID)) {
                    const Str &name = state->repr;
                    accept(TokenType::ID);
                    if (expect(TokenType::COLON)) {
                        accept(TokenType::COLON);
                        const Str &type = state->repr;
                        accept(TokenType::ID);

                        params.push_back(makeast<Param>(name, type));
                    } else {
                        params.push_back(makeast<Param>(name));
                    }

                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    accept(TokenType::COMMA);
                }

                accept(TokenType::RIGHT_PAREN);
            }
            return params;
        }

        ASTRef<Statement> statement() {
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

        ASTRef<SimpleStatement> simpleStatement() {
            auto expr = expression();

            accept(TokenType::SEMICOLON);
            auto stmt = makeast<SimpleStatement>();
            stmt->expression = expr;

            return stmt;
        }

        ASTRef<ValDefStatement> valDefStatement() {
            accept(TokenType::KEYWORD_VAL);
            auto name = state->repr;
            accept(TokenType::ID);
            if (state->operatorType != Operators::ASSIGN) {
                unexpected(state);
                return nullptr;
            }
            accept(TokenType::OPERATOR); // Assignment operator
            auto value = expression();
            accept(TokenType::SEMICOLON);
            auto def = makeast<ValDefStatement>(name);
            def->value = value;
            return def;
        }

        ASTRef<IfStatement> ifStatement() {

            auto ifstmt = makeast<IfStatement>();
            accept(TokenType::KEYWORD_IF);
            accept(TokenType::LEFT_PAREN);

            ifstmt->testing = expression();
            accept(TokenType::RIGHT_PAREN);
            ifstmt->consequence = statement();

            if (expect(TokenType::KEYWORD_ELSE)) {
                accept(TokenType::KEYWORD_ELSE);
                ifstmt->alternative = statement();
            }

            return ifstmt;
        }

        ASTRef<CompoundStatement> compoundStatement() {
            accept(TokenType::LEFT_CURLY);
            auto stmt = makeast<CompoundStatement>();
            while (!expect(TokenType::RIGHT_CURLY)) {
                stmt->statements.push_back(statement());
            }
            accept(TokenType::RIGHT_CURLY);

            return stmt;
        }

        ASTRef<ReturnStatement> returnBy(TokenType type) {
            accept(type);
            auto expr = expression();
            auto ret = makeast<ReturnStatement>();
            ret->expression = expr;
            accept(TokenType::SEMICOLON);
            return ret;
        }

        ASTRef<ReturnStatement> returnStatement() {
            return returnBy(TokenType::KEYWORD_RETURN);
        }

        ASTRef<ReturnStatement> arrowReturn() {
            return returnBy(TokenType::DUAL_ARROW);
        }

        ASTRef<Expression> expression() {
            auto expr = primaryExpression();

            while (!expectExpressionTerminator() || expect(TokenType::OPERATOR)) {
                if (expect(TokenType::LEFT_PAREN)) {
                    expr = funCallExpression(expr);
                } else if (expect(TokenType::DOT)) {
                    expr = idAccessorExpression(expr);
                } else if (expect(TokenType::OPERATOR)) {
                    expr = binaryExpression(expr);
                } else if (expect(TokenType::LEFT_SQUARE)) {
                    expr = indexAccessorExpression(expr);
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

        ASTRef<BinaryExpression> binaryExpression(ASTRef<Expression> expr) {
            auto &&token = state.current();
            accept(TokenType::OPERATOR);
            auto binexpr = makeast<BinaryExpression>();
            binexpr->optr = new Token{token};
            binexpr->left = expr;
            binexpr->right = expression();
            return binexpr;
        }

        ASTRef<FunCallExpression> funCallExpression(ASTRef<Expression> primaryExpression) {
            accept(TokenType::LEFT_PAREN);
            Vec<ASTRef<Expression>> args{};
            while (!expect(TokenType::RIGHT_PAREN)) {
                auto arg = expression();
                args.push_back(arg);
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_PAREN);
            auto funcall = makeast<FunCallExpression>();
            funcall->primaryExpression = primaryExpression;
            funcall->arguments.insert(funcall->arguments.begin(), args.begin(), args.end());
            return funcall;
        }

        ASTRef<IdAccessorExpression> idAccessorExpression(ASTRef<Expression> expr) {
            accept(TokenType::DOT);
            auto idacc = makeast<IdAccessorExpression>();
            idacc->primaryExpression = expr;

            if (expect(TokenType::ID)) {
                idacc->accessor = idExpression();
            } else {
                unexpected(state);
            }

            if (expect(TokenType::LEFT_PAREN)) {
                accept(TokenType::LEFT_PAREN);

                Vec<ASTRef<Expression>> args{};

                while (!expect(TokenType::RIGHT_PAREN)) {
                    args.push_back(expression());
                    if (!expect(TokenType::COMMA)) {
                        break;
                    }
                    accept(TokenType::COMMA);
                }

                accept(TokenType::RIGHT_PAREN);

                idacc->arguments = args;
            }
            return idacc;
        }

        ASTRef<Expression> indexAccessorExpression(ASTRef<Expression> primary) {
            accept(TokenType::LEFT_SQUARE);

            auto accessor = expression();

            accept(TokenType::RIGHT_SQUARE);
            if (expect(TokenType::OPERATOR) && state->operatorType == Operators::ASSIGN) {
                accept(TokenType::OPERATOR);
                auto value = expression();
                return makeast<IndexAssignmentExpression>(primary, accessor, value);
            }
            return makeast<IndexAccessorExpression>(primary, accessor);
        }

        ASTRef<NewObjectExpression> newObjectExpression() {
            ASTRef<NewObjectExpression> newObj = makeast<NewObjectExpression>();

            accept(TokenType::KEYWORD_NEW);

            newObj->typeName = idExpression()->repr();

            accept(TokenType::LEFT_CURLY);

            while (!expect(TokenType::RIGHT_CURLY)) {
                auto &&propertyName = idExpression()->repr();
                accept(TokenType::COLON);
                auto &&expr = expression();

                newObj->properties[propertyName] = expr;
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                accept(TokenType::COMMA);
            }

            accept(TokenType::RIGHT_CURLY);

            return newObj;
        }

        ASTRef<Expression> primaryExpression() {
            if (expect(TokenType::LEFT_PAREN)) {
                accept(TokenType::LEFT_PAREN);
                auto expr = expression();
                if (expect(TokenType::COMMA)) {
                    // TODO: TupleLiteral!
                }
                accept(TokenType::RIGHT_PAREN);

                return expr;
            } else if (expect(TokenType::ID)) {
                return idExpression();
            } else if (expect(TokenType::NUMBER)) {
                return numberLiteral();
            } else if (expect(TokenType::STRING)) {
                return stringValue();
            } else if (expect(TokenType::KEYWORD_TRUE)) {
                auto &val = state->type;
                accept(TokenType::KEYWORD_TRUE);

                return makeast<BooleanValue>(true);
            } else if (expect(TokenType::KEYWORD_FALSE)) {
                auto &val = state->type;
                accept(TokenType::KEYWORD_FALSE);

                return makeast<BooleanValue>(false);
            } else if (expect(TokenType::LEFT_SQUARE)) {
                return arrayLiteral();
            } else if (expect(TokenType::KEYWORD_NEW)) {
                return newObjectExpression();
            }
            return nullptr;
        }

        ASTRef<StringValue> stringValue() {
            auto &str = state->repr;
            accept(TokenType::STRING);
            return makeast<StringValue>(str);
        }

        ASTRef<Expression> numberLiteral() {
            auto integer = state->repr;
            accept(TokenType::NUMBER);
            return makeast<IntegerValue>(std::stoi(integer));
        }

        ASTRef<IdExpression> idExpression() {
            auto id = state->repr;
            accept(TokenType::ID);
            return makeast<IdExpression>(id);
        }

        ASTRef<Expression> arrayLiteral() {
            accept(TokenType::LEFT_SQUARE);

            Vec<ASTRef<Expression>> elements{};

            while (!expect(TokenType::RIGHT_SQUARE)) {
                auto elem = expression();
                elements.push_back(elem);
                if (!expect(TokenType::COMMA)) {
                    break;
                }
                accept(TokenType::COMMA);
            }
            accept(TokenType::RIGHT_SQUARE);

            return makeast<ArrayLiteral>(elements);
        }

    };


    ASTRef<ASTNode> Parser::parse(const Str &fileName) {
        return ParserImpl(state).parse(fileName);
    }

} // namespace NG
