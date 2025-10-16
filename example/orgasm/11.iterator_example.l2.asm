// Level-2 ORGASM for 11.iterator_example.ng

// NOTE: This is a high-level translation. Actual implementation may require more detailed type and method handling.

.module default
.symbols [None, Node, List, _next, content, node, size, append, hasNext, getNext, toString, print, printList, v, i, text]
.str [append]
.str [, ]
.str [ ]

.val addr 0    // List object reference


// Type: None
.type None
.endtype

// Type: Node
.type Node
.property _next
.property content
.method hasNext
.method getNext
.endtype

.method Node.hasNext
.param []
00:    load_self
01:    call_method _next
02:    is_type None
03:    not
04:    return
.endmethod

.method Node.getNext
.param []
00:    load_self
01:    call_method _next
02:    return
.endmethod

// Type: List
.type List
.property node
.property size
.method append
.method hasNext
.method getNext
.method toString
.endtype

// (Method implementations for List would be expanded here, but are omitted for brevity)

.start
00:    load_type List
01:    new_object
02:    load_type None
03:    new_object
04:    set_property node
05:    load_const.i32 0i32
06:    set_property size
07:    store_value.addr val.0; x

// x.append(1)
08:    load_value.addr val.0; x
09:    load_const.i32 1i32
10:    call_method append

// x.append(2)
11:    load_value.addr val.0; x
12:    load_const.i32 2i32
13:    call_method append

// x.append(3)
14:    load_value.addr val.0; x
15:    load_const.i32 3i32
16:    call_method append

// print(x)
17:    load_value.addr val.0; x
18:    call import.0; print

// print(x.toString())
19:    load_value.addr val.0; x
20:    call_method toString
21:    call import.0; print

// printList(x)
22:    load_value.addr val.0; x
23:    call_function printList

.end
.endmodule
