import{_ as a,o as n,c as e,a6 as i}from"./chunks/framework.DL_pga9_.js";const g=JSON.parse('{"title":"Traits","description":"","frontmatter":{},"headers":[],"relativePath":"guide/traits.md","filePath":"guide/traits.md","lastUpdated":1781686130000}'),t={name:"guide/traits.md"};function l(p,s,r,o,c,d){return n(),e("div",null,[...s[0]||(s[0]=[i(`<h1 id="traits" tabindex="-1">Traits <a class="header-anchor" href="#traits" aria-label="Permalink to “Traits”">​</a></h1><p>Traits are NG&#39;s mechanism for defining shared behavior across types. They are similar to interfaces in Java or type classes in Haskell.</p><h2 id="defining-a-trait" tabindex="-1">Defining a Trait <a class="header-anchor" href="#defining-a-trait" aria-label="Permalink to “Defining a Trait”">​</a></h2><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Show {</span></span>
<span class="line"><span>    fun show(self: ref&lt;Self&gt;) -&gt; string;</span></span>
<span class="line"><span>}</span></span></code></pre></div><ul><li><code>trait Show</code> — declares a trait named <code>Show</code></li><li><code>fun show(self: ref&lt;Self&gt;) -&gt; string</code> — declares a required method</li><li><code>Self</code> (capital S) is the type that implements this trait</li></ul><h3 id="traits-with-multiple-methods" tabindex="-1">Traits with Multiple Methods <a class="header-anchor" href="#traits-with-multiple-methods" aria-label="Permalink to “Traits with Multiple Methods”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Comparable {</span></span>
<span class="line"><span>    fun lessThan(self: ref&lt;Self&gt;, other: ref&lt;Self&gt;) -&gt; bool;</span></span>
<span class="line"><span>    fun equals(self: ref&lt;Self&gt;, other: ref&lt;Self&gt;) -&gt; bool;</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="implementing-a-trait" tabindex="-1">Implementing a Trait <a class="header-anchor" href="#implementing-a-trait" aria-label="Permalink to “Implementing a Trait”">​</a></h2><p>Use <code>impl &lt;Trait&gt; for &lt;Type&gt;</code>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point { x: i32; y: i32; }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Show for Point {</span></span>
<span class="line"><span>    fun show(self: ref&lt;Self&gt;) -&gt; string {</span></span>
<span class="line"><span>        return &quot;Point(&quot; + self.x + &quot;, &quot; + self.y + &quot;)&quot;;</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val p = new Point { x: 3, y: 4 };</span></span>
<span class="line"><span>print(p.show());  // &quot;Point(3, 4)&quot;</span></span></code></pre></div><h2 id="calling-trait-methods" tabindex="-1">Calling Trait Methods <a class="header-anchor" href="#calling-trait-methods" aria-label="Permalink to “Calling Trait Methods”">​</a></h2><p>Once implemented, trait methods are available on instances of the type:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val s: string = point.show();</span></span></code></pre></div><p>You can also call qualified trait methods to disambiguate:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>print(Show::show(point));   // qualified call</span></span></code></pre></div><h2 id="generic-trait-bounds" tabindex="-1">Generic Trait Bounds <a class="header-anchor" href="#generic-trait-bounds" aria-label="Permalink to “Generic Trait Bounds”">​</a></h2><p>Traits serve as <strong>bounds</strong> on generic type parameters, similar to interfaces in Java or constraints in Rust:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun printIt&lt;T: Show&gt;(value: ref&lt;T&gt;) {</span></span>
<span class="line"><span>    print(value.show());</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>printIt(point);   // works because Point implements Show</span></span></code></pre></div><h3 id="multiple-bounds" tabindex="-1">Multiple Bounds <a class="header-anchor" href="#multiple-bounds" aria-label="Permalink to “Multiple Bounds”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun compareAndShow&lt;T: Show + Comparable&gt;(a: ref&lt;T&gt;, b: ref&lt;T&gt;) {</span></span>
<span class="line"><span>    print(a.show());</span></span>
<span class="line"><span>    if (a.lessThan(b)) {</span></span>
<span class="line"><span>        print(&quot;less&quot;);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="supertraits" tabindex="-1">Supertraits <a class="header-anchor" href="#supertraits" aria-label="Permalink to “Supertraits”">​</a></h2><p>A trait can extend another trait, inheriting its requirements:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Show {</span></span>
<span class="line"><span>    fun show(self: ref&lt;Self&gt;) -&gt; string;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>trait Log: Show {                     // Log requires Show</span></span>
<span class="line"><span>    fun log(self: ref&lt;Self&gt;) -&gt; string {</span></span>
<span class="line"><span>        return &quot;[LOG] &quot; + self.show(); // uses show() from supertrait</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Show for Point { ... }</span></span>
<span class="line"><span>impl Log for Point { ... }             // must implement Show AND Log (or use defaults)</span></span></code></pre></div><h2 id="default-methods" tabindex="-1">Default Methods <a class="header-anchor" href="#default-methods" aria-label="Permalink to “Default Methods”">​</a></h2><p>Traits can provide default implementations:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Greeter {</span></span>
<span class="line"><span>    fun greet(self: ref&lt;Self&gt;) -&gt; string;</span></span>
<span class="line"><span>    fun greetLoudly(self: ref&lt;Self&gt;) -&gt; string {</span></span>
<span class="line"><span>        return self.greet() + &quot;!!!&quot;;   // default implementation using required method</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Greeter for Point {</span></span>
<span class="line"><span>    fun greet(self: ref&lt;Self&gt;) -&gt; string {</span></span>
<span class="line"><span>        return &quot;Hello from Point&quot;;</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    // greetLoudly uses the default</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>print(point.greetLoudly());  // &quot;Hello from Point!!!&quot;</span></span></code></pre></div><p>Implementations can override defaults:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>impl Greeter for Point {</span></span>
<span class="line"><span>    fun greet(self: ref&lt;Self&gt;) -&gt; string =&gt; &quot;Hello&quot;;</span></span>
<span class="line"><span>    fun greetLoudly(self: ref&lt;Self&gt;) -&gt; string =&gt; &quot;HELLO!!!&quot;;  // overrides default</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="inherent-methods-vs-trait-methods" tabindex="-1">Inherent Methods vs Trait Methods <a class="header-anchor" href="#inherent-methods-vs-trait-methods" aria-label="Permalink to “Inherent Methods vs Trait Methods”">​</a></h2><p>Methods defined directly on a type (inherent) take precedence over trait methods with the same name:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Counter {</span></span>
<span class="line"><span>    value: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Inherent method</span></span>
<span class="line"><span>fun get(self: ref&lt;Counter&gt;) -&gt; i32 =&gt; self.value;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>trait Accessor {</span></span>
<span class="line"><span>    fun get(self: ref&lt;Self&gt;) -&gt; i32 =&gt; 0;  // default</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Accessor for Counter {</span></span>
<span class="line"><span>    // The inherent get() takes precedence over trait get()</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val c = new Counter { value: 42 };</span></span>
<span class="line"><span>print(c.get());  // 42 (inherent method wins)</span></span></code></pre></div><h2 id="trait-objects-ref-dyn-trait" tabindex="-1">Trait Objects: <code>ref dyn Trait</code> <a class="header-anchor" href="#trait-objects-ref-dyn-trait" aria-label="Permalink to “Trait Objects: ref dyn Trait”">​</a></h2><p>Trait objects enable <strong>dynamic dispatch</strong> for heterogeneous collections and interfaces:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Show { fun show(self: ref&lt;Self&gt;) -&gt; string; }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>type Point { x: i32; y: i32; }</span></span>
<span class="line"><span>impl Show for Point { ... }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>type Circle { radius: i32; }</span></span>
<span class="line"><span>impl Show for Circle { ... }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val items: [ref dyn Show] = [</span></span>
<span class="line"><span>    new Point { x: 1, y: 2 },</span></span>
<span class="line"><span>    new Circle { radius: 5 }</span></span>
<span class="line"><span>];</span></span>
<span class="line"><span></span></span>
<span class="line"><span>loop item in items {</span></span>
<span class="line"><span>    print(item.show());  // dynamic dispatch</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="object-safety" tabindex="-1">Object Safety <a class="header-anchor" href="#object-safety" aria-label="Permalink to “Object Safety”">​</a></h3><p>A trait is <strong>object-safe</strong> if all its methods:</p><ul><li>Take <code>self</code> by <code>ref&lt;Self&gt;</code></li><li>Do not reference <code>Self</code> in non-receiver positions (return types, generic parameters)</li></ul><h2 id="copy-clone-and-drop" tabindex="-1">Copy, Clone, and Drop <a class="header-anchor" href="#copy-clone-and-drop" aria-label="Permalink to “Copy, Clone, and Drop”">​</a></h2><p>These are built-in marker traits for defining value semantics:</p><h3 id="copy" tabindex="-1">Copy <a class="header-anchor" href="#copy" aria-label="Permalink to “Copy”">​</a></h3><p>Types that are trivially copyable can derive <code>Copy</code>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point: derive(Copy) {</span></span>
<span class="line"><span>    x: i32;</span></span>
<span class="line"><span>    y: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Point can now be copied implicitly</span></span>
<span class="line"><span>val a = Point { x: 1, y: 2 };</span></span>
<span class="line"><span>val b = a;        // implicit copy (not move)</span></span></code></pre></div><h3 id="clone" tabindex="-1">Clone <a class="header-anchor" href="#clone" aria-label="Permalink to “Clone”">​</a></h3><p>Requires an explicit <code>.clone()</code> method:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point: derive(Copy + Clone) {</span></span>
<span class="line"><span>    x: i32;</span></span>
<span class="line"><span>    y: i32;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val a = Point { x: 1, y: 2 };</span></span>
<span class="line"><span>val b = a.clone();  // explicit clone</span></span></code></pre></div><h3 id="drop" tabindex="-1">Drop <a class="header-anchor" href="#drop" aria-label="Permalink to “Drop”">​</a></h3><p>When a value goes out of scope, its <code>Drop</code> implementation runs:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>trait Drop {</span></span>
<span class="line"><span>    fun drop(self: ref&lt;Self&gt;) -&gt; unit;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>type FileHandle = i32;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>impl Drop for FileHandle {</span></span>
<span class="line"><span>    fun drop(self: ref&lt;Self&gt;) {</span></span>
<span class="line"><span>        nativeClose(self);  // cleanup</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><blockquote><p><strong>Note:</strong> <code>Copy</code> and <code>Drop</code> are mutually exclusive — if you implement <code>Drop</code>, you cannot derive <code>Copy</code>.</p></blockquote><h2 id="derive" tabindex="-1">Derive <a class="header-anchor" href="#derive" aria-label="Permalink to “Derive”">​</a></h2><p>The <code>derive</code> attribute auto-implements standard traits:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Point: derive(Copy + Clone) {</span></span>
<span class="line"><span>    x: i32;</span></span>
<span class="line"><span>    y: i32;</span></span>
<span class="line"><span>}</span></span></code></pre></div><p>Supported derivable traits:</p><ul><li><code>Copy</code> — implicit copy semantics</li><li><code>Clone</code> — explicit <code>.clone()</code> method</li></ul><p>Derive generates correct implementations only for structural types (not tagged unions with certain payload types).</p><h2 id="auto-traits" tabindex="-1">Auto Traits <a class="header-anchor" href="#auto-traits" aria-label="Permalink to “Auto Traits”">​</a></h2><p>Auto traits are <strong>automatically implemented</strong> for all types that satisfy their structural requirements:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>auto trait Send;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// All types automatically satisfy Send, unless explicitly opted out</span></span>
<span class="line"><span>type NotSend { ... }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// impl !Send for NotSend;</span><span>  // exclude (future feature)</span></span></code></pre></div><p>Auto traits cannot have methods — they are purely marker traits.</p><h2 id="explicit-impl-selection-use-impl" tabindex="-1">Explicit Impl Selection: <code>use impl</code> <a class="header-anchor" href="#explicit-impl-selection-use-impl" aria-label="Permalink to “Explicit Impl Selection: use impl”">​</a></h2><p>When multiple implementations exist for the same trait on the same type, use <code>use impl</code> to select:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>use impl Show for Point;  // explicitly selects which impl to use</span></span></code></pre></div><p>This is especially useful for module-qualified impls:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>use impl my_module::Show for Point;</span></span></code></pre></div><h2 id="module-level-impls" tabindex="-1">Module-Level Impls <a class="header-anchor" href="#module-level-impls" aria-label="Permalink to “Module-Level Impls”">​</a></h2><p>Traits can be implemented for foreign types in your module. The impl is visible when your module is imported:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// my_ext.ng</span></span>
<span class="line"><span>export impl Show for i32 {</span></span>
<span class="line"><span>    fun show(self: ref&lt;Self&gt;) -&gt; string =&gt; &quot;custom int: &quot; + *self;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// main.ng</span></span>
<span class="line"><span>import my_ext (*);</span></span>
<span class="line"><span>print(42.show());  // uses the custom Show impl</span></span></code></pre></div><h2 id="what-s-next" tabindex="-1">What&#39;s Next? <a class="header-anchor" href="#what-s-next" aria-label="Permalink to “What&#39;s Next?”">​</a></h2><p>Continue to <a href="./type-system-in-depth">Type System in Depth</a> for a deeper exploration of NG&#39;s type system.</p><blockquote><p><strong>Try it:</strong> <code>example/25.trait_show.ng</code> — Basic trait <strong>Try it:</strong> <code>example/26.trait_generic_bound.ng</code> — Generic trait bounds <strong>Try it:</strong> <code>example/27.trait_receiver_ref.ng</code> — Trait with ref receiver <strong>Try it:</strong> <code>example/28.trait_supertraits.ng</code> — Supertraits <strong>Try it:</strong> <code>example/29.trait_qualified_call.ng</code> — Qualified trait calls <strong>Try it:</strong> <code>example/30.trait_inherent_precedence.ng</code> — Inherent vs trait method <strong>Try it:</strong> <code>example/31-33.trait_default*.ng</code> — Default methods, override, supertraits <strong>Try it:</strong> <code>example/34-36.trait_object*.ng</code> — Trait objects <strong>Try it:</strong> <code>example/37.copy_marker.ng</code> — Copy marker trait <strong>Try it:</strong> <code>example/38.clone_trait.ng</code> — Clone trait <strong>Try it:</strong> <code>example/39.drop_raii.ng</code> — Drop / RAII <strong>Try it:</strong> <code>example/55.auto_derive_traits.ng</code> — Auto traits and derive</p></blockquote>`,70)])])}const u=a(t,[["render",l]]);export{g as __pageData,u as default};
