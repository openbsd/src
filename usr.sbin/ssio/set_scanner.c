static char *rcs_id = "$Id: set_scanner.c,v 1.4 2002/05/29 19:01:48 deraadt Exp $";
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
 * set-scanner.c:
 *
 *   Set the scan area in decimal fractions of inches.
 *   If the defaults are to be changed use system("chdev...")
 *   otherwise use ioctl()
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/ioctl.h>
#endif
#include <sys/scanio.h>

#define UNINITIALIZED -10

int inches_to_1200th(char *numeral);
void usage(char *prog_name);
int xlate_image_code(char *);
int ipow10(int);
int scanner_type(char *);

main(int argc, char *argv[])
{
  int width, height;
  int x_origin, y_origin;
  int brightness, contrast;
  int resolution, image_mode;

  int defaults = FALSE;

  char *logical_name = "scan0";
  char device_special[255];

  int    c;			/* for command line parsing */
  extern int   optind;		/* for getopt() */
  extern char *optarg;		/* for getopt() */

  char command[256];
  struct scan_io s_io;

  int s_fd;

  brightness = contrast = resolution =
    width = height = x_origin = y_origin =
      image_mode =
	UNINITIALIZED;

  while ((c = getopt(argc, argv, "w:h:x:y:l:b:c:r:i:d")) != -1) {
    switch (c) {
    case 'w':
      width = inches_to_1200th(optarg);
      break;
    case 'h':
      height = inches_to_1200th(optarg);
      break;
    case 'x':
      x_origin = inches_to_1200th(optarg);
      break;
    case 'y':
      y_origin = inches_to_1200th(optarg);
      break;
    case 'l':
      logical_name = optarg;
      break;
    case 'd':
      defaults = TRUE;		/* use chdev to set defaults */
      break;
    case 'b':
      brightness = atol(optarg);
      break;
    case 'c':
      contrast = atol(optarg);
      break;
    case 'r':
      resolution = atol(optarg);
      break;
    case 'i':
      image_mode = xlate_image_code(optarg);
      if (image_mode == -1) {
	usage(argv[0]);
      }
      break;
    case '?':
      usage(argv[0]);
    }
  }

#ifdef __IBMR2
  if (defaults) {
    snprintf(command, sizeof command, "chdev -l %s", logical_name);

    if (width != UNINITIALIZED)
      snprintf(command, sizeof command, "%s -a window_width=%d", command, width);

    if (height != UNINITIALIZED)
      snprintf(command, sizeof command, "%s -a window_length=%d", command, height);

    if (x_origin != UNINITIALIZED)
      snprintf(command, sizeof command, "%s -a x_origin=%d", command, x_origin);

    if (y_origin != UNINITIALIZED)
      snprintf(command, sizeof command, "%s -a y_origin=%d", command, y_origin);

    if (brightness != UNINITIALIZED)
      snprintf(command, sizeof command, "%s -a brightness=%d", command, brightness);

    /* note that the FUJITSU doesn't support contrast via the ODM */
    if (contrast != UNINITIALIZED && scanner_type(logical_name) != FUJITSU)
      snprintf(command, sizeof command, "%s -a contrast=%d", command, contrast);

    if (resolution != UNINITIALIZED) {
      snprintf(command, sizeof command, "%s -a x_resolution=%d", command, resolution);
      snprintf(command, sizeof command, "%s -a y_resolution=%d", command, resolution);
    }

    if (image_mode != UNINITIALIZED) {
      snprintf(command, sizeof command, "%s -a image_mode=", command);
      switch (image_mode) {
      case SIM_BINARY_MONOCHROME:
	snprintf(command, sizeof command, "%smonochrome ", command);
	break;
      case SIM_DITHERED_MONOCHROME:
	snprintf(command, sizeof command, "%sdithered ", command);
	break;
      case SIM_GRAYSCALE:
	snprintf(command, sizeof command, "%sgrayscale ", command);
	break;
      case SIM_COLOR:
	snprintf(command, sizeof command, "%scolor ", command);
	break;
      case SIM_RED:
	snprintf(command, sizeof command, "%sred ", command);
	break;
      case SIM_GREEN:
	snprintf(command, sizeof command, "%sgreen ", command);
	break;
      case SIM_BLUE:
	snprintf(command, sizeof command, "%sblue ", command);
	break;
      }
    }

    system(command);

  } else { 			/* use ioctl() instead of chdev */
#endif
    snprintf(device_special, sizeof device_special, "/dev/%s", logical_name);
    if ((s_fd = open(device_special, O_RDONLY)) < 0) {
      fprintf(stderr, "open of %s failed: ", device_special);
      perror("");
      exit(-1);
    }

    if (ioctl(s_fd, SCIOCGET, &s_io) < 0) {
      perror("ioctl SCIOCGET");
      exit(-1);
    }

    if (width != UNINITIALIZED)
      s_io.scan_width = width;

    if (height != UNINITIALIZED)
      s_io.scan_height = height;

    if (x_origin != UNINITIALIZED)
      s_io.scan_x_origin = x_origin;

    if (y_origin != UNINITIALIZED)
      s_io.scan_y_origin = y_origin;

    if (brightness != UNINITIALIZED)
      s_io.scan_brightness = brightness;

    if (contrast != UNINITIALIZED)
      s_io.scan_contrast = contrast;

    if (resolution != UNINITIALIZED) {
      s_io.scan_x_resolution = resolution;
      s_io.scan_y_resolution = resolution;
    }

    if (image_mode != UNINITIALIZED)
      s_io.scan_image_mode = image_mode;

    if (ioctl(s_fd, SCIOCSET, &s_io) < 0) {
      perror("ioctl SCIOCSET");
      exit(-1);
    }
#ifdef __IBMR2
  }
#endif

  exit(0);
}

