package net.kimleo.ng.parser.node

import net.kimleo.ng.parser.Visitor

data class FunType(val left: Node, val right: Node) : Node {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}