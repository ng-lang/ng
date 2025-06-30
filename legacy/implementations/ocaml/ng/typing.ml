exception TypingError of string

type exp =
  | Var of string
  | Lam of string * exp
  | App of exp * exp
  | Let of string * exp * exp

type typ = TVar of tv ref | QVar of string | TArrow of typ * typ
and tv = Unbound of string | Link of typ

let rec occurs tvr = function
  | TVar tvr' when tvr = tvr' -> failwith "occurs check"
  | TVar { contents = Link ty } -> occurs tvr ty
  | TArrow (t1, t2) ->
      occurs tvr t1;
      occurs tvr t2
  | _ -> ()

let rec unify : typ -> typ -> unit =
 fun t1 t2 ->
  if t1 = t2 then ()
  else
    match (t1, t2) with
    | TVar { contents = Link t1 }, t2 | t1, TVar { contents = Link t2 } ->
        unify t1 t2
    | TVar ({ contents = Unbound _ } as tv), t'
    | t', TVar ({ contents = Unbound _ } as tv) ->
        occurs tv t';
        tv := Link t'
    | TArrow (tyl1, tyl2), TArrow (tyr1, tyr2) ->
        unify tyl1 tyr1;
        unify tyl2 tyr2
    | _, _ -> raise (TypingError "failed to unify types")

type env = (string * typ) list

let gensym_counter = ref 0
let reset_gensym : unit -> unit = fun () -> gensym_counter := 0

let gensym : unit -> string =
 fun () ->
  let n = !gensym_counter in
  let () = incr gensym_counter in
  if n < 26 then String.make 1 (Char.chr (Char.code 'a' + n))
  else "t" ^ string_of_int n

let newvar : unit -> typ = fun () -> TVar (ref (Unbound (gensym ())))

let inst : typ -> typ =
  let rec loop subst = function
    | QVar name -> (
        try (List.assoc name subst, subst)
        with Not_found ->
          let tv = newvar () in
          (tv, (name, tv) :: subst))
    | TVar { contents = Link ty } -> loop subst ty
    | TArrow (ty1, ty2) ->
        let ty1, subst = loop subst ty1 in
        let ty2, subst = loop subst ty2 in
        (TArrow (ty1, ty2), subst)
    | ty -> (ty, subst)
  in
  fun ty -> fst (loop [] ty)

let rec gen : typ -> typ = function
  | TVar { contents = Unbound name } -> QVar name
  | TVar { contents = Link ty } -> gen ty
  | TArrow (t1, t2) -> TArrow (gen t1, gen t2)
  | ty -> ty

let rec typeof : env -> exp -> typ =
 fun env -> function
  | Var x -> inst (List.assoc x env)
  | Lam (x, e) ->
      let ty_x = newvar () in
      let ty_e = typeof ((x, ty_x) :: env) e in
      TArrow (ty_x, ty_e)
  | App (e1, e2) ->
      let ty_fun = typeof env e1 in
      let ty_arg = typeof env e2 in
      let ty_res = newvar () in
      unify ty_fun (TArrow (ty_arg, ty_res));
      ty_res
  | Let (x, e1, e2) ->
      let ty_e = typeof env e1 in
      typeof ((x, gen ty_e) :: env) e2

let%test _ =
  let id = Lam ("x", Var "x") in
  let () = reset_gensym () in
  TArrow (TVar (ref (Unbound "a")), TVar (ref (Unbound "a"))) = typeof [] id
