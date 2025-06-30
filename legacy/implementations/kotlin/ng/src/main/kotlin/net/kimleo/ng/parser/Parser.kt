package net.kimleo.ng.parser

import net.kimleo.ng.parser.node.FunType
import net.kimleo.ng.parser.node.ID
import net.kimleo.ng.parser.node.Node
import net.kimleo.ng.parser.node.Program
import net.kimleo.ng.parser.node.decl.Decl
import net.kimleo.ng.parser.node.decl.Func
import net.kimleo.ng.parser.node.decl.Signature
import net.kimleo.ng.parser.node.decl.ValDecl
import net.kimleo.ng.parser.node.decl.type.DeclTypeName
import net.kimleo.ng.parser.node.decl.type.TypeAlias
import net.kimleo.ng.parser.node.decl.type.TypeArg
import net.kimleo.ng.parser.node.decl.type.TypeDecl
import net.kimleo.ng.parser.node.decl.type.param.MappedTypeParam
import net.kimleo.ng.parser.node.decl.type.param.SimpleTypeParam
import net.kimleo.ng.parser.node.decl.type.param.TypeParam
import net.kimleo.ng.parser.node.expr.*
import net.kimleo.ng.parser.node.literal.*
import net.kimleo.ng.parser.node.pattern.Pattern
import net.kimleo.ng.parser.node.pattern.TaggedTuplePattern
import net.kimleo.ng.parser.node.pattern.TuplePattern
import net.kimleo.ng.parser.node.typecons.EnumCons
import net.kimleo.ng.parser.node.typecons.ProductCons
import net.kimleo.ng.parser.node.typecons.Struct
import net.kimleo.ng.parser.node.typecons.TypeCons
import net.kimleo.ng.util.Transactional

class Parser(val tokens: List<Token>) : Transactional<Int> {
    override var state = 0
    override var savedState: Int? = null
    val token: Token
        get() = tokens[state]

    val nextToken: Token?
        get() {
            state++
            if (!EOF()) return token
            return null
        }

    private fun EOF(): Boolean = state >= tokens.size

    fun rewind(step: Int = 1) {
        state -= step
    }

    fun accept(type: TokenType, value: Any? = null): Boolean {
        if (!EOF() && token.sym == type && value != null && token.value == value) {
            nextToken
            return true
        }
        if (!EOF() && this.token.sym == type) {
            nextToken
            return true
        }
        return false
    }

    /**
     * type := 'type' typeName '=' (typeArg | typeBody)
     */
    fun type(): Decl {
        if (accept(TokenType.TYPE)) {
            val name = typeName()
            expect(TokenType.EQUAL)
            if (accept(TokenType.ID)) {
                rewind(1)
                val alias = typeArg()
                return TypeAlias(name, alias)
            }
            val value = typeBody()
            return TypeDecl(name, value)
        }
        throw unexpected(token, TokenType.TYPE)
    }

    /**
     * typeName := ID ('<' typeParams '>')?
     */
    fun typeName(): DeclTypeName {
        val name = identifier()
        if (accept(TokenType.LESS)) {
            val params = typeParameters()
            accept(TokenType.GREATER)
            return DeclTypeName(name, params)
        } else {
            return DeclTypeName(name)
        }
    }

    /**
     * typeBody := typeCons | ('\' typeCons)+
     */
    fun typeBody(): List<Struct> {
        val cons = arrayListOf<Struct>()
        while (!EOF()) {
            if (accept(TokenType.PIPE))
                cons.add(typeCons())
            else break
        }
        if (cons.isEmpty() && !EOF()) {
            cons.add(typeCons())
        }
        return cons
    }

    /**
     * typeCons := 'cons' ID (typeArgs)? | typeRecords
     */
    private fun typeCons(): TypeCons {
        if (accept(TokenType.CONS)) {
            val name = identifier()
            if (accept(TokenType.LEFT_PAREN)) {
                val typeArgs = typeArgs()
                accept(TokenType.RIGHT_PAREN)
                return ProductCons(name, typeArgs)
            }
            return EnumCons(name)
        } else if (accept(TokenType.LEFT_CURLY)) {
        }
        throw unexpected(token, TokenType.CONS, TokenType.LEFT_CURLY)
    }

    /**
     * typeArgs := typeArg+
     */
    private fun typeArgs(): List<TypeArg> {
        val args = arrayListOf<TypeArg>()

        while (!EOF()) {
            if (accept(TokenType.ID)) {
                rewind(1)
                args.add(typeArg())
                if (accept(TokenType.COMMA)) {
                    continue
                } else break
            }
        }
        return args
    }

