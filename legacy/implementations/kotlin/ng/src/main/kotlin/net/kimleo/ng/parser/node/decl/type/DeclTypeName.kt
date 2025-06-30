package net.kimleo.ng.parser.node.decl.type

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.decl.type.param.TypeParam

data class DeclTypeName(val id: ID, val params: List<TypeParam>? = null) : Node {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}