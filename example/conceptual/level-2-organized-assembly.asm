// this is OGRASM code for every example files:

// 01.id.ng

.module example01

.symbols [example01, id, n]

.start none

.fun id
.param [n]
00:    load_param param.0 // n
01:    return
.endfun id
.endmodule

// 02.many_defs.ng

.module default

.symbols [id1, n, id2, m, id3, x, id5, y, z, id6, x1, x2, id7]

.fun id1
.param [n]
00:    load_param param.0 // n
01:    return
.endfun id1

.fun id2
.param [m]
02:    load_param param.0
03:    return
.endfun id2

.fun id3
.param [x]
04:    load_param param.0
05:    return
.endfun id3

.fun id5
.param [x, y, z]
06:    load_param param.0
07:    br id5_01, id5_04
.label id5_01, +1
08:    load_param param.1
09:    return
.label id5_04,
10:    load_param param.2
11:    return
.endfun id5

.fun id6
.param [x, y, z, x1, x2]
12:    load_param param.0
13:    br id6_01, id6_02
.label id6_01
14:    load_param param.1
15:    br id6_03, id6_04
.label id6_03
16:    load_param param.2
17:    return
.label id6_04
18:    load_param param.3
19:    return
.label id6_02
20:    load_param 05
21:    return
.endfn

.fun id7
.param [x, y, z, x1]
22:    load_param param.0
23:    br id7_01, id7_02
.label id7_01
24:    load_param param.1
25:    br id7_03, id7_04
.label id7_03
26:    load_param param.2
27:    return
.label id7_04
28:    load_param param.3
29:    return
.label id7_02
30:    load_param param.1
31:    br id7_05, id7_06
.label id7_05
32:    load_param param.4
33:    return
.label id7_06
34:    return # return nothing (stack was consumed
.endfun

.endmodule
// 03.funcall_and_idexpr.ng

.module default

.symbols [fact, n, fact5, core, print, assert, opEquals]

.import print, core
.import assert, core

.const 0
.const 1
.const 5
.const 120

.val fact5

.fun fact
.param [n]
00:    load_param param.0; n
01:    load_const const.0; 0
02:    gt
03:    br fact_01, fact_02; +3, +9
.label fact_01
04:    load_param param.0; n
05:    load_param param.0; n
06:    load_const const.1; 1
07:    subtract
08:    push_param
09:    call 00; fact
10:    multiply
11:    return
.label fact_02
12:    load_const const.1; 1
13:    return
.endfun

.start

14:    load_const const.2; 5
15:    push_param
16:    call 00; fact
17:    store_value val.0; fact5

18:    load_value val.0; fact5
19:    load_const const.3; 120
20:    push_param
21:    push_self
22:    load_symbol symbol.6; opEquals
23:    invoke_method
24:    push_param
25:    call import.01; core.assert

26:    load_value val.0; fact5
27:    push_param
28:    call import.0; core.print

.end

.endmodule
// 04.str.ng

.module hello

.symbols [hello, hi, hellosize, opPlus, size, core, print]

.import print, core

.str [hello world]
.str [ how are you]

.fun hi
.param []
00:    load_str str.0 ; [hello world]
01:    return
.endfun

.fun hellosize
.param []
02:    call 00; hi
03:    load_str str.1 ; [ how are you]
04:    push_param
05:    push_self
06:    load_symbol symbol.3; opPlus
07:    invoke_method
08:    push_self
09:    load_symbol symbol.4; size
10:    invoke_method
11:    return
.endfun

.start
12:    call 00; hi
13:    push_param
14:    call import.0; core.print

15:    call 02; hellosize
16:    push_param
17:    call import.0; core.print
.end
.endmodule

// 05.valdef.ng

.module default

.symbols [y, add, charAt, opPlus, size, z, core, print]

.import print, core

.const 0
.const 1

.str [hello world]

.val y
.val z

.start

00:    load_str str.0; [hello world]
01:    store_value val.0; y
02:    goto 22 // skip function definition

.fun add
.param []
03:    load_value val.0; y
04:    load_const const.0; 0
05:    push_param
06:    push_self
07:    load_symbol symbol.2; charAt
08:    invoke_method
09:    load_value val.0; y
10:    load_symbol symbol.4; size
11:    invoke_method
12:    push_param
13:    push_self
14:    load_symbol symbol.3; opPlus
15:    invoke_method
16:    load_const const.1; 1
17:    push_param
18:    push_self
19:    load_symbol symbol.3; opPlus
20:    invoke_method
21:    return
.endfun

22:    call 00; add
23:    store_value val.1; z

24:    load_value val.1; z
25:    push_param
26:    call import.0; core.print

.end
.endmodule

