package net.kimleo.ng.parser

import net.kimleo.ng.util.bind
import net.kimleo.ng.util.orElse

class Lexer(val source: String) {
    var column = 1
    var line = 1
    var index = 0

    private val KEYWORDS = arrayOf("if", "while", "type", "cons", "type!", "then", "else", "val", "case", "do", "sig", "fun", "true", "false")

    private val OPERATOR_PREFIX = "+~$%^&*/@#"
    private val OPERATOR_CONTINUE = OPERATOR_PREFIX + "|=:!>"
    private val ESCAPE_MAPPING = mapOf(
            'b' to '\b', 'n' to '\n', 't' to '\t', 'r' to '\r', '\\' to '\\', '\'' to '\'', '"' to '"')
    private val HEX_DIGITS = "0123456789abcdef"


    private val BRACKETS = "()[]{}"

    private val SPECIALS = "<>:=-,.|"

    private val BLANKS = " \t\r\n"

    val current: Char
        get() = source[index]

    private val SYM_OF_CHAR: Map<Char, TokenType> =
            mapOf('(' to TokenType.LEFT_PAREN,
                    ')' to TokenType.RIGHT_PAREN,
                    '[' to TokenType.LEFT_SQUARE,
                    ']' to TokenType.RIGHT_SQUARE,
                    '{' to TokenType.LEFT_CURLY,
                    '}' to TokenType.RIGHT_CURLY,
                    '<' to TokenType.LESS,
                    '>' to TokenType.GREATER,
                    '-' to TokenType.HYPHEN,
                    '=' to TokenType.EQUAL)

    fun nextToken(): Token? {
        when (current) {
            in BLANKS -> {
                next(1)
                return nextToken()
            }
            in ('a'..'z').union('A'..'Z') -> {
                val id = tryNextId()
                if (id in KEYWORDS) {
                    return Token(TokenType.values().first { it.repr == id }, column - id.length, line)
                } else {
                    return Token(TokenType.ID, column - id.length, line, id)
                }
            }
            in ('0'..'9') -> {
                val num = tryNextNumber()
                return Token(TokenType.NUM, column - num.length, line, num)
            }
            in OPERATOR_PREFIX -> {
                val operator = tryNextOperator()
                return Token(TokenType.OPERATOR, column - operator.length, line, operator)
            }
            in BRACKETS -> {
                val sym = current
                next(1)
                return Token(SYM_OF_CHAR[sym]!!, column - 1, line)
            }
            in "<>" -> {
                val sym = current
                return tryCompareOperator().bind { Token(TokenType.OPERATOR, column - it.length, line, it) }
                        .orElse { Token(SYM_OF_CHAR[sym]!!, column - 1, line, sym.toString()) }
            }
            in "=-" -> {
                val sym = current
                return tryArrow().bind { Token(TokenType.ARROW, column - it.length, line, it) }
                        .orElse { Token(SYM_OF_CHAR[sym]!!, column - 1, line, sym.toString()) }

            }

            ':' -> {
                return trySeparator().bind { Token(TokenType.SEPARATOR, column - it.length, line) }
                        .orElse { Token(TokenType.COLUMN, column - 1, line) }
            }
            '.' -> {
                next(1)
                return Token(TokenType.DOT, column - 1, line)
            }
            ',' -> {
                next(1)
                return Token(TokenType.COMMA, column - 1, line)
            }
            '|' -> {
                val operator = tryNextOperator()
                if (operator == "|") {
                    return Token(TokenType.PIPE, column - 1, line)
                } else {
                    return Token(TokenType.OPERATOR, column - operator.length, line, operator)
                }
            }
            '"' -> {
                val start = column
                val string = tryString()
                return Token(TokenType.STRING, start, line, string)
            }
            '\'' -> {
                val start = column
                val char = tryChar()
                return Token(TokenType.CHAR, start, line, char)
            }
            else -> {
                throw LexerError("Unrecognized symbol $current")
            }

        }
    }

    private fun tryChar(): Char {
        if (current == '\'') {
            next(1)
            var c = current
            if (c == '\'') {
                throw LexerError("Unexpected character literal $current at [$line:$column]")
            }
            if (current == '\\') {
                next(1)
                c = make_escape(current)
            }
            next(1)
            if (current != '\'')
                throw LexerError("Unexpected character literal $current at [$line:$column]")
            next(1)
            return c
        } else throw LexerError("Unexpected character literal")
    }

    private fun tryString(): String {
        val sb = StringBuffer()
        if (current == '"') {
            next(1)
            while (current != '"' && !EOF()) {
                if (current != '\\') {
                    sb.append(current)
                    next(1)
                } else {
                    next(1)
                    val escape = current
                    sb.append(make_escape(escape))
                    next(1)
                }
            }
            next(1)
            return sb.toString()
        }
        throw LexerError("Unknown string literal")
    }

    private fun make_escape(escape: Char): Char {
        when (escape) {
            in "bntr\\\'\"" -> {
                return ESCAPE_MAPPING[escape]!!
            }
            'u' -> {
                next(1)
                var hexVal: Int? = null
                while (!EOF() && (current.isDigit() || current in "abcdefABCDEF")) {
                    if (hexVal != null)
                        hexVal = hexVal * 16 + HEX_DIGITS.indexOf(current.toLowerCase())
                    else hexVal = HEX_DIGITS.indexOf(current.toLowerCase())
                    next(1)
                }
                reset(1)
                if (hexVal != null) {
                    return hexVal.toChar()
                } else throw LexerError("Unexpected unicode literal $current at [$line:$column]")
            }
            else -> {
                throw LexerError("Unknown escape sequence")
            }
        }
    }

    fun run(): List<Token> {
        val tokens = arrayListOf<Token>()
        while (!EOF()) {
            tokens.add(nextToken()!!)
        }
        return tokens
    }

    private fun tryWith(c: Char): String? {
        val sym = current
        next(1)
        if (!EOF() && current == c) {
            next(1)
            return "$sym$c"
        }
        return null
    }

    private fun trySeparator(): String? = tryWith(':')

    private fun tryArrow(): String? = tryWith('>')

    private fun tryCompareOperator(): String? = tryWith('=')

    private fun tryNextOperator(): String {
        val sb = StringBuilder()
        while (!EOF() && current in OPERATOR_CONTINUE) {
            sb.append(current)
            next(1)
        }
        return sb.toString()
    }

    private fun tryNextNumber(): String {
        val sb = StringBuffer()
        while (!EOF() && isContinuous()) {
            if (!Character.isDigit(current)) {
                reset(sb.length)
                throw LexerError("[$line:$column] $current after $sb is not a number")
            }
            sb.append(current)
            next(1)
        }
        return sb.toString()
    }

    private fun reset(length: Int) {
        index -= length
        column -= length
    }

    private fun tryNextId(): String {
        val sb = StringBuffer()
        while (!EOF() && isContinuous()) {
            sb.append(current)
            next(1)
        }
        return sb.toString()
    }

    private fun isContinuous() = current !in BLANKS + OPERATOR_PREFIX + BRACKETS + SPECIALS

    private fun EOF(): Boolean {
        return index >= source.length
    }

    private fun next(n: Int = 1) {
        if (current == '\n') {
            column = 0
            line++
        }
        for (i in 1..n) {
            index++
            column++
            if (EOF()) break
        }
    }

}
