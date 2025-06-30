package net.kimleo.ng.parser.node.literal

import net.kimleo.ng.parser.Visitor

data class NumLiteral(val value: String) : Literal {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}