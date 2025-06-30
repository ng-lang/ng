package net.kimleo.ng.parser

data class Token(val sym: TokenType, val column: Int, val line: Int, val value: Any? = null)

