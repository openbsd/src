/*	$OpenBSD: binpatch.c,v 1.5 1999/03/02 16:09:39 espie Exp $	*/
/*	$NetBSD: binpatch.c,v 1.6 1995/08/18 15:28:28 chopps Exp $	*/

/* Author: Markus Wild mw@eunet.ch ???   */
/* Modified: Rob Leland leland@mitre.org */

#include <sys/types.h>
#include <a.out.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __NetBSD__
/*
 * assume NMAGIC files are linked at 0 (for kernel)
 */
#undef N_TXTADDR
#define N_TXTADDR(ex) \
	((N_GETMAGIC2(ex) == (ZMAGIC|0x10000) || N_GETMAGIC2(ex) == NMAGIC) ? \
	0 : __LDPGSZ)
#endif


static char synusage[] = "
NAME
\t%s - Allows the patching of BSD binaries
SYNOPSIS
\t%s [-HELP]
\t%s [-b|-w|-l] -s symbol[[[index]][=value]] binary 
\t%s [-b|-w|-l] [-o offset] -s symbol [-r value] binary
\t%s [-b|-w|-l] [-o offset] -a address [-r value] binary
";
static char desusage[] = "DESCRIPTION
\tAllows the patching of BSD binaries, for example, a distributed
\tkernel. Recent additions allows the user to index into an array
\tand assign a value. Binpatch has internal variables to allow
\tyou to test it on itself under NetBSD.
OPTIONS
\t-a  patch variable by specifying address in hex
\t-b  symbol or address to be patched is 1 byte
\t-l  symbol or address to be patched is 4 bytes  (default)
\t-o  offset to begin patching value relative to symbol or address
\t-r  replace value, and print out previous value to stdout
\t-s  patch variable by specifying symbol name. Use '[]'
\t    to specify the 'index'. If '-b, -w or -l' not specified
\t    then index value is used like an offset. Also can use '='
\t    to assign value
\t-w  symbol or address to be patched is 2 bytes
EXAMPLES
\tThis should print 100 (this is a nice reality check...)
\t\tbinpatch -l -s _hz netbsd
\tNow it gets more advanced, replace the value:
\t\tbinpatch -l -s _sbic_debug -r 1 netbsd
\tNow patch a variable at a given 'index' not offset, 
\tunder NetBSD you must use '', under AmigaDos CLI '' is optional.:
\t\tbinpatch -w -s '_vieww[4]' -r 0 a.out
\tsame as
\t\tbinpatch -w -o 8 -s _vieww -r 0 a.out
\tAnother example of using []
\t\tbinpatch -s '_viewl[4]' -r 0 a.out
\tsame as
\t\tbinpatch -o 4 -s _viewl -r 0 a.out
\tOne last example using '=' and []
\t\tbinpatch -w -s '_vieww[4]=2' a.out
\tSo if the kernel is not finding your drives, you could enable
\tall available debugging options, helping to shed light on that problem.
\t\tbinpatch -l -s _sbic_debug -r 1 netbsd	scsi-level
\t\tbinpatch -l -s _sddebug -r 1 netbsd	sd-level (disk-driver)
\t\tbinpatch -l -s _acdebug -r 1 netbsd	autoconfig-level
SEE ALSO
\tbinpatch.c binpatch(1)
";

extern char *optarg;
extern int optind;

volatile void error (char *);
static void Synopsis(char *program_name);
static void Usage(char *program_name);
static u_long FindAssign(char *symbol,u_long *rvalue);
static void FindOffset(char *symbol,u_long *index);

