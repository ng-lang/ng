Should parse hir program
  $ ngast  ../../example/hir.ng
  (Common_ast.Program
     [(Common_ast.Statement
         (Common_ast.SimpleStatement
            (Common_ast.ValueBindingExpression
               [((Common_ast.DirectBinding "m"), (Common_ast.IntegerValue 1))])));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("print",
                [(Common_ast.DirectBinding "x")],
                (Common_ast.FunExprBody (Common_ast.IdentityExpr "x"))))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("invoke",
                [Common_ast.UnitBinding],
                (Common_ast.FunExprBody
                   (Common_ast.TupleLiteral
                      [(Common_ast.IntegerValue 1);
                        (Common_ast.IntegerValue 2);
                        (Common_ast.IntegerValue 3);
                        (Common_ast.IntegerValue 4)]))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ApplicationExpr ((Common_ast.IdentityExpr "print"),
                [(Common_ast.IdentityExpr "m")]))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "a");
                       (Common_ast.DirectBinding "b");
                       (Common_ast.DirectBinding "c");
                       (Common_ast.DirectBinding "d")]
                     )),
                  (Common_ast.ApplicationExpr (
                     (Common_ast.IdentityExpr "invoke"),
                     [Common_ast.UnitLiteral])))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [(Common_ast.WildcardBinding,
                  (Common_ast.BinaryOperation ("+",
                     (Common_ast.IdentityExpr "a"), (Common_ast.IntegerValue 1)
                     )))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [(Common_ast.UnitBinding,
                  (Common_ast.ApplicationExpr (
                     (Common_ast.IdentityExpr "print"),
                     [(Common_ast.IntegerValue 1)])))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "x");
                       (Common_ast.DirectBinding "y")]
                     )),
                  (Common_ast.TupleLiteral
                     [(Common_ast.BinaryOperation ("+",
                         (Common_ast.IdentityExpr "a"),
                         (Common_ast.IdentityExpr "b")));
                       (Common_ast.IdentityExpr "b")]))
                  ])));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("hello",
                [(Common_ast.DestructTupleBinding (None,
                    [(Common_ast.DirectBinding "x")]))
                  ],
                (Common_ast.FunExprBody
                   (Common_ast.ApplicationExpr (
                      (Common_ast.IdentityExpr "print"),
                      [(Common_ast.IdentityExpr "x")])))
                ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("fn",
                [(Common_ast.DirectBinding "a");
                  (Common_ast.DirectBinding "b");
                  (Common_ast.ListBinding (
                     [(Common_ast.DirectBinding "c");
                       (Common_ast.DirectBinding "d");
                       (Common_ast.DirectBinding "e")],
                     false));
                  (Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "f");
                       (Common_ast.DirectBinding "g")]
                     ));
                  (Common_ast.ListBinding (
                     [(Common_ast.DirectBinding "h");
                       (Common_ast.DirectBinding "i")],
                     true))
                  ],
                (Common_ast.FunExprBody
                   (Common_ast.ArrayLiteral
                      [(Common_ast.IdentityExpr "a");
                        (Common_ast.IdentityExpr "b");
                        (Common_ast.IdentityExpr "c");
                        (Common_ast.IdentityExpr "d");
                        (Common_ast.IdentityExpr "e");
                        (Common_ast.IdentityExpr "f");
                        (Common_ast.IdentityExpr "g");
                        (Common_ast.IdentityExpr "h");
                        (Common_ast.ApplicationExpr (
                           (Common_ast.IdentityExpr "sizeof"),
                           [(Common_ast.IdentityExpr "i")]))
                        ]))
                ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("test_statement_body",
                [(Common_ast.DestructTupleBinding (None,
                    [(Common_ast.DirectBinding "x")]))
                  ],
                (Common_ast.FunStmtsBody
                   [(Common_ast.SimpleStatement
                       (Common_ast.ValueBindingExpression
                          [((Common_ast.DirectBinding "y"),
                            (Common_ast.BinaryOperation ("+",
                               (Common_ast.IdentityExpr "x"),
                               (Common_ast.IntegerValue 1))))
                            ]));
                     (Common_ast.ReturnStatement
                        (Common_ast.BinaryOperation ("+",
                           (Common_ast.IdentityExpr "x"),
                           (Common_ast.IdentityExpr "y"))))
                     ])
                ))))
       ])

