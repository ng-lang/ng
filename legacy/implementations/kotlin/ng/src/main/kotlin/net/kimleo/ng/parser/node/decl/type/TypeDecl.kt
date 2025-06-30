package net.kimleo.ng.parser.node.decl.type

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.decl.Decl

data class TypeDecl(val name: DeclTypeName, val value: List<Node>) : Decl {
    override val id: ID
        get() = name.id

    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)

}