/* The following variables are so binpatch can be tested on itself */
int test = 1;
int testbss;
char foo = 23;
char  viewb[10] = {0,0,1,0,1,1,0,1,1,1};
short vieww[10] = {0,0,1,0,1,1,0,1,1,1};
long  viewl[10] = {0,0,1,0,1,1,0,1,1,1};
/* End of test binpatch variables */
int
main(int argc, char *argv[])
{
  struct exec e;
  int c;
  u_long addr = 0, offset = 0;
  u_long index = 0;/* Related to offset */
  u_long replace = 0, do_replace = 0;
  char *symbol = 0;
  char size = 4;  /* default to long */
  char size_opt = 0; /* Flag to say size option was set, used with index */
  char *fname;
  char *pgname = argv[0]; /* Program name */
  int fd;
  int type, off;
  u_long  lval;
  u_short sval;
  u_char  cval;
  
 
  while ((c = getopt (argc, argv, "H:a:bwlr:s:o:")) != -1)
    switch (c)
      {
      case 'H':
        Usage(argv[0]);
        break;
      case 'a':
	if (addr || symbol)
	  error ("only one address/symbol allowed");
	if (! strncmp (optarg, "0x", 2))
	  sscanf (optarg, "%x", &addr);
	else
	  addr = atoi (optarg);
	if (! addr)
	  error ("invalid address");
	break;

      case 'b':
	size = 1;
        size_opt = 1;
	break;

      case 'w':
	size = 2;
        size_opt = 1;
	break;

      case 'l':
	size = 4;
        size_opt = 1;
	break;

      case 'r':
	do_replace = 1;
	if (! strncmp (optarg, "0x", 2))
	  sscanf (optarg, "%x", &replace);
	else
	  replace = atoi (optarg);
	break;

      case 's':
	if (addr || symbol)
	  error ("only one address/symbol allowed");
	symbol = optarg;
	break;

      case 'o':
	if (offset)
	  error ("only one offset allowed");
	if (! strncmp (optarg, "0x", 2))
	  sscanf (optarg, "%x", &offset);
	else
          offset = atoi (optarg);
        break;
      }/* while switch() */
  
  if (argc > 1)
  {
    if (addr || symbol)
    {
      argv += optind;
      argc -= optind;

      if (argc < 1)
        error ("No file to patch.");

      fname = argv[0];
      if ((fd = open (fname, 0)) < 0)
        error ("Can't open file");

      if (read (fd, &e, sizeof (e)) != sizeof (e)
        || N_BADMAG (e))
        error ("Not a valid executable.");

      /* fake mid, so the N_ macros work on the amiga.. */
      e.a_midmag |= 127 << 16;

      if (symbol)
      {
        struct nlist nl[2];
        if (offset == 0)
	{
            u_long new_do_replace = 0;
            new_do_replace = FindAssign(symbol,&replace);
            if (new_do_replace && do_replace)
              error("Cannot use both '=' and '-r' option!");
            FindOffset(symbol,&index);
            if (size_opt)
               offset = index*size; /* Treat like an index */
            else
               offset = index; /* Treat index like an offset */
	    if (new_do_replace)
	       do_replace = new_do_replace;
	}
        nl[0].n_un.n_name = symbol;
        nl[1].n_un.n_name = 0;
        if (nlist (fname, nl) != 0)
	{
          fprintf(stderr,"Symbol is %s ",symbol);
	  error ("Symbol not found.");
        }
        addr = nl[0].n_value;
        type = nl[0].n_type & N_TYPE;
      }
      else
      {
        type = N_UNDF;
        if (addr >= N_TXTADDR(e) && addr < N_DATADDR(e))
	  type = N_TEXT;
        else if (addr >= N_DATADDR(e) && addr < N_DATADDR(e) + e.a_data)
	  type = N_DATA;
      }
      addr += offset;

      /* if replace-mode, have to reopen the file for writing.
         Can't do that from the beginning, or nlist() will not 
         work (at least not under AmigaDOS) */
      if (do_replace)
      {
        close (fd);
        if ((fd = open (fname, 2)) == -1)
	  error ("Can't reopen file for writing.");
      }

      if (type != N_TEXT && type != N_DATA)
        error ("address/symbol is not in text or data section.");

      if (type == N_TEXT)
        off = addr - N_TXTADDR(e) + N_TXTOFF(e);
      else
        off = addr - N_DATADDR(e) + N_DATOFF(e);

      if (lseek (fd, off, 0) == -1)
        error ("lseek");

      /* not beautiful, but works on big and little endian machines */
      switch (size)
        {
        case 1:
          if (read (fd, &cval, 1) != 1)
	    error ("cread");
          lval = cval;
          break;

        case 2:
          if (read (fd, &sval, 2) != 2)
	    error ("sread");
          lval = sval;
          break;

        case 4:
          if (read (fd, &lval, 4) != 4)
	    error ("lread");
          break;
        }/* switch size */

      
      if (symbol)
        printf ("%s(0x%x): %d (0x%x)\n", symbol, addr, lval, lval);
      else
        printf ("0x%x: %d (0x%x)\n", addr, lval, lval);

      if (do_replace)
      {
        if (lseek (fd, off, 0) == -1)
	  error ("write-lseek");
        switch (size)
	  {
	  case 1:
	    cval = replace;
	    if (cval != replace)
	      error ("byte-value overflow.");
	    if (write (fd, &cval, 1) != 1)
	      error ("cwrite");
	    break;

	  case 2:
	    sval = replace;
	    if (sval != replace)
	      error ("word-value overflow.");
	    if (write (fd, &sval, 2) != 2)
	      error ("swrite");
	    break;

	  case 4:
	    if (write (fd, &replace, 4) != 4)
	      error ("lwrite");
	    break;
	  }/* switch(size) */
      }/* if (do_replace) */

      close (fd);
    }/* if(addr || symbol ) */
    else
    {
      error("Must specify either address or symbol.");
    }
  }/* if argc < 1 */
  else
  {
    Synopsis(pgname);
  }
  return(0);
}/* main () */



