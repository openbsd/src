/*	$NetBSD: genprom.c,v 1.3 1994/10/27 04:21:13 cgd Exp $	*/

/*
 * Read a binary image of a bios extension, generate the
 * appropriate block count and checksum and write them
 * into the rom image (replacing 2nd and 5th bytes)
 * The binary image should be sized before being filtered
 * through this routine.
 */

#include <stdio.h>

#define USE_SIZE ROM_SIZE
#define PROM_SIZE 0x10000

main() {
  char w[PROM_SIZE], ck;
  int i, sum;
  memset(w, 0xff, PROM_SIZE);
  i = fread(w, 1, PROM_SIZE, stdin);
  fprintf(stderr, "bios extension size: %d (0x%x), read %d bytes\n",
	  USE_SIZE, USE_SIZE, i);
  w[2] = USE_SIZE / 512;
  for (sum=0, i=0; i<USE_SIZE; i++) {
    sum += w[i];
  }
  w[5] = -sum;
  for (ck=0, i=0; i<USE_SIZE; i++) {
    ck += w[i];
  }
  fwrite(w, 1, PROM_SIZE, stdout);
}
  
