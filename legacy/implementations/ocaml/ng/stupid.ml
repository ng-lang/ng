open Common_ast

(* simplified test-only ugly prototype-intepreter demo (STUPID)*)

exception EvalError of string
exception NameNotFound of string
exception CompileError of string

open Debug.DebugEnabler (Debug.DoNothing)
open Occam_ext.Lists

module Environment = struct
  let anon_counter = ref 0

  let anon ?(t = "obj") () =
    anon_counter := !anon_counter + 1;
    let sym = "anon_" ^ t ^ "_" ^ string_of_int !anon_counter in
    sym

  type 'a t = {
    content : (string, 'a el) Hashtbl.t;
    uplevel : 'a t list;
    id : string;
  }

  and 'a el = Val of 'a | Env of 'a t

  let create ?(uplevel = []) id : 'a t =
    { content = Hashtbl.create 100; uplevel; id }

  let append it key el =
    let () = Hashtbl.add it.content key el in
    ()

  let fork uplevel id : 'a t =
    let env = create ~uplevel:[ uplevel ] id in
    let () = append uplevel id (Env env) in
    env

  let put it el =
    let () = Hashtbl.add it.content (anon ()) el in
    ()

  let route it =
    let rec iroute m path =
      match m.uplevel with
      | p :: _ -> iroute p (m.id :: path)
      | [] -> m.id :: path
    in
    iroute it []

  let route_name it = String.concat "." (route it)

  exception Invalid_Path of string list

  let rec locate it path =
    match path with
    | [] -> None
    | key :: rest -> (
        match (Hashtbl.find_opt it.content key, rest) with
        | Some (Val el), [] -> Some el
        | Some (Env child), _ -> locate child rest
        | _, _ -> None)

  let rec traverse ?(remains = []) it fn =
    match fn it with
    | Some result -> Some result
    | None -> (
        let remains = remains @ it.uplevel in
        match remains with h :: t -> traverse h fn ~remains:t | [] -> None)

  let lookup it symbol =
    traverse it (fun env -> Hashtbl.find_opt env.content symbol)

  let rec subenv it path =
    match path with
    | [] -> it
    | key :: rest -> (
        if key = it.id then subenv it rest
        else
          match (lookup it key, rest) with
          | Some (Env child), [] -> child
          | Some (Env child), rest -> subenv child rest
          | _, _ -> raise (Invalid_Path path))

  let subenv_by_name it qualified_name =
    try
      print_endline ("find_subenv: >>>" ^ qualified_name);
      subenv it (String.split_on_char '.' qualified_name)
    with Invalid_Path _ -> raise (Invalid_Path [ qualified_name ])

  let%test _ =
    let env : int t = create "root" in
    let sub = fork env "sub" in
    subenv env [ "root" ] == env
    && subenv env [ "sub" ] == sub
    && subenv env [ "root"; "sub" ] == sub
    && subenv sub [ "sub" ] == sub
    && subenv env [] == env
    && subenv sub [] == sub

  let locate_name it qualified_name =
    locate it (String.split_on_char '.' qualified_name)

  let attach m1 m2 =
    let uplevel = merge_dedup m1.uplevel (append_dedup m2.uplevel m1) in
    { m2 with uplevel }
end

type exp =
  | Str of string
  | Int of int
  | Bool of bool
  | Arr of exp list
  | Tup of exp list
  | Obj of string option * (string * exp) list
  | UnitObj
  | Invoke of exp * exp list
  | VarRef of string
  | Op of string * exp * exp
  | Condition of exp * exp * exp option
  | Anon of string * string list
  | Fn of { name : string; anon : bool; params : string list; body : exp list }
  | Bind of string * exp
[@@deriving show]

let empty () : exp Environment.t = Environment.create "root"
let sym_counter = ref 0
let indent n = String.make n ' '

let rec print_env ?(lvl = 0) (env : exp Environment.t) =
  print_endline
    (indent lvl ^ "environment name: " ^ env.id ^ " + parents: "
    ^ String.concat "," (List.map Environment.route_name env.uplevel));
  Hashtbl.iter
    (fun key content ->
      print_endline (indent (lvl + 1) ^ key);
      match content with
      | Environment.Val it -> print_endline (indent (lvl + 1) ^ show_exp it)
      | Environment.Env e -> print_env ~lvl:(lvl + 1) e)
    env.content

