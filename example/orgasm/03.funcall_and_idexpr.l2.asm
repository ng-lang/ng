// Level-2 ORGASM for 03.funcall_and_idexpr.ng

.module default
.symbols [fact, n, fact5, print, assert]
.import print, core
.import assert, core
.const i32 0
.const i32 1
.const i32 5
.const i32 120
.val i32 0    // fact5

.fun fact
.param [n:i32]
00:    load_param.i32 param.0    // n
01:    load_const.i32 const.0    // 0
02:    gt.i32
03:    br fact_01, fact_02; +3, +9
.label fact_01
04:    load_param.i32 param.0    // n
05:    load_param.i32 param.0    // n
06:    load_const.i32 const.1    // 1
07:    subtract.i32
08:    push_param
09:    call fun.0                  // fact
10:    multiply.i32
11:    return
.label fact_02
12:    load_const.i32 const.1    // 1
13:    return
.endfun

.start
00:    load_const.i32 const.2      // 5
01:    push_param
02:    call fun.0                  // fact
03:    store_value.i32 val.0       // fact5
04:    load_value.i32 val.0        // fact5
05:    load_const.i32 const.3      // 120
06:    eq.i32
07:    push_param
08:    call import.1               // assert
09:    load_value.i32 val.0        // fact5
10:    push_param
11:    call import.0               // print
12:    return
.end

.endmodule
