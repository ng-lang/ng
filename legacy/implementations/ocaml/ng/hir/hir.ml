exception Unexpected_hir_node

type symbol = string [@@deriving show]
type line = HLineNo of int [@@deriving show]
type offset = HOffset of int [@@deriving show]
type primitives = HBoolType | HStrType | HIntType [@@deriving show]
type type_kind = Alias | Data | Record | Enum [@@deriving show]

type indexing_kind =
  | IndexAt of int
  | Start
  | End
  | IndexBetween of indexing_kind * indexing_kind
[@@deriving show]

let index_starts_from x = IndexBetween (IndexAt x, End)
let index_ends_to x = IndexBetween (Start, IndexAt x)

type typ =
  | Primitive of primitives
  | Product of typ list
  | Parameter of symbol
  | Customized of symbol
  | Parametric of symbol * typ list
  | FuncType of typ list
  | UnknownType
[@@deriving show]

type obj =
  | HInt of int
  | HStr of string
  | HBool of bool
  | HUnitValue
  | HArray of typ * obj list
  | HTuple of (typ * obj) list
  | HRef of symbol
  | HTyped of typ * obj
  | HClosure of invokable * obj list
  | HTypeInfo of type_kind * (symbol * typ) list
  | HAssign of symbol * obj
  | HApply of obj * obj list
  | HBinOp of symbol * obj * obj
  | HAccess of obj * symbol
  | HGetIndex of obj * indexing_kind
  | HCondExpr of obj * obj * obj option
  | HLoopCollect of obj * obj
  | HNewScope of cmd list * obj
[@@deriving show]

and cmd =
  | HBindVal of symbol * typ option * obj option
  | HSimple of obj
  | HBindFn of invokable
  | HGetParam of symbol
  | HIfElse of obj * cmd * cmd
  | HBlock of cmd list
  | HLoop of cmd * cmd list
  | HBreak
  | HContinue
  | HLabel of symbol * line
  | HReturn of obj
  | HStart
  | HGoto of symbol
[@@deriving show]

and invokable = { name : symbol; arity : int; body : cmd list }
[@@deriving show]

type hir_module = cmd list [@@deriving show]

module HMod = struct
  let append_cmd cmd hmod = hmod @ [ cmd ]

  let rec append_cmds cmds hmod =
    match cmds with
    | [] -> hmod
    | cmd :: rest -> append_cmds rest (append_cmd cmd hmod)

  let empty : hir_module = []
end

module HFn = struct end

module Show = struct
  let show_module = show_hir_module
end