    /**
     * typeArg := ID ('<' typeArgs '>)?
     */
    private fun typeArg(): TypeArg {
        val name = identifier()
        if (accept(TokenType.LESS)) {
            val args = typeArgs()
            accept(TokenType.GREATER)
            return TypeArg(name, args)
        }
        return TypeArg(name)
    }

    /**
     * typeParameter := typeParam+
     */
    private fun typeParameters(): List<TypeParam> {
        val params = arrayListOf<TypeParam>()
        try {
            while (!EOF()) {
                params.add(typeParam())
                if (accept(TokenType.COMMA))
                    continue
                else break
            }
        } catch (ex: ParserError) {
        }
        return params
    }

    /**
     * typeParam := ID ':' ID | ID
     */
    private fun typeParam(): TypeParam {
        val name = identifier()
        if (accept(TokenType.COLUMN)) {
            val type = expression()
            return MappedTypeParam(name, type)
        } else {
            return SimpleTypeParam(name)
        }
    }

    /**
     * sig := 'sig' ID '::' [typeParameters '=>'] FuncType
     */
    fun sig(): Signature {
        expect(TokenType.SIG)
        val name = identifier()
        accept(TokenType.SEPARATOR)
        val parameters = arrayListOf<TypeParam>()
        transactional {
            parameters.addAll(typeParameters())
            expect(TokenType.ARROW, "=>")
        }
        val type = funType()
        return Signature(name, type, parameters)
    }

    /**
     * funType := typeName ['->' funType]
     */
    private fun funType(): Node {
        val left = typeArg()
        if (accept(TokenType.ARROW, "->")) {
            val right = funType()
            return FunType(left, right)
        }
        return left
    }

    /**
     * valDecl := 'val' ID '=' expression
     */
    private fun valDecl(): ValDecl {
        expect(TokenType.VAL)
        val name = identifier()
        expect(TokenType.EQUAL)
        val value = expression()

        return ValDecl(name, value)
    }

    /**
     * funDecl := 'fun' ID ID* '=' expression
     */
    fun funDecl(): Func {
        if (accept(TokenType.FUN)) {
            val name = identifier()
            val args = patterns()
            val expr = expression()
            return Func(name, args, expr)
        }
        throw unexpected(token, TokenType.FUN)
    }


    fun patterns(): List<Pattern> {

        val args = arrayListOf<Pattern>()

        while(!accept(TokenType.EQUAL)) {
            args.add(pattern())
        }

        return args
    }

    private fun pattern(): Pattern {
        if (accept(TokenType.ID)) {
            rewind(1)
            val id =  identifier()
            if (accept(TokenType.LEFT_PAREN)) {
                rewind(1)
                val tuple = pattern()
                return TaggedTuplePattern(id, tuple)
            } else {
                return id
            }

        } else if (accept(TokenType.LEFT_PAREN)) {
            val patterns = arrayListOf<Pattern>()
            while (!accept(TokenType.RIGHT_PAREN)) {
                patterns.add(pattern())
                if (accept(TokenType.RIGHT_PAREN)) {
                    break
                }
                accept(TokenType.COMMA)
            }
            if (patterns.size == 1) {
                return patterns.first()
            }
            return TuplePattern(patterns)
        }
        throw unexpected(token)
    }

    /**
     * expression := expression0+ (operator expression0+)*
     */
    private fun expression(): Expr {
        var expr = expression0()
        try {
            while (!EOF() && !isKeyword(token) && !isEndOfExpr(token)) {
                try {
                    expr = ApplyExpression(expr, expression0())
                } catch (ex: ParserError) {
                    if (isOperator()) {
                        rewind(1)
                        expr = operator(expr)
                    } else throw ex
                }
            }
            return expr
        } catch(ex: ParserError) {
            return expr
        }
    }

    /**
     * expression0 := '(' expression ')' | number | string | char | array | ifExpr
     */
    private fun expression0(): Expr {
        if (accept(TokenType.LEFT_PAREN)) {
            val expr = expression()
            if (accept(TokenType.COMMA)) {
                rewind(1)
                val tuple = arrayListOf(expr)
                while (!accept(TokenType.RIGHT_PAREN)) {
                    expect(TokenType.COMMA)
                    tuple.add(expression())
                }
                return TupleExpr(tuple)
            }
            expect(TokenType.RIGHT_PAREN)
            return expr
        } else if (accept(TokenType.ID)) {
            rewind(1)
            return identifier()
        } else if (accept(TokenType.NUM)) {
            rewind(1)
            return number()
        } else if (accept(TokenType.STRING)) {
            rewind(1)
            return string()
        } else if (accept(TokenType.CHAR)) {
            rewind(1)
            return char()
        } else if (accept(TokenType.LEFT_SQUARE)) {
            rewind(1)
            return array()
        } else if (accept(TokenType.IF)) {
            rewind(1)
            return ifExpr()
        } else if (accept(TokenType.TRUE)) {
            return bool(true)
        } else if (accept(TokenType.FALSE)) {
            return bool(false)
        }
        throw unexpected(token, TokenType.ID, TokenType.NUM)
    }

