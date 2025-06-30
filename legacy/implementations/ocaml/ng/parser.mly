
%{
open Common_ast
%}

%token EOF
%token <int> INTEGER
%token <string> ID
%token <string> STRING

%token KEYWORD_TYPE
%token KEYWORD_FUN
%token KEYWORD_VAL
%token KEYWORD_SIG
%token KEYWORD_DATA
%token KEYWORD_PROPERTY
%token KEYWORD_NEW
%token KEYWORD_CONCEPT
%token KEYWORD_IMPL
%token KEYWORD_FOR

%token KEYWORD_MODULE
%token KEYWORD_EXPORT
%token KEYWORD_EXPORTS
%token KEYWORD_IMPORT
%token KEYWORD_USE

%token KEYWORD_IF
%token KEYWORD_THEN
%token KEYWORD_ELSE
%token KEYWORD_LOOP
%token KEYWORD_COLLECT
%token KEYWORD_LET
%token KEYWORD_AND
%token KEYWORD_IN
%token KEYWORD_SWITCH
%token KEYWORD_MATCH
%token KEYWORD_CASE
%token KEYWORD_OTHERWISE
%token KEYWORD_RETURN
%token KEYWORD_BREAK
%token KEYWORD_CONTINUE

%token KEYWORD_TRUE
%token KEYWORD_FALSE
%token KEYWORD_UNIT


%token LPAREN  // (
%token RPAREN  // )
%token LSQUARE // [
%token RSQUARE // ]
%token LCURLY   // {
%token RCURLY  // }

%token DUAL_ARROW   // =>
%token SINGLE_ARROW // ->
%token SEPARATOR // ::
%token COLON    // :
%token SEMICOLON // ;
%token COMMA // ,
%token SPREAD // ...
%token WILDCARD // _
%token DOT  // .
%token BAR // |

%token <string> OPERATOR

%token OPPLUS
%token OPMINUS
%token OPTIMES
%token OPDIVIDE
%token OPMODULUS

%token OPASSIGN // =
%token OPEQUAL  // ==
%token OPNOTEQUAL
%token OPGE
%token OPGT
%token OPLE
%token OPLT

%token OPNOT
%token OPAND
%token OPOR

%token OPLSHIFT
%token OPRSHIFT

%nonassoc KEYWORD_IN
%nonassoc KEYWORD_IF
%nonassoc KEYWORD_THEN
%nonassoc KEYWORD_ELSE
%nonassoc KEYWORD_LET KEYWORD_AND
%nonassoc below_SEMICOLON
%nonassoc SEMICOLON

%nonassoc below_ARROW
%right SINGLE_ARROW

%right OPASSIGN

%nonassoc below_COMMA

%left COMMA
%left COLON
%right OPNOT
%right OPAND OPOR

%nonassoc below_EQUAL

%left OPEQUAL OPGE OPGT OPLE OPLT OPNOTEQUAL

%nonassoc below_SEPERATOR

%left SEPARATOR

%left OPERATOR OPPLUS OPMINUS
%left OPTIMES OPDIVIDE OPMODULUS
%left OPNEG

%nonassoc below_DOT

%nonassoc DOT
%nonassoc INTEGER STRING KEYWORD_TRUE KEYWORD_FALSE KEYWORD_UNIT ID LPAREN LSQUARE LCURLY KEYWORD_NEW WILDCARD SPREAD

%start program
%type <program> program

%start value
%type <expr> value

%start expr_checker
%type <expr> expr_checker

%start type_annotation
%type <type_annotation> type_annotation

%%


reversed_nonempty_llist(X):
    x = X
        { [ x ] }
    | xs = reversed_nonempty_llist(X) x = X
        { x :: xs }
    ;

%inline nonempty_llist(X):
    xs = rev(reversed_nonempty_llist(X))
        { xs }
    ;

program:
    | elems=nonempty_list(program_element) EOF { Program elems }
    ;

program_element:
    | statement SEMICOLON { Statement $1 }
    | definition { Definition $1 }

statement:
    | e=expr { SimpleStatement e }
    | KEYWORD_RETURN e=expr { ReturnStatement e }
      ;

definition:
    | fundef { FunDef $1 }
    | valdef { ValueDef $1 }
    | typedef { TypeDef $1 }
    | signature_def { SignatureDef $1 }
    | concept_def { $1 }
    | impldef { $1 }
    ;

fundef:
    | KEYWORD_FUN id=id params=nonempty_llist(binding_pattern) body=fundef_body { FunctionDefinition(id, params, body) }
    ;


