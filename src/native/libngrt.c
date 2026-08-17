// AI-generated code; reviewed for this repository's vNext rewrite.
//
// libngrt: the single C implementation of the standard native functions.
// The QBE native tier links this archive into every generated executable;
// the VM and const evaluator call the same functions through the thin C++
// wrappers in src/ngrt.cpp.
//
// Value layouts match the Tier 0 representations emitted by
// src/native/lowering.cpp:
//   string: { int64_t len, char bytes[] }
//   array:  { int64_t len, int64_t cap, int64_t-or-pointer elements[] }
// Doubles stored in arrays are bit patterns in int64_t slots.
#define _POSIX_C_SOURCE 200809L

#include "ngrt.h"

#include <ctype.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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

void *ngrt_alloc(long size)
{
  void *result = malloc((size_t)size);
  if (result == NULL)
    abort();
  return result;
}

char *ngrt_new_string(const char *bytes, long length)
{
  char *result = (char *)malloc((size_t)length + 8);
  if (result == NULL)
    abort();
  *(long *)result = length;
  if (length > 0)
    memcpy(result + 8, bytes, (size_t)length);
  return result;
}

void ngrt_free(void *pointer)
{
  free(pointer);
}

/* ------------------------------------------------------------------------- */
/* ABI helpers                                                               */
/* ------------------------------------------------------------------------- */

static long ngrt_str_len(const char *s)
{
  return *(const long *)s;
}

static const char *ngrt_str_bytes(const char *s)
{
  return s + 8;
}

static char *ngrt_c_string(const char *value)
{
  long length = ngrt_str_len(value);
  char *result = (char *)malloc((size_t)length + 1);
  if (result == NULL)
    abort();
  memcpy(result, ngrt_str_bytes(value), (size_t)length);
  result[length] = 0;
  return result;
}

/* ------------------------------------------------------------------------- */
/* Prelude                                                                   */
/* ------------------------------------------------------------------------- */

long ngrt_print_i64(long value)
{
  printf("%ld\n", value);
  return 0;
}

long ngrt_print_u64(uint64_t value)
{
  printf("%lu\n", (unsigned long)value);
  return 0;
}

long ngrt_print_f64(double value)
{
  printf("%g\n", value);
  return 0;
}

long ngrt_print_str(const char *value)
{
  printf("%.*s\n", (int)ngrt_str_len(value), ngrt_str_bytes(value));
  return 0;
}

long ngrt_print_bool(long value)
{
  printf("%s\n", value ? "true" : "false");
  return 0;
}

long ngrt_assert(long condition)
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

long ngrt_length(const char *text)
{
  return ngrt_str_len(text);
}

char *ngrt_trim(const char *text)
{
  long length = ngrt_str_len(text);
  const char *bytes = ngrt_str_bytes(text);
  long begin = 0;
  long end = length;
  while (begin < end && isspace((unsigned char)bytes[begin]))
    ++begin;
  while (end > begin && isspace((unsigned char)bytes[end - 1]))
    --end;
  return ngrt_new_string(bytes + begin, end - begin);
}

