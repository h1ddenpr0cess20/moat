/* Small string helpers, included by the banks that draw text (a copy in each costs less than fixed-bank space). */
#ifndef STRUTIL_H
#define STRUTIL_H
static u8 utoa(u16 v, char *o) {
  char t[6];
  u8 n = 0, i = 0;
  if (!v) { o[0] = '0'; o[1] = 0; return 1; }
  while (v) { t[n++] = '0' + v % 10; v /= 10; }
  while (n) o[i++] = t[--n];
  o[i] = 0;
  return i;
}
static u8 scat(char *d, u8 n, const char *s) { while (*s && n < 33) d[n++] = *s++; d[n] = 0; return n; }
static u8 ncat(char *d, u8 n, u16 v) { return n + utoa(v, d + n); }
#endif
