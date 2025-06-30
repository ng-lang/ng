{
   open Parser
}

let digit = ['0'-'9']

let digits = digit+

let id = ['a'-'z' 'A'-'Z' '_'] ['a'-'z' 'A'-'Z' '0'-'9' '_']*

let whitespace = [' ' '\t']+

let newline = '\r' | '\n' | "\r\n"

let operatorlexmes = ['>' '<' '=' '-' '+' '*' '/' '%' '$' '^' '@']+


rule token = parse
    | whitespace { token lexbuf }
    | digits as lxm { INTEGER(int_of_string lxm) }
    | "true" { KEYWORD_TRUE }
    | "false" { KEYWORD_FALSE }
    | "unit" { KEYWORD_UNIT }

    | "type" { KEYWORD_TYPE }
    | "fun" { KEYWORD_FUN }
    | "val" { KEYWORD_VAL }
    | "sig" { KEYWORD_SIG }
    | "data" { KEYWORD_DATA }
    | "concept" { KEYWORD_CONCEPT }
    | "impl" { KEYWORD_IMPL }
    | "for" { KEYWORD_FOR }
    | "property" { KEYWORD_PROPERTY }

    | "module" { KEYWORD_MODULE }
    | "export" { KEYWORD_EXPORT }
    | "exports" { KEYWORD_EXPORTS }
    | "import" { KEYWORD_IMPORT }
    | "use" { KEYWORD_USE }

    | "if" {  KEYWORD_IF }
    | "then" { KEYWORD_THEN }
    | "else" { KEYWORD_ELSE }
    | "loop" { KEYWORD_LOOP }
    | "collect" { KEYWORD_COLLECT }
    | "let" { KEYWORD_LET }
    | "and" { KEYWORD_AND }
    | "in" { KEYWORD_IN }
    | "switch" { KEYWORD_SWITCH }
    | "match" { KEYWORD_MATCH }
    | "case" { KEYWORD_CASE }
    | "otherwise" { KEYWORD_OTHERWISE }
    | "return" { KEYWORD_RETURN }
    | "break" { KEYWORD_BREAK }
    | "continue" { KEYWORD_CONTINUE }

    | '('  { LPAREN }
    | ')'  { RPAREN }
    | '['  { LSQUARE }
    | ']'  { RSQUARE }
    | '{'  { LCURLY }
    | '}'  { RCURLY }

    | "=>" { DUAL_ARROW }
    | "->" { SINGLE_ARROW }
    | "::" { SEPARATOR }
    | ':' { COLON }
    | ';' { SEMICOLON }
    | ',' { COMMA }
    | "..." { SPREAD }
    | "_" { WILDCARD }
    | '.' { DOT }
    | '|' { BAR }

    | '+' { OPPLUS }
    | '-' { OPMINUS }
    | '*' { OPTIMES }
    | '/' { OPDIVIDE }
    | "mod" { OPMODULUS }

    | "==" { OPEQUAL }
    | '=' { OPASSIGN }
    | "!=" { OPNOTEQUAL }
    | ">=" { OPGE }
    | '>' { OPGT }
    | "<=" { OPLE }
    | '<' { OPLT }

    | '!' { OPNOT }
    | "&&" { OPAND }
    | "||" { OPOR }

    | "<<" { OPLSHIFT }
    | ">>" { OPRSHIFT }

    | id as lxm {ID(lxm)}
    | operatorlexmes as lxm { OPERATOR (lxm) }
    | _ { token lexbuf }
    | eof { EOF }

