import{_ as a,o as n,c as e,a6 as i}from"./chunks/framework.DL_pga9_.js";const g=JSON.parse('{"title":"Control Flow","description":"","frontmatter":{},"headers":[],"relativePath":"guide/control-flow.md","filePath":"guide/control-flow.md","lastUpdated":1781686130000}'),t={name:"guide/control-flow.md"};function p(l,s,o,c,r,h){return n(),e("div",null,[...s[0]||(s[0]=[i(`<h1 id="control-flow" tabindex="-1">Control Flow <a class="header-anchor" href="#control-flow" aria-label="Permalink to “Control Flow”">​</a></h1><p>This chapter covers conditional execution, loops, and pattern matching in NG.</p><h2 id="conditional-execution-if-else" tabindex="-1">Conditional Execution: <code>if / else</code> <a class="header-anchor" href="#conditional-execution-if-else" aria-label="Permalink to “Conditional Execution: if / else”">​</a></h2><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val x = 10;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>if (x &gt; 0) {</span></span>
<span class="line"><span>    print(&quot;positive&quot;);</span></span>
<span class="line"><span>} else if (x &lt; 0) {</span></span>
<span class="line"><span>    print(&quot;negative&quot;);</span></span>
<span class="line"><span>} else {</span></span>
<span class="line"><span>    print(&quot;zero&quot;);</span></span>
<span class="line"><span>}</span></span></code></pre></div><p>The condition must be a <code>bool</code> expression. Parentheses around the condition are required.</p><h3 id="expression-vs-statement" tabindex="-1">Expression vs Statement <a class="header-anchor" href="#expression-vs-statement" aria-label="Permalink to “Expression vs Statement”">​</a></h3><p><code>if/else</code> is an expression that produces a value. You can use it in assignments:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val x = 10;</span></span>
<span class="line"><span>val sign = if (x &gt; 0) { &quot;positive&quot; } else { &quot;negative&quot; };</span></span>
<span class="line"><span>print(sign);  // &quot;positive&quot;</span></span></code></pre></div><p>When used as an expression, both branches must return the same type.</p><h2 id="loops-loop-next" tabindex="-1">Loops: <code>loop / next</code> <a class="header-anchor" href="#loops-loop-next" aria-label="Permalink to “Loops: loop / next”">​</a></h2><p>NG has a single loop construct called <code>loop</code> with explicit <code>next</code> for continuation.</p><h3 id="basic-loop" tabindex="-1">Basic Loop <a class="header-anchor" href="#basic-loop" aria-label="Permalink to “Basic Loop”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>loop {</span></span>
<span class="line"><span>    print(&quot;infinite loop&quot;);</span></span>
<span class="line"><span>    next;  // continue to next iteration</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="loop-with-counter-variable" tabindex="-1">Loop with Counter Variable <a class="header-anchor" href="#loop-with-counter-variable" aria-label="Permalink to “Loop with Counter Variable”">​</a></h3><p>The loop variable is initialized and updated on each <code>next</code>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>fun sum(n: i32) -&gt; i32 {</span></span>
<span class="line"><span>    val s = 0;</span></span>
<span class="line"><span>    loop i = 0 {</span></span>
<span class="line"><span>        s = s + i;</span></span>
<span class="line"><span>        if (i &lt; n) {</span></span>
<span class="line"><span>            next i + 1;   // next iteration with new i</span></span>
<span class="line"><span>        }</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    return s;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>print(sum(10));  // 45</span></span></code></pre></div><h3 id="loop-with-multiple-variables" tabindex="-1">Loop with Multiple Variables <a class="header-anchor" href="#loop-with-multiple-variables" aria-label="Permalink to “Loop with Multiple Variables”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>loop i = 0, j = 10 {</span></span>
<span class="line"><span>    if (i &lt; j) {</span></span>
<span class="line"><span>        next i + 1, j - 1;</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="loops-return-values" tabindex="-1">Loops Return Values <a class="header-anchor" href="#loops-return-values" aria-label="Permalink to “Loops Return Values”">​</a></h3><p>A loop terminates when <code>next</code> is not called — execution falls through to the statement after the loop body. The loop itself can also produce a value:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val result = loop i = 0 {</span></span>
<span class="line"><span>    if (i &gt;= 10) {</span></span>
<span class="line"><span>        break i;   // exit with value</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    next i + 1;</span></span>
<span class="line"><span>};</span></span>
<span class="line"><span>// result == 10</span></span></code></pre></div><blockquote><p><strong>Note:</strong> <code>break</code> with a value is used to exit the loop early and produce a result.</p></blockquote><h3 id="max-loop-stack" tabindex="-1">Max Loop Stack <a class="header-anchor" href="#max-loop-stack" aria-label="Permalink to “Max Loop Stack”">​</a></h3><p>NG protects against infinite recursion in loops via a configurable max stack depth. See <code>example/12.loop_max_stack.ng</code>.</p><h2 id="pattern-matching-switch" tabindex="-1">Pattern Matching: <code>switch</code> <a class="header-anchor" href="#pattern-matching-switch" aria-label="Permalink to “Pattern Matching: switch”">​</a></h2><p>The <code>switch</code> statement performs exhaustive pattern matching on tagged unions.</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>type Result = Ok(value: i32) | Err(msg: string);</span></span>
<span class="line"><span></span></span>
<span class="line"><span>val result = Ok(42);</span></span>
<span class="line"><span></span></span>
<span class="line"><span>switch (result) {</span></span>
<span class="line"><span>    case Ok(value) {</span></span>
<span class="line"><span>        print(&quot;Success:&quot;, value);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    case Err(msg) {</span></span>
<span class="line"><span>        print(&quot;Failure:&quot;, msg);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="the-otherwise-branch" tabindex="-1">The <code>otherwise</code> Branch <a class="header-anchor" href="#the-otherwise-branch" aria-label="Permalink to “The otherwise Branch”">​</a></h3><p>The <code>otherwise</code> branch catches any variant not explicitly handled:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>switch (result) {</span></span>
<span class="line"><span>    case Ok(value) {</span></span>
<span class="line"><span>        print(&quot;Success:&quot;, value);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>    otherwise {</span></span>
<span class="line"><span>        print(&quot;Something else happened&quot;);</span></span>
<span class="line"><span>    }</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="switch-exhaustiveness" tabindex="-1">Switch Exhaustiveness <a class="header-anchor" href="#switch-exhaustiveness" aria-label="Permalink to “Switch Exhaustiveness”">​</a></h3><p>If you omit a variant and don&#39;t provide <code>otherwise</code>, the type checker reports an error. All variants must be covered.</p><h3 id="member-access-on-tagged-unions" tabindex="-1">Member Access on Tagged Unions <a class="header-anchor" href="#member-access-on-tagged-unions" aria-label="Permalink to “Member Access on Tagged Unions”">​</a></h3><p>You can inspect the active variant at runtime:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>print(result.tag);    // &quot;Ok&quot;</span></span>
<span class="line"><span>print(result.index);  // 0</span></span></code></pre></div><h2 id="iteration-with-ranges" tabindex="-1">Iteration with Ranges <a class="header-anchor" href="#iteration-with-ranges" aria-label="Permalink to “Iteration with Ranges”">​</a></h2><p>Ranges provide a way to iterate over sequences without explicit loop variables:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// Inclusive range</span></span>
<span class="line"><span>loop i in 0..=5 {</span></span>
<span class="line"><span>    print(i);  // 0, 1, 2, 3, 4, 5</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Exclusive range</span></span>
<span class="line"><span>loop i in 0..5 {</span></span>
<span class="line"><span>    print(i);  // 0, 1, 2, 3, 4</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Descending range</span></span>
<span class="line"><span>loop i in 5..0 {</span></span>
<span class="line"><span>    print(i);  // 5, 4, 3, 2, 1</span></span>
<span class="line"><span>}</span></span></code></pre></div><h3 id="range-expressions" tabindex="-1">Range Expressions <a class="header-anchor" href="#range-expressions" aria-label="Permalink to “Range Expressions”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>val r1 = 0..10;        // exclusive: 0 to 9</span></span>
<span class="line"><span>val r2 = 0..=10;       // inclusive: 0 to 10</span></span>
<span class="line"><span>val r3 = 10..0;        // descending</span></span></code></pre></div><h2 id="const-if-compile-time" tabindex="-1">Const If (Compile-Time) <a class="header-anchor" href="#const-if-compile-time" aria-label="Permalink to “Const If (Compile-Time)”">​</a></h2><p>NG supports compile-time conditional evaluation with <code>const if</code>:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>const if (true) {</span></span>
<span class="line"><span>    val x = 42;        // Always compiled</span></span>
<span class="line"><span>} else {</span></span>
<span class="line"><span>    val x = 0;         // Eliminated at compile time</span></span>
<span class="line"><span>}</span></span></code></pre></div><p>The condition must be evaluable at compile time (a literal or <code>typeof</code> query). The dead branch is <strong>entirely removed</strong> — no code is generated for it.</p><p>See <a href="./compile-time-programming">Compile-Time Programming</a> for more details.</p><h2 id="what-s-next" tabindex="-1">What&#39;s Next? <a class="header-anchor" href="#what-s-next" aria-label="Permalink to “What&#39;s Next?”">​</a></h2><p>Continue to <a href="./functions">Functions</a> to learn how to define and use functions in NG.</p><blockquote><p><strong>Try it:</strong> <code>example/10.loop.ng</code> — Loop basics <strong>Try it:</strong> <code>example/17.const_if.ng</code> — Compile-time branching <strong>Try it:</strong> <code>example/20.switch_otherwise.ng</code> — Switch with otherwise <strong>Try it:</strong> <code>example/57.ranges_slicing_pipeline.ng</code> — Range and slicing pipeline</p></blockquote>`,48)])])}const u=a(t,[["render",p]]);export{g as __pageData,u as default};
