package net.kimleo.ng.parser

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

interface Visitor<out T> {
    fun visit(node: Node): T {
        return node.accept(this)
    }

    /// Literals
    fun visit(array: ArrayLiteral): T
    fun visit(boolLiteral: BoolLiteral): T
    fun visit(char: CharLiteral): T
    fun visit(numLiteral: NumLiteral): T
    fun visit(string: StringLiteral): T

    /// Constructors
    fun visit(enumCons: EnumCons): T
    fun visit(productCons: ProductCons): T

    /// Type Declarations
    fun visit(declTypeName: DeclTypeName): T
    fun visit(typeArg: TypeArg): T
    fun visit(simpleTypeParam: SimpleTypeParam): T
    fun visit(mappedTypeParam: MappedTypeParam): T

    /// Decls
    fun visit(valDecl: ValDecl): T
    fun visit(typeDecl: TypeDecl): T
    fun visit(func: Func): T
    fun visit(typeAlias: TypeAlias): T
    fun visit(signature: Signature): T

    /// Others
    fun visit(id: ID): T
    fun visit(funType: FunType): T

    /// Expressions
    fun visit(applyExpression: ApplyExpression): T
    fun visit(ifExpr: IfExpr): T
    fun visit(operator: Operator): T
    fun visit(tuple: TupleExpr): T

    /// Patterns
    fun visit(taggedTuplePattern: TaggedTuplePattern): T
    fun visit(tuplePattern: TuplePattern): T

    /// Program
    fun visit(program: Program): T
}
