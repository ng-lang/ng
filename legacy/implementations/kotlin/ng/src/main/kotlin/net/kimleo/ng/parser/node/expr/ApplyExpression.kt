package net.kimleo.ng.parser.node.expr

import net.kimleo.ng.parser.Visitor

data class ApplyExpression(val left: Expr, val right: Expr) : Expr {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}