Should parse stupid program
  $ ngast  ../../example/stupid.ng
  (Common_ast.Program
     [(Common_ast.Definition
         (Common_ast.FunDef
            (Common_ast.FunctionDefinition ("fact",
               [(Common_ast.DirectBinding "x"); (Common_ast.DirectBinding "i")],
               (Common_ast.FunExprBody
                  (Common_ast.IfElseExpr (
                     (Common_ast.BinaryOperation ("==",
                        (Common_ast.IdentityExpr "x"),
                        (Common_ast.IntegerValue 0))),
                     (Common_ast.IdentityExpr "i"),
                     (Some (Common_ast.ApplicationExpr (
                              (Common_ast.IdentityExpr "fact"),
                              [(Common_ast.BinaryOperation ("-",
                                  (Common_ast.IdentityExpr "x"),
                                  (Common_ast.IntegerValue 1)));
                                (Common_ast.BinaryOperation ("*",
                                   (Common_ast.IdentityExpr "x"),
                                   (Common_ast.IdentityExpr "i")))
                                ]
                              )))
                     )))
               ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DirectBinding "result"),
                  (Common_ast.ApplicationExpr (
                     (Common_ast.IdentityExpr "fact"),
                     [(Common_ast.IntegerValue 5); (Common_ast.IntegerValue 1)]
                     )))
                  ])));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("plus1",
                [(Common_ast.DirectBinding "x")],
                (Common_ast.FunExprBody
                   (Common_ast.BinaryOperation ("+",
                      (Common_ast.IdentityExpr "x"),
                      (Common_ast.IntegerValue 1))))
                ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("times2",
                [(Common_ast.DirectBinding "x")],
                (Common_ast.FunExprBody
                   (Common_ast.BinaryOperation ("*",
                      (Common_ast.IdentityExpr "x"),
                      (Common_ast.IntegerValue 2))))
                ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("compose",
                [(Common_ast.DirectBinding "f"); (Common_ast.DirectBinding "g")
                  ],
                (Common_ast.FunExprBody
                   (Common_ast.FunctionLiteral (
                      [(Common_ast.DirectBinding "x")],
                      (Common_ast.FunExprBody
                         (Common_ast.ApplicationExpr (
                            (Common_ast.IdentityExpr "f"),
                            [(Common_ast.ApplicationExpr (
                                (Common_ast.IdentityExpr "g"),
                                [(Common_ast.IdentityExpr "x")]))
                              ]
                            )))
                      )))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DirectBinding "v"),
                  (Common_ast.ApplicationExpr (
                     (Common_ast.ApplicationExpr (
                        (Common_ast.IdentityExpr "compose"),
                        [(Common_ast.IdentityExpr "plus1");
                          (Common_ast.IdentityExpr "times2")]
                        )),
                     [(Common_ast.IntegerValue 3)])))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "x");
                       (Common_ast.DirectBinding "y")]
                     )),
                  (Common_ast.TupleLiteral
                     [(Common_ast.IntegerValue 1); (Common_ast.IntegerValue 2)]))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.ListBinding (
                     [(Common_ast.DirectBinding "a");
                       (Common_ast.DirectBinding "b")],
                     true)),
                  (Common_ast.ArrayLiteral
                     [(Common_ast.IntegerValue 1); (Common_ast.IntegerValue 2);
                       (Common_ast.IntegerValue 3)]))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.ListBinding (
                     [(Common_ast.DirectBinding "c");
                       (Common_ast.DirectBinding "d")],
                     false)),
                  (Common_ast.IdentityExpr "b"))])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DirectBinding "m"),
                  (Common_ast.IfElseExpr (
                     (Common_ast.BinaryOperation ("==",
                        (Common_ast.IdentityExpr "x"),
                        (Common_ast.IntegerValue 1))),
                     (Common_ast.IntegerValue 1),
                     (Some (Common_ast.IntegerValue 10)))))
                  ])))
       ])

