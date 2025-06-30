package net.kimleo.ng.parser.node.typecons

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID

data class EnumCons(override val id: ID) : TypeCons {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}