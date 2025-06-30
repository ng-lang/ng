package net.kimleo.ng.parser.node.decl.type.param

import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node

interface TypeParam: Node {
    val id: ID
}