Should parse definitions
  $ ngast  ../../example/definitions.ng
  (Common_ast.Program
     [(Common_ast.Definition
         (Common_ast.FunDef
            (Common_ast.FunctionDefinition ("hello",
               [(Common_ast.DestructTupleBinding (None,
                   [(Common_ast.DirectBinding "x")]))
                 ],
               (Common_ast.FunStmtsBody
                  [(Common_ast.SimpleStatement
                      (Common_ast.ApplicationExpr (
                         (Common_ast.IdentityExpr "print"),
                         [(Common_ast.IdentityExpr "x")])))
                    ])
               ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("id",
                [(Common_ast.DestructTupleBinding (None,
                    [(Common_ast.DirectBinding "x")]))
                  ],
                (Common_ast.FunExprBody (Common_ast.IdentityExpr "x"))))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("compose",
                [(Common_ast.DirectBinding "f"); (Common_ast.DirectBinding "g")
                  ],
                (Common_ast.FunExprBody Common_ast.SpreadLiteral)))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DirectBinding "x"), (Common_ast.IntegerValue 1));
                  ((Common_ast.DirectBinding "y"), (Common_ast.IntegerValue 2))
                  ])));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "x");
                       (Common_ast.DirectBinding "y")]
                     )),
                  (Common_ast.TupleLiteral
                     [(Common_ast.IdentityExpr "y");
                       (Common_ast.IdentityExpr "x")]));
                  ((Common_ast.ListBinding (
                      [(Common_ast.DirectBinding "a");
                        (Common_ast.DirectBinding "b");
                        (Common_ast.DirectBinding "c");
                        (Common_ast.DirectBinding "rest")],
                      true)),
                   (Common_ast.ApplicationExpr (
                      (Common_ast.IdentityExpr "get_list"),
                      [Common_ast.UnitLiteral])))
                  ])));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("hd",
                [(Common_ast.ListBinding (
                    [(Common_ast.DirectBinding "x");
                      (Common_ast.DirectBinding "ys")],
                    true))
                  ],
                (Common_ast.FunExprBody (Common_ast.IdentityExpr "x"))))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("tl",
                [(Common_ast.ListBinding (
                    [Common_ast.WildcardBinding;
                      (Common_ast.DirectBinding "rest")],
                    true))
                  ],
                (Common_ast.FunExprBody (Common_ast.IdentityExpr "rest"))))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.DestructTupleBinding ((Some "BinExpr"),
                     [(Common_ast.DirectBinding "left");
                       (Common_ast.DirectBinding "right")]
                     )),
                  (Common_ast.ApplicationExpr (
                     (Common_ast.IdentityExpr "get_binexpr"),
                     [Common_ast.UnitLiteral])))
                  ])));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("int", [], Common_ast.TypeDeclaration
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("option",
                [(Common_ast.SimpleTypeParameter "t")],
                Common_ast.TypeDeclaration))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("result",
                [(Common_ast.ParametricTypeParameter ("l",
                    [(Common_ast.SimpleTypeParameter "r")]))
                  ],
                Common_ast.TypeDeclaration))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("multimap",
                [(Common_ast.ParametricTypeParameter ("k",
                    [(Common_ast.ParametricTypeParameter ("list",
                        [(Common_ast.SimpleTypeParameter "v")]))
                      ]
                    ))
                  ],
                Common_ast.TypeDeclaration))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("map",
                [(Common_ast.ParametricTypeParameter ("k",
                    [(Common_ast.SimpleTypeParameter "v")]))
                  ],
                Common_ast.TypeDeclaration))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("name_alias", [],
                (Common_ast.TypeAlias (Common_ast.SimpleTypeAnn "string"))))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("age_alias", [],
                (Common_ast.TypeAlias (Common_ast.SimpleTypeAnn "int"))))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("name_t", [],
                (Common_ast.DataTypeDefinition
                   [(Common_ast.SimpleTypeAnn "string")])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("age_t", [],
                (Common_ast.DataTypeDefinition
                   [(Common_ast.SimpleTypeAnn "string")])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("pair",
                [(Common_ast.SimpleTypeParameter "l");
                  (Common_ast.SimpleTypeParameter "r")],
                (Common_ast.DataTypeDefinition
                   [(Common_ast.SimpleTypeAnn "l");
                     (Common_ast.SimpleTypeAnn "r")])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("node",
                [(Common_ast.SimpleTypeParameter "t")],
                (Common_ast.RecordDataTypeDefinition
                   [("value", (Common_ast.SimpleTypeAnn "t"));
                     ("next",
                      (Common_ast.ParamerticTypeAnn ("option",
                         [(Common_ast.ParamerticTypeAnn ("ref",
                             [(Common_ast.ParamerticTypeAnn ("node",
                                 [(Common_ast.SimpleTypeAnn "t")]))
                               ]
                             ))
                           ]
                         )))
                     ])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("basic_color", [],
                (Common_ast.SumTypeDefinition
                   [("Red", None); ("Green", None); ("Blue", None)])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("list",
                [(Common_ast.SimpleTypeParameter "t")],
                (Common_ast.SumTypeDefinition
                   [("Cons",
                     (Some (Common_ast.TupleTypeAnn
                              [(Common_ast.SimpleTypeAnn "t");
                                (Common_ast.ParamerticTypeAnn ("list",
                                   [(Common_ast.SimpleTypeAnn "t")]))
                                ])));
                     ("Nil", None)])
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("ints", [],
                (Common_ast.TypeAlias
                   (Common_ast.ListTypeAnn (Common_ast.SimpleTypeAnn "int")))
                ))));
       (Common_ast.Definition
          (Common_ast.SignatureDef
             (Common_ast.SignatureDefinition ("id",
                (Common_ast.FuncSignature
                   ([(Common_ast.SimpleTypeParameter "t")],
                    [(Common_ast.SimpleTypeAnn "t");
                      (Common_ast.SimpleTypeAnn "t")]))
                ))));
       (Common_ast.Definition
          (Common_ast.SignatureDef
             (Common_ast.SignatureDefinition ("plus",
                (Common_ast.FuncSignature
                   ([],
                    [(Common_ast.SimpleTypeAnn "int");
                      (Common_ast.FuncSignature
                         ([],
                          [(Common_ast.SimpleTypeAnn "int");
                            (Common_ast.SimpleTypeAnn "int")]))
                      ]))
                ))));
       (Common_ast.Definition
          (Common_ast.SignatureDef
             (Common_ast.SignatureDefinition ("compose",
                (Common_ast.FuncSignature
                   ([(Common_ast.SimpleTypeParameter "a");
                      (Common_ast.SimpleTypeParameter "b");
                      (Common_ast.SimpleTypeParameter "c")],
                    [(Common_ast.FuncSignature
                        ([],
                         [(Common_ast.SimpleTypeAnn "a");
                           (Common_ast.SimpleTypeAnn "b")]));
                      (Common_ast.FuncSignature
                         ([],
                          [(Common_ast.FuncSignature
                              ([],
                               [(Common_ast.SimpleTypeAnn "b");
                                 (Common_ast.SimpleTypeAnn "c")]));
                            (Common_ast.FuncSignature
                               ([],
                                [(Common_ast.SimpleTypeAnn "a");
                                  (Common_ast.SimpleTypeAnn "c")]))
                            ]))
                      ]))
                ))));
       (Common_ast.Definition
          (Common_ast.SignatureDef
             (Common_ast.SignatureDefinition ("map",
                (Common_ast.FuncSignature
                   ([(Common_ast.ParametricTypeParameter ("Monad",
                        [(Common_ast.ParametricTypeParameter ("m",
                            [Common_ast.PlaceHolderParameter]))
                          ]
                        ));
                      (Common_ast.SimpleTypeParameter "a");
                      (Common_ast.SimpleTypeParameter "b")],
                    [(Common_ast.ParamerticTypeAnn ("m",
                        [(Common_ast.SimpleTypeAnn "a")]));
                      (Common_ast.FuncSignature
                         ([],
                          [(Common_ast.FuncSignature
                              ([],
                               [(Common_ast.SimpleTypeAnn "a");
                                 (Common_ast.SimpleTypeAnn "b")]));
                            (Common_ast.ParamerticTypeAnn ("m",
                               [(Common_ast.SimpleTypeAnn "b")]))
                            ]))
                      ]))
                ))));
       (Common_ast.Definition
          (Common_ast.TypeDef
             (Common_ast.TypeDefinition ("arr",
                [(Common_ast.ParametricTypeParameter ("a",
                    [(Common_ast.SimpleTypeParameter "b")]))
                  ],
                (Common_ast.TypeAlias
                   (Common_ast.FuncSignature
                      ([],
                       [(Common_ast.SimpleTypeAnn "a");
                         (Common_ast.SimpleTypeAnn "b")])))
                ))));
       (Common_ast.Definition
          (Common_ast.ConceptDef ("Eq", [(Common_ast.SimpleTypeParameter "t")],
             [(Common_ast.ConceptFunDeclaration
                 (Common_ast.SignatureDefinition ("equal",
                    (Common_ast.FuncSignature
                       ([],
                        [(Common_ast.SimpleTypeAnn "t");
                          (Common_ast.FuncSignature
                             ([],
                              [(Common_ast.SimpleTypeAnn "t");
                                (Common_ast.SimpleTypeAnn "bool")]))
                          ]))
                    )))
               ]
             )));
       (Common_ast.Definition
          (Common_ast.ConceptDef ("Iterable",
             [(Common_ast.SimpleTypeParameter "t")],
             [(Common_ast.ConceptTypeDeclaration
                 (Common_ast.TypeDefinition ("elem_t", [],
                    (Common_ast.TypeAlias (Common_ast.SimpleTypeAnn "t")))));
               (Common_ast.ConceptTypeDeclaration
                  (Common_ast.TypeDefinition ("itereator_t", [],
                     Common_ast.TypeDeclaration)));
               (Common_ast.ConceptValDeclaration
                  (Common_ast.ValueDeclaration ("iterator",
                     (Common_ast.SimpleTypeAnn "itereator_t"))));
               Common_ast.ConceptDeclaration]
             )));
       (Common_ast.Definition
          (Common_ast.ValueDef
             (Common_ast.ValueDeclaration ("x",
                (Common_ast.SimpleTypeAnn "int")))));
       (Common_ast.Definition
          (Common_ast.ValueDef
             (Common_ast.ValueDeclaration ("y", (Common_ast.SimpleTypeAnn "t")
                ))));
       (Common_ast.Definition
          (Common_ast.ImplDef ("Iterable",
             (Common_ast.ParamerticTypeAnn ("List",
                [(Common_ast.SimpleTypeAnn "t")])),
             [(Common_ast.ImplTypeDef
                 (Common_ast.TypeDefinition ("elem_t", [],
                    (Common_ast.TypeAlias (Common_ast.SimpleTypeAnn "t")))));
               Common_ast.ImplDeclaration]
             )));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ValueBindingExpression
                [((Common_ast.AnnotatedBinding ("m",
                     (Common_ast.SimpleTypeAnn "int"))),
                  (Common_ast.IntegerValue 1))])));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("plus",
                [(Common_ast.DestructTupleBinding (None,
                    [(Common_ast.AnnotatedBinding ("x",
                        (Common_ast.SimpleTypeAnn "int")))
                      ]
                    ));
                  (Common_ast.DestructTupleBinding (None,
                     [(Common_ast.AnnotatedBinding ("y",
                         (Common_ast.SimpleTypeAnn "int")))
                       ]
                     ))
                  ],
                (Common_ast.FunExprBody
                   (Common_ast.BinaryOperation ("+",
                      (Common_ast.IdentityExpr "x"),
                      (Common_ast.IdentityExpr "y"))))
                ))))
       ])


