// AI-generated code; reviewed for this repository's vNext rewrite.
//
// libngrt: the single C implementation of the standard native functions.
// The QBE native tier links this archive into every generated executable;
// the frontend calls the same functions through the thin C++ wrappers in
// src/ngrt.cpp (const evaluation and the driver's native hosts).
//
// Value layouts match the Tier 0 representations emitted by
// src/native/lowering.cpp:
//   string: { int64_t len, char bytes[] }
//   array:  { int64_t len, int64_t cap, int64_t-or-pointer elements[] }
// Doubles stored in arrays are bit patterns in int64_t slots.
//
// Every ABI-facing size, length, and value is int64_t so the surface is
// independent of the platform's `long` width.
#define _POSIX_C_SOURCE 200809L

#ifdef __APPLE__
// Restore the BSD mmap flags (MAP_ANON/MAP_ANONYMOUS) that _POSIX_C_SOURCE
// hides; every other extension stays POSIX (INC-18 bump arena).
#define _DARWIN_C_SOURCE 1
#endif

#include "ngrt.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* ------------------------------------------------------------------------- */
/* Error handling                                                            */
/* ------------------------------------------------------------------------- */

__attribute__((weak)) const char ng_ngi_path[] = "ngi";

static char ngrt_error_message[512];

static void ngrt_default_error(const char *message)
{
  fprintf(stderr, "%s\n", message);
  abort();
}

static ngrt_error_fn ngrt_error_handler = ngrt_default_error;

void ngrt_set_error_handler(ngrt_error_fn handler)
{
  ngrt_error_handler = handler != NULL ? handler : ngrt_default_error;
}

const char *ngrt_last_error(void)
{
  return ngrt_error_message[0] != '\0' ? ngrt_error_message : NULL;
}

void ngrt_clear_error(void)
{
  ngrt_error_message[0] = '\0';
}

static void ngrt_fail(const char *message)
{
  snprintf(ngrt_error_message, sizeof ngrt_error_message, "%s", message);
  ngrt_error_handler(message);
}

/* ------------------------------------------------------------------------- */
/* INC-18 (exit-time bump arena)                                              */
/* ------------------------------------------------------------------------- */

/* The program heap is served by a per-process bump arena. Every program-owned
 * value allocation (strings, arrays, aggregate headers, box cells, closure
 * cells) comes from one of a chain of mmap'd anonymous blocks; individual
 * blocks are never returned. The whole arena is released when the process
 * exits — an `atexit` handler munmaps the blocks, and the OS reclaims them in
 * any case — which is the transitional hygiene layer of DEC-C (RC + CoW land
 * in INC-19). It deliberately does not bound within-run growth, and a pointer
 * whose physical storage lives in the arena is not individually freeable
 * (`ngrt_free` no-ops for those; `ngrt_release` keeps the logical
 * live-handle removal only).
 *
 * The arena is single-threaded by design (the runtime has no concurrency
 * today); guard against lock-order concerns if a parallel executor ever
 * lands, and note that ANONYMOUS mmap is never visible to malloc-based leak
 * reporters (leaks(1) / LeakSanitizer), so a program that exercises only the
 * arena reports zero malloc leaks. */

typedef struct NgrtArenaBlock NgrtArenaBlock;
struct NgrtArenaBlock
{
  NgrtArenaBlock *next;
  size_t capacity; /* usable bytes after the header */
  size_t used;     /* bump watermark within this block */
  size_t mappingSize;
  char data[] __attribute__((aligned(16)));
};

#define NGRT_ARENA_BLOCK_CAPACITY ((size_t) 1 << 20)

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

static NgrtArenaBlock *ngrt_arena_head = NULL;
static NgrtArenaBlock *ngrt_arena_current = NULL;
static int ngrt_arena_exit_registered = 0;

void ngrt_arena_release(void)
{
  NgrtArenaBlock *block = ngrt_arena_head;
  while (block != NULL)
  {
    NgrtArenaBlock *next = block->next;
    munmap(block, block->mappingSize);
    block = next;
  }
  ngrt_arena_head = NULL;
  ngrt_arena_current = NULL;
}

static void ngrt_arena_exit_handler(void)
{
  ngrt_arena_release();
}

