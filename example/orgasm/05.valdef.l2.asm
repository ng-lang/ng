// Level-2 ORGASM for 05.valdef.ng

.module default
.symbols [y, add, charAt, opPlus, size, z, a, b, c, fa, fb, print, assert]
.import print, core
.import assert, core
.const i32 0
.const i32 1
.const i16 1
.const u32 1
.const u64 1
.const f32 1.0
.const f64 1.0
.str [hello world]
.val addr 0    // y
.val i32 0     // z
.val i16 0     // a
.val u32 0     // b
.val u64 0     // c
.val f32 0     // fa
.val f64 0     // fb

.fun add
.param []
00:    load_value.addr val.0         // y
01:    load_const.i32 const.0        // 0
02:    push_param
03:    push_self
04:    load_symbol symbol.2          // charAt
05:    invoke_method
06:    load_value.addr val.0         // y
07:    load_symbol symbol.4          // size
08:    invoke_method
09:    push_param
10:    push_self
11:    load_symbol symbol.3          // opPlus
12:    invoke_method
13:    load_const.i32 const.1        // 1
14:    push_param
15:    push_self
16:    load_symbol symbol.3          // opPlus
17:    invoke_method
18:    return
.endfun

.start
00:    load_str str.0                // [hello world]
01:    store_value.addr val.0        // y
02:    goto 10                       // skip function definition
03:    call fun.0                    // add
04:    store_value.i32 val.1         // z
05:    load_value.i32 val.1          // z
06:    push_param
07:    call import.0                 // print
08:    load_const.i16 const.2        // 1i16
09:    store_value.i16 val.2         // a
10:    load_const.u32 const.3        // 1u32
11:    store_value.u32 val.3         // b
12:    load_const.u64 const.4        // 1u64
13:    store_value.u64 val.4         // c
14:    load_value.i16 val.2          // a
15:    load_value.u32 val.3          // b
16:    eq.i16
17:    push_param
18:    call import.1                 // assert
19:    load_value.u32 val.3          // b
20:    load_value.u64 val.4          // c
21:    eq.u32
22:    push_param
23:    call import.1                 // assert
24:    load_value.i16 val.2          // a
25:    load_value.u64 val.4          // c
26:    eq.u64
27:    push_param
28:    call import.1                 // assert
29:    load_const.f32 const.5        // 1f32
30:    store_value.f32 val.5         // fa
31:    load_const.f64 const.6        // 1f64
32:    store_value.f64 val.6         // fb
33:    load_value.f32 val.5          // fa
34:    load_value.f64 val.6          // fb
35:    cast.f64
36:    eq.f64
37:    push_param
38:    call import.1                 // assert
39:    return
.end
.endmodule