    private fun bool(b: Boolean): BoolLiteral {
        return BoolLiteral(b)
    }

    private fun operator(left: Expr): Expr {
        val sym = token
        nextToken
        var expr = expression0()
        if (sym.sym == TokenType.DOT) {
            return ApplyExpression(expr, left)
        }

        while (!EOF() && !isKeyword(token) && !isEndOfExpr(token)) {
            try {
                expr = ApplyExpression(expr, expression0())
            } catch(ex: ParserError) {
            }
            if (isOperator()) {
                rewind(1)
                break
            }
        }
        return Operator(sym.value!! as String, left, expr)
    }

    /**
     * ifExpr := 'if' expression 'then' expression 'else' expression
     */
    private fun ifExpr(): IfExpr {
        expect(TokenType.IF)
        val test = expression()
        expect(TokenType.THEN)
        val consequence = expression()
        expect(TokenType.ELSE)
        val alternative = expression()
        return IfExpr(test, consequence, alternative)
    }

    /**
     * program := (type | sig | funDecl | valDecl) +
     */
    fun program(): Node {
        val nodes = arrayListOf<Decl>()
        while (!EOF()) {
            if (accept(TokenType.TYPE)) {
                rewind(1)
                nodes.add(type())
            } else if (accept(TokenType.SIG)) {
                rewind(1)
                nodes.add(sig())
            } else if (accept(TokenType.FUN)) {
                rewind(1)
                nodes.add(funDecl())
            } else if (accept(TokenType.VAL)) {
                rewind(1)
                nodes.add(valDecl())
            } else
                throw unexpected(token,
                        TokenType.TYPE,
                        TokenType.SIG,
                        TokenType.FUN,
                        TokenType.VAL)
        }
        return Program(nodes)
    }

    fun identifier(): ID {
        val sym = token
        if (accept(TokenType.ID)) {
            return ID(sym.value.toString())
        }
        throw unexpected(sym, TokenType.ID)
    }

    /**
     * array := '[' expression? (',' expression)+ ']'
     */
    private fun array(): ArrayLiteral {
        expect(TokenType.LEFT_SQUARE)
        val array = arrayListOf<Expr>()
        while (!accept(TokenType.RIGHT_SQUARE)) {
            array.add(expression())
            if (accept(TokenType.RIGHT_SQUARE)) {
                break
            }
            expect(TokenType.COMMA)
        }
        return ArrayLiteral(array)
    }

    private fun char(): CharLiteral {
        val sym = token
        nextToken
        return CharLiteral(sym.value!! as Char)
    }

    private fun string(): StringLiteral {
        val sym = token
        nextToken
        return StringLiteral(sym.value!! as String)
    }

    private fun number(): NumLiteral {
        val sym = token
        nextToken
        return NumLiteral(sym.value!! as String)
    }

    /**
     * EndOfExpr := ')' | ']' | ','
     */
    private fun isEndOfExpr(token: Token): Boolean {
        return token.sym in listOf(
                TokenType.RIGHT_PAREN,
                TokenType.RIGHT_SQUARE,
                TokenType.RIGHT_CURLY,
                TokenType.COMMA)
    }


    /**
     * Operator := OPERATOR | '-' | '<' | '>' | '=' | '.'
     */
    private fun isOperator() =
            accept(TokenType.OPERATOR) ||
                    accept(TokenType.HYPHEN) ||
                    accept(TokenType.GREATER) ||
                    accept(TokenType.LESS) ||
                    accept(TokenType.EQUAL) ||
                    accept(TokenType.DOT)

    private fun isNewRoot(token: Token): Boolean {
        return token.sym in listOf(
                TokenType.VAL,
                TokenType.TYPE,
                TokenType.FUN,
                TokenType.SIG,
                TokenType.NEW_TYPE)
    }

    private fun isKeyword(token: Token): Boolean {
        return isNewRoot(token) ||
                token.sym in listOf(
                        TokenType.IF,
                        TokenType.THEN,
                        TokenType.ELSE,
                        TokenType.CASE,
                        TokenType.DO)
    }

    private fun expect(type: TokenType, value: Any? = null) {
        if (!accept(type, value)) {
            throw unexpected(token, type)
        }
    }

    private fun unexpected(actual: Token, vararg expected: TokenType) =
            ParserError("Unexpected token ${actual.sym}[${actual.line}:${actual.column}], " +
                    "expect ${expected.joinToString { it.toString() }}")

}

