package net.kimleo.ng.analysis

import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.util.bind

class SymbolDict(val root: Scope, val children: Map<Node, Scope> = mapOf()) {


    fun lookup(path: String): Node? {
        val segments = path.split("::")

        return lookup(segments)
    }

    fun lookup(segments: List<String>): Node? {
        assert(segments.size > 0)

        var node: Node? = null
        var scope: Scope? = root
        segments.forEach {
            if (scope != null) {
                node = scope?.lookup(it)
            } else {
                throw UnexpectedSymbolException(segments.joinToString("::"))
            }
            scope = node.bind { children[it] }
        }
        return node
    }
}