static char *ngrt_str_case(const char *value, int upper)
{
  long length = ngrt_str_len(value);
  const char *bytes = ngrt_str_bytes(value);
  char *result = (char *)malloc((size_t)length + 8);
  if (result == NULL)
    abort();
  *(long *)result = length;
  for (long index = 0; index < length; ++index)
  {
    unsigned char byte = (unsigned char)bytes[index];
    result[8 + index] = (char)(upper ? toupper(byte) : tolower(byte));
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

char *ngrt_charAt(const char *text, long index)
{
  long length = ngrt_str_len(text);
  if (index < 0 || index >= length)
  {
    char message[128];
    snprintf(message, sizeof message, "charAt index out of bounds: index %ld, length %ld", index, length);
    ngrt_fail(message);
    return NULL;
  }
  return ngrt_new_string(ngrt_str_bytes(text) + index, 1);
}

char *ngrt_substring(const char *text, long start, long end)
{
  long length = ngrt_str_len(text);
  if (start < 0 || end < start || end > length)
  {
    char message[160];
    snprintf(message, sizeof message, "substring bounds out of range: [%ld..%ld) of length %ld", start, end,
             length);
    ngrt_fail(message);
    return NULL;
  }
  return ngrt_new_string(ngrt_str_bytes(text) + start, end - start);
}

long ngrt_contains(const char *text, const char *needle)
{
  long textLength = ngrt_str_len(text);
  long needleLength = ngrt_str_len(needle);
  const char *textBytes = ngrt_str_bytes(text);
  const char *needleBytes = ngrt_str_bytes(needle);
  if (needleLength == 0)
    return 1;
  if (needleLength > textLength)
    return 0;
  for (long index = 0; index + needleLength <= textLength; ++index)
    if (memcmp(textBytes + index, needleBytes, (size_t)needleLength) == 0)
      return 1;
  return 0;
}

long ngrt_startsWith(const char *text, const char *prefix)
{
  long prefixLength = ngrt_str_len(prefix);
  long textLength = ngrt_str_len(text);
  return prefixLength <= textLength &&
         memcmp(ngrt_str_bytes(text), ngrt_str_bytes(prefix), (size_t)prefixLength) == 0;
}

long ngrt_endsWith(const char *text, const char *suffix)
{
  long suffixLength = ngrt_str_len(suffix);
  long textLength = ngrt_str_len(text);
  return suffixLength <= textLength &&
         memcmp(ngrt_str_bytes(text) + textLength - suffixLength, ngrt_str_bytes(suffix),
                (size_t)suffixLength) == 0;
}

char *ngrt_replace(const char *text, const char *needle, const char *replacement)
{
  long textLength = ngrt_str_len(text);
  long needleLength = ngrt_str_len(needle);
  long replacementLength = ngrt_str_len(replacement);
  const char *textBytes = ngrt_str_bytes(text);
  const char *needleBytes = ngrt_str_bytes(needle);
  const char *replacementBytes = ngrt_str_bytes(replacement);
  if (needleLength == 0)
    return ngrt_new_string(textBytes, textLength);
  long occurrences = 0;
  for (long index = 0; index + needleLength <= textLength;)
  {
    if (memcmp(textBytes + index, needleBytes, (size_t)needleLength) == 0)
    {
      ++occurrences;
      index += needleLength;
    }
    else
      ++index;
  }
  long resultLength = textLength + occurrences * (replacementLength - needleLength);
  char *result = (char *)malloc((size_t)resultLength + 8);
  if (result == NULL)
    abort();
  *(long *)result = resultLength;
  long write = 0;
  long index = 0;
  while (index < textLength)
  {
    if (index + needleLength <= textLength &&
        memcmp(textBytes + index, needleBytes, (size_t)needleLength) == 0)
    {
      memcpy(result + 8 + write, replacementBytes, (size_t)replacementLength);
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
  long textLength = ngrt_str_len(text);
  long delimiterLength = ngrt_str_len(delimiter);
  const char *textBytes = ngrt_str_bytes(text);
  const char *delimiterBytes = ngrt_str_bytes(delimiter);
  if (delimiterLength == 0)
  {
    /* Split on an empty delimiter returns the whole text as one part. */
    long *result = (long *)malloc(sizeof(long) * 3);
    if (result == NULL)
      abort();
    result[0] = 1;
    result[1] = 1;
    result[2] = (long)(uintptr_t)ngrt_new_string(textBytes, textLength);
    return result;
  }
  long parts = 1;
  for (long index = 0; index + delimiterLength <= textLength; ++index)
    if (memcmp(textBytes + index, delimiterBytes, (size_t)delimiterLength) == 0)
    {
      ++parts;
      index += delimiterLength - 1;
    }
  long *result = (long *)malloc(sizeof(long) * (size_t)(parts + 2));
  if (result == NULL)
    abort();
  result[0] = parts;
  result[1] = parts;
  long start = 0;
  long written = 0;
  for (long index = 0; index + delimiterLength <= textLength && written < parts;)
  {
    if (memcmp(textBytes + index, delimiterBytes, (size_t)delimiterLength) == 0)
    {
      result[2 + written++] = (long)(uintptr_t)ngrt_new_string(textBytes + start, index - start);
      index += delimiterLength;
      start = index;
    }
    else
      ++index;
  }
  result[2 + written] = (long)(uintptr_t)ngrt_new_string(textBytes + start, textLength - start);
  return result;
}

char *ngrt_join(const void *items, const char *separator)
{
  const long *header = items;
  long count = header[0];
  long separatorLength = ngrt_str_len(separator);
  long total = 0;
  for (long index = 0; index < count; ++index)
  {
    total += ngrt_str_len((const char *)(uintptr_t)header[2 + index]);
    if (index + 1 < count)
      total += separatorLength;
  }
  char *result = (char *)malloc((size_t)total + 8);
  if (result == NULL)
    abort();
  *(long *)result = total;
  long written = 0;
  for (long index = 0; index < count; ++index)
  {
    const char *item = (const char *)(uintptr_t)header[2 + index];
    long itemLength = ngrt_str_len(item);
    memcpy(result + 8 + written, ngrt_str_bytes(item), (size_t)itemLength);
    written += itemLength;
    if (index + 1 < count)
    {
      memcpy(result + 8 + written, ngrt_str_bytes(separator), (size_t)separatorLength);
      written += separatorLength;
    }
  }
  return result;
}

long ngrt_regexMatch(const char *text, const char *pattern)
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

long ngrt_len(const void *array)
{
  return ((const long *)array)[0];
}

long ngrt_sum(const void *array)
{
  const long *header = array;
  long total = 0;
  for (long index = 0; index < header[0]; ++index)
    total += header[2 + index];
  return total;
}

long ngrt_arrayContains(const void *array, long value)
{
  const long *header = array;
  for (long index = 0; index < header[0]; ++index)
    if (header[2 + index] == value)
      return 1;
  return 0;
}

void *ngrt_reverse(const void *array)
{
  const long *header = array;
  long length = header[0];
  long *result = (long *)malloc(sizeof(long) * (size_t)(length + 2));
  if (result == NULL)
    abort();
  result[0] = length;
  result[1] = length;
  for (long index = 0; index < length; ++index)
    result[2 + index] = header[2 + length - 1 - index];
  return result;
}

/* ------------------------------------------------------------------------- */
/* std.memory                                                                */
/* ------------------------------------------------------------------------- */

static long *ngrt_live_cells = NULL;
static long ngrt_live_count = 0;
static long ngrt_live_capacity = 0;

static int ngrt_handle_valid(long handle)
{
  if (handle == 0)
    return 0;
  for (long index = 0; index < ngrt_live_count; ++index)
    if (ngrt_live_cells[index] == handle)
      return 1;
  return 0;
}

static void ngrt_live_add(long handle)
{
  if (ngrt_live_count == ngrt_live_capacity)
  {
    long newCapacity = ngrt_live_capacity == 0 ? 16 : ngrt_live_capacity * 2;
    long *grown = (long *)realloc(ngrt_live_cells, sizeof(long) * (size_t)newCapacity);
    if (grown == NULL)
      abort();
    ngrt_live_cells = grown;
    ngrt_live_capacity = newCapacity;
  }
  ngrt_live_cells[ngrt_live_count++] = handle;
}

static void ngrt_live_remove(long handle)
{
  for (long index = 0; index < ngrt_live_count; ++index)
    if (ngrt_live_cells[index] == handle)
    {
      ngrt_live_cells[index] = ngrt_live_cells[ngrt_live_count - 1];
      --ngrt_live_count;
      return;
    }
}

long ngrt_allocate(long value)
{
  long *cell = (long *)malloc(sizeof(long));
  if (cell == NULL)
    abort();
  *cell = value;
  ngrt_live_add((long)(uintptr_t)cell);
  return (long)(uintptr_t)cell;
}

long ngrt_load(long handle)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return 0;
  }
  return *(long *)(uintptr_t)handle;
}

long ngrt_store(long handle, long value)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return -1;
  }
  *(long *)(uintptr_t)handle = value;
  return 0;
}

