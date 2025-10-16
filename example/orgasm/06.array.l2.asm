// Level-2 ORGASM for 06.array.ng

.module default
.symbols [arr, twoDimArr, print, assert]
.import print, core
.import assert, core

// === Constants Section ===
.const i32 0
.const i32 1
.const i32 2
.const i32 3
.const i32 4
.const i32 5
.const i32 6
.const i32 7

// === Arrays Section ===
.array i32 [1, 2, 3, 4, 5] [align=16]
.array i32 [1, 2, 3, 4] [align=16]    // Flattened: [1, 2], [3, 4] -> [1, 2, 3, 4]

// === Variables Section ===
.val addr 0    // arr
.val addr 1    // twoDimArr

.start
00:    load_array.i32 array.0        // [1, 2, 3, 4, 5]
01:    store_value.addr val.0        // arr
02:    load_value.addr val.0         // arr
03:    push_param
04:    call import.0                 // print
05:
06:    // print(arr[3]) - should print 4
07:    load_array.i32 array.0        // direct array access
08:    load_const.i32 const.2        // index 3
09:    calculate_offset.i32          // calculate element offset
10:    load_element.i32              // load arr[3]
11:    push_param
12:    call import.0                 // print
13:
14:    // assert(arr[3] == 4)
15:    load_array.i32 array.0
16:    load_const.i32 const.2        // index 3
17:    calculate_offset.i32
18:    load_element.i32              // arr[3]
19:    load_const.i32 const.3        // 4
20:    eq.i32
21:    push_param
22:    call import.1                 // assert
23:
24:    // assert(arr[4] == 5)
25:    load_array.i32 array.0
26:    load_const.i32 const.3        // index 4
27:    calculate_offset.i32
28:    load_element.i32              // arr[4]
29:    load_const.i32 const.4        // 5
30:    eq.i32
31:    push_param
32:    call import.1                 // assert
33:
34:    // arr[4] := 6 (assignment)
35:    load_array.i32 array.0
36:    load_const.i32 const.3        // index 4
37:    calculate_offset.i32
38:    load_const.i32 const.5        // 6
39:    store_element.i32             // arr[4] = 6
40:
41:    // assert(arr[4] == 6)
42:    load_array.i32 array.0
43:    load_const.i32 const.3        // index 4
44:    calculate_offset.i32
45:    load_element.i32              // arr[4]
46:    load_const.i32 const.5        // 6
47:    eq.i32
48:    push_param
49:    call import.1                 // assert
50:
51:    // arr << 7 (append) - create new array
52:    load_array.i32 array.0        // original array
53:    load_const.i32 const.6        // 7 (element to append)
54:    array_append.i32              // create new array with appended element
55:    store_value.addr val.0        // update arr reference
56:
57:    // assert(arr[5] == 7)
58:    load_value.addr val.0         // updated arr
59:    load_const.i32 const.5        // index 5
60:    calculate_offset.i32
61:    load_element.i32              // arr[5]
62:    load_const.i32 const.7        // 7
63:    eq.i32
64:    push_param
65:    call import.1                 // assert
66:
67:    // print(arr)
68:    load_value.addr val.0         // updated arr
69:    push_param
70:    call import.0                 // print
71:
72:    // print(arr[5])
73:    load_value.addr val.0
74:    load_const.i32 const.5        // index 5
75:    calculate_offset.i32
76:    load_element.i32              // arr[5]
77:    push_param
78:    call import.0                 // print
79:
80:    // twoDimArr = [[1, 2], [3, 4]]
81:    load_array.i32 array.1        // flattened [1, 2, 3, 4]
82:    store_value.addr val.1        // twoDimArr
83:
84:    // assert(twoDimArr[0][0] == 1) -> assert(flattened[0] == 1)
85:    load_array.i32 array.1
86:    load_const.i32 const.1        // index 0
87:    calculate_offset.i32
88:    load_element.i32              // twoDimArr[0][0] equivalent
89:    load_const.i32 const.1        // 1
90:    eq.i32
91:    push_param
92:    call import.1                 // assert
93:
94:    // assert(twoDimArr[1][1] == 4) -> assert(flattened[3] == 4)
95:    load_array.i32 array.1
96:    load_const.i32 const.3        // index 3 (row 1, col 1 in flattened)
97:    calculate_offset.i32
98:    load_element.i32              // twoDimArr[1][1] equivalent
99:    load_const.i32 const.4        // 4
100:    eq.i32
101:    push_param
102:    call import.1                 // assert
103:
104:    // print(twoDimArr)
105:    load_value.addr val.1         // twoDimArr
106:    push_param
107:    call import.0                 // print
108:
109:    // print(twoDimArr[1][0]) -> print(flattened[2])
110:    load_value.addr val.1
111:    load_const.i32 const.2        // index 2 (row 1, col 0 in flattened)
112:    calculate_offset.i32
113:    load_element.i32              // twoDimArr[1][0] equivalent
114:    push_param
115:    call import.0                 // print
116:    return
.end
.endmodule
