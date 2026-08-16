// AI-generated code; reviewed for this repository's vNext rewrite.
//
// AOT shim implementations of the standard native functions for the QBE
// native tier (linked into every `ngi --native` executable). Value layouts
// match the Tier 0 representations emitted by src/native/lowering.cpp:
//   string: { int64_t len, char bytes[len] }
//   array:  { int64_t len, int64_t cap, int64_t elements[] }
// Opaque handles are plain pointers to allocated cells. Doubles stored in
// arrays are bit patterns in int64_t slots.
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static long shim_str_len(const char *s) { return *(const long *)s; }
static const char *shim_str_bytes(const char *s) { return s + 8; }

static char *shim_new_string(long len, const char *bytes)
{
  char *result = malloc((size_t)len + 8);
  if (result == NULL)
    abort();
  *(long *)result = len;
  if (len > 0)
    memcpy(result + 8, bytes, (size_t)len);
  return result;
}

long ngshim_print_i64(long value)
{
  printf("%ld\n", value);
  return 0;
}

long ngshim_print_u64(uint64_t value)
{
  printf("%lu\n", (unsigned long)value);
  return 0;
}

long ngshim_print_f64(double value)
{
  printf("%g\n", value);
  return 0;
}

long ngshim_print_str(const char *value)
{
  printf("%.*s\n", (int)shim_str_len(value), shim_str_bytes(value));
  return 0;
}

long ngshim_print_bool(long value)
{
  printf("%s\n", value ? "true" : "false");
  return 0;
}

void ngshim_assert(long condition)
{
  if (!condition)
  {
    fprintf(stderr, "assertion failed\n");
    abort();
  }
}

long ngshim_str_len(const char *value) { return shim_str_len(value); }

char *ngshim_str_trim(const char *value)
{
  long length = shim_str_len(value);
  const char *bytes = shim_str_bytes(value);
  long begin = 0;
  long end = length;
  while (begin < end && isspace((unsigned char)bytes[begin]))
    ++begin;
  while (end > begin && isspace((unsigned char)bytes[end - 1]))
    --end;
  return shim_new_string(end - begin, bytes + begin);
}