long ngrt_release(long handle)
{
  if (!ngrt_handle_valid(handle))
  {
    ngrt_fail("invalid heap handle");
    return -1;
  }
  ngrt_live_remove(handle);
  free((void *)(uintptr_t)handle);
  return 0;
}

long ngrt_outstanding(void)
{
  return ngrt_live_count;
}

/* ------------------------------------------------------------------------- */
/* std.io                                                                    */
/* ------------------------------------------------------------------------- */

char *ngrt_currentExecutablePath(void)
{
  static char buffer[4096];
  if (getcwd(buffer, sizeof buffer) == NULL)
    buffer[0] = 0;
  return ngrt_new_string(buffer, (long)strlen(buffer));
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
  char *result = ngrt_new_string(line, (long)length);
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
  char *buffer = (char *)malloc((size_t)size);
  if (buffer == NULL)
  {
    fclose(file);
    abort();
  }
  if (fread(buffer, 1, (size_t)size, file) != (size_t)size)
  {
    free(buffer);
    fclose(file);
    ngrt_fail("cannot read file");
    return NULL;
  }
  fclose(file);
  char *result = ngrt_new_string(buffer, size);
  free(buffer);
  return result;
}

long ngrt_writeFile(const char *path, const char *content)
{
  char *cPath = ngrt_c_string(path);
  FILE *file = fopen(cPath, "wb");
  free(cPath);
  if (file == NULL)
  {
    ngrt_fail("cannot write file");
    return -1;
  }
  fwrite(ngrt_str_bytes(content), 1, (size_t)ngrt_str_len(content), file);
  fclose(file);
  return 0;
}