let add_obj = function Int x, Int y -> Int (x + y) | _, _ -> raise (EvalError "eval failed")

let subtract_obj = function
  | Int x, Int y -> Int (x - y)
  | _, _ -> raise (EvalError "eval failed")

let multiply_obj = function
  | Int x, Int y -> Int (x * y)
  | _, _ -> raise (EvalError "eval failed")

let eql_obj = function x, y -> Bool (x = y)

let rec param_symbols l =
  match l with
  | [] -> []
  | h :: t -> (
      match h with
      | DirectBinding x -> x :: param_symbols t
      | UnitBinding -> param_symbols t
      | DestructTupleBinding (_, l) -> param_symbols l @ param_symbols t
      | ListBinding (l, _) -> param_symbols l @ param_symbols t
      | _ -> raise (EvalError "eval failed"))

let lookup env sym =
  let found = Environment.lookup env sym in
  match found with Some (Val v) -> v | _ -> raise (NameNotFound sym)

let rec eval_program_element el env =
  let () = print_endline ("Executing >>>" ^ show_program_element el) in
  let () = print_env env in
  match el with
  | Definition defn -> compile_definition defn env
  | Statement (SimpleStatement (ValueBindingExpression bindings)) ->
      compile_bindings ~force:true bindings env
  | Statement stmt -> compile_statement stmt env

and compile_definition defn env =
  match defn with
  | FunDef (FunctionDefinition (id, params, body)) ->
      compile_fundef id params body env
  | _ -> raise (CompileError "Unsupported definition type.")

and compile_bindings ?(force = false) ?(res = UnitObj) bindings env =
  match bindings with
  | [] -> res
  | h :: t ->
      let res = compile_binding ~force h env in
      compile_bindings ~force ~res t env

and compile_fundef ?(anon = false) name param_bindings fundef_body env =
  let body =
    match fundef_body with
    | FunExprBody expr_body -> [ compile_expr expr_body env ]
    | FunStmtsBody exprs -> compile_fn_stmt_body exprs env
  in
  let params = param_symbols param_bindings in
  let fn = Fn { name; anon; params; body } in
  let () = Environment.append env name (Val fn) in
  fn

and compile_binding ?(force = false) binding env =
  match binding with
  | DirectBinding i, expr ->
      let result = compile_expr expr env in
      let result = if force then eval_exp result env else Bind (i, result) in
      let () = Environment.append env i (Val result) in
      result
      (** todo: bind destructive in function body *)
  | DestructTupleBinding (_, pat), expr ->
      compile_destructive pat (eval_exp (compile_expr expr env) env) env
  | ListBinding (pat, spread), expr ->
      compile_list_destructive pat spread
        (eval_exp (compile_expr expr env) env)
        env
  | WildcardBinding, expr ->
      let res = eval_exp (compile_expr expr env) env in
      res
  | _ -> raise (CompileError "unsupported binding type")

and compile_destructive patterns res env =
  match patterns with
  | [] -> res
  | pattern :: prest -> (
      match res with
      | Tup (ob :: rest) ->
          let _ =
            match pattern with
            | DirectBinding i ->
                let () = Environment.append env i (Val ob) in
                ob
            | _ -> raise (CompileError "unknown value to destruct")
          in
          compile_destructive prest (Tup rest) env
      | _ -> raise (EvalError "eval failed"))

and compile_list_destructive patterns spread res env =
  match patterns with
  | [] -> res
  | pattern :: prest -> (
      if List.is_empty prest && spread then
        match pattern with
        | DirectBinding i ->
            let () = Environment.append env i (Val res) in
            res
        | _ -> raise (CompileError "unknown value to list/destruct")
      else
        match res with
        | Arr (ob :: rest) ->
            let _ =
              match pattern with
              | DirectBinding i ->
                  let () = Environment.append env i (Val ob) in
                  ob
              | _ -> raise (CompileError "unknown value to list/destruct")
            in
            compile_list_destructive prest spread (Arr rest) env
        | _ -> raise (EvalError "eval failed"))

