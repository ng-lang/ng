package net.kimleo.ng.analysis

import net.kimleo.ng.FileBasedIntegrationTest
import net.kimleo.ng.parser.Lexer
import net.kimleo.ng.parser.Parser
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.decl.type.param.SimpleTypeParam
import org.junit.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ScopeTest: FileBasedIntegrationTest() {

    @Test
    fun should_find_scope_symbol() {
        val scope = Scope(null)
        val hello = ID("hello")

        scope.add("hello", hello)

        assertTrue(scope.lookup("hello")!! == hello)
        assertNull(scope.lookup("world"))
    }

    @Test
    fun should_find_parent_symbol() {
        val parent = Scope(null)
        val scope = Scope(parent)
        val hello = ID("hello")

        parent.add("hello", hello)

        assertTrue(scope.lookup("hello")!! == hello)
        assertNull(scope.lookup("world"))
    }

    @Test
    fun should_find_override_symbol() {
        val parent = Scope(null)
        val scope = Scope(parent)
        val hello = ID("hello")
        val world = ID("world")

        parent.add("hello", hello)
        scope.add("hello", world)

        assertTrue(scope.lookup("hello")!! == world)
    }

    @Test
    fun testIntegrate() {
        open("parser_test.fed").use {
            val source = it.lineSequence().joinToString("\n")
            val lexer = Lexer(source)
            val tokens = lexer.run()
            tokens.forEach { println("${it.sym} -- ${it.value}") }
            val parser = Parser(tokens)

            val program = parser.program()

            val visitor = ScopeBuildVisitor()
            program.accept(visitor)

            val dict = visitor.dict()

            assertTrue(dict.lookup("fact::x") is ID)
            assertTrue(dict.lookup("get::t") is SimpleTypeParam)

            assertFailsWith(UnexpectedSymbolException::class) {
                dict.lookup("hello::world")
            }
        }
    }
}