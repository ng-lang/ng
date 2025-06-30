package net.kimleo.ng

import net.kimleo.ng.parser.Lexer
import net.kimleo.ng.parser.Parser
import net.kimleo.ng.parser.node.decl.type.TypeDecl
import org.junit.Test
import kotlin.test.assertTrue

class ParserTest : FileBasedIntegrationTest() {
    @Test
    fun parse_simple_type_alias() {
        val lexer = Lexer("type a = b\ntype list<b> = c\ntype array<a, arity: Int> = c")
        val parser = Parser(lexer.run())

        val type = parser.type()
        val type2 = parser.type()
        val type3 = parser.type()
        arrayOf(type, type2, type3).forEach { println(it) }
    }

    @Test
    fun parse_sum_type() {
        val lexer = Lexer("type a = |cons Shit(a, List<a, b>) |cons Fuck |cons Recursive(List<List<a, List<b>>, c>)")

        val parser = Parser(lexer.run())

        val type = parser.type()

        assertTrue(type is TypeDecl)
    }

    @Test
    fun parse_fun_sig() {
        val lexer = Lexer("sig println :: a => a -> IO<a>\nsig shit :: fuck -> good -> why")
        val parser = Parser(lexer.run())

        val sig = parser.sig()
        val sig2 = parser.sig()

        arrayOf(sig, sig2).forEach { println(it) }
    }

    @Test
    fun parse_fun_def() {
        val lexer = Lexer("fun get a index = a.get index\n fun triple a = a + a + a")

        val parser = Parser(lexer.run())

        val func = parser.funDecl()
        val func2 = parser.funDecl()

        arrayOf(func, func2).forEach { println(it) }
    }

    @Test
    fun parse_value_def() {
        val lexer = Lexer("val a = 1\nval b = [1, 2]\nval c = \"abc\"")

        val parser = Parser(lexer.run())

        val program = parser.program()

        println(program)
    }

    @Test
    fun parse_if_expr() {
        val lexer = Lexer("val a = if (if x then x1 (y1 y2) (z1 z2) c else x2) then y else z")
        val parser = Parser(lexer.run())

        val program = parser.program()

        println(program)

    }

    @Test
    fun test_anything() {
        val lexer = Lexer("fun fact x = if x = 0 then 1 else x *  fact(x - 1)")
        val parser = Parser(lexer.run())

        val program = parser.program()

        println(program)
    }

    @Test
    fun parse_boolean_type() {
        val lexer = Lexer("val x = true")
        val parser = Parser(lexer.run())

        val program = parser.program()

        println(program)
    }

    @Test
    fun parse_tuple() {
        val lexer = Lexer("val a = (1, 2, (4, (5, 6) , (3)))")
        val parser = Parser(lexer.run())

        val program = parser.program()

        println(program)
    }

    @Test
    fun parse_uncurried_fun() {
        val lexer = Lexer("fun add (a, (c, (d, b))) = a + b")

        val program = Parser(lexer.run()).program()
        println(program)
    }

    @Test
    fun parse_pattern_with_ctor() {
        val lexer = Lexer("fun first (List(h, t), Cons(a), Matrix(b)) fn = fn h")

        val program = Parser(lexer.run()).program()

        println(program)
    }

    @Test
    fun testIntegrate() {
        open("parser_test.fed").use {
            val source = it.lineSequence().joinToString("\n")
            val lexer = Lexer(source)
            val tokens = lexer.run()
//            tokens.forEach { println("${it.sym} -- ${it.value}") }

            val program = Parser(tokens).program()

            println(program)
        }
    }

}
