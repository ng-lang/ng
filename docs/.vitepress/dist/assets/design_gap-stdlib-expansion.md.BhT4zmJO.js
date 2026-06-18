import{_ as n,o as a,c as e,a6 as t}from"./chunks/framework.DL_pga9_.js";const h=JSON.parse('{"title":"Standard Library Expansion","description":"","frontmatter":{},"headers":[],"relativePath":"design/gap-stdlib-expansion.md","filePath":"design/gap-stdlib-expansion.md","lastUpdated":1781692128000}'),p={name:"design/gap-stdlib-expansion.md"};function i(l,s,o,r,d,c){return a(),e("div",null,[...s[0]||(s[0]=[t(`<h1 id="standard-library-expansion" tabindex="-1">Standard Library Expansion <a class="header-anchor" href="#standard-library-expansion" aria-label="Permalink to “Standard Library Expansion”">​</a></h1><blockquote><p><strong>Status:</strong> Refined with per-module API signatures. INVEST: 5/5/5/5/4/5 after refinement.</p></blockquote><h2 id="order" tabindex="-1">Order <a class="header-anchor" href="#order" aria-label="Permalink to “Order”">​</a></h2><p>Recommended implementation order: <strong>2</strong> (must follow error handling for Result-based APIs).</p><h2 id="goal" tabindex="-1">Goal <a class="header-anchor" href="#goal" aria-label="Permalink to “Goal”">​</a></h2><p>Expand NG&#39;s standard library from ~450 lines of NG to a production-ready set covering collections, JSON, time, math, and testing.</p><h2 id="modules-listed-by-implementation-priority" tabindex="-1">Modules (Listed by Implementation Priority) <a class="header-anchor" href="#modules-listed-by-implementation-priority" aria-label="Permalink to “Modules (Listed by Implementation Priority)”">​</a></h2><h3 id="module-a-std-collections-—-hashmap-hashset-2-weeks" tabindex="-1">Module A: <code>std.collections</code> — HashMap &amp; HashSet (2 weeks) <a class="header-anchor" href="#module-a-std-collections-—-hashmap-hashset-2-weeks" aria-label="Permalink to “Module A: std.collections — HashMap &amp; HashSet (2 weeks)”">​</a></h3><p>Provides hash-based data structures. Requires a <code>Hash</code> trait.</p><h4 id="hash-trait-prelude-addition" tabindex="-1"><code>Hash</code> Trait (Prelude Addition) <a class="header-anchor" href="#hash-trait-prelude-addition" aria-label="Permalink to “Hash Trait (Prelude Addition)”">​</a></h4><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// In prelude:</span></span>
<span class="line"><span>export trait Hash {</span></span>
<span class="line"><span>    fun hash(self: ref&lt;Self&gt;) -&gt; u64;</span></span>
<span class="line"><span>}</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Auto-implement for primitives:</span></span>
<span class="line"><span>impl Hash for i32 { ... }</span></span>
<span class="line"><span>impl Hash for i64 { ... }</span></span>
<span class="line"><span>impl Hash for string { ... }</span></span>
<span class="line"><span>impl Hash for bool { ... }</span></span>
<span class="line"><span>impl Hash for u32 { ... }</span></span>
<span class="line"><span>impl Hash for u64 { ... }</span></span>
<span class="line"><span>impl Hash for f32 { ... }</span></span>
<span class="line"><span>impl Hash for f64 { ... }</span></span></code></pre></div><h4 id="hashmap" tabindex="-1">HashMap <a class="header-anchor" href="#hashmap" aria-label="Permalink to “HashMap”">​</a></h4><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.collections exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type HashMap&lt;K: Hash, V&gt; = native;  // Opaque native type</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export fun newHashMap&lt;K: Hash, V&gt;() -&gt; HashMap&lt;K, V&gt; = native;</span></span>
<span class="line"><span>export fun insert&lt;K: Hash, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;, key: K, value: V) -&gt; Option&lt;V&gt; = native;</span></span>
<span class="line"><span>export fun get&lt;K: Hash, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;, key: K) -&gt; Option&lt;V&gt; = native;</span></span>
<span class="line"><span>export fun remove&lt;K: Hash, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;, key: K) -&gt; Option&lt;V&gt; = native;</span></span>
<span class="line"><span>export fun contains&lt;K: Hash, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;, key: K) -&gt; bool = native;</span></span>
<span class="line"><span>export fun len&lt;K, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;) -&gt; u32 = native;</span></span>
<span class="line"><span>export fun keys&lt;K, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;) -&gt; vector&lt;K&gt; = native;</span></span>
<span class="line"><span>export fun values&lt;K, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;) -&gt; vector&lt;V&gt; = native;</span></span>
<span class="line"><span>export fun clear&lt;K, V&gt;(map: ref&lt;HashMap&lt;K, V&gt;&gt;) -&gt; unit = native;</span></span></code></pre></div><h4 id="hashset" tabindex="-1">HashSet <a class="header-anchor" href="#hashset" aria-label="Permalink to “HashSet”">​</a></h4><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>export type HashSet&lt;T: Hash&gt; = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export fun newHashSet&lt;T: Hash&gt;() -&gt; HashSet&lt;T&gt; = native;</span></span>
<span class="line"><span>export fun insert&lt;T: Hash&gt;(set: ref&lt;HashSet&lt;T&gt;&gt;, value: T) -&gt; bool = native;</span></span>
<span class="line"><span>export fun contains&lt;T: Hash&gt;(set: ref&lt;HashSet&lt;T&gt;&gt;, value: T) -&gt; bool = native;</span></span>
<span class="line"><span>export fun remove&lt;T: Hash&gt;(set: ref&lt;HashSet&lt;T&gt;&gt;, value: T) -&gt; bool = native;</span></span>
<span class="line"><span>export fun len&lt;T: Hash&gt;(set: ref&lt;HashSet&lt;T&gt;&gt;) -&gt; u32 = native;</span></span>
<span class="line"><span>export fun toVector&lt;T: Hash&gt;(set: ref&lt;HashSet&lt;T&gt;&gt;) -&gt; vector&lt;T&gt; = native;</span></span></code></pre></div><h4 id="c-backing-implementation" tabindex="-1">C++ Backing Implementation <a class="header-anchor" href="#c-backing-implementation" aria-label="Permalink to “C++ Backing Implementation”">​</a></h4><p>Use existing vendored dependencies or standard C++:</p><ul><li><code>HashMap&lt;K,V&gt;</code> backs to <code>std::unordered_map</code> (or <code>ska::flat_hash_map</code> if vendored)</li><li><code>HashSet&lt;T&gt;</code> backs to <code>std::unordered_set</code></li><li>The <code>Hash</code> trait calls a C++ method on the NG value to produce <code>size_t</code></li></ul><hr><h3 id="module-b-std-json-—-json-parser-2-3-weeks" tabindex="-1">Module B: <code>std.json</code> — JSON Parser (2-3 weeks) <a class="header-anchor" href="#module-b-std-json-—-json-parser-2-3-weeks" aria-label="Permalink to “Module B: std.json — JSON Parser (2-3 weeks)”">​</a></h3><p>A DOM-style JSON parser that produces a traversable tree.</p><h4 id="types" tabindex="-1">Types <a class="header-anchor" href="#types" aria-label="Permalink to “Types”">​</a></h4><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.json exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type JsonValue = JsonNull | JsonBool(value: bool) | JsonInt(value: i64)</span></span>
<span class="line"><span>                      | JsonFloat(value: f64) | JsonString(value: string)</span></span>
<span class="line"><span>                      | JsonArray(items: vector&lt;JsonValue&gt;)</span></span>
<span class="line"><span>                      | JsonObject(fields: HashMap&lt;string, JsonValue&gt;);</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type JsonError {</span></span>
<span class="line"><span>    message: string;</span></span>
<span class="line"><span>    position: u32;</span></span>
<span class="line"><span>};</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type IOError = std.io.IOError;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type JsonParseResult = Result&lt;JsonValue, JsonError&gt;;</span></span></code></pre></div><h4 id="functions" tabindex="-1">Functions <a class="header-anchor" href="#functions" aria-label="Permalink to “Functions”">​</a></h4><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>// Parse string to JSON</span></span>
<span class="line"><span>export fun parse(text: string) -&gt; JsonParseResult = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Read and parse file</span></span>
<span class="line"><span>export fun parseFile(path: string) -&gt; Result&lt;JsonValue, JsonError | IOError&gt; = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Serialize JSON back to string</span></span>
<span class="line"><span>export fun stringify(value: JsonValue) -&gt; string = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Pretty-print with indentation</span></span>
<span class="line"><span>export fun stringifyPretty(value: JsonValue, indent: u32) -&gt; string = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Query helpers</span></span>
<span class="line"><span>export fun getField(obj: JsonValue, key: string) -&gt; Option&lt;JsonValue&gt;;</span></span>
<span class="line"><span>export fun getIndex(arr: JsonValue, index: u32) -&gt; Option&lt;JsonValue&gt;;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Type checks (convenience)</span></span>
<span class="line"><span>export fun isNull(value: JsonValue) -&gt; bool;</span></span>
<span class="line"><span>export fun asString(value: JsonValue) -&gt; Option&lt;string&gt;;</span></span>
<span class="line"><span>export fun asInt(value: JsonValue) -&gt; Option&lt;i64&gt;;</span></span>
<span class="line"><span>export fun asFloat(value: JsonValue) -&gt; Option&lt;f64&gt;;</span></span>
<span class="line"><span>export fun asBool(value: JsonValue) -&gt; Option&lt;bool&gt;;</span></span>
<span class="line"><span>export fun asArray(value: JsonValue) -&gt; Option&lt;vector&lt;JsonValue&gt;&gt;;</span></span>
<span class="line"><span>export fun asObject(value: JsonValue) -&gt; Option&lt;HashMap&lt;string, JsonValue&gt;&gt;;</span></span></code></pre></div><h4 id="c-backing" tabindex="-1">C++ Backing <a class="header-anchor" href="#c-backing" aria-label="Permalink to “C++ Backing”">​</a></h4><p>Use <code>simdjson</code> (vendored or CMake FetchContent) for high-performance JSON parsing. The <code>JsonValue</code> tagged union maps to a runtime discriminated value.</p><hr><h3 id="module-c-std-time-—-datetime-duration-2-weeks" tabindex="-1">Module C: <code>std.time</code> — DateTime &amp; Duration (2 weeks) <a class="header-anchor" href="#module-c-std-time-—-datetime-duration-2-weeks" aria-label="Permalink to “Module C: std.time — DateTime &amp; Duration (2 weeks)”">​</a></h3><p>UTC-only initially. No timezone support in MVP.</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.time exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type Instant = i64;        // Nanoseconds since epoch</span></span>
<span class="line"><span>export type Duration = i64;       // Nanoseconds</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Creating values</span></span>
<span class="line"><span>export fun now() -&gt; Instant = native;</span></span>
<span class="line"><span>export fun durationFromNanos(ns: i64) -&gt; Duration;</span></span>
<span class="line"><span>export fun durationFromMicros(us: i64) -&gt; Duration;</span></span>
<span class="line"><span>export fun durationFromMillis(ms: i64) -&gt; Duration;</span></span>
<span class="line"><span>export fun durationFromSeconds(s: i64) -&gt; Duration;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Duration arithmetic</span></span>
<span class="line"><span>export fun durationNanos(d: Duration) -&gt; i64;</span></span>
<span class="line"><span>export fun durationMicros(d: Duration) -&gt; i64;</span></span>
<span class="line"><span>export fun durationMillis(d: Duration) -&gt; i64;</span></span>
<span class="line"><span>export fun durationSeconds(d: Duration) -&gt; i64;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Instant arithmetic</span></span>
<span class="line"><span>export fun addDuration(instant: Instant, d: Duration) -&gt; Instant;</span></span>
<span class="line"><span>export fun subDuration(instant: Instant, d: Duration) -&gt; Instant;</span></span>
<span class="line"><span>export fun diff(a: Instant, b: Instant) -&gt; Duration;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Comparison</span></span>
<span class="line"><span>export fun compare(a: Instant, b: Instant) -&gt; i32;  // -1, 0, 1</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Human-readable formatting</span></span>
<span class="line"><span>export fun instantToRFC3339(instant: Instant) -&gt; string = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Sleep (blocking)</span></span>
<span class="line"><span>export fun sleep(d: Duration) -&gt; unit = native;</span></span></code></pre></div><hr><h3 id="module-d-std-test-—-testing-framework-2-weeks" tabindex="-1">Module D: <code>std.test</code> — Testing Framework (2 weeks) <a class="header-anchor" href="#module-d-std-test-—-testing-framework-2-weeks" aria-label="Permalink to “Module D: std.test — Testing Framework (2 weeks)”">​</a></h3><p><em>Note: Depends on <code>test</code> and <code>describe</code> being implemented as library functions taking closure arguments, OR as special AST nodes (see <a href="./gap-syntax-ergonomics">Syntax Ergonomics Phase 2</a>).</em></p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.test exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// --- Test Registration ---</span></span>
<span class="line"><span>// These are implemented as macro-like functions or special forms:</span></span>
<span class="line"><span>export fun test(name: string, body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun describe(name: string, body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// --- Assertions ---</span></span>
<span class="line"><span>export fun expect(condition: bool) -&gt; unit;</span></span>
<span class="line"><span>export fun expectEq&lt;T&gt;(actual: T, expected: T) -&gt; unit where T: Eq;</span></span>
<span class="line"><span>export fun expectNe&lt;T&gt;(actual: T, expected: T) -&gt; unit where T: Eq;</span></span>
<span class="line"><span>export fun expectApprox(actual: f64, expected: f64, epsilon: f64 = 1e-9) -&gt; unit;</span></span>
<span class="line"><span>export fun expectError(body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun expectType&lt;T&gt;(value: unknown) -&gt; unit;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// --- Setup/Teardown ---</span></span>
<span class="line"><span>export fun before(body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun after(body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun beforeEach(body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun afterEach(body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// --- Test Runner API ---</span></span>
<span class="line"><span>export fun runTests() -&gt; TestResults;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type TestResults {</span></span>
<span class="line"><span>    total: u32;</span></span>
<span class="line"><span>    passed: u32;</span></span>
<span class="line"><span>    failed: u32;</span></span>
<span class="line"><span>    duration: Duration;</span></span>
<span class="line"><span>};</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// --- Benchmark ---</span></span>
<span class="line"><span>export fun bench(name: string, body: () -&gt; unit) -&gt; unit;</span></span>
<span class="line"><span>export fun measure(body: () -&gt; unit) -&gt; Duration;</span></span></code></pre></div><p><strong>Test discovery algorithm:</strong></p><ol><li>The test runner parses all <code>.ng</code> files in <code>tests/</code></li><li>Scans the top-level for <code>test(...)</code> and <code>describe(...)</code> calls</li><li>Executes matching tests in sequence</li><li>Each test runs in a fresh scope to prevent state leakage</li><li>A single <code>assert(false)</code> or uncaught error marks the test as failed</li></ol><p><strong>Test isolation:</strong></p><ul><li>Each <code>test</code> block runs in its own VM frame</li><li>Globals modified by one test are visible to subsequent tests (opt-in via <code>--no-isolate</code>)</li><li><code>before</code>/<code>after</code> hooks run in the same scope as their containing <code>describe</code></li></ul><hr><h3 id="module-e-std-math-—-math-functions-1-week" tabindex="-1">Module E: <code>std.math</code> — Math Functions (1 week) <a class="header-anchor" href="#module-e-std-math-—-math-functions-1-week" aria-label="Permalink to “Module E: std.math — Math Functions (1 week)”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.math exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Constants</span></span>
<span class="line"><span>export val PI: f64 = 3.14159265358979323846;</span></span>
<span class="line"><span>export val E: f64 = 2.71828182845904523536;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Basic</span></span>
<span class="line"><span>export fun abs&lt;T: i32 | i64 | f32 | f64&gt;(x: T) -&gt; T = native;</span></span>
<span class="line"><span>export fun min&lt;T&gt;(a: T, b: T) -&gt; T = native;</span></span>
<span class="line"><span>export fun max&lt;T&gt;(a: T, b: T) -&gt; T = native;</span></span>
<span class="line"><span>export fun clamp&lt;T&gt;(value: T, minVal: T, maxVal: T) -&gt; T;</span></span>
<span class="line"><span>export fun lerp&lt;T: f32 | f64&gt;(a: T, b: T, t: T) -&gt; T;</span></span>
<span class="line"><span>export fun sign&lt;T: i32 | i64 | f32 | f64&gt;(x: T) -&gt; i32 = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Power / Root</span></span>
<span class="line"><span>export fun sqrt(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun cbrt(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun pow(base: f64, exp: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun exp(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun ln(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun log2(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun log10(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun hypot(a: f64, b: f64) -&gt; f64 = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Trigonometry</span></span>
<span class="line"><span>export fun sin(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun cos(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun tan(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun asin(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun acos(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun atan(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun atan2(y: f64, x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Rounding</span></span>
<span class="line"><span>export fun floor(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun ceil(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun round(x: f64) -&gt; f64 = native;</span></span>
<span class="line"><span>export fun trunc(x: f64) -&gt; f64 = native;</span></span></code></pre></div><hr><h3 id="module-f-std-regex-—-regular-expressions-2-weeks-deferred" tabindex="-1">Module F: <code>std.regex</code> — Regular Expressions (2 weeks, deferred) <a class="header-anchor" href="#module-f-std-regex-—-regular-expressions-2-weeks-deferred" aria-label="Permalink to “Module F: std.regex — Regular Expressions (2 weeks, deferred)”">​</a></h3><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.regex exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export type Regex = native;</span></span>
<span class="line"><span>export type RegexError { message: string; }</span></span>
<span class="line"><span>export type RegexMatch {</span></span>
<span class="line"><span>    start: u32;</span></span>
<span class="line"><span>    end: u32;</span></span>
<span class="line"><span>    groups: vector&lt;(start: u32, end: u32)&gt;;</span></span>
<span class="line"><span>};</span></span>
<span class="line"><span></span></span>
<span class="line"><span>export fun compile(pattern: string) -&gt; Result&lt;Regex, RegexError&gt; = native;</span></span>
<span class="line"><span>export fun isMatch(regex: ref&lt;Regex&gt;, text: string) -&gt; bool = native;</span></span>
<span class="line"><span>export fun find(regex: ref&lt;Regex&gt;, text: string) -&gt; Option&lt;RegexMatch&gt; = native;</span></span>
<span class="line"><span>export fun findAll(regex: ref&lt;Regex&gt;, text: string) -&gt; vector&lt;RegexMatch&gt; = native;</span></span>
<span class="line"><span>export fun replace(regex: ref&lt;Regex&gt;, text: string, replacement: string) -&gt; string = native;</span></span>
<span class="line"><span>export fun split(regex: ref&lt;Regex&gt;, text: string) -&gt; vector&lt;string&gt; = native;</span></span></code></pre></div><p>Backed by <code>pcre2</code> or <code>std::regex</code> (pcre2 recommended for performance and Unicode support).</p><hr><h3 id="migration-replacing-existing-std-array" tabindex="-1">Migration: Replacing Existing <code>std.array</code> <a class="header-anchor" href="#migration-replacing-existing-std-array" aria-label="Permalink to “Migration: Replacing Existing std.array”">​</a></h3><p>Current <code>lib/std/array.ng</code> (3 lines) should be expanded:</p><div class="language-ng"><button title="Copy Code" class="copy"></button><span class="lang">ng</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>module std.array exports *;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// Existing</span></span>
<span class="line"><span>fun reverse&lt;T&gt;(xs: vector&lt;T&gt;) -&gt; vector&lt;T&gt; = native;</span></span>
<span class="line"><span></span></span>
<span class="line"><span>// New additions</span></span>
<span class="line"><span>fun sort&lt;T&gt;(xs: vector&lt;T&gt;) -&gt; vector&lt;T&gt; = native;</span></span>
<span class="line"><span>fun isEmpty&lt;T&gt;(xs: vector&lt;T&gt;) -&gt; bool;</span></span>
<span class="line"><span>fun contains&lt;T&gt;(xs: vector&lt;T&gt;, value: T) -&gt; bool where T: Eq;</span></span>
<span class="line"><span>fun first&lt;T&gt;(xs: vector&lt;T&gt;) -&gt; Option&lt;T&gt;;</span></span>
<span class="line"><span>fun last&lt;T&gt;(xs: vector&lt;T&gt;) -&gt; Option&lt;T&gt;;</span></span>
<span class="line"><span>fun slice&lt;T&gt;(xs: vector&lt;T&gt;, start: i32, end: i32) -&gt; vector&lt;T&gt;;</span></span>
<span class="line"><span>fun concat&lt;T&gt;(a: vector&lt;T&gt;, b: vector&lt;T&gt;) -&gt; vector&lt;T&gt;;</span></span></code></pre></div><hr><h2 id="implementation-order" tabindex="-1">Implementation Order <a class="header-anchor" href="#implementation-order" aria-label="Permalink to “Implementation Order”">​</a></h2><table tabindex="0"><thead><tr><th>Order</th><th>Module</th><th>Effort</th><th>Dependencies</th></tr></thead><tbody><tr><td>1</td><td><code>Hash</code> trait + <code>std.collections</code> (HashMap only)</td><td>2 weeks</td><td>Prelude modifications</td></tr><tr><td>2</td><td><code>std.math</code></td><td>1 week</td><td>None</td></tr><tr><td>3</td><td><code>std.json</code> (MVP: parse + stringify)</td><td>2 weeks</td><td><code>std.collections</code> (HashMap)</td></tr><tr><td>4</td><td><code>std.time</code> (UTC only)</td><td>2 weeks</td><td>None</td></tr><tr><td>5</td><td><code>std.test</code> (MVP: expect + test runner)</td><td>2 weeks</td><td>Error handling</td></tr><tr><td>6</td><td><code>std.array</code> expansion</td><td>0.5 week</td><td>None</td></tr><tr><td>7</td><td><code>std.regex</code></td><td>2 weeks</td><td>None</td></tr></tbody></table><p><strong>Total Phase 1 effort:</strong> ~12 weeks (can be parallelized across 2-3 developers).</p><hr><h2 id="acceptance-criteria-per-module" tabindex="-1">Acceptance Criteria (Per Module) <a class="header-anchor" href="#acceptance-criteria-per-module" aria-label="Permalink to “Acceptance Criteria (Per Module)”">​</a></h2><h3 id="std-collections" tabindex="-1">std.collections <a class="header-anchor" href="#std-collections" aria-label="Permalink to “std.collections”">​</a></h3><ul><li><code>HashMap</code> with 10,000 entries: insert, get, remove all complete in &lt; 50ms</li><li><code>HashSet</code> deduplicates correctly</li><li><code>Hash</code> trait is auto-implemented for all primitive types</li><li>Memory: 10,000 entries use &lt; 2MB</li></ul><h3 id="std-json" tabindex="-1">std.json <a class="header-anchor" href="#std-json" aria-label="Permalink to “std.json”">​</a></h3><ul><li><code>parse(&#39;{&quot;a&quot;:1, &quot;b&quot;:[2,3]}&#39;)</code> produces correct tree</li><li>Nested objects 100 levels deep parse correctly</li><li>Malformed JSON returns <code>Err</code> with position</li><li><code>stringify</code> round-trips: <code>stringify(parse(x)) == x</code></li></ul><h3 id="std-time" tabindex="-1">std.time <a class="header-anchor" href="#std-time" aria-label="Permalink to “std.time”">​</a></h3><ul><li><code>now()</code> returns a progressively increasing value</li><li><code>durationFromSeconds(5)</code> → <code>durationSeconds(d)</code> → 5</li><li><code>addDuration(instant, duration)</code> produces correct future/past</li></ul><h3 id="std-test" tabindex="-1">std.test <a class="header-anchor" href="#std-test" aria-label="Permalink to “std.test”">​</a></h3><ul><li>A test file with 2 passing and 1 failing test reports 2/3 passed</li><li><code>expectEq</code> failure shows expected vs actual values</li><li><code>before</code>/<code>after</code> hooks execute in the correct order</li></ul><h3 id="std-math" tabindex="-1">std.math <a class="header-anchor" href="#std-math" aria-label="Permalink to “std.math”">​</a></h3><ul><li><code>sqrt(4.0) == 2.0</code>, <code>sqrt(-1.0)</code> returns NaN</li><li><code>sin(PI) ≈ 0</code> within 1e-10</li><li>All functions handle edge cases (Infinity, NaN) without crashing</li></ul><h3 id="std-regex" tabindex="-1">std.regex <a class="header-anchor" href="#std-regex" aria-label="Permalink to “std.regex”">​</a></h3><ul><li><code>compile(&quot;[0-9]+&quot;)</code> matches &quot;abc123def&quot; → find returns &quot;123&quot;</li><li>Replace works with backreferences: <code>replace(r&quot;(\\w+) (\\w+)&quot;, &quot;$2 $1&quot;)</code></li><li>Invalid pattern returns <code>Err</code></li></ul><hr><h2 id="dependencies" tabindex="-1">Dependencies <a class="header-anchor" href="#dependencies" aria-label="Permalink to “Dependencies”">​</a></h2><ul><li><a href="./gap-error-handling">Error Handling</a>: <code>Result&lt;T, E&gt;</code> required for non-crashing APIs.</li><li><code>Hash</code> trait definition must be added to prelude.</li><li>Native (C++) backing for performance-critical functions.</li></ul><h2 id="potential-challenges" tabindex="-1">Potential Challenges <a class="header-anchor" href="#potential-challenges" aria-label="Permalink to “Potential Challenges”">​</a></h2><ul><li><code>Hash</code> trait interacts with the existing auto trait system — manually implementing <code>Hash</code> for all primitives is tedious but mechanical.</li><li>HashMap with <code>string</code> keys requires string hashing in C++ that matches NG string representation.</li><li>JSON parser in C++ vs NG: C++ is faster (simdjson). NG is safer. Hybrid approach recommended (C++ for parsing, NG for traversal).</li><li>Regex engine: <code>pcre2</code> is GPL-licensed. <code>std::regex</code> has poor performance. Recommend <code>RE2</code> (BSD-licensed) or <code>Oniguruma</code>.</li><li><code>std.time</code> timezone support is intentionally excluded — UTC-only simplifies everything enormously.</li></ul>`,73)])])}const u=n(p,[["render",i]]);export{h as __pageData,u as default};
