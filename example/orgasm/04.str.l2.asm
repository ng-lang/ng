// Level-2 ORGASM for 04.str.ng

.module default
.symbols [hi, hellosize, print, opPlus, size]
.import print, core
.str [hello world]
.str [ how are you]

.fun hi
.param []
00:    load_str str.0                // [hello world]
01:    return
.endfun

.fun hellosize
.param []
00:    call fun.0                    // hi
01:    load_str str.1                // [ how are you]
02:    push_param
03:    push_self
04:    load_symbol symbol.3          // opPlus
05:    invoke_method
06:    push_self
07:    load_symbol symbol.4          // size
08:    invoke_method
09:    return
.endfun

.start
00:    call fun.0                    // hi
01:    push_param
02:    call import.0                 // print
03:    call fun.1                    // hellosize
04:    push_param
05:    call import.0                 // print
06:    return
.end
.endmodule