volatile void error (char *str)
{
  fprintf (stderr, "%s\n", str);
  exit (1);
}

/* Give user very short help to avoid scrolling screen much */
static void Synopsis(char *pgname)
{
  fprintf(stdout,synusage,pgname,pgname,pgname,pgname,pgname);
}


static void Usage(char *pgname)
{
  Synopsis(pgname);
  fprintf(stdout,desusage);
  exit(0);
}


/* FindOffset() - Determine if there is an offset, -or- index
                 embedded in the symbol.
                 If there is, return it, and truncate symbol to
                 exclude the [...].
                 Example: If view is declared as short view[10],
                          and we want to index the 3rd. element.
                          which is offset = (3 -1)*sizeof(short) =4.
                 we would use view[4], which becomes view,4.
                 The was the code is implemented the [value] is
                 treated as a index if-and-only-if a '-b -w -l' option
                 was given. Otherwise it is treated like an offset.
                 See above documentation in for of help!
*/ 
static void FindOffset(char *symbol,u_long *index)
{
  char *sb=strchr(symbol,'['); /* Start of '[', now line must
                                 contain matching']' */
  char *eb=strchr(symbol,']'); /* End of ']' */
  short sz=strlen(symbol);    /* symbol size */
  if (sb)
  {
    if (eb && (eb > sb))
    {
      if ((eb - symbol) == (sz - 1))
      {
        char *sindex; /* Start of index */
        u_long newindex = 0;
        /* In the future we could get fancy and parse the
           sindex string for mathmatical expressions like:
           (3 - 1)*2 = 4 from above example,
           ugh forget I mentioned ot :-) !
        */
        sindex = sb + 1;
        *eb = '\0';
        newindex = (u_long)atoi(sindex);
        if (*index == 0)
        {
          *index = newindex;
          *sb = '\0'; /* Make _view[3] look like _view */
        }
        else
          fprintf(stderr,"Error index can only be specified once!\n");
      }
      else
      {
        fprintf(stderr,"Error: Garbage trailing ']'\n");
      } 
    }
    else
    {
       fprintf(stderr,"Error ']' in symbol before '[' !\n");
    }
  }/* if sb != 0 */
}/* FindOffset */

/* FindAssign : Scans symbol name for an '=number' strips it off
   of the symbol and proceeds.
*/
static u_long FindAssign(char *symbol,u_long *rvalue)
{
  char *ce = strrchr(symbol,'='); /* Assign symbol some number */
  char *cn = ce + 1; /* This should point at some number, no spaces allowed */
  u_long dr = 0; /* flag for do_replace */
  if (ce)
  {
    int nscan; /* number of variaables scanned in */
    /* get the number to assign to symbol and strip off = */
    for (cn=ce + 1;((*cn==' ')&&(*cn!='\0'));cn++)
    ;
    if (! strncmp (cn, "0x", 2))
	nscan = sscanf (cn, "%x",rvalue);
    else
        nscan = sscanf(cn,"%d",rvalue);
    if (nscan != 1)
      error("Invalid value following '='");
    dr = 1;
    *ce = '\0';/* Now were left with just symbol */ 
  }/* if (ce) */
  return(dr);
}/* FindAssign */
