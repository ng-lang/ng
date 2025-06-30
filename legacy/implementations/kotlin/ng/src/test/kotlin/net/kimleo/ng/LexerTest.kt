package net.kimleo.ng

import net.kimleo.ng.parser.Lexer
import net.kimleo.ng.parser.TokenType
import net.kimleo.ng.util.bind
import net.kimleo.ng.util.orElse
import org.junit.Test
import kotlin.test.assertEquals

class LexerTest: FileBasedIntegrationTest(){
    @Test
    fun should_lex_keywords() {
        val lexer = Lexer("type val cons type! case if else then do")

        assertEquals(lexer.nextToken()?.sym, TokenType.TYPE)
        assertEquals(lexer.nextToken()?.sym, TokenType.VAL)
        assertEquals(lexer.nextToken()?.sym, TokenType.CONS)
        assertEquals(lexer.nextToken()?.sym, TokenType.NEW_TYPE)
        assertEquals(lexer.nextToken()?.sym, TokenType.CASE)
        assertEquals(lexer.nextToken()?.sym, TokenType.IF)
        assertEquals(lexer.nextToken()?.sym, TokenType.ELSE)
        assertEquals(lexer.nextToken()?.sym, TokenType.THEN)
        assertEquals(lexer.nextToken()?.sym, TokenType.DO)
    }

    @Test
    fun should_lex_column() {
        val lexer = Lexer("if\n   val\n\ncons type!\n")

        assertEquals(lexer.nextToken()?.column, 1)
        assertEquals(lexer.nextToken()?.column, 4)
        assertEquals(lexer.nextToken()?.column, 1)
        assertEquals(lexer.nextToken()?.column, 6)
    }

    @Test
    fun should_lex_line() {
        val lexer = Lexer("if\n   val\n\ncons type!\n")

        assertEquals(lexer.nextToken()?.line, 1)
        assertEquals(lexer.nextToken()?.line, 2)
        assertEquals(lexer.nextToken()?.line, 4)
        assertEquals(lexer.nextToken()?.line, 4)
    }

    @Test
    fun should_lex_id() {
        val lexer = Lexer("hello world")

        assertEquals(lexer.nextToken()?.sym, TokenType.ID)
        assertEquals(lexer.nextToken()?.value, "world")
    }

    @Test
    fun should_lex_number() {
        val lexer = Lexer("123 456")

        assertEquals(lexer.nextToken()?.sym, TokenType.NUM)
        assertEquals(lexer.nextToken()?.value, "456")
    }

    @Test
    fun should_lex_operator() {
        val lexer = Lexer("~>")
        val token = lexer.nextToken()!!

        assertEquals(token.sym, TokenType.OPERATOR)
        assertEquals(token.value, "~>")
    }

    @Test
    fun should_lex_mixed() {
        val lexer = Lexer("({+=1}->--<><>)")

        val lst = lexer.run()

        println(lst)
    }

    @Test
    fun should_lex_pipe_symbol() {
        val lexer = Lexer("| |> || |&")

        val lst = lexer.run()

        lst.forEach {
            if (it.sym != TokenType.PIPE) {
                assertEquals(it.value.toString().commonPrefixWith("|"), "|")
            }
        }
    }

    @Test
    fun lex_char_literal() {
        val lexer = Lexer("'\\\\' '\\\'' 'a' 'b'")

        val lst = lexer.run()

        println(lst)
    }

    @Test
    fun lex_string_literal() {
        val lexer = Lexer("\"\\uabcGREATHEHE\\u0\\\'\\\"\"")

        val lst = lexer.run()

        println(lst)
    }

    @Test
    fun testIntegrate() {
        open("lexer_test.fed").use {
            val lexer = Lexer(it.lineSequence().joinToString("\n"))

            val lst = lexer.run()
            for(sym in lst) {
                println("[${sym.line}:${sym.column}]${sym.sym}${sym.value.bind { " -- $it " }.orElse { "" }}")
            }
        }
    }
}



