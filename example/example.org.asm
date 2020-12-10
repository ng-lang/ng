// this is OGRASM code for every example files:

// 01.id.ng

.module example01
.fun id
.param 1
    load_param 01
    return
.endfun id
.endmodule

// 02.many_defs.ng

.module default
.fun id1
.param 1
    load_param 01
    return
.endfun id1

.fun id2
.param 1
    load_param 01
    return
.endfun id2

.fun id3
.param 1
    load_param 01
    return
.endfun id3

.fun id5
.param 3
    load_param 01
    br id5_01.1, id5_02.3
.label id5_01
    load_param 02
    return
.label id5_02
    load_param 03
    return
.endfun id5

.fun id6
.param 5
    load_param 01
    br id6_01.1, id6_02.7
.label id6_01
    load_param 02
    br id6_03.1, id6_04.3
.label id6_03
    load_param 03
    return
.label id6_04
    load_param 04
    return
.label id6_02
    load_param 05
    return
.endfn

.fun id7
.param 4
    load_param 01
    br id7_01, id7_02
.label id7_01
    load_param 02
    br id7_03, id7_04
.label id7_03
    load_param 03
    return
.label id7_04
    load_param 04
    return
.label id7_02
    load_param 02
    br id7_05, id7_06
.label id7_05
    load_param 05
    return
.label id7_06
    return # return nothing (stack was consumed
.endfun

.endmodule
// 03.funcall_and_idexpr.ng

.module default

.fun fact
.param 1
    load_param 01
    load_const 0
    gt
    br fact_01, fact_02
.label fact_01
    load_param 01
    load_param 01
    load_const 1
    subtract
    call fact
    multiply
    return
.label fact_02
    load_const 1
    return
.endfun

.start

.val fact5
    load_const 5
    call fact
    store_value 01
.endval fact5

    load_value 01
    load_const 120
    eq
    call assert

    load_variable
    call print

.end

.endmodule
// 04.str.ng

.module hello

.str [hello world]
.str [how are you]

.fun hi
.param 0
    load_str 01
    return
.endfun

.fun hellosize
.param 0
    load_str 02
    call hi
    call_method opPlus
    call_method size
    return
.endfun

.start
    call hi
    call print

    call hellosize
    call print
.end
.endmodule

// 05.valdef.ng

.module default

.str [helloo world]

.fun add
.param 0
    load_const 0
    load_str 01
    call_method charAt
    load_str 01
    call_method size
    add
    load_const 1
    add
    return
.endfun

.start

.val z
    call add
    store_value 01
.endval

    load_value 01
    call print

.end
.endmodule

// 06.array.ng

.module default

.start

.array [1, 2, 3, 4, 5]
.array [[1, 2], [3, 4]]

.val arr
    load_array 01
    store_value 01
.endval

    load_value 01
    call print

    load_value 01
    load_const 3
    index_at
    call print

    load_value 01
    load_const 3
    index_at
    load_const 4
    eq
    call assert

    load_value 01
    load_const 4
    index_at
    load_const 5
    eq
    call assert

    load_value 01
    load_const 4
    load_const 6
    index_assign
    ignore // clear stack, ingore expression result

    load_value 01
    load_const 4
    index_at
    load_const 6
    eq
    call assert

    load_value 01
    load_const 7
    shl // left shift
    ignore

.val twoDimArr
    load_array 02
    store_value 02
.endval

    load_value 02
    load_const 0
    index_at
    load_const 0
    index_at
    load_const 1
    eq
    call assert

    load_value 02
    load_const 1
    index_at
    load_const 1
    index_at
    load_const 4
    eq
    call assert

    load_value 02
    call print

    load_value 02
    load_const 1
    index_at
    load_const 0
    index_at
    call print
.end
.endmodule

// 07.object.ng

.module default

.str [ ]
.str [Kimmy]
.str [Leo]
.str [Tiny]

.type Person
.property firstName
.property lastName
.property kid
.method name
.endtype

.method Person.name
.param 0
    load_self
    call_method firstName
    load_str 01
    add
    load_self
    call_method lastName
    add
    return
.endmethod

.start

.val person
    load_type Person
    new_object
    load_str 02
    set_property firstName
    load_str 03
    set_property lastName
    load_type Person
    new_object
    load_str 04
    set_property firstName
    load_str 03
    set_property lastName
    set_property kid
    store_value 01
.endval

    load_value 01
    call_method name
    call_method size
    load_const 9
    call assert

    load_value 01
    call_method kid
    call_method name
    call print

.end

.endmodule

// external.ng

.module external

.export hello

.str [hello world]

.fun hello
    load_str 01
    call print
    return
.endfun

// 08.imports.ng

.module default

.import "external" ext

.start

.val ext
    load_module 01
    store_value 01
.endval

    load_value 01
    call_method hello
.end

.endmodule