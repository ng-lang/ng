package net.kimleo.ng.parser.node.decl.type

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node

data class TypeArg(val name: ID, val args: List<TypeArg>? = null) : Node {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)

}