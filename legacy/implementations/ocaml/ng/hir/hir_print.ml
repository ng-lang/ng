open Occam_ext.Buf.Text
open Hir

exception Unexpected_indent
exception Unimplemented_print of string

let rec pad = function
  | 0 -> ""
  | n when n > 0 -> " " ^ pad (n - 1)
  | _ -> raise Unexpected_indent

let rec print_hir_module hmod buffer =
  match hmod with
  | [] -> buffer
  | cmd :: rest ->
      print_hir_module rest (buffer |> append (print_cmd cmd ~indent:0))

and print_cmd ?(indent = 0) cmd =
  match cmd with
  | HSimple obj -> Printf.sprintf "%s%s;\n" (pad indent) (print_obj ~indent obj)
  | HBindVal (sym, _, Some obj) ->
      Printf.sprintf "%sval %s = %s;\n" (pad indent) sym (print_obj ~indent obj)
  | HBindFn fn -> Printf.sprintf "%s\n" (print_invokable fn indent)
  | HReturn obj ->
      Printf.sprintf "%sret %s;\n" (pad indent) (print_obj ~indent obj)
  | _ -> ""

and print_obj ?(indent = 0) obj =
  match obj with
  | HInt i -> Int.to_string i
  | HStr str -> Printf.sprintf "`%s`" str
  | HBool b -> Bool.to_string b
  | HRef symbol -> symbol
  | HArray (_, objs) -> Printf.sprintf "[%s]" (print_objs indent objs ",")
  | HTuple pairs ->
      Printf.sprintf "(%s)" (print_objs indent (List.map snd pairs) ",")
  | HApply (fn, params) ->
      Printf.sprintf "%s(%s)" (print_obj ~indent fn)
        (print_objs indent params ",")
  | HGetIndex (obj, index) ->
      Printf.sprintf "%s[%s]" (print_obj ~indent obj) (print_index index)
  | HBinOp (sym, left, right) ->
      Printf.sprintf "(%s %s %s)" (print_obj ~indent left) sym
        (print_obj ~indent right)
  | HNewScope (cmds, obj) -> print_newscope indent cmds obj
  | HUnitValue -> "unit"
  | _ -> raise (Unimplemented_print (show_obj obj))

and print_objs indent objs seperator =
  String.concat seperator (List.map (print_obj ~indent) objs)

and print_index = function
  | IndexAt index -> Int.to_string index
  | Start -> "0"
  | End -> "#"
  | IndexBetween (left, right) ->
      Printf.sprintf "%s, %s" (print_index left) (print_index right)

and print_invokable { name; arity; body } indent =
  let cmds =
    String.concat "" (List.map (print_cmd ~indent:(indent + 2)) body)
  in
  Printf.sprintf "%s.define %s/%i:\n%s%s.end_define %s" (pad indent) name arity
    cmds (pad indent) name

and print_newscope indent cmds obj =
  let cmds =
    String.concat "" (List.map (print_cmd ~indent:(indent + 2)) cmds)
  in
  Printf.sprintf "%s.newscope\n%s\n%s%s%s\n.end_newscope" (pad indent) cmds
    (pad (indent + 2))
    (print_obj obj) (pad indent)

let print hmod =
  let buf = Buffer.create 256 in
  String.of_bytes (Buffer.to_bytes (print_hir_module hmod buf))
