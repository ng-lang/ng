// Level-2 ORGASM for 14.tuple.ng
// Type-Aware Memory-Based Tuple Design

.module default
.symbols [x, y, a, b, c, d, assert, print]
.import assert, core
.import print, core

// === Constants Section ===
.const i32 1
.const i32 0
.const i32 4
.const bool false
.const bool true

// === Strings Section ===
.str [hello]

// === Variables Section ===
.val addr 0    // x (tuple) - (i32, bool, string)
.val addr 1    // y (tuple) - (i32, i32, bool, string, i32)
.val addr 2    // d (tuple) - (string, i32)
.val i32 3     // a
.val i32 4     // b
.val bool 5    // c

.start
// ===== Create tuple x: (i32, bool, string) =====
// Total size = 4 + 1 + 8 = 13 bytes (aligned to 16)
// Memory layout: [i32:4][bool:1][string:8]
// Offsets:      [0    ][4    ][8     ]

00:    load_const.i32 16           // 16 bytes total (aligned)
01:    tuple_create
02:    store_value.addr val.0; x

// Set element 0 (i32) at offset 0
03:    load_value.addr val.0; x
04:    load_const.i32 const.0      // value 1
05:    tuple_set.i32 0             // set i32 at offset 0

// Set element 1 (bool) at offset 4
06:    load_value.addr val.0; x
07:    load_const.bool const.3     // value false
08:    tuple_set.bool 4            // set bool at offset 4

// Set element 2 (string/addr) at offset 8
09:    load_value.addr val.0; x
10:    load_str str.0              // string address
11:    tuple_set.addr 8            // set addr at offset 8

// ===== Verify tuple x contents =====

// Verify x[0] == 1 (i32 at offset 0)
12:    load_value.addr val.0; x
13:    tuple_get.i32 0             // get i32 from offset 0
14:    load_const.i32 const.0      // expected value 1
15:    eq.i32
16:    push_param
17:    call import.0; assert

// Verify x[1] == false (bool at offset 4)
18:    load_value.addr val.0; x
19:    tuple_get.bool 4            // get bool from offset 4
20:    load_const.bool const.3     // expected value false
21:    eq.bool
22:    push_param
23:    call import.0; assert

// Verify x[2] == "hello" (string at offset 8)
24:    load_value.addr val.0; x
25:    tuple_get.addr 8            // get addr from offset 8
26:    load_str str.0              // expected string
27:    eq.str
28:    push_param
29:    call import.0; assert

// ===== Create tuple y: (i32, i32, bool, string, i32) =====
// This implements y = (0, ...x, 4) where x = (1, false, "hello")
// Total size = 4 + 4 + 1 + 8 + 4 = 21 bytes (aligned to 24)
// Memory layout: [i32:4][i32:4][bool:1][string:8][i32:4]
// Offsets:      [0    ][4    ][8     ][12     ][20   ]

30:    load_const.i32 24           // 24 bytes total (aligned)
31:    tuple_create
32:    store_value.addr val.1; y

// Set element 0 (i32) at offset 0 - value 0
33:    load_value.addr val.1; y
34:    load_const.i32 const.1      // value 0
35:    tuple_set.i32 0             // set i32 at offset 0

// Set element 1 (i32) at offset 4 - from x[0] = 1
36:    load_value.addr val.1; y
37:    load_value.addr val.0; x    // get tuple x
38:    tuple_get.i32 0             // get x[0] (i32 at offset 0)
39:    tuple_set.i32 4             // set i32 at offset 4

// Set element 2 (bool) at offset 8 - from x[1] = false
40:    load_value.addr val.1; y
41:    load_value.addr val.0; x    // get tuple x
42:    tuple_get.bool 4            // get x[1] (bool at offset 4)
43:    tuple_set.bool 8            // set bool at offset 8

// Set element 3 (string) at offset 12 - from x[2] = "hello"
44:    load_value.addr val.1; y
45:    load_value.addr val.0; x    // get tuple x
46:    tuple_get.addr 8            // get x[2] (string at offset 8)
47:    tuple_set.addr 12           // set addr at offset 12

// Set element 4 (i32) at offset 20 - value 4
48:    load_value.addr val.1; y
49:    load_const.i32 const.2      // value 4
50:    tuple_set.i32 20            // set i32 at offset 20

// ===== Verify tuple y contents =====

// Verify y[0] == 0 (i32 at offset 0)
51:    load_value.addr val.1; y
52:    tuple_get.i32 0             // get i32 from offset 0
53:    load_const.i32 const.1      // expected value 0
54:    eq.i32
55:    push_param
56:    call import.0; assert

// Verify y[1] == 1 (i32 at offset 4, from x[0])
57:    load_value.addr val.1; y
58:    tuple_get.i32 4             // get i32 from offset 4
59:    load_const.i32 const.0      // expected value 1
60:    eq.i32
61:    push_param
62:    call import.0; assert

// Verify y[2] == false (bool at offset 8, from x[1])
63:    load_value.addr val.1; y
64:    tuple_get.bool 8            // get bool from offset 8
65:    load_const.bool const.3     // expected value false
66:    eq.bool
67:    push_param
68:    call import.0; assert

// Verify y[3] == "hello" (string at offset 12, from x[2])
69:    load_value.addr val.1; y
70:    tuple_get.addr 12           // get addr from offset 12
71:    load_str str.0              // expected string
72:    eq.str
73:    push_param
74:    call import.0; assert

// Verify y[4] == 4 (i32 at offset 20)
75:    load_value.addr val.1; y
76:    tuple_get.i32 20            // get i32 from offset 20
77:    load_const.i32 const.2      // expected value 4
78:    eq.i32
79:    push_param
80:    call import.0; assert

// ===== Tuple unpack: val (a, b, c, ...d) = y =====
// Extract first 3 elements into a, b, c
// Then create tuple d from remaining elements (last 2)

// Extract a = y[0] (i32)
81:    load_value.addr val.1; y
82:    tuple_get.i32 0             // get y[0] (i32 at offset 0)
83:    store_value.i32 val.3; a    // store in variable a

// Extract b = y[1] (i32)
84:    load_value.addr val.1; y
85:    tuple_get.i32 4             // get y[1] (i32 at offset 4)
86:    store_value.i32 val.4; b    // store in variable b

// Extract c = y[2] (bool)
87:    load_value.addr val.1; y
88:    tuple_get.bool 8            // get y[2] (bool at offset 8)
89:    store_value.bool val.5; c   // store in variable c

// Create tuple d from remaining elements: y[3] and y[4]
// d = (y[3], y[4]) = ("hello", 4) - type (string, i32)
// Total size = 8 + 4 = 12 bytes (aligned to 16)
90:    load_const.i32 16           // 16 bytes total (aligned)
91:    tuple_create
92:
93:    // Set element 0 (string from y[3])
94:    load_value.addr val.1; y
95:    tuple_get.addr 12           // get y[3] (string at offset 12)
96:    tuple_set.addr 0            // set addr at offset 0
97:
98:    // Set element 1 (i32 from y[4])
99:    load_value.addr val.1; y
100:    tuple_get.i32 20            // get y[4] (i32 at offset 20)
101:    tuple_set.i32 8             // set i32 at offset 8
102:
103:    // Store the new tuple in d
104:    store_value.addr val.2; d

// ===== Print results =====
105:    load_value.addr val.0; x
106:    push_param
107:    call import.1; print

108:    load_value.addr val.1; y
109:    push_param
110:    call import.1; print

111:    load_value.addr val.5; d
112:    push_param
113:    call import.1; print

114:    return
.end
.endmodule