Should parse expressions
  $ ngast  ../../example/expressions.ng
  (Common_ast.Program
     [(Common_ast.Statement
         (Common_ast.SimpleStatement
            (Common_ast.BinaryOperation ("+", (Common_ast.IntegerValue 1),
               (Common_ast.IntegerValue 1)))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.BinaryOperation ("+", (Common_ast.IdentityExpr "a"),
                (Common_ast.IdentityExpr "b")))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.LetBindingExpr (
                [((Common_ast.DirectBinding "x"), (Common_ast.IntegerValue 1))],
                (Common_ast.BinaryOperation ("+",
                   (Common_ast.IdentityExpr "x"), (Common_ast.IntegerValue 1)))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.LetBindingExpr (
                [((Common_ast.DirectBinding "x"), (Common_ast.IntegerValue 1));
                  ((Common_ast.DirectBinding "y"), (Common_ast.IntegerValue 2))
                  ],
                (Common_ast.IfElseExpr (
                   (Common_ast.BinaryOperation (">",
                      (Common_ast.IdentityExpr "x"),
                      (Common_ast.IdentityExpr "y"))),
                   (Common_ast.IdentityExpr "x"),
                   (Some (Common_ast.IdentityExpr "y"))))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.LetBindingExpr (
                [((Common_ast.DirectBinding "x"),
                  (Common_ast.ArrayLiteral
                     [(Common_ast.IntegerValue 1); (Common_ast.IntegerValue 2);
                       (Common_ast.IntegerValue 3)]))
                  ],
                (Common_ast.AccessorExpr ((Common_ast.IdentityExpr "x"),
                   "length"))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.LetBindingExpr (
                [((Common_ast.DirectBinding "t"),
                  (Common_ast.TupleLiteral
                     [(Common_ast.IntegerValue 123);
                       (Common_ast.BooleanValue true)]))
                  ],
                (Common_ast.ApplicationExpr ((Common_ast.IdentityExpr "snd"),
                   [(Common_ast.IdentityExpr "t")]))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.LetBindingExpr (
                [((Common_ast.DestructTupleBinding (None,
                     [(Common_ast.DirectBinding "f");
                       (Common_ast.DirectBinding "g")]
                     )),
                  (Common_ast.ApplicationExpr (
                     (Common_ast.IdentityExpr "destruct"),
                     [Common_ast.UnitLiteral])))
                  ],
                (Common_ast.ApplicationExpr (
                   (Common_ast.IdentityExpr "compose"),
                   [(Common_ast.IdentityExpr "f");
                     (Common_ast.IdentityExpr "g")]
                   ))
                ))));
       (Common_ast.Statement
          (Common_ast.SimpleStatement
             (Common_ast.ApplicationExpr ((Common_ast.IdentityExpr "flip"),
                [(Common_ast.ApplicationExpr ((Common_ast.IdentityExpr "flip"),
                    [(Common_ast.IdentityExpr "flip")]));
                  (Common_ast.IdentityExpr "flip")]
                ))));
       (Common_ast.Definition
          (Common_ast.FunDef
             (Common_ast.FunctionDefinition ("compose",
                [(Common_ast.DirectBinding "f"); (Common_ast.DirectBinding "g")
                  ],
                (Common_ast.FunExprBody
                   (Common_ast.FunctionLiteral (
                      [(Common_ast.DirectBinding "x")],
                      (Common_ast.FunExprBody
                         (Common_ast.ApplicationExpr (
                            (Common_ast.IdentityExpr "f"),
                            [(Common_ast.ApplicationExpr (
                                (Common_ast.IdentityExpr "g"),
                                [(Common_ast.IdentityExpr "x")]))
                              ]
                            )))
                      )))
                ))))
       ])
