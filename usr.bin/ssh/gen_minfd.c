/*

gen_minfd.c

Author: David Mazieres <dm@lcs.mit.edu>
	Contributed to be part of ssh.

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Tue Aug 22 17:22:57 1995 ylo
Last modified: Tue Aug 22 17:44:32 1995 ylo

*/

#include "includes.h"
#include <sys/resource.h>
#include "fdlim.h"

static char *self;

static void
child_test (char *fdstr)
{
  int fd = atoi (fdstr);
  if (fcntl (fd, F_GETFL, NULL) < 0) {
    exit (1);
  }
  exit (0);
}

static int
run_child (char *shell, int fd)
{
  char cmd[128];
  int pid;
  int status;

  if (dup2 (0, fd) < 0) {
    perror ("dup2");
    return (-1);
  }

  sprintf (cmd, "%s -fd %d", self, fd);

  fflush (stdout);
  pid = fork ();
  if (! pid) {
    close (1);  /* prevent any garbage from entering the output */
    dup (2);
    execlp (shell, shell, "-c", cmd, NULL);
    exit (1);
  }
  close (fd);
  if (wait (&status) != pid) {
    fprintf (stderr, "wrong/no child??\n");
    exit (1);
  }
  return (status ? -1 : 0);
}

static int
do_shell (char *shell)
{
  int fd, min, max;

  min = 3;
  max = fdlim_get (0) - 1;
  if (max < 0) {
    printf ("fdlim_get: bad value\n");
    exit (1);
  }

  if (run_child (shell, max)
      && run_child (shell, --max))  /* bizarre ultrix weirdness */
    return (-1);

  while (min + 1 < max) {
    fd = (min + max) / 2;
    if (run_child (shell, fd))
      min = fd;
    else
      max = fd;
  }
  return (max);
}

int
main (int argc, char **argv)
{
  int fd;
  int i;
  char *p;

  if (argc == 3 && !strcmp (argv[1], "-fd"))
    child_test (argv[2]);
  self = argv[0];

  fd = fdlim_get (1);
  if (fd < 0) {
    fprintf (stderr, "fdlim_get: bad value\n");
    exit (1);
  }
  fdlim_set (fd);
  printf ("/* maximum file descriptors = %d */\n\n", fd);
  printf ("struct Min_Auth_Fd {\n"
	  "  int fd;\n"
	  "  char shell[32];\n"
	  "};\n\n"
	  "static struct Min_Auth_Fd mafd[] = {\n");
  for (i = 1; i < argc; i++) {
    fd = do_shell (argv[i]);
    if ((p = strrchr (argv[i], '/')))
      p++;
    else
      p = argv[i];
    if (fd > 0)
      printf ("  { %d, \"%s\" },\n", fd, p);
  }
  printf ("  { 0, \"\" },\n};\n\n"
	  "#define MAFD_MAX (sizeof (mafd) / sizeof (mafd[0]) - 1)\n");
  return (0);
}
