// Level-2 ORGASM for 07.object.ng

.module default
.symbols [Person, firstName, lastName, kid, name, person, assert, size, print]
.str [Kimmy]
.str [Leo]
.str [Tiny]
.str [ ]

.val addr 0    // person (object reference)

.type Person
.property firstName
.property lastName
.property kid
.method name
.endtype

.method Person.name
.param []
00:    load_self
01:    call_method firstName
02:    load_str str.3; [ ]
03:    add.str
04:    load_self
05:    call_method lastName
06:    add.str
07:    return.str
.endmethod

.start
00:    load_type Person
01:    new_object
02:    load_str str.0; [Kimmy]
03:    set_property firstName
04:    load_str str.1; [Leo]
05:    set_property lastName
06:    load_type Person
07:    new_object
08:    load_str str.2; [Tiny]
09:    set_property firstName
10:    load_str str.1; [Leo]
11:    set_property lastName
12:    set_property kid
13:    store_value.addr val.0; person
14:    load_value.addr val.0; person
15:    call_method name
16:    call_method size
17:    load_const.i32 9i32
18:    eq.i32
19:    call import.0; assert
20:    load_value.addr val.0; person
21:    call_method name
22:    call import.2; print
23:    load_value.addr val.0; person
24:    call_method kid
25:    call_method name
26:    call import.2; print
.end
.endmodule
