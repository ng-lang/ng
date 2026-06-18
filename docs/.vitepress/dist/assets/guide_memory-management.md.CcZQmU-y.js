import{_ as s,o as n,c as e,a6 as i}from"./chunks/framework.DL_pga9_.js";const g=JSON.parse('{"title":"Memory Management","description":"","frontmatter":{},"headers":[],"relativePath":"guide/memory-management.md","filePath":"guide/memory-management.md","lastUpdated":1781686130000}'),t={name:"guide/memory-management.md"};function p(l,a,r,o,c,d){return n(),e("div",null,[...a[0]||(a[0]=[i(`<h1 id="memory-management" tabindex="-1">Memory Management <a class="header-anchor" href="#memory-management" aria-label="Permalink to “Memory Management”">​</a></h1><p>NG uses automatic memory management with compile-time ownership tracking. This chapter explains how memory works in NG.</p><h2 id="value-semantics-stack" tabindex="-1">Value Semantics (Stack) <a class="header-anchor" href="#value-semantics-stack" aria-label="Permalink to “Value Semantics (Stack)”">​</a></h2><p>By default, values live on the <strong>stack</strong> and are copied on assignment:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val a = 42;</span></span>
<span class="line"><span>val b = a;    // a is copied into b (both on stack)</span></span></code></pre></div><p>Primitive types (<code>i32</code>, <code>f64</code>, <code>bool</code>, <code>unit</code>) and fixed-size arrays are always stack-allocated and cheap to copy.</p><h2 id="heap-allocation-with-new" tabindex="-1">Heap Allocation with <code>new</code> <a class="header-anchor" href="#heap-allocation-with-new" aria-label="Permalink to “Heap Allocation with new”">​</a></h2><p>The <code>new</code> keyword allocates on the <strong>managed heap</strong> and returns a <code>ref&lt;T&gt;</code>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point { x: i32; y: i32; }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val p = new Point { x: 1, y: 2 };</span></span>
<span class="line"><span>// p: ref&lt;Point&gt; — heap-allocated</span></span></code></pre></div><h3 id="reference-semantics" tabindex="-1">Reference Semantics <a class="header-anchor" href="#reference-semantics" aria-label="Permalink to “Reference Semantics”">​</a></h3><p>Heap objects are reference-counted. Assignment shares the reference:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val a = new Point { x: 1, y: 2 };</span></span>
<span class="line"><span>val b = a;         // b points to the same heap object</span></span>
<span class="line"><span>a.x = 10;</span></span>
<span class="line"><span>print(b.x);        // 10 (same object)</span></span></code></pre></div><h2 id="automatic-deallocation" tabindex="-1">Automatic Deallocation <a class="header-anchor" href="#automatic-deallocation" aria-label="Permalink to “Automatic Deallocation”">​</a></h2><p>When all references to a heap object go out of scope, the memory is automatically reclaimed:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun example() {</span></span>
<span class="line"><span>    val p = new Point { x: 1, y: 2 };</span></span>
<span class="line"><span>    // p is alive here</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span>// p goes out of scope → memory freed</span></span></code></pre></div><h2 id="garbage-collection-for-cycles" tabindex="-1">Garbage Collection for Cycles <a class="header-anchor" href="#garbage-collection-for-cycles" aria-label="Permalink to “Garbage Collection for Cycles”">​</a></h2><p>NG&#39;s managed heap includes a <strong>tracing garbage collector</strong> that detects and collects unreachable cycles:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Node {</span></span>
<span class="line"><span>    value: i32;</span></span>
<span class="line"><span>    next: ref&lt;Node&gt;;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun cycle() {</span></span>
<span class="line"><span>    val a = new Node { value: 1, next: null };</span></span>
<span class="line"><span>    val b = new Node { value: 2, next: a };</span></span>
<span class="line"><span>    a.next = b;   // cycle: a → b → a</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span>// When function exits, both a and b are unreachable</span></span>
<span class="line"><span>// The GC collects the cycle even with reference counting</span></span></code></pre></div><h3 id="gc-safety" tabindex="-1">GC Safety <a class="header-anchor" href="#gc-safety" aria-label="Permalink to “GC Safety”">​</a></h3><p>The GC runs automatically when:</p><ul><li>The heap grows beyond a threshold</li><li>Explicit collection is triggered</li><li>During idle time</li></ul><p>It traces from <strong>root references</strong> (globals, call frames, operand stacks) and sweeps unmarked cells.</p><h2 id="drop-and-raii" tabindex="-1">Drop and RAII <a class="header-anchor" href="#drop-and-raii" aria-label="Permalink to “Drop and RAII”">​</a></h2><p>Types implementing the <code>Drop</code> trait get a <strong>finalizer</strong> called when the value is deallocated:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Drop {</span></span>
<span class="line"><span>    fun drop(self: ref&lt;Self&gt;) -&gt; unit;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>type FileHandle {</span></span>
<span class="line"><span>    fd: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Drop for FileHandle {</span></span>
<span class="line"><span>    fun drop(self: ref&lt;Self&gt;) {</span></span>
<span class="line"><span>        nativeClose(self.fd);     // release OS resource</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun readFile(path: string) -&gt; FileHandle {</span></span>
<span class="line"><span>    val fd = nativeOpen(path);</span></span>
<span class="line"><span>    return FileHandle { fd: fd };</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span>// FileHandle.drop() is called automatically</span></span></code></pre></div><h3 id="drop-order" tabindex="-1">Drop Order <a class="header-anchor" href="#drop-order" aria-label="Permalink to “Drop Order”">​</a></h3><p>Drop runs when:</p><ul><li>A local variable goes out of scope</li><li>A heap object is collected by the GC</li><li>An object property is overwritten (old value is dropped)</li></ul><h3 id="rule-no-copy-drop" tabindex="-1">Rule: No Copy + Drop <a class="header-anchor" href="#rule-no-copy-drop" aria-label="Permalink to “Rule: No Copy + Drop”">​</a></h3><p>If a type implements <code>Drop</code>, it <strong>cannot</strong> derive <code>Copy</code>. This ensures that the finalizer runs exactly once:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Handle {</span></span>
<span class="line"><span>    fd: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Drop for Handle { ... }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// handle is moved, not copied</span></span>
<span class="line"><span>fun example() {</span></span>
<span class="line"><span>    val h1 = Handle { fd: 1 };</span></span>
<span class="line"><span>    val h2 = move h1;     // transfer ownership</span></span>
<span class="line"><span>    // h1.drop() does NOT run — h2.drop() runs when h2 goes out of scope</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="move-semantics" tabindex="-1">Move Semantics <a class="header-anchor" href="#move-semantics" aria-label="Permalink to “Move Semantics”">​</a></h2><p>Use <code>move</code> to transfer ownership without copying:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val arr = [1, 2, 3, 4, 5];</span></span>
<span class="line"><span>val moved = move arr;</span></span>
<span class="line"><span>// arr is now invalid — don&#39;t use it</span></span>
<span class="line"><span>print(moved[0]);  // OK</span></span></code></pre></div><h3 id="use-after-move-detection" tabindex="-1">Use-After-Move Detection <a class="header-anchor" href="#use-after-move-detection" aria-label="Permalink to “Use-After-Move Detection”">​</a></h3><p>The runtime detects use-after-move and raises an error:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val x = [1, 2, 3];</span></span>
<span class="line"><span>val y = move x;</span></span>
<span class="line"><span>// print(x[0]);</span><span>   // Runtime error: use after move</span></span>
<span class="line"><span>x = [4, 5, 6];    // Reassign — OK now</span></span>
<span class="line"><span>print(x[0]);      // OK</span></span></code></pre></div><h3 id="move-in-function-arguments" tabindex="-1">Move in Function Arguments <a class="header-anchor" href="#move-in-function-arguments" aria-label="Permalink to “Move in Function Arguments”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun takeOwnership(arr: [i32]) {</span></span>
<span class="line"><span>    // arr owns the data</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val data = [1, 2, 3];</span></span>
<span class="line"><span>takeOwnership(move data);   // transfer ownership</span></span>
<span class="line"><span>// data is now invalid</span></span></code></pre></div><h2 id="partial-moves" tabindex="-1">Partial Moves <a class="header-anchor" href="#partial-moves" aria-label="Permalink to “Partial Moves”">​</a></h2><p>Individual fields of an object can be moved independently:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Person { name: string; age: i32; }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val p = Person { name: &quot;Alice&quot;, age: 30 };</span></span>
<span class="line"><span>val name = move p.name;     // only name is moved</span></span>
<span class="line"><span>// print(p.name);</span><span>            // ERROR: partially moved</span></span>
<span class="line"><span>print(p.age);               // OK: age is still valid</span></span>
<span class="line"><span></span></span>
<span class="line"><span>p.name = &quot;Bob&quot;;             // restore the field</span></span>
<span class="line"><span>print(p.name);              // OK now</span></span></code></pre></div><h3 id="nested-partial-moves" tabindex="-1">Nested Partial Moves <a class="header-anchor" href="#nested-partial-moves" aria-label="Permalink to “Nested Partial Moves”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Inner { x: i32; y: i32; }</span></span>
<span class="line"><span>type Outer { inner: Inner; }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val o = Outer { inner: Inner { x: 1, y: 2 } };</span></span>
<span class="line"><span>val x = move o.inner.x;</span></span>
<span class="line"><span>print(o.inner.y);   // OK: y is still accessible</span></span>
<span class="line"><span>// print(o.inner.x);</span><span> // ERROR: partially moved</span></span></code></pre></div><h2 id="smart-pointers" tabindex="-1">Smart Pointers <a class="header-anchor" href="#smart-pointers" aria-label="Permalink to “Smart Pointers”">​</a></h2><p>The <code>std.memory</code> module provides smart pointer types:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>import std.memory;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Unique ownership</span></span>
<span class="line"><span>val ptr: UniquePtr&lt;i32&gt; = nativeMalloc(8);</span></span>
<span class="line"><span>// ptr is automatically freed via Drop</span></span></code></pre></div><h2 id="clone" tabindex="-1">Clone <a class="header-anchor" href="#clone" aria-label="Permalink to “Clone”">​</a></h2><p>For types deriving <code>Clone</code>, use <code>.clone()</code> for explicit deep copies:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point: derive(Clone) {</span></span>
<span class="line"><span>    x: i32;</span></span>
<span class="line"><span>    y: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val a = Point { x: 1, y: 2 };</span></span>
<span class="line"><span>val b = a.clone();   // explicit deep copy</span></span></code></pre></div><h2 id="memory-safety-summary" tabindex="-1">Memory Safety Summary <a class="header-anchor" href="#memory-safety-summary" aria-label="Permalink to “Memory Safety Summary”">​</a></h2><table tabindex="0"><thead><tr><th>Mechanism</th><th>Enforced By</th><th>Purpose</th></tr></thead><tbody><tr><td>Value semantics</td><td>Compiler</td><td>Default copy behavior</td></tr><tr><td>Reference counting</td><td>Runtime</td><td>Shared heap ownership</td></tr><tr><td>GC cycle detection</td><td>Runtime</td><td>Collect cyclic garbage</td></tr><tr><td>Use-after-move check</td><td>Runtime</td><td>Prevent invalid access</td></tr><tr><td>Partial move tracking</td><td>Type checker</td><td>Field-level ownership</td></tr><tr><td>Drop finalizers</td><td>Runtime</td><td>Resource cleanup</td></tr><tr><td>No Copy + Drop</td><td>Type checker</td><td>Ensure unique finalization</td></tr></tbody></table><h2 id="what-s-next" tabindex="-1">What&#39;s Next? <a class="header-anchor" href="#what-s-next" aria-label="Permalink to “What&#39;s Next?”">​</a></h2><p>Continue to <a href="./imgui-integration">ImGui Integration</a> for GUI programming with NG.</p><blockquote><p><strong>Try it:</strong> <code>example/39.drop_raii.ng</code> — Drop and RAII <strong>Try it:</strong> <code>example/41.drop_smart_pointer.ng</code> — Smart pointers with Drop <strong>Try it:</strong> <code>example/50.partial_move.ng</code> — Partial move semantics <strong>Try it:</strong> <code>example/51.partial_move_drop.ng</code> — Partial moves with Drop</p></blockquote>`,55)])])}const m=s(t,[["render",p]]);export{g as __pageData,m as default};
