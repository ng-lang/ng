// Level-2 ORGASM for 12.loop_max_stack.ng

.module default
.symbols [sum, n, s, i, sum2, sum3, assert, print]
.const 2969i32
.val n
.val s
.val i


.fun sum
.param [n:i32]
00:    load_const.i32 0i32
01:    store_value.i32 val.1; s
02:    load_const.i32 0i32
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
14:    load_const.i32 1i32
15:    add.i32
16:    store_value.i32 val.2; i
17:    goto 05
.label loop_end
18:    load_value.i32 val.1; s
19:    return
.endfun

.fun sum2
.param [i:i32, n:i32]
00:    load_value.i32 param.0; i
01:    load_const.i32 0i32
02:    eq.i32
03:    br sum2_base, sum2_recur
.label sum2_base
04:    load_value.i32 param.1; n
05:    return
.label sum2_recur
06:    load_value.i32 param.0; i
07:    load_const.i32 1i32
08:    subtract.i32
09:    push_param
10:    load_value.i32 param.1; n
11:    load_value.i32 param.0; i
12:    add.i32
13:    push_param
14:    call fun.1; sum2
15:    return
.endfun

.fun sum3
.param [i:i32, n:i32]
00:    load_value.i32 param.0; i
01:    load_const.i32 0i32
02:    eq.i32
03:    br sum3_base, sum3_recur
.label sum3_base
04:    load_value.i32 param.1; n
05:    return
.label sum3_recur
06:    load_value.i32 param.0; i
07:    load_const.i32 1i32
08:    subtract.i32
09:    push_param
10:    load_value.i32 param.1; n
11:    load_value.i32 param.0; i
12:    add.i32
13:    push_param
14:    call fun.2; sum3
15:    return
.endfun

.start
00:    load_const.i32 const.0; 2969i32
01:    store_value.i32 val.0; n
02:    load_value.i32 val.0; n
03:    call fun.0; sum
04:    load_value.i32 val.0; n
05:    call fun.1; sum2
06:    eq.i32
07:    call import.0; assert
08:    load_value.i32 val.0; n
09:    call fun.1; sum2
10:    load_value.i32 val.0; n
11:    call fun.2; sum3
12:    eq.i32
13:    call import.0; assert
14:    load_value.i32 val.0; n
15:    call fun.2; sum3
16:    call import.1; print
.end
.endmodule
