module type Print = sig
  val print_list : 'a list -> ('a -> string) -> unit
  val print_string : string -> unit
  val print_endline : string -> unit
end

module DoPrint : Print = struct
  let print_list l fn = List.iter (fun it -> print_endline (fn it)) l
  let print_string = Stdlib.print_string
  let print_endline = Stdlib.print_endline
end

module DoNothing : Print = struct
  let print_list _ _ = ()
  let print_string _ = ()
  let print_endline _ = ()
end

module DebugEnabler (P : Print) : Print = P
