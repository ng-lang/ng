package net.kimleo.ng.parser.node.pattern

import net.kimleo.ng.parser.Visitor
import net.kimleo.ng.parser.node.ID

data class TaggedTuplePattern(val id: ID, val tuple: Pattern) : Pattern {
    override fun <T> accept(visitor: Visitor<T>): T {
        return visitor.visit(this)
    }
}