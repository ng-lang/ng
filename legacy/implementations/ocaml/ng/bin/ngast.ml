open Ng_lib
open Common_ast

let input_file = ref ""
let usage_msg = "ngast <file> ..."
let anon_fun filename = input_file := filename
let speclist = []

let () =
  Arg.parse speclist anon_fun usage_msg;
  try
    In_channel.with_open_text !input_file (fun ch ->
        let lexbuf = Lexing.from_channel ch in
        let _ = Parsing.set_trace true in
        let ast = Parser.program Lexer.token lexbuf in
        print_endline (show_program ast))
  with Parsing.Parse_error -> print_string "error"
