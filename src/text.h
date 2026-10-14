// text.h - small text helpers for the dialogue system (no standard library here).
#pragma once
#include "core.h"

namespace sky::text {

inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
inline bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\''; }
inline bool upper(char c) { return c >= 'A' && c <= 'Z'; }
inline int len(const char* s) { int n = 0; while (s[n]) n++; return n; }

/** Copies at most cap-1 bytes and terminates. Returns the length written. */
inline int copy(char* dst, int cap, const char* src, int n = -1) {
  if (n < 0) n = len(src);
  n = min(n, cap - 1);
  memcpy(dst, src, n);
  dst[n] = 0;
  return n;
}

/** Lowercased copy with runs of whitespace squeezed, for matching. */
inline int fold(char* dst, int cap, const char* src, int n) {
  int o = 0;
  bool space = false;
  for (int i = 0; i < n && o < cap - 1; i++) {
    char c = src[i];
    if (c == '\n' || c == '\t' || c == '\r') c = ' ';
    if (c == ' ') { if (space || o == 0) continue; space = true; } else space = false;
    dst[o++] = lower(c);
  }
  while (o > 0 && dst[o - 1] == ' ') o--;
  dst[o] = 0;
  return o;
}

/** Does `hay` (already lowercase) contain `needle` (lowercase), starting and ending on word edges? */
inline bool has_word(const char* hay, const char* needle) {
  int n = len(needle);
  if (n == 0) return false;
  for (int i = 0; hay[i]; i++) {
    if (i > 0 && letter(hay[i - 1])) continue;
    int k = 0;
    while (k < n && hay[i + k] && hay[i + k] == needle[k]) k++;
    if (k == n && !letter(hay[i + n])) return true;
  }
  return false;
}

/** Plain substring search on lowercase text. */
inline bool contains(const char* hay, const char* needle) {
  int n = len(needle);
  for (int i = 0; hay[i]; i++) {
    int k = 0;
    while (k < n && hay[i + k] == needle[k]) k++;
    if (k == n) return true;
  }
  return false;
}

inline int count_words(const char* hay, const char* const* words, int n) {
  int c = 0;
  for (int i = 0; i < n; i++) c += has_word(hay, words[i]) ? 1 : 0;
  return c;
}

inline u32 hash(const char* s) {
  u32 h = 2166136261u;
  for (int i = 0; s[i]; i++) { h ^= (u8)lower(s[i]); h *= 16777619u; }
  return h;
}

}  // namespace sky::text