static void ngrt_arena_grow(size_t size)
{
  if (!ngrt_arena_exit_registered)
  {
    ngrt_arena_exit_registered = 1;
    atexit(ngrt_arena_exit_handler);
  }
  const size_t headerSize = sizeof(NgrtArenaBlock);
  const size_t capacity = size > NGRT_ARENA_BLOCK_CAPACITY ? size : NGRT_ARENA_BLOCK_CAPACITY;
  const long page = sysconf(_SC_PAGESIZE);
  const size_t pageSize = page > 0 ? (size_t) page : 4096;
  size_t mappingSize = headerSize + capacity;
  if (mappingSize % pageSize != 0)
    mappingSize += pageSize - mappingSize % pageSize;
  void *base = mmap(NULL, mappingSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED)
    abort();
  NgrtArenaBlock *block = (NgrtArenaBlock *) base;
  block->next = NULL;
  block->capacity = mappingSize - headerSize;
  block->used = 0;
  block->mappingSize = mappingSize;
  if (ngrt_arena_current != NULL)
    ngrt_arena_current->next = block;
  else
    ngrt_arena_head = block;
  ngrt_arena_current = block;
}

static void *ngrt_arena_bump(size_t size)
{
  size = (size + 15) & ~(size_t) 15;
  if (ngrt_arena_current == NULL || ngrt_arena_current->used + size > ngrt_arena_current->capacity)
    ngrt_arena_grow(size);
  void *result = ngrt_arena_current->data + ngrt_arena_current->used;
  ngrt_arena_current->used += size;
  return result;
}

static int ngrt_pointer_in_arena(const void *pointer)
{
  for (const NgrtArenaBlock *block = ngrt_arena_head; block != NULL; block = block->next)
  {
    const char *begin = block->data;
    const char *end = begin + block->used;
    const char *p = (const char *) pointer;
    if (p >= begin && p < end)
      return 1;
  }
  return 0;
}

/* Replaces the malloc-per-value allocation of the pre-INC-18 runtime: the
 * generated code emits `$ngrt_alloc` (not `$malloc`) for every program
 * allocation, and both this and `ngrt_new_string` bump into the arena. */
void *ngrt_alloc(int64_t size)
{
  if (size < 0)
    abort();
  return ngrt_arena_bump((size_t) size);
}

char *ngrt_new_string(const char *bytes, int64_t length)
{
  char *result = (char *) ngrt_arena_bump((size_t) length + 8);
  *(int64_t *) result = length;
  if (length > 0)
    memcpy(result + 8, bytes, (size_t) length);
  return result;
}

/* Arena-owned pointers are not individually freeable (bump allocation);
 * malloc'd scratch (C-string conversions, file/command buffers, the live
 * handle table) still goes through plain free(). */
void ngrt_free(void *pointer)
{
  if (pointer == NULL || ngrt_pointer_in_arena(pointer))
    return;
  free(pointer);
}

int64_t ngrt_arena_blocks(void)
{
  int64_t count = 0;
  for (const NgrtArenaBlock *block = ngrt_arena_head; block != NULL; block = block->next)
    ++count;
  return count;
}

int64_t ngrt_arena_bytes(void)
{
  int64_t total = 0;
  for (const NgrtArenaBlock *block = ngrt_arena_head; block != NULL; block = block->next)
    total += (int64_t) block->used;
  return total;
}

/* ------------------------------------------------------------------------- */
/* ABI helpers                                                               */
/* ------------------------------------------------------------------------- */

static int64_t ngrt_str_len(const char *s)
{
  return *(const int64_t *) s;
}

static const char *ngrt_str_bytes(const char *s)
{
  return s + 8;
}

static char *ngrt_c_string(const char *value)
{
  int64_t length = ngrt_str_len(value);
  char *result = (char *) malloc((size_t) length + 1);
  if (result == NULL)
    abort();
  memcpy(result, ngrt_str_bytes(value), (size_t) length);
  result[length] = 0;
  return result;
}

/* ------------------------------------------------------------------------- */
/* Prelude                                                                   */
/* ------------------------------------------------------------------------- */

int64_t ngrt_print_i64(int64_t value)
{
  printf("%" PRId64 "\n", value);
  return 0;
}

int64_t ngrt_print_u64(uint64_t value)
{
  printf("%" PRIu64 "\n", value);
  return 0;
}

