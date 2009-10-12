#include <stdio.h>
#include <stdlib.h>

static const char PL_uuemap[]
= "`!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

typedef unsigned char U8;

/* This will ensure it is all zeros.  */
static char PL_uudmap[256];

int main() {
  size_t i;
  char *p;

  for (i = 0; i < sizeof(PL_uuemap) - 1; ++i)
    PL_uudmap[(U8)PL_uuemap[i]] = (char)i;
  /*
   * Because ' ' and '`' map to the same value,
   * we need to decode them both the same.
   */
  PL_uudmap[(U8)' '] = 0;

  i = sizeof(PL_uudmap);
  p = PL_uudmap;

  fputs("{\n    ", stdout);
  while (i--) {
    printf("%d", *p);
    p++;
    if (i) {
      fputs(", ", stdout);
      if (!(i & 15)) {
	fputs("\n    ", stdout);
      }
    }
  }
  puts("\n}");

  return 0;
}

  
