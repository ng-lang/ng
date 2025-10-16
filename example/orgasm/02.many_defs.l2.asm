// Level-2 ORGASM for 02.many_defs.ng

.module default
.symbols [id1, n, id2, m, id3, x, id5, y, z, id6, x1, x2, id7, print, assert, opEquals]
.import print, core
.import assert, core
.const i32 1
.const i32 2
.const i32 3
.const i32 4
.const i32 5
.const i32 6
.const i32 7
.const bool true
.const bool false

.fun id1
.param [n:i32]
00:    load_param.i32 param.0    // n
01:    return
.endfun id1

.fun id2
.param [m:i32]
00:    load_param.i32 param.0
01:    return
.endfun id2

.fun id3
.param [x:i32]
00:    load_param.i32 param.0
01:    return
.endfun id3

.fun id5
.param [x:bool, y:i32, z:i32]
00:    load_param.bool param.0
01:    br id5_01, id5_04
.label id5_01, +1
02:    load_param.i32 param.1
03:    return
.label id5_04,
04:    load_param.i32 param.2
05:    return
.endfun id5

.fun id6
.param [x:bool, y:bool, z:i32, x1:i32, x2:i32]
00:    load_param.bool param.0
01:    br id6_01, id6_02
.label id6_01
02:    load_param.bool param.1
03:    br id6_03, id6_04
.label id6_03
04:    load_param.i32 param.2
05:    return
.label id6_04
06:    load_param.i32 param.3
07:    return
.label id6_02
08:    load_param.i32 param.4
09:    return
.endfun id6

.fun id7
.param [x:bool, y:bool, z:i32, x1:i32]
00:    load_param.bool param.0
01:    br id7_01, id7_02
.label id7_01
02:    load_param.bool param.1
03:    br id7_03, id7_04
.label id7_03
04:    load_param.i32 param.2
05:    return
.label id7_04
06:    load_param.i32 param.3
07:    return
.label id7_02
08:    load_param.bool param.1
09:    br id7_05, id7_06
.label id7_05
10:    load_param.i32 param.2
11:    return
.label id7_06
12:    return    // return nothing (stack was consumed)
.endfun id7

.start
00:    // print(id1(id2(id3(1))))
01:    load_const.i32 const.0        // 1
02:    push_param
03:    call fun.2                    // id3
04:    push_param
05:    call fun.1                    // id2
06:    push_param
07:    call fun.0                    // id1
08:    push_param
09:    call import.0                 // print
10:
11:    // print(id5(id1(true), id2(2), id3(3)))
12:    load_const.bool const.6       // true
13:    push_param
14:    call fun.0                    // id1
15:    load_const.i32 const.1        // 2
16:    push_param
17:    call fun.1                    // id2
18:    load_const.i32 const.2        // 3
19:    push_param
20:    call fun.2                    // id3
21:    push_param
22:    push_param
23:    call fun.3                    // id5
24:    push_param
25:    call import.0                 // print
26:
27:    // print(id6(true, false, 2, 3, 4))
28:    load_const.bool const.6       // true
29:    load_const.bool const.7       // false
30:    load_const.i32 const.1        // 2
31:    load_const.i32 const.2        // 3
32:    load_const.i32 const.3        // 4
33:    push_param
34:    push_param
35:    push_param
36:    push_param
37:    push_param
38:    call fun.4                    // id6
39:    push_param
40:    call import.0                 // print
41:
42:    // print(id7(true, true, 4, 5))
43:    load_const.bool const.6       // true
44:    load_const.bool const.6       // true
45:    load_const.i32 const.3        // 4
46:    load_const.i32 const.4        // 5
47:    push_param
48:    push_param
49:    push_param
50:    push_param
51:    call fun.5                    // id7
52:    push_param
53:    call import.0                 // print
54:
55:    // print(id7(true, false, 4, 5))
56:    load_const.bool const.6       // true
57:    load_const.bool const.7       // false
58:    load_const.i32 const.3        // 4
59:    load_const.i32 const.4        // 5
60:    push_param
61:    push_param
62:    push_param
63:    push_param
64:    call fun.5                    // id7
65:    push_param
66:    call import.0                 // print
67:
68:    // print(id7(false, true, 6, 7))
69:    load_const.bool const.7       // false
70:    load_const.bool const.6       // true
71:    load_const.i32 const.5        // 6
72:    load_const.i32 const.6        // 7
73:    push_param
74:    push_param
75:    push_param
76:    push_param
77:    call fun.5                    // id7
78:    push_param
79:    call import.0                 // print
80:
81:    return
.end
.endmodule
