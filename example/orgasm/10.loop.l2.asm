// Level-2 ORGASM for 10.loop.ng

.module default
.symbols [sum, n, s, i, assert, sum_recur]
.import assert, core
.const i32 10
.const i32 55
.val i32 0    // n
.val i32 1    // s
.val i32 2    // i

.fun sum
.param [n:i32]
00:    load_const.i32 0
01:    store_value.i32 val.1; s
02:    load_const.i32 0
03:    store_value.i32 val.2; i
04:    loop_start
05:    load_value.i32 val.2; i
06:    load_value.i32 val.0; n
07:    lt.i32
08:    br loop_body, loop_end
.label loop_body
09:    load_value.i32 val.1; s
10:    load_value.i32 val.2; i
11:    add.i32
12:    store_value.i32 val.1; s
13:    load_value.i32 val.2; i
14:    load_const.i32 1
15:    add.i32
16:    store_value.i32 val.2; i
17:    goto 05
.label loop_end
18:    load_value.i32 val.1; s
19:    return
.endfun

.start
00:    load_const.i32 const.0          // 10
01:    push_param
02:    call fun.0                      // sum
03:    load_const.i32 const.1          // 55
04:    eq.i32
05:    push_param
06:    call import.0                   // assert
07:    return
.end
.endmodule