{

open Common_ast

let expect_lex text tok = token (Lexing.from_string text) = tok

let%test _ = expect_lex "1" (INTEGER 1)

let%test _ = expect_lex "false" (KEYWORD_FALSE)
  && expect_lex "true" (KEYWORD_TRUE)
  && expect_lex "unit" (KEYWORD_UNIT)

let%test _ = expect_lex "if" (KEYWORD_IF)
  && expect_lex "then" (KEYWORD_THEN)
  && expect_lex "else" (KEYWORD_ELSE)
  && expect_lex "loop" ( KEYWORD_LOOP )
  && expect_lex "collect" ( KEYWORD_COLLECT )
  && expect_lex "switch" ( KEYWORD_SWITCH )
  && expect_lex "match" ( KEYWORD_MATCH )
  && expect_lex "case" ( KEYWORD_CASE )
  && expect_lex "otherwise" ( KEYWORD_OTHERWISE )
  && expect_lex "return" ( KEYWORD_RETURN )
  && expect_lex "break" ( KEYWORD_BREAK )
  && expect_lex "continue" ( KEYWORD_CONTINUE )

let%test _ = expect_lex "type" ( KEYWORD_TYPE )
    && expect_lex "fun" ( KEYWORD_FUN )
    && expect_lex "val" ( KEYWORD_VAL )
    && expect_lex "sig" ( KEYWORD_SIG )
    && expect_lex "data" ( KEYWORD_DATA )
    && expect_lex "property" ( KEYWORD_PROPERTY )

let%test _ = expect_lex "module" ( KEYWORD_MODULE )
    && expect_lex "export" ( KEYWORD_EXPORT )
    && expect_lex "exports" ( KEYWORD_EXPORTS )
    && expect_lex "import" ( KEYWORD_IMPORT )
    && expect_lex "use" ( KEYWORD_USE )

let%test _ = expect_lex "i" (ID "i")
    && expect_lex "id" (ID "id")
    && expect_lex "identity" (ID "identity")
    && expect_lex "identification" (ID "identification")
    && expect_lex "i0" (ID "i0")
    && expect_lex "id1" (ID "id1")
    && expect_lex "CaseSensitiveID" (ID "CaseSensitiveID")

let%test _ = expect_lex "+" (OPPLUS)
let%test _ = expect_lex "=" (OPASSIGN)
let%test _ = expect_lex "=>" (DUAL_ARROW)
let%test _ = expect_lex "+>" (OPERATOR ("+>"))
let%test _ = expect_lex ">" (OPGT)
let%test _ = expect_lex ">>" (OPRSHIFT)
let%test _ = expect_lex ">>>" (OPERATOR ">>>")
let%test _ = expect_lex ">=" (OPGE)

let enable_trace = false

let try_parse text =
    let _ = Parsing.set_trace true in
    let _ = program token (Lexing.from_string text)
    in true

let trace_parse text =
    let _ = Parsing.set_trace true in
    let res = program token (Lexing.from_string text) in
    if enable_trace then
        print_string (as_string res);
    true

let expect_parse text ast =
    let _ = Parsing.set_trace true in
    let res = program token (Lexing.from_string text) in
    if enable_trace then print_string (as_string res);
    res = ast

let expect_parse_expr text ast =
    let _ = Parsing.set_trace true in
    let res = expr_checker token (Lexing.from_string text) in
    if enable_trace then print_string (expr_as_string res);
    res = ast

let trace_file filename =
    let file = In_channel.open_text filename in
    let text = In_channel.input_all file in
    trace_parse text


let parse_file filename =
    let file = In_channel.open_text filename in
    let text = In_channel.input_all file in
    try_parse text


let%test _ = expect_parse_expr "1 + 2" (BinaryOperation
    ("+", (IntegerValue 1), (IntegerValue 2)))

let%test _ = expect_parse_expr "1 + 2 * 3 + 4" (BinaryOperation
    ("+",
       (BinaryOperation ("+", (IntegerValue 1),
          (BinaryOperation ("*", (IntegerValue 2),
             (IntegerValue 3)))
          )),
       (IntegerValue 4)))


let%test _ = expect_parse_expr "print(1, 2, 3)" (ApplicationExpr ((IdentityExpr "print"),
   [(TupleLiteral
       [(IntegerValue 1); (IntegerValue 2); (IntegerValue 3)])
     ]
   ))

let%test _ = expect_parse_expr "a.b().c.d(e,f)"  (ApplicationExpr (
   (AccessorExpr (
      (AccessorExpr (
         (ApplicationExpr (
            (AccessorExpr ((IdentityExpr "a"),
               ("b"))),
            [UnitLiteral])),
         ("c"))),
      ("d"))),
   [(TupleLiteral
       [(IdentityExpr "e");
         (IdentityExpr "f")])
     ]
   ))

let%test _ = expect_parse_expr "if a then b else c" (IfElseExpr ((IdentityExpr "a"),
       (IdentityExpr "b"),
       Some (IdentityExpr "c")))

let%test _ = parse_file "example/expressions.ng"

(* let%test _ = trace_file "example/definitions.ng" *)

let%test _ = try_parse {|
    val x = 1;
    val [x, y...] = [1, 2, 3];
|}

let%test _ = try_parse {|
1 + 1;

a + b;

let x = 1 in x + 1;

let x = 1 and y = 2 in
if x > y then x else y;

let x = [1, 2, 3] in
x.length;

let t = (123, true) in
snd t;

let (f, g) = destruct() in
compose f g;

flip (flip flip) flip;

fun compose f g = fun x -> f (g x);
|}

(* let%test _ = trace_parse "compose f g;" *)

}
