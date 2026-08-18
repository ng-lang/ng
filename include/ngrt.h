// AI-generated code; reviewed for this repository's vNext rewrite.
//
// libngrt C API: the single implementation of the standard native functions,
// linked as libngrt.a into every generated executable and called by the
// frontend through the thin C++ wrappers in include/ngrt.hpp (const
// evaluation and the driver's native hosts).
//
// Value layouts match the Tier 0 representations emitted by
// src/native/lowering.cpp:
//   string: { int64_t len, char bytes[] }
//   array:  { int64_t len, int64_t cap, int64_t-or-pointer elements[] }
// Opaque handles are libngrt-owned heap cells.
//
// Every ABI-facing size, length, and value is int64_t (fixed-width) so the
// surface is independent of the platform's `long` width; pointers and doubles
// keep their own types.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Error reporting. The default handler prints `message` to stderr and
/// aborts (the behavior expected by AOT programs). The C++ wrappers
/// (include/ngrt.hpp) install a no-op handler and inspect `ngrt_last_error()`
/// so they can translate failures into host exceptions instead of aborting
/// the embedding process.
typedef void (*ngrt_error_fn)(const char *message);
void ngrt_set_error_handler(ngrt_error_fn handler);
const char *ngrt_last_error(void);
void ngrt_clear_error(void);

void *ngrt_alloc(int64_t size);
char *ngrt_new_string(const char *bytes, int64_t length);
void ngrt_free(void *pointer);

// Prelude.
int64_t ngrt_print_i64(int64_t value);
int64_t ngrt_print_u64(uint64_t value);
int64_t ngrt_print_f64(double value);
int64_t ngrt_print_str(const char *value);
int64_t ngrt_print_bool(int64_t value);
int64_t ngrt_assert(int64_t condition);

// std.string.
int64_t ngrt_length(const char *text);
char *ngrt_trim(const char *text);
char *ngrt_toUpper(const char *text);
char *ngrt_toLower(const char *text);
char *ngrt_charAt(const char *text, int64_t index);
char *ngrt_substring(const char *text, int64_t start, int64_t end);
int64_t ngrt_contains(const char *text, const char *needle);
int64_t ngrt_startsWith(const char *text, const char *prefix);
int64_t ngrt_endsWith(const char *text, const char *suffix);
char *ngrt_replace(const char *text, const char *needle, const char *replacement);
void *ngrt_split(const char *text, const char *delimiter);
char *ngrt_join(const void *items, const char *separator);
int64_t ngrt_regexMatch(const char *text, const char *pattern);

// std.seq.
int64_t ngrt_len(const void *array);
int64_t ngrt_sum(const void *array);
int64_t ngrt_arrayContains(const void *array, int64_t value);
void *ngrt_reverse(const void *array);

// std.memory.
int64_t ngrt_allocate(int64_t value);
int64_t ngrt_load(int64_t handle);
int64_t ngrt_store(int64_t handle, int64_t value);
int64_t ngrt_release(int64_t handle);
int64_t ngrt_outstanding(void);

// std.io.
char *ngrt_currentExecutablePath(void);
char *ngrt_readLine(void);
char *ngrt_readFile(const char *path);
int64_t ngrt_writeFile(const char *path, const char *content);

// std.io system command bindings.
int64_t ngrt_system(const char *command);
char *ngrt_systemOutput(const char *command);

// imgui: implemented by the SDL3 GPU and Dear ImGui backend
// (src/native/imgui_ngrt.cpp), linked into generated executables that import
// lib/std/imgui.ng.
int64_t ngrt_imguiInit(void);
int64_t ngrt_imguiCleanup(void);
int64_t ngrt_imguiEventLoop(void);
int64_t ngrt_imguiNewFrame(void);
int64_t ngrt_imguiRender(void);
int64_t ngrt_imguiAborted(void);
int64_t ngrt_imguiBegin(const char *title);
int64_t ngrt_imguiEnd(void);
int64_t ngrt_imguiSetNextWindowSize(double width, double height);
int64_t ngrt_imguiBeginChild(const char *id, double width, double height);
int64_t ngrt_imguiEndChild(void);
int64_t ngrt_imguiText(const char *text);
int64_t ngrt_imguiTextWrapped(const char *text);
int64_t ngrt_imguiSeparator(void);
int64_t ngrt_imguiButton(const char *label);
int64_t ngrt_imguiCheckbox(const char *label, int64_t checked);
char *ngrt_imguiInputTextMultiline(const char *label, const char *value, double width, double height);
int64_t ngrt_imguiStyleColorsDark(void);
int64_t ngrt_imguiStyleColorsLight(void);
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
