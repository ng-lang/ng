package net.kimleo.ng.parser.node.decl.type

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.decl.Decl

data class TypeAlias(val name: DeclTypeName, val alias: TypeArg) : Decl {
    override val id: ID
        get() = name.id
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}