int64_t ngrt_print_f64(double value)
{
  printf("%g\n", value);
  return 0;
}

int64_t ngrt_print_str(const char *value)
{
  /* fwrite the full length: an int cast would truncate huge strings, and
   * %.*s would stop at embedded NULs. */
  fwrite(ngrt_str_bytes(value), 1, (size_t) ngrt_str_len(value), stdout);
  fputc('\n', stdout);
  return 0;
}

int64_t ngrt_str_cmp(const char *left, const char *right)
{
  /* DEC-E: total order is bytewise memcmp; if a common prefix ties, the
   * shorter string is less. UTF-8 makes this the same as code-point order. */
  int64_t leftLength = ngrt_str_len(left);
  int64_t rightLength = ngrt_str_len(right);
  int64_t prefix = leftLength < rightLength ? leftLength : rightLength;
  int compared = memcmp(ngrt_str_bytes(left), ngrt_str_bytes(right), (size_t) prefix);
  if (compared != 0)
    return compared < 0 ? (int64_t) -1 : (int64_t) 1;
  if (leftLength < rightLength)
    return -1;
  if (leftLength > rightLength)
    return 1;
  return 0;
}

int64_t ngrt_print_bool(int64_t value)
{
  printf("%s\n", value ? "true" : "false");
  return 0;
}

/* INC-7: string conversions and integer parsing. toString mirrors the print
 * formatters exactly (%g for floats, true/false for bools) so round-trips
 * through the two families stay consistent. parse accepts an optional sign
 * followed by decimal digits only — no whitespace, no separators — and the
 * full i64 range including INT64_MIN. */
char *ngrt_toString_i64(int64_t value)
{
  char buffer[32];
  snprintf(buffer, sizeof buffer, "%" PRId64, value);
  return ngrt_new_string(buffer, (int64_t) strlen(buffer));
}

char *ngrt_toString_u64(uint64_t value)
{
  char buffer[32];
  snprintf(buffer, sizeof buffer, "%" PRIu64, value);
  return ngrt_new_string(buffer, (int64_t) strlen(buffer));
}

char *ngrt_toString_f64(double value)
{
  char buffer[64];
  snprintf(buffer, sizeof buffer, "%g", value);
  return ngrt_new_string(buffer, (int64_t) strlen(buffer));
}

char *ngrt_toString_bool(int64_t value)
{
  return ngrt_new_string(value ? "true" : "false", value ? 4 : 5);
}

static int ngrt_parseI64Strict(const char *bytes, int64_t length, int64_t *out)
{
  int64_t index = 0;
  int negative = 0;
  if (length > 0 && (bytes[0] == '+' || bytes[0] == '-'))
  {
    negative = (bytes[0] == '-');
    index = 1;
  }
  if (index == length)
    return 0;
  const uint64_t limit = negative ? 9223372036854775808ULL : 9223372036854775807ULL;
  uint64_t accumulated = 0;
  for (; index < length; index++)
  {
    if (bytes[index] < '0' || bytes[index] > '9')
      return 0;
    const uint64_t digit = (uint64_t) (bytes[index] - '0');
    if (accumulated > (limit - digit) / 10)
      return 0;
    accumulated = accumulated * 10 + digit;
  }
  if (negative && accumulated == 9223372036854775808ULL)
    *out = -9223372036854775807LL - 1;
  else
    *out = negative ? -(int64_t) accumulated : (int64_t) accumulated;
  return 1;
}

int64_t ngrt_canParseInt(const char *text)
{
  int64_t ignored;
  return ngrt_parseI64Strict(ngrt_str_bytes(text), ngrt_str_len(text), &ignored);
}

int64_t ngrt_parseRawInt(const char *text)
{
  int64_t value = 0;
  if (!ngrt_parseI64Strict(ngrt_str_bytes(text), ngrt_str_len(text), &value))
  {
    ngrt_fail("parseRawInt: input is not a valid i64 literal");
    return 0;
  }
  return value;
}

/* INC-6 (DEC-A): explicit numeric conversions. toFloat widens either integer
 * sign class into a double; toSigned reinterprets a u64 bit pattern as i64
 * so full-range unsigned values can be compared against signed constants. */
double ngrt_toFloat_i64(int64_t value)
{
  return (double) value;
}

double ngrt_toFloat_u64(uint64_t value)
{
  return (double) value;
}

