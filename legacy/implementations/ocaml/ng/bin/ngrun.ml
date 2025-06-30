open Ng_lib.Stupid

let input_file = ref ""
let usage_msg = "ngast <file> ..."
let anon_fun filename = input_file := filename
let speclist = []

let () =
  Arg.parse speclist anon_fun usage_msg;
  let env = empty () in
  try
    In_channel.with_open_text !input_file (fun ch ->
        let result =
          eval ~env (String.concat "\n" (In_channel.input_lines ch))
        in
        print_endline (show_exp result))
  with Parsing.Parse_error | End_of_file -> print_string "error"