/* ------------------------------------------------------------------------- */
/* std.io: system command bindings                                            */
/* ------------------------------------------------------------------------- */

long ngrt_system(const char *command)
{
  char *cCommand = ngrt_c_string(command);
  int status = system(cCommand);
  free(cCommand);
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
  char *buffer = (char *)malloc(capacity);
  if (buffer == NULL)
    abort();
  for (;;)
  {
    if (length == capacity)
    {
      capacity *= 2;
      char *grown = (char *)realloc(buffer, capacity);
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
  if (status != 0)
  {
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    char suffix[64];
    snprintf(suffix, sizeof suffix, "[exit %d]", code);
    size_t suffixLength = strlen(suffix);
    char *result = (char *)malloc(length + suffixLength + 8);
    if (result == NULL)
      abort();
    *(long *)result = (long)(length + suffixLength);
    memcpy(result + 8, buffer, length);
    memcpy(result + 8 + length, suffix, suffixLength);
    free(buffer);
    return result;
  }
  char *result = ngrt_new_string(buffer, (long)length);
  free(buffer);
  return result;
}

/* ------------------------------------------------------------------------- */
/* B3 fixtures                                                               */
/* ------------------------------------------------------------------------- */

int64_t ngrt_fixture_point_sum(struct ngrt_fixture_point point)
{
  return (int64_t)point.x + (int64_t)point.y;
}

int64_t ngrt_fixture_mixed_sum(struct ngrt_fixture_mixed mixed)
{
  return (int64_t)mixed.a + (int64_t)mixed.b + (int64_t)mixed.c;
}

struct ngrt_fixture_point ngrt_fixture_point_swap(struct ngrt_fixture_point point)
{
  struct ngrt_fixture_point swapped;
  swapped.x = point.y;
  swapped.y = point.x;
  return swapped;
}
