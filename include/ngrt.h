// AI-generated code; reviewed for this repository's vNext rewrite.
//
// libngrt C API: the single implementation of the standard native functions
// for both the QBE native tier (linked as libngrt.a into every generated
// executable) and the VM/const-evaluator tiers (through the thin C++
// wrappers in include/ngrt.hpp).
//
// Value layouts match the Tier 0 representations emitted by
// src/native/lowering.cpp:
//   string: { int64_t len, char bytes[] }
//   array:  { int64_t len, int64_t cap, int64_t-or-pointer elements[] }
// Opaque handles are libngrt-owned heap cells.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Error reporting. The default handler prints `message` to stderr and
/// aborts (the behavior expected by AOT programs). The VM/const-evaluator
/// wrappers install a no-op handler and inspect `ngrt_last_error()` so they
/// can translate failures into BytecodeError / ConstEvalError instead of
/// aborting the host process.
typedef void (*ngrt_error_fn)(const char *message);
void ngrt_set_error_handler(ngrt_error_fn handler);
const char *ngrt_last_error(void);
void ngrt_clear_error(void);

void *ngrt_alloc(long size);
char *ngrt_new_string(const char *bytes, long length);
void ngrt_free(void *pointer);

// Prelude.
long ngrt_print_i64(long value);
long ngrt_print_u64(uint64_t value);
long ngrt_print_f64(double value);
long ngrt_print_str(const char *value);
long ngrt_print_bool(long value);
long ngrt_assert(long condition);

// std.string.
long ngrt_length(const char *text);
char *ngrt_trim(const char *text);
char *ngrt_toUpper(const char *text);
char *ngrt_toLower(const char *text);
char *ngrt_charAt(const char *text, long index);
char *ngrt_substring(const char *text, long start, long end);
long ngrt_contains(const char *text, const char *needle);
long ngrt_startsWith(const char *text, const char *prefix);
long ngrt_endsWith(const char *text, const char *suffix);
char *ngrt_replace(const char *text, const char *needle, const char *replacement);
void *ngrt_split(const char *text, const char *delimiter);
char *ngrt_join(const void *items, const char *separator);
long ngrt_regexMatch(const char *text, const char *pattern);

// std.seq.
long ngrt_len(const void *array);
long ngrt_sum(const void *array);
long ngrt_arrayContains(const void *array, long value);
void *ngrt_reverse(const void *array);

// std.memory.
long ngrt_allocate(long value);
long ngrt_load(long handle);
long ngrt_store(long handle, long value);
long ngrt_release(long handle);
long ngrt_outstanding(void);

// std.io.
char *ngrt_currentExecutablePath(void);
char *ngrt_readLine(void);
char *ngrt_readFile(const char *path);
long ngrt_writeFile(const char *path, const char *content);

// std.io system command bindings.
long ngrt_system(const char *command);
char *ngrt_systemOutput(const char *command);

// imgui (headless AOT stubs for now).
long ngrt_imguiInit(void);
long ngrt_imguiCleanup(void);
long ngrt_imguiEventLoop(void);
long ngrt_imguiNewFrame(void);
long ngrt_imguiRender(void);
long ngrt_imguiAborted(void);
long ngrt_imguiBegin(const char *title);
long ngrt_imguiEnd(void);
long ngrt_imguiSetNextWindowSize(double width, double height);
long ngrt_imguiBeginChild(const char *id, double width, double height);
long ngrt_imguiEndChild(void);
long ngrt_imguiText(const char *text);
long ngrt_imguiTextWrapped(const char *text);
long ngrt_imguiSeparator(void);
long ngrt_imguiButton(const char *label);
long ngrt_imguiCheckbox(const char *label, long checked);
char *ngrt_imguiInputTextMultiline(const char *label, const char *value, double width, double height);
long ngrt_imguiStyleColorsDark(void);
long ngrt_imguiStyleColorsLight(void);
double ngrt_imguiGetTime(void);

// B3 fixtures for `extern "C"` / `repr(C)` end-to-end tests. Keep these
// prototypes in sync with the definitions at the bottom of libngrt.c.
struct ngrt_fixture_point
{
  int32_t x;
  int32_t y;
};

struct ngrt_fixture_mixed
{
  int8_t a;
  int16_t b;
  int32_t c;
};

int64_t ngrt_fixture_point_sum(struct ngrt_fixture_point point);
int64_t ngrt_fixture_mixed_sum(struct ngrt_fixture_mixed mixed);
struct ngrt_fixture_point ngrt_fixture_point_swap(struct ngrt_fixture_point point);

#ifdef __cplusplus
}
#endif
