static char *rcs_id = "$Id: reset_scanner.c,v 1.2 1999/05/23 17:19:23 aaron Exp $";
#ifdef _IBMR2
/*
 *  PINT Pint Is Not TWAIN - common scanner driver interface for UN*X
 *  Copyright (C) 1994 Kenneth Stailey kstailey@leidecker.gsfc.nasa.gov
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * reset_scanner.c: clear scanner of error conditions
 * Ricoh IS-50's and IS-410's don't have reset buttons, but could use them.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/scsi.h>

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
  char device[255];
  char cmd[255];

  int fast = FALSE;

  int c;
  extern int optind;
  extern char *optarg;

  while ((c = getopt(argc, argv, "fl:")) != -1) {
    switch (c) {
    case 'l':
      logical_name = optarg;
      break;

    case 'f':
      fast = TRUE;
      break;

    case '?':
      usage(argv[0]);
      break;
    }
  }

  strcpy(device, "/dev/");
  strcat(device, logical_name);

  if ((sfd = openx(device, O_RDONLY, 0, SC_FORCED_OPEN)) < 0) {
    fprintf(stderr, "openx of %s failed: ", device);
    perror("");
    exit(-1);
  }

  close(sfd);

  if (fast)
    exit(0);

  sprintf(cmd, "rmdev -l %s", logical_name);
  system(cmd);
  sprintf(cmd, "mkdev -l %s", logical_name);
  system(cmd);
}
#else
main()
{
  printf("this only works for AIX 3.2\n");
}
#endif