// 06.array.ng

.module default

.symbols [arr, core, print, assert, opIndex]
.symbols [opEquals, opIndexAssign, opLShift, twoDimArr]

.import print, core
.import assert, core

.array [1, 2, 3, 4, 5]
.array [[1, 2], [3, 4]]

.const 1
.const 2
.const 3
.const 4
.const 5
.const 6
.const 7
.const 0

.val arr
.val twoDimArr

.start

00:    load_array array.0; [1, 2, 3, 4, 5]
01:    store_value val.0; arr

02:    load_value val.0; arr
03:    push_param
04:    call import.0; print

05:    load_value val.0; arr
06:    load_const const.2; 3
07:    push_param
08:    push_self
09:    load_symbol symbol.4; opIndex
10:    invoke_method
11:    push_param
12:    call import.0; print

13:    load_value val.0; arr
14:    load_const const.2; 3
15:    push_param
16:    push_self
17:    load_symbol symbol.4; opIndex
18:    invoke_method
19:    load_const const.3; 4
20:    push_param
21:    push_self
22:    load_symbol symbol.5; opEquals
23:    invoke_method
24:    push_param
25:    call import.1; assert

26:    load_value val.0; arr
27:    load_const const.3; 4
28:    push_param
29:    push_self
30:    load_symbol symbol.4; opIndex
31:    invoke_method
32:    load_const const.4; 5
33:    push_param
34:    push_self
35:    load_symbol symbol.5; opEquals
36:    invoke_method
37:    push_param
38:    call import.1; assert

39:    load_value val.0; arr
40:    load_const const.3; 4
41:    push_param
42:    load_const const.5; 6
43:    push_param
44:    push_self
45:    load_symbol symbol.6; opIndexAssign
46:    invoke_method
47:    ignore // clear stack, ingore expression result

48:    load_value val.0; arr
49:    load_const const.3; 4
50:    push_param
51:    push_self
52:    load_symbol symbol.4; opIndex
53:    invoke_method
54:    load_const const.5; 6
55:    push_param
56:    push_self
57:    load_symbol symbol.5; opEquals
58:    invoke_method
59:    push_param
60:    call import.1; assert

61:    load_value val.0; arr
62:    load_const const.6; 7
63:    push_param
64:    push_self
65:    load_symbol symbol.7; opLShift
66:    invoke_method
67:    ignore

68:    load_value val.0; arr
69:    load_const const.4; 5
70:    push_param
71:    push_self
72:    load_symbol symbol.4; opIndex
73:    invoke_method
74:    load_const const.6; 7
75:    push_param
76:    push_self
77:    load_symbol symbol.5; opEquals
78:    invoke_method
79:    push_param
80:    call import.1; assert

81:    load_array array.1; [[1, 2], [3, 4]]
82:    store_value val.1; twoDimArr

83:    load_value val.1; twoDimArr
84:    load_const const.7; 0
85:    push_param
86:    push_self
87:    load_symbol symbol.4; opIndex
88:    invoke_method
89:    load_const const.7; 0
90:    push_param
91:    push_self
92:    load_symbol symbol.4; opIndex
93:    invoke_method
94:    load_const const.0; 1
95:    push_param
96:    push_self
97:    load_symbol symbol.5; opEquals
98:    invoke_method
99:    push_param
100:    call import.1; core.assert

101:    load_value val.1; twoDimArr
102:    load_const const.0; 1
103:    push_param
104:    push_self
105:    load_symbol symbol.4; opIndex
106:    invoke_method
107:    load_const const.0; 1
108:    push_param
109:    push_self
110:    load_symbol symbol.4; opIndex
111:    invoke_method
112:    load_const const.3; 4
113:    push_param
114:    push_self
115:    load_symbol symbol.5; opEquals
116:    invoke_method
117:    push_param
118:    call import.1; core.assert

119:    load_value val.1; twoDimArr
120:    push_param
121:    call import.0; core.print

122:    load_value val.1; twoDimArr
123:    load_const const.0; 1
124:    push_param
125:    push_self
126:    load_symbol symbol.4; opIndex
127:    invoke_method
128:    load_const const.7; 0
129:    push_param
130:    push_self
131:    load_symbol symbol.4; opIndex
132:    invoke_method
133:    push_param
134:    call import.0; core.print

.end
.endmodule

// 07.object.ng
// TODO: expand following to l2 orgasm:

.module default

.import print, core
.import assert, core

.symbols [Person, firstName, lastName, kid]; 0, 1, 2, 3
.symbols [name, person, assert, size, print]; 4, 5, 6, 7, 8

.const 9

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

.method Person.firstName
.param []
    get_property property.1; firstName
    return
.end

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