#ifndef DS_H
#define DS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <assert.h>
#include <stdio.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef ptrdiff_t ssize;

#define arrlen(arr) (ssize) (sizeof(arr)/sizeof(arr[0]))

typedef struct {
  u8 *buf;
  ssize cap, len;
} Arena;

typedef struct {
  u8 *buf;
  ssize len;
} s8;

#define List(T) struct { T *buf; ssize len, cap; }

Arena new_arena(ssize cap);
#define new(a, T, c) arena_alloc(a, sizeof(T), _Alignof(T), c)
void *arena_alloc(Arena *a, ssize size, ssize align, ssize count);

#define s8(lit) (s8){ .buf = (u8 *) lit, .len = arrlen(lit) - 1, }
s8 s8_copy(Arena *perm, s8 s);
void s8_modcat(Arena *perm, s8 *a, s8 b);
s8 s8_newcat(Arena *perm, s8 a, s8 b);
bool s8_equals(s8 a, s8 b);
void s8_print(s8 s);
u64 s8_hash(s8 s);

#define strify(c) #c

#endif // DS_H

#ifdef DS_IMPL
#ifndef DS_IMPL_GUARD
#define DS_IMPL_GUARD

Arena new_arena(ssize cap) {
  return (Arena) { .buf = malloc(cap), .cap = cap, };
}

void *arena_alloc(Arena *a, ssize size, ssize align, ssize count) {
  assert(align >= 0);
  assert(size >= 0);
  assert(count >= 0);

  ssize pad = -(uintptr_t)(&a->buf[a->len]) & (align - 1);
  ssize available = a->cap - a->len - pad;

  assert(available >= 0);
  assert(count <= available/size);

  void *p = &a->buf[a->len] + pad;
  a->len += pad + count * size;
  return memset(p, 0, count * size);
}

s8 s8_copy(Arena *perm, s8 s) {
  s8 copy = s;
  copy.buf = new(perm, u8, copy.len);
  memmove(copy.buf, s.buf, copy.len * sizeof(u8));
  return copy;
}

void s8_modcat(Arena *perm, s8 *a, s8 b) {
  assert(&a->buf[a->len] == &perm->buf[perm->len]);
  s8_copy(perm, b);
  a->len += b.len;
}

s8 s8_newcat(Arena *perm, s8 a, s8 b) {
  s8 c = s8_copy(perm, a);
  s8_modcat(perm, &c, b);
  return c;
}

bool s8_equals(s8 a, s8 b) {
  if (a.len != b.len) return false;
  if (a.buf == NULL || b.buf == NULL) return false;
  for (int i = 0; i < a.len; i++) {
    if (a.buf[i] != b.buf[i]) return false;
  }
  return true;
}

void s8_print(s8 s) {
  for (ssize i = 0; i < s.len; i++) {
    printf("%c", s.buf[i]);
  }
}

u64 s8_hash(s8 s) {
  u64 h = 0x100;
  for (ssize i = 0; i < s.len; i++) {
    h ^= s.buf[i];
    h *= 1111111111111111111u;
  }
  return h;
}

#endif // DS_IMPL_GUARD
#endif // DS_IMPL
