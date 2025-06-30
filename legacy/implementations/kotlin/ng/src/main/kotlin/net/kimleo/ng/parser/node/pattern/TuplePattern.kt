package net.kimleo.ng.parser.node.pattern

import net.kimleo.ng.parser.Visitor

data class TuplePattern(val patterns: List<Pattern>) : Pattern {
    override fun <T> accept(visitor: Visitor<T>): T {
        return visitor.visit(this)
    }
}


