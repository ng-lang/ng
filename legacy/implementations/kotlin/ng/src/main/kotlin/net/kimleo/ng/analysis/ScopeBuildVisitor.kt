package net.kimleo.ng.analysis

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.FunType
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.Program
import net.kimleo.ng.parser.node.decl.Func
import net.kimleo.ng.parser.node.decl.Signature
import net.kimleo.ng.parser.node.decl.ValDecl
import net.kimleo.ng.parser.node.decl.type.DeclTypeName
import net.kimleo.ng.parser.node.decl.type.TypeAlias
import net.kimleo.ng.parser.node.decl.type.TypeArg
import net.kimleo.ng.parser.node.decl.type.TypeDecl
import net.kimleo.ng.parser.node.decl.type.param.MappedTypeParam
import net.kimleo.ng.parser.node.decl.type.param.SimpleTypeParam
import net.kimleo.ng.parser.node.expr.ApplyExpression
import net.kimleo.ng.parser.node.expr.IfExpr
import net.kimleo.ng.parser.node.expr.Operator
import net.kimleo.ng.parser.node.expr.TupleExpr
import net.kimleo.ng.parser.node.literal.*
import net.kimleo.ng.parser.node.pattern.TaggedTuplePattern
import net.kimleo.ng.parser.node.pattern.TuplePattern
import net.kimleo.ng.parser.node.typecons.EnumCons
import net.kimleo.ng.parser.node.typecons.ProductCons

class ScopeBuildVisitor: Visitor<Scope> {

    var currentScope = Scope(null)
    var scopes = mutableMapOf<Node, Scope>()

    override fun visit(array: ArrayLiteral): Scope {
        for (item in array.items) {
            item.accept(this)
        }
        return currentScope
    }

    override fun visit(boolLiteral: BoolLiteral): Scope {
        return currentScope
    }

    override fun visit(char: CharLiteral): Scope {
        return currentScope
    }

    override fun visit(numLiteral: NumLiteral): Scope {
        return currentScope
    }

    override fun visit(string: StringLiteral): Scope {
        return currentScope
    }

    override fun visit(enumCons: EnumCons): Scope {
        currentScope.add(enumCons.id.name, enumCons)
        return currentScope
    }

    override fun visit(productCons: ProductCons): Scope {
        currentScope.add(productCons.id.name, productCons)
        return currentScope
    }

    override fun visit(declTypeName: DeclTypeName): Scope {
        declTypeName.params?.forEach {
            it.accept(this)
        }
        return currentScope
    }

    override fun visit(typeArg: TypeArg): Scope {
        return currentScope
    }

    override fun visit(simpleTypeParam: SimpleTypeParam): Scope {
        currentScope.add(simpleTypeParam.id.name, simpleTypeParam)
        return currentScope
    }

    override fun visit(mappedTypeParam: MappedTypeParam): Scope {
        currentScope.add(mappedTypeParam.id.name, mappedTypeParam)

        mappedTypeParam.type.accept(this)

        return currentScope
    }

    override fun visit(valDecl: ValDecl): Scope {
        valDecl.accept(this)
        currentScope.add(valDecl.id.name, valDecl.value)
        return currentScope
    }

    override fun visit(typeDecl: TypeDecl): Scope {
        currentScope.add(typeDecl.id.name, typeDecl)

        withSubScope(typeDecl) { scope ->
            typeDecl.name.accept(this)
            for (node in typeDecl.value) {
                node.accept(this)
            }
        }

        return currentScope
    }

    override fun visit(func: Func): Scope {
        val existed = currentScope.lookup(func.id.name)
        if (existed is Signature)
            currentScope.addFunc(func, existed)
        else currentScope.add(func.id.name, func)

        withSubScope(func) { scope ->
            func.args.forEach {
                // TODO: Change simple ID to FuncParam
                if (it is ID) {
                    currentScope.add(it.name, it)
                }
                it.accept(this)
            }
            func.body.accept(this)
        }

        return currentScope
    }

    fun withSubScope(node: Node, fn: (scope: Scope) -> Unit) {
        val scope = Scope(currentScope)
        scopes.put(node, scope)
        currentScope = scope
        fn.invoke(scope)
        currentScope = scope.parent!!
    }

    override fun visit(typeAlias: TypeAlias): Scope {
        currentScope.add(typeAlias.id.name, typeAlias)
        withSubScope(typeAlias) {
            typeAlias.name.accept(this)
            typeAlias.alias.accept(this)
        }

        return currentScope
    }

    override fun visit(signature: Signature): Scope {
        currentScope.add(signature.id.name, signature)

        withSubScope(signature) {
            signature.parameters.forEach {
                it.accept(this)
            }
            signature.type.accept(this)
        }

        return currentScope
    }

    override fun visit(id: ID): Scope {
        return currentScope
    }

    override fun visit(funType: FunType): Scope {
        funType.left.accept(this)
        funType.right.accept(this)
        return currentScope
    }

    override fun visit(applyExpression: ApplyExpression): Scope {
        applyExpression.left.accept(this)
        applyExpression.right.accept(this)

        return currentScope
    }

    override fun visit(ifExpr: IfExpr): Scope {

        withSubScope(ifExpr) {
            ifExpr.test.accept(this)
            withSubScope(ifExpr.consequence) {
                ifExpr.consequence.accept(this)
            }
            withSubScope(ifExpr.alternative) {
                ifExpr.alternative.accept(this)
            }
        }
        return currentScope
    }

    override fun visit(operator: Operator): Scope {
        operator.left.accept(this)
        operator.right.accept(this)

        return currentScope
    }

    override fun visit(tuple: TupleExpr): Scope {
        tuple.items.forEach {
            it.accept(this)
        }

        return currentScope
    }

    override fun visit(taggedTuplePattern: TaggedTuplePattern): Scope {
        taggedTuplePattern.tuple.accept(this)

        return currentScope
    }

    override fun visit(tuplePattern: TuplePattern): Scope {
        tuplePattern.patterns.forEach {
            it.accept(this)
        }

        return currentScope
    }

    override fun visit(program: Program): Scope {
        scopes.put(program, currentScope)

        program.decls.forEach {
            it.accept(this)
        }

        return currentScope
    }

    fun dict(): SymbolDict {
        return SymbolDict(currentScope, scopes)
    }
}