and compile_statement stmt env =
  match stmt with
  | SimpleStatement expr -> compile_expr expr env
  | _ -> raise (CompileError "Unsupported statement type.")

and compile_fn_stmt_body stmts env =
  let rec compile_fn_stmt stmt env =
    match stmt with
    | SimpleStatement (ValueBindingExpression bindings) ->
      compile_fn_body_bindings bindings env
    | SimpleStatement expr -> [compile_expr expr env]
    | _ -> raise (CompileError "unsupported statement type")
  and compile_fn_stmts env stmts exprs =
    match stmts with
    | [] -> List.rev exprs
    | h :: t -> compile_fn_stmts env t (compile_fn_stmt h env @ exprs)
  in
  compile_fn_stmts env stmts []

and compile_fn_body_bindings bindings env =
  match bindings with
  | [] -> []
  | (DirectBinding id, expr) :: t -> (Bind (id, compile_expr expr env)) :: compile_fn_body_bindings t env
  | _ -> raise (CompileError "unsupported binding expression in function body")

and compile_exprs exprs env =
  match exprs with
  | [] -> []
  | SimpleStatement exp :: rest ->
      compile_expr exp env :: compile_exprs rest env
  | _ -> raise (CompileError "unsupporeted multiple statement body")

and compile_expr expr env =
  match expr with
  | IntegerValue i -> Int i
  | BooleanValue b -> Bool b
  | UnitLiteral -> UnitObj
  | ArrayLiteral a -> Arr (List.map (fun e -> compile_expr e env) a)
  | TupleLiteral t -> Tup (List.map (fun e -> compile_expr e env) t)
  | BinaryOperation (op, exl, exr) ->
      Op (op, compile_expr exl env, compile_expr exr env)
  | IdentityExpr sym -> VarRef sym
  | ApplicationExpr (expr, exprs) ->
      Invoke
        (compile_expr expr env, List.map (fun e -> compile_expr e env) exprs)
  | IfElseExpr (cond, cons, Some alter) ->
      Condition
        ( compile_expr cond env,
          compile_expr cons env,
          Some (compile_expr alter env) )
  | IfElseExpr (cond, cons, None) ->
      Condition (compile_expr cond env, compile_expr cons env, None)
  | FunctionLiteral (params, body) ->
      let anon_name = Environment.anon ~t:"fun_lit" () in
      let defenv = Environment.fork env anon_name in
      let namespace = Environment.route_name defenv in
      let anon_fn = compile_fundef ~anon:true anon_name params body defenv in
      let () = Environment.append defenv anon_name (Val anon_fn) in
      Anon (anon_name, [ namespace ])
  | ValueBindingExpression bindings -> compile_bindings bindings env
  (* | LetBindingExpr (bindings, body) ->
      compile_bin *)
  | _ -> raise (CompileError "unsuppored expression")

and eval_fn fn args env =
  let () = print_endline ("eval fn >>" ^ show_exp fn) in
  let () = print_env env in
  match fn with
  | VarRef name ->
      let fn = lookup env name in
      eval_fn fn args env
  | Fn { name; params; body = closure; anon = _ } ->
      let exec_env = Environment.fork env (name ^ "exec_env") in
      let () = bind_params params args exec_env in
      eval_exps closure exec_env
  | Anon (name, defenv) ->
      let defenvs =
        Environment.create
          ~uplevel:(List.map (Environment.subenv_by_name env) defenv)
          (Environment.anon ~t:"afn_env" ())
      in
      let exec_env = Environment.attach env defenvs in
      eval_fn (eval_exp (VarRef name) exec_env) args exec_env
  | _ -> raise (EvalError "eval failed")

and bind_params params values env =
  match (params, values) with
  | h1 :: t1, v :: t2 ->
      let () = Environment.append env h1 (Val v) in
      bind_params t1 t2 env
  | [], [] -> ()
  | [], [ UnitObj ] -> ()
  | [], _ -> raise (EvalError "eval failed")
  | _, _ -> raise (EvalError "eval failed")

and eval_exps ?(result = UnitObj) exps env =
  match exps with
  | [] -> result
  | [ exp ] -> eval_exp exp env
  | exp :: rest ->
      let result = eval_exp exp env in
      eval_exps ~result rest env

