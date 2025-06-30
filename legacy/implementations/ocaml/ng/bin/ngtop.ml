open Ng_lib.Stupid

let append_semicolumn text =
  let plain = String.trim text in
  if String.ends_with ~suffix:";" plain then plain else plain ^ ";"

let () =
  let env = empty () in
  while true do
    try
      let line = String.trim (read_line ()) in
      if String.length line != 0 then
        let result = eval ~env (append_semicolumn line) in
        print_endline (show_exp result)
      else ()
    with End_of_file -> exit 0
  done
