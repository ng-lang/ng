// Level-2 ORGASM for 13.import_std_prelude.ng

.module default
.symbols [assert, print, not]
.const bool false
.const bool true
.start
00:    load_const bool const.0; false
01:    not.bool
02:    call import.0; assert
03:    load_const bool const.0; false
04:    not.bool
05:    call import.2; not
06:    call import.1; print
.end
.endmodule
