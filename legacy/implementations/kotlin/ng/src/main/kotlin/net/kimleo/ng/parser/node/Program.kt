package net.kimleo.ng.parser.node

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.decl.Decl
import java.util.*

data class Program(val decls: ArrayList<Decl>) : Node {
    override fun <T> accept(visitor: Visitor<T>): T {
        return visitor.visit(this)
    }

}