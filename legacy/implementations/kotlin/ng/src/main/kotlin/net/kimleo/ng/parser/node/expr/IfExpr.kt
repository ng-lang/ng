package net.kimleo.ng.parser.node.expr

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.Node

data class IfExpr(val test: Node, val consequence: Node, val alternative: Node) : Expr {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)

}