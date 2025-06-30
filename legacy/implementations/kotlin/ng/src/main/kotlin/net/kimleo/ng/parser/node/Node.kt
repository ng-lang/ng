package net.kimleo.ng.parser.node

import net.kimleo.ng.parser.Visitor

interface Node {
    fun <T> accept(visitor: Visitor<T>): T
}