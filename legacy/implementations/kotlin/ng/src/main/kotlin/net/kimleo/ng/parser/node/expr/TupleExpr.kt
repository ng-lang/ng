package net.kimleo.ng.parser.node.expr

import net.kimleo.ng.parser.Visitor
import java.util.*

data class TupleExpr(val items: ArrayList<Expr>) : Expr {
    override fun <T> accept(visitor: Visitor<T>): T = visitor.visit(this)
}