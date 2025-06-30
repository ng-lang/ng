package net.kimleo.ng.parser.node.decl

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node

data class ValDecl(override val id: ID, val value: Node) : Decl {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}