fundef_body:
    | LCURLY stmts=separated_list(SEMICOLON, statement) option(SEMICOLON) RCURLY { FunStmtsBody stmts }
    | OPASSIGN expr=expr SEMICOLON { FunExprBody expr }
    ;

valdef:
    | valdecl { $1 }
    ;

valdecl:
    | KEYWORD_VAL id=id COLON ann=type_annotation SEMICOLON { ValueDeclaration(id, ann) }
    ;

type_annotation:
    | non_func_type_annotation { $1 }
    | function_type_annotation { FuncSignature ([], $1) }
    ;

non_func_type_annotation:
    | id { SimpleTypeAnn $1 }
    | id nonempty_llist(non_func_type_annotation) { ParamerticTypeAnn($1, $2) }
    | LSQUARE type_annotation RSQUARE { ListTypeAnn ($2) }
    | LPAREN separated_nonempty_list(COMMA, type_annotation) RPAREN { TupleTypeAnn $2 }
    | LPAREN type_annotation RPAREN { $2 }
    | LPAREN RPAREN { UnitTypeAnn }
    ;
signature_def:
    | option(KEYWORD_SIG) id=id SEPARATOR
        fnann=function_type_annotation SEMICOLON
        { SignatureDefinition (id, FuncSignature([], fnann )) }
    | option(KEYWORD_SIG) id=id SEPARATOR params=separated_list(COMMA, type_parameters) DUAL_ARROW
        fnann=function_type_annotation SEMICOLON
        { SignatureDefinition (id, FuncSignature(params, fnann )) }
    ;

function_type_annotation:
    | ann=type_annotation anns=nonempty_llist(funnction_type_arr) { ann :: anns }
    ;

funnction_type_arr:
    | preceded(SINGLE_ARROW, type_annotation) { $1 }

typedef:
    | KEYWORD_TYPE id=id params=list(type_parameters) SEMICOLON
        { TypeDefinition (id, params, TypeDeclaration) }
    | KEYWORD_TYPE id=id params=list(type_parameters) OPASSIGN SPREAD SEMICOLON
        { TypeDefinition (id, params, TypeDeclaration) }
    | KEYWORD_TYPE id=id params=list(type_parameters) OPASSIGN ann=type_annotation SEMICOLON
        { TypeDefinition (id, params, TypeAlias ann) }
    | KEYWORD_TYPE id=id params=list(type_parameters) OPASSIGN anns=nonempty_llist(sum_type_declaration) SEMICOLON
        { TypeDefinition (id, params, SumTypeDefinition anns); }
    | KEYWORD_DATA id=id params=list(simple_type_parameter) LPAREN anns=separated_list(COMMA, type_annotation) RPAREN SEMICOLON
        { TypeDefinition (id, params, DataTypeDefinition anns) }
    | KEYWORD_DATA id=id params=list(simple_type_parameter) LCURLY anns=nonempty_list(record_type_field) RCURLY
        { TypeDefinition (id, params, RecordDataTypeDefinition anns) }
    ;

type_parameters:
    | simple_type_parameter { $1 }
    | id nonempty_llist(type_parameters) { ParametricTypeParameter($1, $2) }
    | WILDCARD { PlaceHolderParameter }
    | LPAREN type_parameters RPAREN { $2 }
    ;
simple_type_parameter:
    | id { SimpleTypeParameter $1 }
    ;

record_type_field:
    | id=id COLON ann=type_annotation SEMICOLON { (id, ann) }
    ;

sum_type_declaration:
    | BAR cons=id ann=option(type_annotation) { (cons, ann) }

concept_def:
    | KEYWORD_CONCEPT id=id params=list(type_parameters) LCURLY
        body=list(concept_body_elem)
        RCURLY
        { ConceptDef (id, params, body) }
    ;

concept_body_elem:
    | typedef { ConceptTypeDeclaration $1 }
    | signature_def { ConceptFunDeclaration $1 }
    | valdecl { ConceptValDeclaration $1 }
    | SPREAD { ConceptDeclaration }
    ;

impldef:
    | KEYWORD_IMPL id=id KEYWORD_FOR ann=type_annotation LCURLY body=list(impl_body_elem) RCURLY
        { ImplDef (id, ann, body)}
    ;

impl_body_elem:
    | fundef { ImplFunDef $1 }
    | typedef { ImplTypeDef $1 }
    | valdef { ImplValBinding $1 }
    | SPREAD { ImplDeclaration }
    ;

simple_binding:
    | id = id { DirectBinding id }
    | id = id COLON ann=type_annotation { AnnotatedBinding(id, ann) }
    | WILDCARD { WildcardBinding }
    ;

