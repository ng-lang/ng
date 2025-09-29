// Level-2 ORGASM for 09.scope.ng

.module default
.symbols [a, b, assert]
.const 1i32
.const 2i32
.val i32 0
.val i32 0
.start
00:    load_const.i32 const.0; 1i32
01:    store_value.i32 val.0; a
02:    load_value.i32 val.0; a
03:    load_const.i32 const.0; 1i32
04:    eq.i32
05:    call import.0; assert
06:    load_const.i32 const.1; 2i32
07:    store_value.i32 val.0; a
08:    load_value.i32 val.0; a
09:    load_const.i32 const.1; 2i32
10:    eq.i32
11:    call import.0; assert
12:    load_value.i32 val.0; a
13:    load_const.i32 const.0; 1i32
14:    eq.i32
15:    call import.0; assert
16:    load_const.i32 const.0; 1i32
17:    store_value.i32 val.1; b
18:    load_value.i32 val.1; b
19:    load_const.i32 const.0; 1i32
20:    eq.i32
21:    call import.0; assert
22:    load_const.i32 const.1; 2i32
23:    store_value.i32 val.1; b
24:    load_value.i32 val.1; b
25:    load_const.i32 const.1; 2i32
26:    eq.i32
27:    call import.0; assert
.end
.endmodule
