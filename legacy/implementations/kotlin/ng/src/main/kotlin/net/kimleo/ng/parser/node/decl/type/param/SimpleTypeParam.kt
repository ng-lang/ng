package net.kimleo.ng.parser.node.decl.type.param

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID

data class SimpleTypeParam(override val id: ID) : TypeParam {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}