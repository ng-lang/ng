package net.kimleo.ng.parser.node.typecons

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.decl.type.TypeArg

data class ProductCons(override val id: ID, val typeArgs: List<TypeArg>) : TypeCons {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}