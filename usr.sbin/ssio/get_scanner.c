static char *rcs_id = "$Id: get_scanner.c,v 1.4 2002/05/29 19:01:47 deraadt Exp $";
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
 * get_scanner.c: retrieve the a scanner's parameters
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#if defined (__NetBSD__) || defined(__OpenBSD__)
#include <sys/ioctl.h>
#endif
#include <sys/scanio.h>

int print_type(int type) {
  static char *scanners[] = {
    "kibo",
    "ricoh_is410",
    "fujitsu_m3069g",
    "hp_scanjet_IIc",
    "ricoh_fs1",
    "sharp_jx600",
    "ricoh_is50",
    "ibm_2456",
    "umax_uc630",
    "umax_ug630",
    "mustek_06000cx",
    "mustek_12000cx",
  };

  if (type < 0 || type > MUSTEK_12000CX)
    return 1;

  puts(scanners[type]);
  return 0;
}

void
usage(char *prog_name)
{
  fprintf(stderr, "usage: %s [-p][-q][-l <logical name>]\n", prog_name);
  exit(-1);
}


main(int argc, char *argv[])
{
  int s_fd;
  struct scan_io s_io;

  int quiet = FALSE;
  int do_pixels = FALSE;
  int do_type = FALSE;

  char *logical_name = "scan0";
  char device_special[255];

  int c;
  extern int optind;
  extern char *optarg;

  while ((c = getopt(argc, argv, "tpql:")) != -1) {
    switch (c) {
    case 'q':
      quiet = TRUE;
      break;

    case 'l':
      logical_name = optarg;
      break;

    case 'p':
      do_pixels = TRUE;
      break;

    case 't':
      do_type = TRUE;
      break;

    case '?':
      usage(argv[0]);
      break;
    }
  }

  snprintf(device_special, sizeof device_special, "/dev/%s", logical_name);
  if ((s_fd = open(device_special, O_RDONLY)) < 0) {
    perror("open of scanner");
    exit(-1);
  }

  if (ioctl(s_fd, SCIOCGET, &s_io) < 0) {
    perror("ioctl on scanner");
    exit(-1);
  }

  if (do_pixels) {
    printf("%d %d\n", s_io.scan_pixels_per_line, s_io.scan_lines);
    exit(0);
  }

  if (do_type)
    return (print_type(s_io.scan_scanner_type));

  if (quiet) {
    printf("-w %g -h %g -x %g -y %g -b %d -c %d -r %d",
	   (double)s_io.scan_width / 1200.0,
	   (double)s_io.scan_height / 1200.0,
	   (double)s_io.scan_x_origin / 1200.0,
	   (double)s_io.scan_y_origin / 1200.0,
	   s_io.scan_brightness,
	   s_io.scan_contrast,
	   s_io.scan_x_resolution);
    switch (s_io.scan_image_mode) {
    case SIM_BINARY_MONOCHROME:
      printf(" -i m\n");
      break;
    case SIM_DITHERED_MONOCHROME:
      printf(" -i d\n");
      break;
    case SIM_GRAYSCALE:
      printf(" -i g\n");
      break;
    case SIM_COLOR:
      printf(" -i c\n");
      break;
    case SIM_RED:
      printf(" -i R\n");
      break;
    case SIM_GREEN:
      printf(" -i G\n");
      break;
    case SIM_BLUE:
      printf(" -i B\n");
      break;
    }
  } else {
    printf("width = %g inches\n", (double)s_io.scan_width / 1200.0);
    printf("height = %g inches\n", (double)s_io.scan_height / 1200.0);
    printf("x_origin = %g inches\n", (double)s_io.scan_x_origin / 1200.0);
    printf("y_origin = %g inches\n", (double)s_io.scan_y_origin / 1200.0);
    printf("brightness = %d\n", s_io.scan_brightness);
    printf("contrast = %d\n", s_io.scan_contrast);
    printf("resolution = %d dpi\n", s_io.scan_x_resolution);
    printf("image_mode = ");
    switch (s_io.scan_image_mode) {
    case SIM_BINARY_MONOCHROME:
      printf("binary_monochrome\n");
      break;
    case SIM_DITHERED_MONOCHROME:
      printf("dithered_monochrome\n");
      break;
    case SIM_GRAYSCALE:
      printf("grayscale\n");
      break;
    case SIM_COLOR:
      printf("color\n");
      break;
    case SIM_RED:
      printf("red\n");
      break;
    case SIM_GREEN:
      printf("green\n");
      break;      
    case SIM_BLUE:
      printf("blue\n");
      break;      
    }
  }

  close(s_fd);
}
