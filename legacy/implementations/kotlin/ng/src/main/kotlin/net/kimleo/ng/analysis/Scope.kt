package net.kimleo.ng.analysis

import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.decl.Func
import net.kimleo.ng.parser.node.decl.Signature
import net.kimleo.ng.util.bind

data class Scope(val parent: Scope?, val symbols: MutableMap<String, Node> = hashMapOf()) {

    fun lookup(symbol: String): Node? {
        if (symbol in symbols) {
            return symbols[symbol]
        } else {
            return parent.bind { it.lookup(symbol) }
        }
    }

    fun add(symbol: String, node: Node) {
        if (symbol in symbols) {
            throw SymbolExistedInCurrentScopeException()
        }
        symbols.put(symbol, node)
    }

    fun addFunc(func: Func, signature: Signature) {
        assert(func.id == signature.id)

        func.signature = signature
        symbols.put(func.id.name, func)
    }
}