int64_t ngrt_toSigned(uint64_t value)
{
  return (int64_t) value;
}

int64_t ngrt_assert(int64_t condition)
{
  if (!condition)
  {
    ngrt_fail("assertion failed");
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------------- */
/* std.string                                                                */
/* ------------------------------------------------------------------------- */

int64_t ngrt_length(const char *text)
{
  return ngrt_str_len(text);
}

char *ngrt_trim(const char *text)
{
  int64_t length = ngrt_str_len(text);
  const char *bytes = ngrt_str_bytes(text);
  int64_t begin = 0;
  int64_t end = length;
  while (begin < end && isspace((unsigned char) bytes[begin]))
    ++begin;
  while (end > begin && isspace((unsigned char) bytes[end - 1]))
    --end;
  return ngrt_new_string(bytes + begin, end - begin);
}

static char *ngrt_str_case(const char *value, int upper)
{
  int64_t length = ngrt_str_len(value);
  const char *bytes = ngrt_str_bytes(value);
  char *result = (char *) ngrt_alloc((size_t) length + 8);
  if (result == NULL)
    abort();
  *(int64_t *) result = length;
  for (int64_t index = 0; index < length; ++index)
  {
    unsigned char byte = (unsigned char) bytes[index];
    result[8 + index] = (char) (upper ? toupper(byte) : tolower(byte));
  }
  return result;
}

char *ngrt_toUpper(const char *text)
{
  return ngrt_str_case(text, 1);
}

char *ngrt_toLower(const char *text)
{
  return ngrt_str_case(text, 0);
}

char *ngrt_charAt(const char *text, int64_t index)
{
  int64_t length = ngrt_str_len(text);
  if (index < 0 || index >= length)
  {
    char message[128];
    snprintf(message, sizeof message, "charAt index out of bounds: index %ld, length %ld", (long) index, (long) length);
    ngrt_fail(message);
    return NULL;
  }
  return ngrt_new_string(ngrt_str_bytes(text) + index, 1);
}

char *ngrt_substring(const char *text, int64_t start, int64_t end)
{
  int64_t length = ngrt_str_len(text);
  if (start < 0 || end < start || end > length)
  {
    char message[160];
    snprintf(message, sizeof message, "substring bounds out of range: [%ld..%ld) of length %ld", (long) start,
             (long) end, (long) length);
    ngrt_fail(message);
    return NULL;
  }
  return ngrt_new_string(ngrt_str_bytes(text) + start, end - start);
}

int64_t ngrt_contains(const char *text, const char *needle)
{
  int64_t textLength = ngrt_str_len(text);
  int64_t needleLength = ngrt_str_len(needle);
  const char *textBytes = ngrt_str_bytes(text);
  const char *needleBytes = ngrt_str_bytes(needle);
  if (needleLength == 0)
    return 1;
  if (needleLength > textLength)
    return 0;
  for (int64_t index = 0; index + needleLength <= textLength; ++index)
    if (memcmp(textBytes + index, needleBytes, (size_t) needleLength) == 0)
      return 1;
  return 0;
}

int64_t ngrt_startsWith(const char *text, const char *prefix)
{
  int64_t prefixLength = ngrt_str_len(prefix);
  int64_t textLength = ngrt_str_len(text);
  return prefixLength <= textLength && memcmp(ngrt_str_bytes(text), ngrt_str_bytes(prefix), (size_t) prefixLength) == 0;
}

int64_t ngrt_endsWith(const char *text, const char *suffix)
{
  int64_t suffixLength = ngrt_str_len(suffix);
  int64_t textLength = ngrt_str_len(text);
  return suffixLength <= textLength &&
         memcmp(ngrt_str_bytes(text) + textLength - suffixLength, ngrt_str_bytes(suffix), (size_t) suffixLength) == 0;
}

char *ngrt_replace(const char *text, const char *needle, const char *replacement)
{
  int64_t textLength = ngrt_str_len(text);
  int64_t needleLength = ngrt_str_len(needle);
  int64_t replacementLength = ngrt_str_len(replacement);
  const char *textBytes = ngrt_str_bytes(text);
  const char *needleBytes = ngrt_str_bytes(needle);
  const char *replacementBytes = ngrt_str_bytes(replacement);
  if (needleLength == 0)
    return ngrt_new_string(textBytes, textLength);
  int64_t occurrences = 0;
  for (int64_t index = 0; index + needleLength <= textLength;)
  {
    if (memcmp(textBytes + index, needleBytes, (size_t) needleLength) == 0)
    {
      ++occurrences;
      index += needleLength;
    }
    else
      ++index;
  }
  int64_t resultLength = textLength + occurrences * (replacementLength - needleLength);
  char *result = (char *) ngrt_alloc((size_t) resultLength + 8);
  if (result == NULL)
    abort();
  *(int64_t *) result = resultLength;
  int64_t write = 0;
  int64_t index = 0;
  while (index < textLength)
  {
    if (index + needleLength <= textLength && memcmp(textBytes + index, needleBytes, (size_t) needleLength) == 0)
    {
      memcpy(result + 8 + write, replacementBytes, (size_t) replacementLength);
      write += replacementLength;
      index += needleLength;
    }
    else
      result[8 + write++] = textBytes[index++];
  }
  return result;
}

void *ngrt_split(const char *text, const char *delimiter)
{
  int64_t textLength = ngrt_str_len(text);
  int64_t delimiterLength = ngrt_str_len(delimiter);
  const char *textBytes = ngrt_str_bytes(text);
  const char *delimiterBytes = ngrt_str_bytes(delimiter);
  if (delimiterLength == 0)
  {
    /* Split on an empty delimiter returns the whole text as one part. */
    int64_t *result = (int64_t *) ngrt_alloc(sizeof(int64_t) * 3);
    if (result == NULL)
      abort();
    result[0] = 1;
    result[1] = 1;
    result[2] = (int64_t) (uintptr_t) ngrt_new_string(textBytes, textLength);
    return result;
  }
  int64_t parts = 1;
  for (int64_t index = 0; index + delimiterLength <= textLength; ++index)
    if (memcmp(textBytes + index, delimiterBytes, (size_t) delimiterLength) == 0)
    {
      ++parts;
      index += delimiterLength - 1;
    }
  int64_t *result = (int64_t *) ngrt_alloc(sizeof(int64_t) * (size_t) (parts + 2));
  if (result == NULL)
    abort();
  result[0] = parts;
  result[1] = parts;
  int64_t start = 0;
  int64_t written = 0;
  for (int64_t index = 0; index + delimiterLength <= textLength && written < parts;)
  {
    if (memcmp(textBytes + index, delimiterBytes, (size_t) delimiterLength) == 0)
    {
      result[2 + written++] = (int64_t) (uintptr_t) ngrt_new_string(textBytes + start, index - start);
      index += delimiterLength;
      start = index;
    }
    else
      ++index;
  }
  result[2 + written] = (int64_t) (uintptr_t) ngrt_new_string(textBytes + start, textLength - start);
  return result;
}

char *ngrt_join(const void *items, const char *separator)
{
  const int64_t *header = items;
  int64_t count = header[0];
  int64_t separatorLength = ngrt_str_len(separator);
  int64_t total = 0;
  for (int64_t index = 0; index < count; ++index)
  {
    total += ngrt_str_len((const char *) (uintptr_t) header[2 + index]);
    if (index + 1 < count)
      total += separatorLength;
  }
  char *result = (char *) ngrt_alloc((size_t) total + 8);
  if (result == NULL)
    abort();
  *(int64_t *) result = total;
  int64_t written = 0;
  for (int64_t index = 0; index < count; ++index)
  {
    const char *item = (const char *) (uintptr_t) header[2 + index];
    int64_t itemLength = ngrt_str_len(item);
    memcpy(result + 8 + written, ngrt_str_bytes(item), (size_t) itemLength);
    written += itemLength;
    if (index + 1 < count)
    {
      memcpy(result + 8 + written, ngrt_str_bytes(separator), (size_t) separatorLength);
      written += separatorLength;
    }
  }
  return result;
}

int64_t ngrt_regexMatch(const char *text, const char *pattern)
{
  char *cPattern = ngrt_c_string(pattern);
  regex_t compiled;
  if (regcomp(&compiled, cPattern, REG_EXTENDED) != 0)
  {
    free(cPattern);
    ngrt_fail("regexMatch: invalid pattern");
    return -1;
  }
  free(cPattern);
  char *cText = ngrt_c_string(text);
  int status = regexec(&compiled, cText, 0, NULL, 0);
  free(cText);
  regfree(&compiled);
  return status == 0;
}

/* ------------------------------------------------------------------------- */
/* std.seq                                                                   */
/* ------------------------------------------------------------------------- */

int64_t ngrt_len(const void *array)
{
  return ((const int64_t *) array)[0];
}

int64_t ngrt_sum(const void *array)
{
  const int64_t *header = array;
  int64_t total = 0;
  for (int64_t index = 0; index < header[0]; ++index)
    total += header[2 + index];
  return total;
}

int64_t ngrt_arrayContains(const void *array, int64_t value)
{
  const int64_t *header = array;
  for (int64_t index = 0; index < header[0]; ++index)
    if (header[2 + index] == value)
      return 1;
  return 0;
}

void *ngrt_reverse(const void *array)
{
  const int64_t *header = array;
  int64_t length = header[0];
  int64_t *result = (int64_t *) ngrt_alloc(sizeof(int64_t) * (size_t) (length + 2));
  if (result == NULL)
    abort();
  result[0] = length;
  result[1] = length;
  for (int64_t index = 0; index < length; ++index)
    result[2 + index] = header[2 + length - 1 - index];
  return result;
}

/* ------------------------------------------------------------------------- */
/* std.memory                                                                */
/* ------------------------------------------------------------------------- */

/* Open-addressing hash set of live handles (malloc'd cell addresses, which
 * are never zero), so handle validation is constant-time. Capacity is a
 * power of two; 0 marks an empty slot. */
static int64_t *ngrt_live_table = NULL;
static int64_t ngrt_live_capacity = 0;
static int64_t ngrt_live_count = 0;

static uint64_t ngrt_hash_handle(int64_t handle)
{
  uint64_t value = (uint64_t) handle;
  value ^= value >> 33;
  value *= 0xff51afd7ed558ccdULL;
  value ^= value >> 33;
  value *= 0xc4ceb9fe1a85ec53ULL;
  value ^= value >> 33;
  return value;
}

static int ngrt_handle_valid(int64_t handle)
{
  if (handle == 0 || ngrt_live_capacity == 0)
    return 0;
  int64_t mask = ngrt_live_capacity - 1;
  int64_t slot = (int64_t) (ngrt_hash_handle(handle) & (uint64_t) mask);
  for (int64_t probe = 0; probe < ngrt_live_capacity; ++probe)
  {
    if (ngrt_live_table[slot] == 0)
      return 0;
    if (ngrt_live_table[slot] == handle)
      return 1;
    slot = (slot + 1) & mask;
  }
  return 0;
}

static void ngrt_live_add(int64_t handle)
{
  if (ngrt_live_capacity == 0 || (ngrt_live_count + 1) * 10 >= ngrt_live_capacity * 7)
  {
    int64_t newCapacity = ngrt_live_capacity == 0 ? 16 : ngrt_live_capacity * 2;
    int64_t *grown = (int64_t *) calloc((size_t) newCapacity, sizeof(int64_t));
    if (grown == NULL)
      abort();
    int64_t newMask = newCapacity - 1;
    for (int64_t index = 0; index < ngrt_live_capacity; ++index)
    {
      int64_t value = ngrt_live_table[index];
      if (value == 0)
        continue;
      int64_t slot = (int64_t) (ngrt_hash_handle(value) & (uint64_t) newMask);
      while (grown[slot] != 0)
        slot = (slot + 1) & newMask;
      grown[slot] = value;
    }
    free(ngrt_live_table);
    ngrt_live_table = grown;
    ngrt_live_capacity = newCapacity;
  }
  int64_t mask = ngrt_live_capacity - 1;
  int64_t slot = (int64_t) (ngrt_hash_handle(handle) & (uint64_t) mask);
  while (ngrt_live_table[slot] != 0)
    slot = (slot + 1) & mask;
  ngrt_live_table[slot] = handle;
  ++ngrt_live_count;
}

static void ngrt_live_remove(int64_t handle)
{
  if (ngrt_live_capacity == 0)
    return;
  int64_t mask = ngrt_live_capacity - 1;
  int64_t slot = (int64_t) (ngrt_hash_handle(handle) & (uint64_t) mask);
  for (int64_t probe = 0; probe < ngrt_live_capacity; ++probe)
  {
    if (ngrt_live_table[slot] == 0)
      return;
    if (ngrt_live_table[slot] == handle)
    {
      ngrt_live_table[slot] = 0;
      --ngrt_live_count;
      /* Back-shift the following probe-chain elements so lookups stay
       * correct after the hole is opened. */
      int64_t hole = slot;
      int64_t next = (slot + 1) & mask;
      while (ngrt_live_table[next] != 0)
      {
        int64_t home = (int64_t) (ngrt_hash_handle(ngrt_live_table[next]) & (uint64_t) mask);
        int canShift = next > hole ? (home <= hole || home > next) : (home <= hole && home > next);
        if (canShift)
        {
          ngrt_live_table[hole] = ngrt_live_table[next];
          ngrt_live_table[next] = 0;
          hole = next;
        }
        next = (next + 1) & mask;
      }
      return;
    }
    slot = (slot + 1) & mask;
  }
}

int64_t ngrt_allocate(int64_t value)
{
  int64_t *cell = (int64_t *) ngrt_alloc(sizeof(int64_t));
  if (cell == NULL)
    abort();
  *cell = value;
  ngrt_live_add((int64_t) (uintptr_t) cell);
  return (int64_t) (uintptr_t) cell;
}

int64_t ngrt_load(int64_t handle)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return 0;
  }
  return *(int64_t *) (uintptr_t) handle;
}

