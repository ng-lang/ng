import{_ as n,o as a,c as e,a6 as i}from"./chunks/framework.DL_pga9_.js";const g=JSON.parse('{"title":"Compile-Time Programming","description":"","frontmatter":{},"headers":[],"relativePath":"guide/compile-time-programming.md","filePath":"guide/compile-time-programming.md","lastUpdated":1781686130000}'),t={name:"guide/compile-time-programming.md"};function p(l,s,c,o,r,d){return a(),e("div",null,[...s[0]||(s[0]=[i(`<h1 id="compile-time-programming" tabindex="-1">Compile-Time Programming <a class="header-anchor" href="#compile-time-programming" aria-label="Permalink to “Compile-Time Programming”">​</a></h1><p>NG offers powerful compile-time metaprogramming features: <code>const if</code>, <code>typeof</code>, <strong>const predicates</strong>, <strong>const functions</strong>, and <strong>const generic parameters</strong>. These allow you to write code that executes during compilation.</p><h2 id="const-if" tabindex="-1">Const If <a class="header-anchor" href="#const-if" aria-label="Permalink to “Const If”">​</a></h2><p><code>const if</code> evaluates a condition at compile time and <strong>eliminates the dead branch entirely</strong>. No code is generated for the eliminated branch.</p><h3 id="basic-usage" tabindex="-1">Basic Usage <a class="header-anchor" href="#basic-usage" aria-label="Permalink to “Basic Usage”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const if (true) {</span></span>
<span class="line"><span>    val x = 42;      // Always compiled</span></span>
<span class="line"><span>    assert(x == 42);</span></span>
<span class="line"><span>} else {</span></span>
<span class="line"><span>    val x = 0;       // Never compiled — OK to be invalid</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>const if (false) {</span></span>
<span class="line"><span>    val x = 999;     // Eliminated</span></span>
<span class="line"><span>} else {</span></span>
<span class="line"><span>    val x = 7;</span></span>
<span class="line"><span>    assert(x == 7);</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="without-else" tabindex="-1">Without Else <a class="header-anchor" href="#without-else" aria-label="Permalink to “Without Else”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const if (true) {</span></span>
<span class="line"><span>    print(&quot;This always runs&quot;);</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="negation-and-composition" tabindex="-1">Negation and Composition <a class="header-anchor" href="#negation-and-composition" aria-label="Permalink to “Negation and Composition”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const if (!false) {</span></span>
<span class="line"><span>    print(&quot;Negation works&quot;);</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="in-generic-functions" tabindex="-1">In Generic Functions <a class="header-anchor" href="#in-generic-functions" aria-label="Permalink to “In Generic Functions”">​</a></h3><p><code>const if</code> is especially useful inside generic functions for type-dependent behavior:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun process&lt;T&gt;(value: T) {</span></span>
<span class="line"><span>    const if (is_ref&lt;T&gt;) {</span></span>
<span class="line"><span>        print(&quot;Processing reference type&quot;);</span></span>
<span class="line"><span>    } else {</span></span>
<span class="line"><span>        print(&quot;Processing value type&quot;);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val x = 42;</span></span>
<span class="line"><span>process(x);                // &quot;Processing value type&quot;</span></span>
<span class="line"><span>process(ref x);            // &quot;Processing reference type&quot;</span></span></code></pre></div><h2 id="typeof-queries" tabindex="-1">Typeof Queries <a class="header-anchor" href="#typeof-queries" aria-label="Permalink to “Typeof Queries”">​</a></h2><p><code>typeof</code> provides compile-time type information. Combined with <code>const if</code>, it enables type-dependent code generation:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun describe&lt;T&gt;(value: T) {</span></span>
<span class="line"><span>    const if (is_ref&lt;T&gt;) {</span></span>
<span class="line"><span>        print(&quot;T is a reference&quot;);</span></span>
<span class="line"><span>    } else {</span></span>
<span class="line"><span>        print(&quot;T is a value type&quot;);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="const-predicates" tabindex="-1">Const Predicates <a class="header-anchor" href="#const-predicates" aria-label="Permalink to “Const Predicates”">​</a></h2><p>Const predicates are <code>const fun</code> declarations that return <code>bool</code> and can be used in <code>where</code> clauses for compile-time type filtering.</p><h3 id="defining-a-const-predicate" tabindex="-1">Defining a Const Predicate <a class="header-anchor" href="#defining-a-const-predicate" aria-label="Permalink to “Defining a Const Predicate”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// In prelude or your module</span></span>
<span class="line"><span>const fun is_integral&lt;T&gt;() -&gt; bool = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun onlyIntegral&lt;T: is_integral&lt;T&gt;()&gt;(value: T) {</span></span>
<span class="line"><span>    print(&quot;Integral:&quot;, value);</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="using-const-predicates-in-where-clauses" tabindex="-1">Using Const Predicates in Where Clauses <a class="header-anchor" href="#using-const-predicates-in-where-clauses" aria-label="Permalink to “Using Const Predicates in Where Clauses”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun constrained&lt;T&gt;(value: T) where is_ref&lt;T&gt;() {</span></span>
<span class="line"><span>    // Only available when T is a reference type</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="const-specialization" tabindex="-1">Const Specialization <a class="header-anchor" href="#const-specialization" aria-label="Permalink to “Const Specialization”">​</a></h2><p>Compose const predicates and where clauses for <strong>conditional compilation</strong>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const fun is_numeric&lt;T&gt;() -&gt; bool = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun add&lt;T&gt;(a: T, b: T) -&gt; T where is_numeric&lt;T&gt;() {</span></span>
<span class="line"><span>    return a + b;  // Only compiles for numeric types</span></span>
<span class="line"><span>}</span></span></code></pre></div><p>Specialization patterns allow multiple definitions with different constraints — the compiler picks the most specific match:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// Base implementation</span></span>
<span class="line"><span>fun describe&lt;T&gt;(value: T) -&gt; string {</span></span>
<span class="line"><span>    return &quot;unknown&quot;;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Specialization for numeric types</span></span>
<span class="line"><span>fun describe&lt;T&gt;(value: T) -&gt; string where is_numeric&lt;T&gt;() {</span></span>
<span class="line"><span>    return &quot;numeric&quot;;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Specialization for strings</span></span>
<span class="line"><span>fun describe&lt;T&gt;(value: T) -&gt; string where T is string {</span></span>
<span class="line"><span>    return &quot;string&quot;;</span></span>
<span class="line"><span>}</span></span></code></pre></div><h2 id="const-functions" tabindex="-1">Const Functions <a class="header-anchor" href="#const-functions" aria-label="Permalink to “Const Functions”">​</a></h2><p><code>const fun</code> declares a function that can be <strong>evaluated at compile time</strong>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const fun factorial(n: i32) -&gt; i32 {</span></span>
<span class="line"><span>    if (n == 0) {</span></span>
<span class="line"><span>        return 1;</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    return n * factorial(n - 1);</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// The result is computed at compile time</span></span>
<span class="line"><span>val result: i32 = factorial(5);   // computed during compilation</span></span></code></pre></div><h3 id="const-functions-in-const-if" tabindex="-1">Const Functions in Const If <a class="header-anchor" href="#const-functions-in-const-if" aria-label="Permalink to “Const Functions in Const If”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const fun isPositive(n: i32) -&gt; bool =&gt; n &gt; 0;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun example() {</span></span>
<span class="line"><span>    const if (isPositive(42)) {</span></span>
<span class="line"><span>        print(&quot;42 is positive&quot;);  // always compiled</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="recursive-const-functions" tabindex="-1">Recursive Const Functions <a class="header-anchor" href="#recursive-const-functions" aria-label="Permalink to “Recursive Const Functions”">​</a></h3><p>Const functions support recursive evaluation at compile time:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const fun fibonacci(n: i32) -&gt; i32 {</span></span>
<span class="line"><span>    if (n &lt;= 1) {</span></span>
<span class="line"><span>        return n;</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    return fibonacci(n - 1) + fibonacci(n - 2);</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val fib10 = fibonacci(10);  // computed at compile time</span></span></code></pre></div><h3 id="limitations" tabindex="-1">Limitations <a class="header-anchor" href="#limitations" aria-label="Permalink to “Limitations”">​</a></h3><ul><li>Const functions can only call other const functions</li><li>They cannot call native functions</li><li>They cannot use runtime I/O or side effects</li><li>They are evaluated by the STUPID interpreter during compilation</li></ul><h2 id="the-is-ref-t-predicate" tabindex="-1">The <code>is_ref&lt;T&gt;</code> Predicate <a class="header-anchor" href="#the-is-ref-t-predicate" aria-label="Permalink to “The is_ref&lt;T&gt; Predicate”">​</a></h2><p>Built into the prelude, <code>is_ref&lt;T&gt;</code> checks whether a type is a reference:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun showType&lt;T&gt;(value: T) {</span></span>
<span class="line"><span>    const if (is_ref&lt;T&gt;) {</span></span>
<span class="line"><span>        print(&quot;T is a reference type&quot;);</span></span>
<span class="line"><span>    } else {</span></span>
<span class="line"><span>        print(&quot;T is a value type&quot;);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val num = 42;</span></span>
<span class="line"><span>showType(num);          // &quot;T is a value type&quot;</span></span>
<span class="line"><span>showType(ref num);      // &quot;T is a reference type&quot;</span></span></code></pre></div><h2 id="const-generic-parameters" tabindex="-1">Const Generic Parameters <a class="header-anchor" href="#const-generic-parameters" aria-label="Permalink to “Const Generic Parameters”">​</a></h2><p>Type parameters can accept compile-time constant values:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun makeArray&lt;T, const N: i32&gt;() -&gt; [T; N] {</span></span>
<span class="line"><span>    val result: [T; N] = [];</span></span>
<span class="line"><span>    loop i = 0 {</span></span>
<span class="line"><span>        if (i &lt; N) {</span></span>
<span class="line"><span>            result &lt;&lt; T();</span></span>
<span class="line"><span>            next i + 1;</span></span>
<span class="line"><span>        }</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    return result;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val arr: [i32; 5] = makeArray&lt;i32, 5&gt;();</span></span></code></pre></div><p>This is used for fixed-size array types and dimension-dependent code.</p><h2 id="delete-specialization" tabindex="-1">Delete Specialization <a class="header-anchor" href="#delete-specialization" aria-label="Permalink to “Delete Specialization”">​</a></h2><p>The <code>delete</code> keyword eliminates specific generic instantiations:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// Accept all types</span></span>
<span class="line"><span>fun process&lt;T&gt;(value: T) { print(&quot;default&quot;); }</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Reject strings</span></span>
<span class="line"><span>delete fun process&lt;string&gt;(value: string);</span></span></code></pre></div><p>Deleted specializations produce compile-time errors for matching calls.</p><h2 id="putting-it-all-together" tabindex="-1">Putting It All Together <a class="header-anchor" href="#putting-it-all-together" aria-label="Permalink to “Putting It All Together”">​</a></h2><p>Here&#39;s a complete example combining const if, typeof, const predicates, and specialization:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const fun is_printable&lt;T&gt;() -&gt; bool = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>fun prettyPrint&lt;T&gt;(value: T) where is_printable&lt;T&gt;() {</span></span>
<span class="line"><span>    const if (is_ref&lt;T&gt;) {</span></span>
<span class="line"><span>        print(&quot;(ref) &quot;, *value);</span></span>
<span class="line"><span>    } else {</span></span>
<span class="line"><span>        print(&quot;(val) &quot;, value);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val x = 42;</span></span>
<span class="line"><span>prettyPrint(x);          // &quot;(val) 42&quot;</span></span>
<span class="line"><span>prettyPrint(ref x);      // &quot;(ref) 42&quot;</span></span></code></pre></div><h2 id="what-s-next" tabindex="-1">What&#39;s Next? <a class="header-anchor" href="#what-s-next" aria-label="Permalink to “What&#39;s Next?”">​</a></h2><p>Continue to <a href="./advanced-generics">Advanced Generics</a> for higher-kinded types, type specialization, and enhanced tuples.</p><blockquote><p><strong>Try it:</strong> <code>example/17.const_if.ng</code> — Const if basics <strong>Try it:</strong> <code>example/42.const_type_predicate.ng</code> — Const type predicates <strong>Try it:</strong> <code>example/43.const_specialization.ng</code> — Const specialization <strong>Try it:</strong> <code>example/45.native_constraints.ng</code> — Native constraints <strong>Try it:</strong> <code>example/46.const_trait_constraints.ng</code> — Const trait constraints <strong>Try it:</strong> <code>example/47.const_generic_instances.ng</code> — Const generic instances <strong>Try it:</strong> <code>example/53.const_fun.ng</code> — Const functions</p></blockquote>`,54)])])}const u=n(t,[["render",p]]);export{g as __pageData,u as default};