/*
 * Convert a numeral representing inches into a number representing
 * 1/1200ths of an inch.  If multipling the input by 1200 still leaves
 * a fractional part then abort with an error message.
 *
 * Note that "numeral" here means string of digits with optional decimal point
 */
int
inches_to_1200th(char *numeral)
{
  FILE *bc;
  char result[50];
  char *p;

  /* test to see if "numeral" really is a numeral */
  p = numeral;
  while (*p) {
    if (!isdigit(*p) && *p != '.')
      usage("set_scanner");
    ++p;
  }

  /* test to see if it is a multiple of 1/1200 */

  if ((bc = fopen("/tmp/set_scanner.bc_work", "w")) == NULL) {
    perror("creating temp file '/tmp/set_scanner.bc_work'");
    exit(-1);
  }
  fprintf(bc, "%s * 1200\nquit\n", numeral);
  fclose(bc);

  if ((bc = popen("bc -l /tmp/set_scanner.bc_work", "r")) == NULL) {
    perror("running bc");
    exit(-1);
  }
  fgets(result, 50, bc);
  result[strlen(result) - 1] = '\0';  /* eat newline from fgets */
  pclose(bc);
  unlink("/tmp/set_scanner.bc_work");

  if ((p = strchr(result, '.')) != NULL) {
    ++p;
    while (*p)
      if (*p++ != '0') {
	fprintf(stderr, "set_scanner: please do not use fractions with a  ");
	fprintf(stderr, "granularity less than\nset_scanner: one ");
	fprintf(stderr, "twelve-thousandths of an inch\n");
	exit(-1);
      }
  }

  return (atoi(result));
}

void
usage(char *prog_name)
{
  fprintf(stderr,
	  "usage: %s [-w width] [-h height] [-x x_origin] [-y y_origin]\n[-r resolution] [-l logical name] [-i image mode] [-d (for setting defaults)]\n",
	  prog_name);
  exit(-1);
}

int xlate_image_code(char *image_code)
{
  switch (image_code[0]) {
  case 'm':
    return (SIM_BINARY_MONOCHROME);
  case 'd':
    return (SIM_DITHERED_MONOCHROME);
  case 'g':
    return (SIM_GRAYSCALE);
  case 'c':
    return (SIM_COLOR);
  case 'R':
    return (SIM_RED);
  case 'G':
    return (SIM_GREEN);
  case 'B':
    return (SIM_BLUE);
  default:
    return (-1);
  }
}

int scanner_type(char *lname)
{
  char special_file[256];
  int scan_fd;
  struct scan_io sp;

  snprintf(special_file, sizeof special_file, "/dev/%s", lname);

  if ((scan_fd = open(special_file, O_RDONLY)) < 0) {
    perror("set_scanner: can't open scanner--");
    exit(1);
  }

  if (ioctl(scan_fd, SCIOCGET, &sp) < 0) {
    perror("set_scanner: can't get parameters from scanner--");
    exit(1);
  }

  close(scan_fd);

  return ((int)sp.scan_scanner_type);
}
