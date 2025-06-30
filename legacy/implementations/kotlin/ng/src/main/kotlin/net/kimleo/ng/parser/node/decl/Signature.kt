package net.kimleo.ng.parser.node.decl

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.decl.type.param.TypeParam

data class Signature(override val id: ID, val type: Node, val parameters: List<TypeParam>) : Decl {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)

}