/* Generic xor handler.

   With no args, xors stdin against 0xFF to stdout.  A single argument is a
   file to read xor-bytes out of.  Any zero in the xor-bytes array is treated
   as the end; if you need to xor against a string that *includes* zeros,
   you're on your own.

   The indirect file can be generated easily with data.c.

   Written because there are so many lame schemes for "masking" plaintext
   passwords and the like floating around, and it's handy to just run an
   obscure binary-format configuration file through this and look for strings.

   *Hobbit*, 960208 */

#include <stdio.h>
#include <fcntl.h>

char buf[8192];
char bytes[256];
char * py;

/* do the xor, in place.  Uses global ptr "py" to maintain "bytes" state */
xorb (buf, len)
  char * buf;
  int len;
{
  register int x;
  register char * pb;

  pb = buf;
  x = len;
  while (x > 0) {
    *pb = (*pb ^ *py);
    pb++;
    py++;
    if (! *py)
      py = bytes;
    x--;
  }
} /* xorb */

/* blah */
main (argc, argv)
  int argc;
  char ** argv;
{
  register int x = 0;
  register int y;

/* manually preload; xor-with-0xFF is all too common */
  memset (bytes, 0, sizeof (bytes));
  bytes[0] = 0xff;

/* if file named in any arg, reload from that */
#ifdef O_BINARY				/* DOS shit... */
  x = setmode (0, O_BINARY);		/* make stdin raw */
  if (x < 0) {
    fprintf (stderr, "stdin binary setmode oops: %d\n", x);
    exit (1);
  }
  x = setmode (1, O_BINARY);		/* make stdout raw */
  if (x < 0) {
    fprintf (stderr, "stdout binary setmode oops: %d\n", x);
    exit (1);
  }
#endif /* O_BINARY */
  
  if (argv[1])
#ifdef O_BINARY
    x = open (argv[1], O_RDONLY | O_BINARY);
#else
    x = open (argv[1], O_RDONLY);
#endif
  if (x > 0) {
    read (x, bytes, 250);		/* nothin' fancy here */
    close (x);
  }
  py = bytes;
  x = 1;
  while (x > 0) {
    x = read (0, buf, sizeof (buf));
    if (x <= 0)
      break;
    xorb (buf, x);
    y = write (1, buf, x);
    if (y <= 0)
      exit (1);
  }
  exit (0);
}