and eval_exp exp env =
  match exp with
  | Str _ | Int _ | UnitObj | Bool _ | Fn _  -> exp
  | Arr xs -> Arr (List.map (fun x -> eval_exp x env) xs)
  | Tup xs -> Tup (List.map (fun x -> eval_exp x env) xs)
  | Obj (t, kvs) -> Obj (t, List.map (fun (k, v) -> (k, eval_exp v env)) kvs)
  | VarRef var -> eval_exp (lookup env var) env
  | Op (op, l, r) -> eval_op op l r env
  | Condition (cond, cons, Some alter) ->
      if eval_exp cond env = Bool true then eval_exp cons env
      else eval_exp alter env
  | Condition (cond, cons, None) ->
      if eval_exp cond env = Bool true then eval_exp cons env else UnitObj
  | Invoke (f, args) ->
      let fn = eval_exp f env in

      eval_fn fn (List.map (fun a -> eval_exp a env) args) env
  | Anon (id, defenv) ->
      Anon (id, append_dedup defenv (Environment.route_name env))
  | Bind (id, exp) ->
    let result = eval_exp exp env in
    let () = Environment.append env id (Val (result))
    in result

and eval_op op expl expr env =
  match op with
  | "+" -> add_obj (eval_exp expl env, eval_exp expr env)
  | "-" -> subtract_obj (eval_exp expl env, eval_exp expr env)
  | "*" -> multiply_obj (eval_exp expl env, eval_exp expr env)
  | "==" -> eql_obj (eval_exp expl env, eval_exp expr env)
  | _ -> raise (EvalError "eval failed")

and eval_program_elements ?(res = UnitObj) pl env =
  match pl with
  | [] -> res
  | h :: t ->
      let res = eval_program_element h env in
      eval_program_elements ~res t env

and eval_program (Program pl) env = eval_program_elements pl env

and eval ?(env = empty ()) source =
  let prog = Parser.program Lexer.token (Lexing.from_string source) in
  eval_program prog env

let%test _ =
  let env = empty () in
  let _ =
    eval ~env
      {|
fun fact x i =
  if x == 0 then i
  else fact (x - 1) (i * x);
val result = fact 5 1;
|}
  in
  let result = lookup env "result" in
  result = Int 120

let%test _ =
  let env = empty () in
  let _ = eval ~env {|
val x = 1 + 1;
|} in
  lookup env "x" = Int 2

let%test _ =
  let env = empty () in
  let _ = eval ~env {|
val x = 1 == 1;
|} in
  lookup env "x" = Bool true

let%test _ =
  let env = empty () in
  let _ =
    eval ~env
      {|
val (x, y) = (1, 2);
val [a, b...] = [1, 2, 3];
val [c, d] = b;
|}
  in
  lookup env "d" = Int 3 && lookup env "x" = Int 1

let%test _ =
  let env = empty () in
  let _ = eval ~env {|
val plus1 = fun x -> x + 1;
val result = plus1 1;
|} in
  lookup env "result" = Int 2

let%test _ =
  let env = empty () in
  let _ =
    eval ~env
      {|
fun plus1 x = x + 1;
fun times2 x = x * 2;
fun compose f g = fun x -> f (g x);
val v = (compose plus1 times2) 3;
|}
  in
  lookup env "v" = Int 7

let%test _ =
  let env = empty () in
  let _ =
    eval ~env
      {|
fun sqrsum x y {
  val xs = x * x;
  val ys = y * y;
  xs + ys
}

val result = sqrsum 3 4;
|}
  in
  lookup env "result" = Int 25


let%test _ =
let env = empty () in
let _ =
  eval ~env
  {|
fun fact x total {
    val next_total = x * total;
    if x == 0 then total
    else fact (x - 1) next_total
}

val result = fact 5 1;
  |} in
  lookup env "result" = Int 120

let%test _ =
  let env = empty () in
  let _ =
  eval ~env
  {|
fun fact x total {
    val next_total = x * total;
    if (x == 0) {
      total
    } else {
      fact (x - 1) next_total
    }
}

val result = fact 5 1;
  |} in
  lookup env "result" = Int 120
