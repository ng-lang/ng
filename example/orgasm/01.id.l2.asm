// Level-2 ORGASM for 01.id.ng

.module default
.symbols [id, n, print]
.import print, core
.const i32 1
.start
00:    load_const.i32 const.0    // 1
01:    push_param
02:    call fun.0                // id
03:    push_param
04:    call import.0             // print
05:    return
.fun id
.param [n:i32]
00:    load_param.i32 param.0    // n
01:    return
.endfun id
.endmodule
