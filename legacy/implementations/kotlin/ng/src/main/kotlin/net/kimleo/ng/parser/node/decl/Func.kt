package net.kimleo.ng.parser.node.decl

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.pattern.Pattern

data class Func(override val id: ID, val args: List<Pattern>, val body: Node, var signature: Signature? = null) : Decl {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}