package net.kimleo.ng.parser


enum class TokenType(val repr:String) {
    TYPE("type"),
    VAL("val"),
    CONS("cons"),

    NEW_TYPE("type!"),

    CASE("case"),
    IF("if"),
    ELSE("else"),
    THEN("then"),
    DO("do"),

    SIG("sig"),
    FUN("fun"),

    TRUE("true"),
    FALSE("false"),

    ID("ID"),
    NUM("NUM"),

    OPERATOR("OPERATOR"),

    LEFT_PAREN("("),
    LEFT_SQUARE("["),
    LEFT_CURLY("{"),

    RIGHT_PAREN(")"),
    RIGHT_SQUARE("]"),
    RIGHT_CURLY("}"),

    LESS("<"),
    GREATER(">"),

    ARROW("ARROW"),

    SEPARATOR("::"),
    COLUMN(":"),
    DOT("."),
    COMMA(","),

    HYPHEN("-"),

    EQUAL("="),

    PIPE("|"),

    STRING("STRING"),

    CHAR("CHAR"),

    ELLIPSIS("..."),
}


