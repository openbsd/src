static char *rcs_id = "$Id: fs1toppm.c,v 1.2 2002/06/01 20:27:15 deraadt Exp $";
/*
 * Copyright (c) 1995 Kenneth Stailey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Kenneth Stailey
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * fs1toppm.c: convert output from Ricoh FS1 into PPM format
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char fp_buf[262144];
char out_buf[262144];

void usage();
int read_rgb(FILE *, char *, int);

main(int argc, char *argv[])
{
  int width;
  FILE *fp;
  char rgb[3];

  if (argc != 4) {
    usage();
    exit(1);
  }

  width = atoi(argv[1]);

  if ((fp = fopen(argv[3], "r")) == NULL) {
    perror("open");
    exit(1);
  }
  setbuffer(fp, fp_buf, 262144);
  setbuffer(stdout, out_buf, 262144);

  printf("P6\n%d %d 255\n", width, atoi(argv[2]));

  while(read_rgb(fp, rgb, width) != EOF) {
    fwrite(rgb, 3, 1, stdout);
  }

  fclose(fp);
}

int read_rgb(FILE *fp, char *rgb, int width)
{
  static int cur_pos = 0;

  if (fseek(fp, cur_pos, SEEK_SET) != 0) {
    perror("fseek red");
    exit(1);
  }
  if (fread(&rgb[0], 1, 1, fp) != 1)
    return EOF;

  if (fseek(fp, cur_pos + width, SEEK_SET) != 0) {
    perror("fseek green");
    exit(1);
  }
  if (fread(&rgb[1], 1, 1, fp) != 1)
    return EOF;

  if (fseek(fp, cur_pos + width * 2, SEEK_SET) != 0) {
    perror("fseek blue");
    exit(1);
  }
  if (fread(&rgb[2], 1, 1, fp) != 1)
    return EOF;

  if (++cur_pos % width == 0)
    cur_pos += width * 2;

  return 0;
}

void usage()
{
  fprintf(stderr, "usage: fs1toppm width height fs1_file\n");
}
