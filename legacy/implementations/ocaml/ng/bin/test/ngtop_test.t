Should execute stupid program
  $ ngtop  < ../../example/stupid.ng
  Stupid.Fn {name = "fact"; anon = false; params = ["x"; "i"];
    body =
    [(Stupid.Condition (
        (Stupid.Op ("==", (Stupid.VarRef "x"), (Stupid.Int 0))),
        (Stupid.VarRef "i"),
        (Some (Stupid.Invoke ((Stupid.VarRef "fact"),
                 [(Stupid.Op ("-", (Stupid.VarRef "x"), (Stupid.Int 1)));
                   (Stupid.Op ("*", (Stupid.VarRef "x"), (Stupid.VarRef "i")))]
                 )))
        ))
      ]}
  (Stupid.Int 120)
  Stupid.Fn {name = "plus1"; anon = false; params = ["x"];
    body = [(Stupid.Op ("+", (Stupid.VarRef "x"), (Stupid.Int 1)))]}
  Stupid.Fn {name = "times2"; anon = false; params = ["x"];
    body = [(Stupid.Op ("*", (Stupid.VarRef "x"), (Stupid.Int 2)))]}
  Stupid.Fn {name = "compose"; anon = false; params = ["f"; "g"];
    body = [(Stupid.Anon ("anon_fun_lit_1", ["root.anon_fun_lit_1"]))]}
  (Stupid.Int 7)
  (Stupid.Tup [])
  (Stupid.Arr [(Stupid.Int 2); (Stupid.Int 3)])
  (Stupid.Arr [])
  (Stupid.Int 1)

# TODO FIX THIS
#Should execute hir program
#  $ ngtop < ../../example/hir.ng
#  (Stupid.Slot ("m", (Stupid.Val (Stupid.Int 1))))
#  (Stupid.Slot ("print", (Stupid.Fn (["x"], (Stupid.Expr <fun>)))))
#  (Stupid.Slot ("invoke", (Stupid.Fn ([], (Stupid.Expr <fun>)))))
#  (Stupid.Val (Stupid.Int 1))
#  (Stupid.Slot ("d", (Stupid.Val (Stupid.Int 4))))
#  (Stupid.Slot ("d", (Stupid.Val (Stupid.Int 4))))
#  (Stupid.Slot ("d", (Stupid.Val (Stupid.Int 4))))
#  (Stupid.Slot ("y", (Stupid.Val (Stupid.Int 2))))
#  (Stupid.Slot ("hello", (Stupid.Fn (["x"], (Stupid.Expr <fun>)))))
#  (Stupid.Slot ("fn",
#     (Stupid.Fn (["a"; "b"; "c"; "d"; "e"; "f"; "g"; "h"; "i"],
#        (Stupid.Expr <fun>)))
#     ))
