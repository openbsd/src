/* primitive arbitrary-data frontend for netcat.  0.9 960226
   only handles one value per ascii line, but at least parses 0xNN too
   an input line containing "%r" during "-g" generates a random byte

   todo:
	make work on msloss jus' for kicks [workin' on it...]

   syntax: data -X [limit]
   where X is one of
	d: dump raw bytes to ascii format
	g: generate raw bytes from ascii input
	c: generate ??? of value -- NOTYET
	r: generate all random bytes
   and limit is how many bytes to generate or dump [unspecified = infinite]

   *Hobbit*, started 951004 or so and randomly screwed around with since */

#include <stdio.h>

#ifdef MSDOS				/* for MSC only at the moment... */
#include <fcntl.h>
#else /* MSDOS */
#include <sys/file.h>
#define HAVE_RANDOM			/* XXX: might have to change */
#endif /* MSDOS */

static char buf_in [128];
static char buf_raw [8192];
static char surveysez[] = "survey sez... XXX\n";

/* fgetss :
   wrapper for fgets, that yanks trailing newlines.  Doing the work ourselves
   instead of calling strchr/strlen/whatever */
char * fgetss (buf, len, from)
  char * buf;
  size_t len;
  FILE * from;
{
  register int x;
  register char * p, * q;
  p = fgets (buf, len, from);		/* returns ptr to buf */
  if (! p)
    return (NULL);
  q = p;
  for (x = 0; x < len; x++) {
    *p = (*p & 0x7f);			/* rip parity, just in case */
    switch (*p) {
      case '\n':
      case '\r':
      case '\0':
	*p = '\0';
	return (q);
    } /* switch */
    p++;
  } /* for */
} /* fgetss */

/* randint:
   swiped from rndb.c.  Generates an INT, you have to mask down to char. */
int randint()
{
  register int q;
  register int x;

#ifndef HAVE_RANDOM
  q = rand();
#else
  q = random();
#endif
  x = ((q >> 8) & 0xff);	/* perturb low byte using some higher bits */
  x = q ^ x;
  return (x);
}

main (argc, argv)
  int argc;
  char ** argv;
{
  register unsigned char * p;
  register char * q;
  register int x;
  int bc = 0;
  int limit = 0;		/* num to gen, or 0 = infinite */
  register int xlimit;		/* running limit */
  FILE * txt;			/* line-by-line ascii file */
  int raw;			/* raw bytes fd */
  int dumping = 0;		/* cmd flags ... */
  int genning = 0;
  int randing = 0;

  memset (buf_in, 0, sizeof (buf_in));
  memset (buf_raw, 0, sizeof (buf_raw));

  xlimit = 1;				/* doubles as "exit flag" */
  bc = 1;				/* preload, assuming "dump" */
  x = getpid() + 687319;
/* if your library doesnt have srandom/random, use srand/rand. [from rnd.c] */
#ifndef HAVE_RANDOM
  srand (time(0) + x);
#else
  srandom (time(0) + x);
#endif

#ifdef O_BINARY
/* DOS stupidity */
/* Aha: *here's* where that setmode() lib call conflict in ?BSD came from */
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

  if (argv[1]) {
    p = argv[1];		/* shit-simple single arg parser... */
    if (*p == '-')		/* dash is optional, we'll deal */
      p++;
    if (*p == 'd')
      dumping++;
    if (*p == 'g')
      genning++;
    if (*p == 'r')
      randing++;
  } /* if argv 1 */

/* optional second argument: limit # of bytes shoveled either way */
  if (argv[2]) {
    x = atoi (argv[2]);
    if (x)
      limit = x;
    else
      goto wrong;
    xlimit = limit;
  }

/* Since this prog would likely best be written in assmbler, I'm gonna
   write it *like* assembler.  So there. */

  if (randing)
    goto do_rand;

nextbuf:				/* loop sleaze */

  if (dumping) {			/* switch off to wherever */
    if (genning)
      goto wrong;
    goto do_dump;
  }
  if (genning)
    goto do_gen;
wrong:
  fprintf (stderr, surveysez);		/* if both or neither */
  exit (1);

do_gen:
/* here if genning -- original functionality */
  q = buf_raw;
  bc = 0;
/* suck up lines until eof or buf_raw is full */
  while (1) {
    p = fgetss (buf_in, 120, stdin);
    if (! p)
      break;				/* EOF */
/* super-primitive version first: one thingie per line */
    if (*p == '#')			/* comment */
      continue;
    if (*p == '\0')			/* blank line */
      continue;
    if (*p == '%') {			/* escape char? */
      p++;
      if (*p == 'r') {			/* random byte */
	x = randint();
	goto stuff;
      } /* %r */
    } /* if "%" escape */
    if (*p == '0')
      if (*(p+1) == 'x')		/* 0x?? */
	goto hex;
    x = atoi (p);			/* reg'lar decimal number */
    goto stuff;

hex:
/* A 65   a 97 */
/* xxx: use a conversion table for this or something.  Since we ripped the
   parity bit, we only need a preset array of 128 with downconversion factors
   loaded in *once*.   maybe look at scanf... */
    p++; p++;				/* point at hex-chars */
    x = 0;
    if ((*p > 96) && (*p < 123))	/* a-z */
      *p = (*p - 32);			/* this is massively clumsy */
    if ((*p > 64) && (*p < 71))		/* A-F */
      x = (*p - 55);
    if ((*p > 47) && (*p < 58))		/* digits */
      x = (*p - 48);
    p++;
    if (*p)				/* another digit? */
      x = (x << 4);			/* shift to hi half */
    if ((*p > 96) && (*p < 123))	/* a-z */
      *p = (*p - 32);
    if ((*p > 64) && (*p < 71))		/* A-F */
      x = (x | (*p - 55));		/* lo half */
    if ((*p > 47) && (*p < 58))		/* digits */
      x = (x | (*p - 48));

/* fall thru */
stuff:					/* cvt to byte and add to buffer */
    *q = (x & 0xff);
    q++;
    bc++;
    if (limit) {
      xlimit--;
      if (xlimit == 0)			/* max num reached */
	break;
    } /* limit */
    if (bc >= sizeof (buf_raw))		/* buffer full */
      break;
  } /* while 1 */

/* now in theory we have our buffer formed; shovel it out */
  x = write (1, buf_raw, bc);
  if (x <= 0) {
    fprintf (stderr, "write oops: %d\n", x);
    exit (1);
  }
  if (xlimit && p)
    goto nextbuf;			/* go get some more */
  exit (0);

do_dump:
/* here if dumping raw stuff into an ascii file */
/* gad, this is *so* much simpler!  can we say "don't rewrite printf"? */
  x = read (0, buf_raw, 8192);
  if (x <= 0)
    exit (0);
  q = buf_raw;
  for ( ; x > 0; x--) {
    p = q;
    printf ("%-3.3d # 0x%-2.2x # ", *p, *p);
    if ((*p > 31) && (*p < 127))
      printf ("%c %d\n", *p, bc);
    else
      printf (". %d\n", bc);
    q++;
    bc++;
    if (limit) {
      xlimit--;
      if (xlimit == 0) {
	fflush (stdout);
	exit (0);
      }
    } /* limit */
  } /* for */
  goto nextbuf;

do_rand:
/* here if generating all-random bytes.  Stays in this loop */
  p = buf_raw;
  while (1) {
    *p = (randint() & 0xff);
    write (1, p, 1);			/* makes very slow! */
    if (limit) {
      xlimit--;
      if (xlimit == 0)
	break;
    }
  } /* while */
  exit (0);

} /* main */
