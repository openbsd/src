static char *rcs_id = "$Id: use_adf.c,v 1.5 2000/02/01 03:24:14 deraadt Exp $";
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
 * use_adf.c: make the scanner use the ADF as the next paper source
 */

#include <sys/param.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
# include <sys/ioctl.h>
#endif
#include <sys/scanio.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>


void
usage(char *prog_name)
{
  fprintf(stderr, "usage: %s [-l <logical name>]\n", prog_name);
  exit(-1);
}

main(int argc, char *argv[])
{
  int sfd;
  char *logical_name = "scan0";
  char device[MAXPATHLEN];

  int c;
  extern int optind;
  extern char *optarg;


  while ((c = getopt(argc, argv, "l:")) != -1) {
    switch (c) {
    case 'l':
      logical_name = optarg;
      break;

    case '?':
      usage(argv[0]);
      break;
    }
  }

  if (snprintf(device, sizeof device, "/dev/%s", logical_name) >=
    sizeof device) {
    fprintf(stderr, "%s: name to long", device);
    exit(1);
  }
	
  strcpy(device, "/dev/");
  strcat(device, logical_name);

  if ((sfd = open(device, O_RDONLY)) < 0) {
    fprintf(stderr, "open of %s failed: ", device);
    perror("");
    exit(errno);
  }

  if (ioctl(sfd, SCIOC_USE_ADF, NULL) < 0) {
    int sv_errno = errno;

    switch (errno) {
    case ENOTTY:
      fprintf(stderr, "use_adf: scanner does not support ADF\n");
      break;
    case EIO:
      fprintf(stderr, "use_adf: ADF probably empty or jammed\n");
      break;
    default:
      perror("use_adf: ioctl SCAN_USE_ADF failed");
    }
    exit(sv_errno);
  }

  close(sfd);
  exit(0);
}
