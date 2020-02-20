
#include "ast.hpp"
#include <token.hpp>
#include <iostream>
#include <string>
#include <functional>

namespace NG::AST {

    struct NewScope {
        size_t &level;

        explicit NewScope(size_t &_level) : level(_level) {
            level++;
        }

        ~NewScope() {
            level--;
        }
    };

    class ASTDumper : public virtual DefaultDummyAstVisitor {
        size_t level = 0;

        template<class... Args>
        void print(Args &&... args) {
            std::cout << std::string(level * 2, ' ');
            (std::cout << ... << args);
            std::cout << std::endl;
        }

    public:
        void visit(ASTNode *node) override {}

        void visit(Module *mod) override {
            print("Module ", mod->name, " {");
            for (auto def : mod->definitions) {
                def->accept(this);
            }
        }

        void visit(IdExpression *idexpr) override {
            NewScope id{level};
            print("IdExpression {", idexpr->id, "}");
        }

        void visit(FunCallExpression *funCall) override {
            NewScope functionCall{level};
            print("FunCallExpression {");
            print("fun:");
            funCall->primaryExpression->accept(this);
            print("args:");
            for (auto args : funCall->arguments) {
                args->accept(this);
            }
            print("}");
        }

        void visit(IdAccessorExpression *idAcc) override {
            NewScope accessor{level};
            print("IdAccessorExpression {");
            print("primary:");
            idAcc->primaryExpression->accept(this);
            print("accessor:");
            idAcc->accessor->accept(this);
            print("}");
        }

        void visit(BinaryExpression *binExpr) override {
            NewScope bin{level};
            print("BinaryExpression [", binExpr->optr->repr, "] {");
            print("left:");
            binExpr->left->accept(this);
            print("right:");
            binExpr->right->accept(this);
            print("}");
        }

        void visit(FunctionDef *funDef) override {
            print("FunctionDef ", funDef->name(), " {");

            funDef->body->accept(this);
            print("}");
        }

        void visit(ReturnStatement *returnStmt) override {
            NewScope return_{level};

            print("ReturnStatement {");
            returnStmt->expression->accept(this);
            print("}");
        }

        void visit(IfStatement *ifstatement) override {
            NewScope ifStmt{level};
            print("IfStatement {");
            print("testing:");
            ifstatement->testing->accept(this);
            print("consequence:");
            ifstatement->consequence->accept(this);
            if (ifstatement->alternative != nullptr) {
                print("alternative:");
                ifstatement->alternative->accept(this);
            }
            print("}");
        }

        void visit(CompoundStatement *stmts) override {
            NewScope compound{level};
            print("CompountStatement {");
            for (auto stmt : stmts->statements) {
                stmt->accept(this);
            }
            print("}");
        }

        void visit(SimpleStatement *simpleStmt) override {
            NewScope simple{level};

            print("SimpleStatement {");
            simpleStmt->expression->accept(this);
            print("}");
        }

        void visit(StringValue *strVal) override {
            NewScope simple{level};
            print("String[", strVal->value, "]");
        }

        void visit(IntegerValue *intVal) override {
            NewScope simple{level};
            print("Integer[", intVal->value, "]");
        }

        void visit(BooleanValue *boolVal) override {
            NewScope simple{level};
            print("Boolean[", boolVal->value, "]");
        }

        void visit(ValDef *valDef) override {
            NewScope simple{level};

            print("ValueDefinition {");
            valDef->body->accept(this);
            print("}");
        }

        void visit(ValDefStatement *stmt) override {
            NewScope simple{level};
            print("ValDefStmt {");
            print("Name: ", stmt->name);
            print("Value:");
            stmt->value->accept(this);
            print("}");
        }
    };

    IASTVisitor *get_ast_dumper() {
        return new ASTDumper{};
    }
} // namespace NG