binding_pattern:
    | simp=simple_binding { simp }
    | LPAREN RPAREN { UnitBinding }
    | LPAREN
        bindings=separated_nonempty_list(COMMA, simple_binding)
        RPAREN { DestructTupleBinding (None, bindings) }
    | typ=id LPAREN
        bindings=separated_list(COMMA, simple_binding)
        RPAREN { DestructTupleBinding (Some typ, bindings) }
    | LSQUARE
        bindings=separated_nonempty_list(COMMA, simple_binding)
        RSQUARE { ListBinding (bindings, false) }
    | LSQUARE
        bindings=separated_nonempty_list(COMMA, simple_binding)
        SPREAD
        RSQUARE { ListBinding (bindings, true) }
    ;

expr:
    | simple_expr %prec below_DOT { $1 }
    | compound_expr { $1 }
    | valbind_expr { $1 }
    ;

valbind_expr:
    | KEYWORD_VAL bindings=separated_list(KEYWORD_AND, binding)
       { ValueBindingExpression bindings }
    ;

expr_checker:
    | expr EOF { $1 }
    ;

compound_expr:
    | if_expr { $1 }
    | c_style_if { $1 }
    | collect_expr { $1 }
    | let_binding_expr { $1 }
    | seq_expr { $1 }
    ;

seq_expr:
    | LCURLY stmts=separated_list(SEMICOLON, statement) option(SEMICOLON) RCURLY { SeqExprBody stmts }

if_expr:
    | KEYWORD_IF condition=expr
        KEYWORD_THEN consequence=expr
        _else=option(else_sec) { IfElseExpr (condition, consequence, _else) }
    ;

c_style_if:
    | KEYWORD_IF LPAREN condition=expr RPAREN LCURLY consequence=expr RCURLY
        _else=option(c_style_else) {IfElseExpr (condition, consequence, _else) }
    ;

c_style_else:
    | KEYWORD_ELSE LCURLY alternative=expr RCURLY { alternative }
    | KEYWORD_ELSE alternative=c_style_if { alternative }
    ;

else_sec:
    | KEYWORD_ELSE alternative=expr { alternative }
    ;

collect_expr:
    | KEYWORD_LOOP body=expr KEYWORD_COLLECT result=expr { LoopCollectExpr(body, result) }
    ;

let_binding_expr:
    | KEYWORD_LET bindings=separated_list(KEYWORD_AND, binding) KEYWORD_IN expr=expr { LetBindingExpr (bindings, expr) }
    ;

binding:
    | pattern=binding_pattern OPASSIGN expr=expr { (pattern, expr) }
    ;

simple_expr:
    | application_expr { $1 }
    | primary_expr { $1 }
    ;

primary_expr:
    | value { $1 }
    | LPAREN exprs=separated_nonempty_list(COMMA, expr) RPAREN { TupleLiteral (exprs) }
    | LPAREN expr RPAREN { $2 }
    | LSQUARE exprs=separated_list(COMMA, expr) RSQUARE { ArrayLiteral (exprs) }
    | binary_expr { $1 }
    | accessor_expr { $1 }
    | function_literal { $1 }
    ;

binary_expr:
    | e1=expr op=binop e2=expr { BinaryOperation(op, e1, e2) }
    ;

application_expr:
    | simple_expr nonempty_llist(primary_expr) { ApplicationExpr ($1, $2) }
    ;

accessor_expr:
    | primary_expr=simple_expr DOT id=id { AccessorExpr(primary_expr, id) }
    | primary_expr=accessor_expr DOT id=id { AccessorExpr(primary_expr, id) }
    ;


function_literal:
    | KEYWORD_FUN params=nonempty_llist(binding_pattern) SINGLE_ARROW body=expr
        { FunctionLiteral (params, FunExprBody body) }
    ;

value:
    | i=INTEGER { IntegerValue i }
    | s=STRING { StringValue s }
    | id=id { IdentityExpr id }
    | KEYWORD_TRUE { BooleanValue true }
    | KEYWORD_FALSE { BooleanValue false }
    | LPAREN RPAREN { UnitLiteral }
    | WILDCARD { WildcardLiteral }
    | SPREAD { SpreadLiteral }
    ;

id:
    | id=ID { id }

    // contextual keywords:
    | KEYWORD_RETURN { "return" }
    ;

%inline binop:
    | op=OPERATOR { op }
    | OPPLUS { "+" }
    | OPMINUS { "-" }
    | OPTIMES { "*" }
    | OPDIVIDE { "/" }
    | OPEQUAL { "==" }
    | OPGE { ">=" }
    | OPGT { ">" }
    | OPLE { "<=" }
    | OPLT { "<" }
    | OPMODULUS { "mod" }
    ;
%%
