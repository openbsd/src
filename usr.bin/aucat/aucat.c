/*	$OpenBSD: aucat.c,v 1.1 1997/01/02 22:12:27 kstailey Exp $	*/
/*
 * Copyright (c) 1997 Kenneth Stailey.  All rights reserved.
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
 *	This product includes software developed by Kenneth Stailey.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <machine/endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* 
 * aucat: concatinate and play Sun 8-bit .au files
 */

/* function playfile: given a file which is positioned at the beginning
 * of what is assumed to be an .au data stream copy it out to the audio
 * device.  Return 0 on sucess, -1 on failure.
 */
int
playfile(int fd)
{
  static int afd = -1;
  int rd;
  char buf[5120];

  if (afd == -1)
    if ((afd = open("/dev/audio", O_WRONLY)) < 0) {
      perror("open /dev/audio");
      return(-1);
    }
  while ((rd = read(fd, buf, sizeof(buf))) > 0) {
    write(afd, buf, rd);
    if (rd < sizeof(buf))
      break;
  }
  
  return (0);
}

int
main(int argc, char **argv)
{
  int fd;
  int argcInc = 0;		/* incrementing version of argc */
  unsigned long data;
  char magic[5];

  while (--argc) {
    ++argcInc;
    if ((fd = open(argv[argcInc], O_RDONLY)) < 0) {
      perror("open file");
      exit(1);
    }

    read(fd, magic, 4);
    magic[4] = '\0';
    if (strcmp(magic, ".snd")) {
      /* not an .au file, bad header.  Assume raw audio data since that's
       * what /dev/audio generates by default.
       */
      lseek(fd, 0, SEEK_SET);
    } else {
      read(fd, &data, sizeof(data));
      if (BYTE_ORDER != BIG_ENDIAN)
	data = htonl(data);
      lseek(fd, (off_t)data, SEEK_SET);
    }
    if (playfile(fd) < 0)
      exit(1);
  }
  exit(0);
}
