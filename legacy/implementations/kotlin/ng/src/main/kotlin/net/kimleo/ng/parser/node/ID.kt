package net.kimleo.ng.parser.node

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.expr.Expr
import net.kimleo.ng.parser.node.pattern.Pattern

data class ID(val name: String) : Expr, Pattern {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)

}