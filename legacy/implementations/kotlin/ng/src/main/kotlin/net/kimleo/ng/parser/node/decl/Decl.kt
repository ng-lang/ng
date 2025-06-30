package net.kimleo.ng.parser.node.decl

import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node

interface Decl: Node {
    val id: ID
}