int64_t ngrt_store(int64_t handle, int64_t value)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return -1;
  }
  *(int64_t *) (uintptr_t) handle = value;
  return 0;
}

int64_t ngrt_release(int64_t handle)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return -1;
  }
  ngrt_live_remove(handle);
  /* Cells are arena-owned (INC-18): the logical release above is the only
   * reclamation within the run; the physical block is returned at exit. */
  ngrt_free((void *) (uintptr_t) handle);
  return 0;
}

int64_t ngrt_outstanding(void)
{
  return ngrt_live_count;
}

/* ------------------------------------------------------------------------- */
/* std.io                                                                    */
/* ------------------------------------------------------------------------- */

char *ngrt_currentExecutablePath(void)
{
  static char buffer[4096];
  size_t length = 0;
#ifdef __linux__
  ssize_t read = readlink("/proc/self/exe", buffer, sizeof buffer - 1);
  if (read > 0)
  {
    buffer[read] = 0;
    length = (size_t) read;
  }
#elif defined(__APPLE__)
  uint32_t size = (uint32_t) sizeof buffer;
  if (_NSGetExecutablePath(buffer, &size) == 0)
  {
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) != NULL)
    {
      length = strlen(resolved);
      memcpy(buffer, resolved, length + 1);
    }
    else
    {
      length = strlen(buffer);
    }
  }
