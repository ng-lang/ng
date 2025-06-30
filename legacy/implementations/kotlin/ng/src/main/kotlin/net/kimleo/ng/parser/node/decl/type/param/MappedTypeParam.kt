package net.kimleo.ng.parser.node.decl.type.param

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.expr.Expr

data class MappedTypeParam(override val id: ID, val type: Expr) : TypeParam {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}