static char *shim_str_case(const char *value, int upper)
{
  long length = shim_str_len(value);
  const char *bytes = shim_str_bytes(value);
  char *result = malloc((size_t)length + 8);
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

char *ngshim_str_to_upper(const char *value) { return shim_str_case(value, 1); }
char *ngshim_str_to_lower(const char *value) { return shim_str_case(value, 0); }

char *ngshim_str_char_at(const char *value, long index)
{
  long length = shim_str_len(value);
  if (index < 0 || index >= length)
  {
    fprintf(stderr, "charAt index out of bounds\n");
    abort();
  }
  return shim_new_string(1, shim_str_bytes(value) + index);
}

char *ngshim_str_substring(const char *value, long start, long end)
{
  long length = shim_str_len(value);
  if (start < 0 || end < start || end > length)
  {
    fprintf(stderr, "substring bounds out of range\n");
    abort();
  }
  return shim_new_string(end - start, shim_str_bytes(value) + start);
}

long ngshim_str_contains(const char *value, const char *needle)
{
  long valueLength = shim_str_len(value);
  long needleLength = shim_str_len(needle);
  const char *valueBytes = shim_str_bytes(value);
  const char *needleBytes = shim_str_bytes(needle);
  if (needleLength == 0)
    return 1;
  if (needleLength > valueLength)
    return 0;
  for (long index = 0; index + needleLength <= valueLength; ++index)
    if (memcmp(valueBytes + index, needleBytes, (size_t)needleLength) == 0)
      return 1;
  return 0;
}

long ngshim_str_starts_with(const char *value, const char *prefix)
{
  long prefixLength = shim_str_len(prefix);
  long valueLength = shim_str_len(value);
  return prefixLength <= valueLength && memcmp(shim_str_bytes(value), shim_str_bytes(prefix), (size_t)prefixLength) == 0;
}

long ngshim_str_ends_with(const char *value, const char *suffix)
{
  long suffixLength = shim_str_len(suffix);
  long valueLength = shim_str_len(value);
  return suffixLength <= valueLength &&
         memcmp(shim_str_bytes(value) + valueLength - suffixLength, shim_str_bytes(suffix), (size_t)suffixLength) == 0;
}

char *ngshim_str_replace(const char *value, const char *needle, const char *replacement)
{
  long valueLength = shim_str_len(value);
  long needleLength = shim_str_len(needle);
  long replacementLength = shim_str_len(replacement);
  const char *valueBytes = shim_str_bytes(value);
  const char *needleBytes = shim_str_bytes(needle);
  const char *replacementBytes = shim_str_bytes(replacement);
  if (needleLength == 0)
    return shim_new_string(valueLength, valueBytes);
  long occurrences = 0;
  for (long index = 0; index + needleLength <= valueLength;)
  {
    if (memcmp(valueBytes + index, needleBytes, (size_t)needleLength) == 0)
    {
      ++occurrences;
      index += needleLength;
    }
    else
      ++index;
  }
  long resultLength = valueLength + occurrences * (replacementLength - needleLength);
  char *result = malloc((size_t)resultLength + 8);
  if (result == NULL)
    abort();
  *(long *)result = resultLength;
  long write = 0;
  long index = 0;
  while (index < valueLength)
  {
    if (index + needleLength <= valueLength &&
        memcmp(valueBytes + index, needleBytes, (size_t)needleLength) == 0)
    {
      memcpy(result + 8 + write, replacementBytes, (size_t)replacementLength);
      write += replacementLength;
      index += needleLength;
    }
    else
      result[8 + write++] = valueBytes[index++];
  }
  return result;
}

void *ngshim_str_split(const char *text, const char *delimiter)
{
  long textLength = shim_str_len(text);
  long delimiterLength = shim_str_len(delimiter);
  const char *textBytes = shim_str_bytes(text);
  const char *delimiterBytes = shim_str_bytes(delimiter);
  if (delimiterLength == 0)
  {
    long *result = malloc(sizeof(long) * 2);
    if (result == NULL)
      abort();
    result[0] = 0;
    result[1] = 0;
    return result;
  }
  long parts = 1;
  for (long index = 0; index + delimiterLength <= textLength; ++index)
    if (memcmp(textBytes + index, delimiterBytes, (size_t)delimiterLength) == 0)
    {
      ++parts;
      index += delimiterLength - 1;
    }
  long *result = malloc(sizeof(long) * (size_t)(parts + 2));
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
      result[2 + written++] = (long)(uintptr_t)shim_new_string(index - start, textBytes + start);
      index += delimiterLength;
      start = index;
    }
    else
      ++index;
  }
  result[2 + written] = (long)(uintptr_t)shim_new_string(textLength - start, textBytes + start);
  return result;
}

char *ngshim_str_join(const void *items, const char *separator)
{
  const long *header = items;
  long count = header[0];
  long separatorLength = shim_str_len(separator);
  long total = 0;
  for (long index = 0; index < count; ++index)
  {
    total += shim_str_len((const char *)(uintptr_t)header[2 + index]);
    if (index + 1 < count)
      total += separatorLength;
  }
  char *result = malloc((size_t)total + 8);
  if (result == NULL)
    abort();
  *(long *)result = total;
  long written = 0;
  for (long index = 0; index < count; ++index)
  {
    const char *item = (const char *)(uintptr_t)header[2 + index];
    long itemLength = shim_str_len(item);
    memcpy(result + 8 + written, shim_str_bytes(item), (size_t)itemLength);
    written += itemLength;
    if (index + 1 < count)
    {
      memcpy(result + 8 + written, shim_str_bytes(separator), (size_t)separatorLength);
      written += separatorLength;
    }
  }
  return result;
}

long ngshim_arr_sum(const void *array)
{
  const long *header = array;
  long total = 0;
  for (long index = 0; index < header[0]; ++index)
    total += header[2 + index];
  return total;
}