#endif
  if (length == 0)
  {
    /* No platform query available (or it failed): fall back to the process
     * working directory, preserving the previous behavior. */
    if (getcwd(buffer, sizeof buffer) == NULL)
      buffer[0] = 0;
    length = strlen(buffer);
  }
  return ngrt_new_string(buffer, (int64_t) length);
}

char *ngrt_readLine(void)
{
  char *line = NULL;
  size_t capacity = 0;
  ssize_t length = getline(&line, &capacity, stdin);
  if (length < 0)
  {
    free(line);
    return ngrt_new_string("", 0);
  }
  if (length > 0 && line[length - 1] == '\n')
    --length;
  char *result = ngrt_new_string(line, (int64_t) length);
  free(line);
  return result;
}

char *ngrt_readFile(const char *path)
{
  char *cPath = ngrt_c_string(path);
  FILE *file = fopen(cPath, "rb");
  free(cPath);
  if (file == NULL)
  {
    ngrt_fail("cannot read file");
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    ngrt_fail("cannot read file");
    return NULL;
  }
  long size = ftell(file);
  if (size < 0)
  {
    fclose(file);
    ngrt_fail("cannot read file");
    return NULL;
  }
  rewind(file);
  if (size == 0)
  {
    fclose(file);
    return ngrt_new_string("", 0);
  }
  char *buffer = (char *) malloc((size_t) size);
  if (buffer == NULL)
  {
    fclose(file);
    abort();
  }
  if (fread(buffer, 1, (size_t) size, file) != (size_t) size)
  {
    free(buffer);
    fclose(file);
    ngrt_fail("cannot read file");
    return NULL;
  }
  fclose(file);
  char *result = ngrt_new_string(buffer, (int64_t) size);
  free(buffer);
  return result;
}

