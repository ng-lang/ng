package net.kimleo.ng.parser.node.literal

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.expr.Expr

data class ArrayLiteral(val items: List<Expr>) : Literal {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}