long ngshim_arr_contains(const void *array, long value)
{
  const long *header = array;
  for (long index = 0; index < header[0]; ++index)
    if (header[2 + index] == value)
      return 1;
  return 0;
}

void *ngshim_arr_reverse(const void *array)
{
  const long *header = array;
  long length = header[0];
  long *result = malloc(sizeof(long) * (size_t)(length + 2));
  if (result == NULL)
    abort();
  result[0] = length;
  result[1] = length;
  for (long index = 0; index < length; ++index)
    result[2 + index] = header[2 + length - 1 - index];
  return result;
}

static long shim_outstanding = 0;

long ngshim_allocate(long value)
{
  long *cell = malloc(sizeof(long));
  if (cell == NULL)
    abort();
  *cell = value;
  ++shim_outstanding;
  return (long)(uintptr_t)cell;
}

long ngshim_load(long handle) { return *(long *)(uintptr_t)handle; }

void ngshim_store(long handle, long value) { *(long *)(uintptr_t)handle = value; }

void ngshim_release(long handle)
{
  free((void *)(uintptr_t)handle);
  --shim_outstanding;
}

long ngshim_outstanding(void) { return shim_outstanding; }

char *ngshim_current_executable_path(void)
{
  static char buffer[4096];
  if (getcwd(buffer, sizeof buffer) == NULL)
    buffer[0] = 0;
  return shim_new_string((long)strlen(buffer), buffer);
}

char *ngshim_read_line(void)
{
  char *line = NULL;
  size_t capacity = 0;
  ssize_t length = getline(&line, &capacity, stdin);
  if (length < 0)
  {
    free(line);
    return shim_new_string(0, "");
  }
  if (length > 0 && line[length - 1] == '\n')
    --length;
  char *result = shim_new_string((long)length, line);
  free(line);
  return result;
}

static char *shim_c_string(const char *value)
{
  long length = shim_str_len(value);
  char *result = malloc((size_t)length + 1);
  if (result == NULL)
    abort();
  memcpy(result, shim_str_bytes(value), (size_t)length);
  result[length] = 0;
  return result;
}

// POSIX extended regular expressions approximate the std::regex ECMAScript
// semantics the VM uses; documented deviation until a full engine lands.
long ngshim_regex_match(const char *text, const char *pattern)
{
  char *cPattern = shim_c_string(pattern);
  regex_t compiled;
  if (regcomp(&compiled, cPattern, REG_EXTENDED) != 0)
  {
    free(cPattern);
    fprintf(stderr, "invalid regex pattern\n");
    abort();
  }
  free(cPattern);
  char *cText = shim_c_string(text);
  int status = regexec(&compiled, cText, 0, NULL, 0);
  free(cText);
  regfree(&compiled);
  return status == 0;
}

char *ngshim_read_file(const char *path)
{
  char *cPath = shim_c_string(path);
  FILE *file = fopen(cPath, "rb");
  free(cPath);
  if (file == NULL)
  {
    fprintf(stderr, "cannot read file\n");
    abort();
  }
  if (fseek(file, 0, SEEK_END) != 0)
    abort();
  long size = ftell(file);
  rewind(file);
  char *buffer = malloc((size_t)size);
  if (buffer == NULL)
    abort();
  if (fread(buffer, 1, (size_t)size, file) != (size_t)size)
    abort();
  fclose(file);
  char *result = shim_new_string(size, buffer);
  free(buffer);
  return result;
}

void ngshim_write_file(const char *path, const char *content)
{
  char *cPath = shim_c_string(path);
  FILE *file = fopen(cPath, "wb");
  free(cPath);
  if (file == NULL)
  {
    fprintf(stderr, "cannot write file\n");
    abort();
  }
  fwrite(shim_str_bytes(content), 1, (size_t)shim_str_len(content), file);
  fclose(file);
}

/* B3 first slice: C-ABI fixtures for `extern "C"` / `repr(C)` end-to-end
   tests. The struct layouts intentionally match NG `repr(C)` records —
   QBE's backend classifies the aggregate by value from the IL type, so
   these fixtures prove the whole boundary (layout, padding, register/
   memory classification) against a compiled C definition. */

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