int64_t ngrt_writeFile(const char *path, const char *content)
{
  char *cPath = ngrt_c_string(path);
  FILE *file = fopen(cPath, "wb");
  free(cPath);
  if (file == NULL)
  {
    ngrt_fail("cannot write file");
    return -1;
  }
  size_t expected = (size_t) ngrt_str_len(content);
  size_t written = fwrite(ngrt_str_bytes(content), 1, expected, file);
  int closeStatus = fclose(file);
  if (written != expected || closeStatus != 0)
  {
    ngrt_fail("cannot write file");
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------------- */
/* std.io: system command bindings                                            */
/* ------------------------------------------------------------------------- */

int64_t ngrt_system(const char *command)
{
  char *cCommand = ngrt_c_string(command);
  int status = system(cCommand);
  free(cCommand);
  if (status == -1)
    return 1; /* wait-status error: report the existing failure result */
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return 1;
}

char *ngrt_systemOutput(const char *command)
{
  char *cCommand = ngrt_c_string(command);
  FILE *pipe = popen(cCommand, "r");
  free(cCommand);
  if (pipe == NULL)
  {
    ngrt_fail("cannot execute command");
    return NULL;
  }
  size_t capacity = 1024;
  size_t length = 0;
  char *buffer = (char *) malloc(capacity);
  if (buffer == NULL)
    abort();
  for (;;)
  {
    if (length == capacity)
    {
      capacity *= 2;
      char *grown = (char *) realloc(buffer, capacity);
      if (grown == NULL)
        abort();
      buffer = grown;
    }
    size_t count = fread(buffer + length, 1, capacity - length, pipe);
    length += count;
    if (count == 0)
      break;
  }
  int status = pclose(pipe);
  if (status == -1)
  {
    /* wait-status error: report the existing failure result. */
    free(buffer);
    ngrt_fail("cannot execute command");
    return NULL;
  }
  if (status != 0)
  {
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    char suffix[64];
    snprintf(suffix, sizeof suffix, "[exit %d]", code);
    size_t suffixLength = strlen(suffix);
    char *result = (char *) ngrt_alloc(length + suffixLength + 8);
    if (result == NULL)
      abort();
    *(int64_t *) result = (int64_t) (length + suffixLength);
    memcpy(result + 8, buffer, length);
    memcpy(result + 8 + length, suffix, suffixLength);
    free(buffer);
    return result;
  }
  char *result = ngrt_new_string(buffer, (int64_t) length);
  free(buffer);
  return result;
}

/* ------------------------------------------------------------------------- */
/* B3 fixtures                                                               */
/* ------------------------------------------------------------------------- */

int64_t ngrt_fixture_point_sum(struct ngrt_fixture_point point)
{
  return (int64_t) point.x + (int64_t) point.y;
}

int64_t ngrt_fixture_mixed_sum(struct ngrt_fixture_mixed mixed)
{
  return (int64_t) mixed.a + (int64_t) mixed.b + (int64_t) mixed.c;
}

struct ngrt_fixture_point ngrt_fixture_point_swap(struct ngrt_fixture_point point)
{
  struct ngrt_fixture_point swapped;
  swapped.x = point.y;
  swapped.y = point.x;
  return swapped;
}

/* INC-29: `export extern "C"` end-to-end fixture. The symbol below is defined
 * by the NG-generated object (unmangled C name), so the linker resolves it. */
/* Weak fallback so the compiler binary itself links; a generated program's
 * strong `export extern "C"` definition overrides it at link time. */
__attribute__((weak)) int64_t ngrt_fixture_exported_add(int64_t a, int64_t b)
{
  return -1;
}

int64_t ngrt_fixture_call_exported_add(int64_t a, int64_t b)
{
  return ngrt_fixture_exported_add(a, b);
}

int64_t ngrt_fixture_variadic(int64_t count, ...)
{
  va_list ap;
  va_start(ap, count);
  int64_t sum = 0;
  for (int64_t index = 0; index < count; ++index)
    sum += va_arg(ap, int64_t);
  va_end(ap);
  return sum;
}
