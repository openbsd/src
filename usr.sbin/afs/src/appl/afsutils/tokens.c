/* 
   tokens.c - a version of 'tokens' for Arla

   Written by Chris Wing - wingc@engin.umich.edu
   based on examples of AFS code: klist.c and libkafs (KTH-KRB)

   This is a reimplementation of tokens from AFS. The following new
   features have been added:
	-athena		Output format similar to the 'klist' program
			in Kerberos 4

	-v, -verbose	Slightly more verbose output
*/

/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * Lists your current Kerberos tickets.
 * Written by Bill Sommerfeld, MIT Project Athena.
 */

#include "appl_locl.h"

RCSID("$KTH: tokens.c,v 1.9 2000/09/02 12:55:56 lha Exp $");

#include "tokens.h"


/*
 * State variables
 */

/* produce verbose output? */
static int arg_verbose = 0;

/* produce kerberos4 style output? */
static int arg_athena = 0;

/* print out help message */
static int arg_help = 0;

/* print out version number */
static int arg_version = 0;

/* do we actually have any valid AFS tokens? */
static int got_tokens = 0;

/* do we have any AFS tokens, valid or not? */
static int got_anything = 0;


/* arguments for agetarg() */

struct agetargs args[] = {
    { "athena", 0, aarg_flag, &arg_athena,
      "generate 'klist' style output instead of AFS style",
      NULL, aarg_optional},
    { "verbose", 'v', aarg_flag, &arg_verbose,
      "generate verbose output",
      NULL, aarg_optional},
    { "help", 0, aarg_flag, &arg_help,
      "display this message",
      NULL, aarg_optional},
    { "version", 0, aarg_flag, &arg_version,
      "print version",
      NULL, aarg_optional},
    { NULL, 0, aarg_end, NULL, NULL }
};

/*
 * Helper functions
 */


/* Print out a help message and exit */

static void
do_help(int exitval)
{
    aarg_printusage(args, NULL, NULL, AARG_AFSSTYLE);
    exit(exitval);
}

/* ASCII time formatting function from klist.c (Kerberos 4) */

static char *short_date(int32_t dp)
{
    char *cp;
    time_t t = (time_t)dp;

    if (t == (time_t)(-1L)) return "***  Never  *** ";
    cp = ctime(&t) + 4;

    /* Only display seconds in 'athena' mode */
    if(arg_athena)
	cp[15] = '\0';
    else
	cp[12] = '\0';

    return (cp);
}


/* Display list of tokens */

void display_tokens(void)
{
    u_int32_t i;
    unsigned char t[128];
    struct ViceIoctl parms;

    int did_banner = 0;

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)t;
    parms.out_size = sizeof(t); 

    /* AFS-style always displays the banner */
    if(!(arg_athena))
	printf("\nTokens held by Arla:\n\n");

    for (i = 0; k_pioctl(NULL, VIOCGETTOK, &parms, 0) == 0; i++) {
	int32_t size_secret_tok, size_public_tok;
	const char *cell;
	struct ClearToken ct;
	const unsigned char *r = t;
	struct timeval tv;
	char buf1[20], buf2[20];

	got_anything = 1;

	if ( (did_banner == 0) && (arg_athena) ) {
	    printf("\nTokens held by Arla:\n\n");
	    printf("  Issued           Expires          Principal\n");

	    did_banner = 1;
	}

	memcpy(&size_secret_tok, r, sizeof(size_secret_tok));
	/* dont bother about the secret token */
	r += size_secret_tok + sizeof(size_secret_tok);
	memcpy(&size_public_tok, r, sizeof(size_public_tok));
	r += sizeof(size_public_tok);
	memcpy(&ct, r, size_public_tok);
	r += size_public_tok;
	/* there is a int32_t with length of cellname, but we dont read it */
	r += sizeof(int32_t);
	cell = (const char *)r;

	/* make sure cell name is null-terminated */
	t[127] = '\0';

#ifdef HAVE_KRB_KDCTIMEOFDAY	
	krb_kdctimeofday (&tv);
#else
	gettimeofday (&tv, NULL);
#endif
	strlcpy (buf1, short_date(ct.BeginTimestamp), sizeof(buf1));
	if (arg_verbose || tv.tv_sec < ct.EndTimestamp)
	    strlcpy (buf2, short_date(ct.EndTimestamp), sizeof(buf2));
	else
	    strlcpy (buf2, ">>> Expired <<<", sizeof(buf2));

	/* only return success if we have non-expired tokens */
	if (tv.tv_sec < ct.EndTimestamp)
	    got_tokens = 1;

	/* make sure strings are null-terminated */
	buf1[19] = '\0';
	buf2[19] = '\0';

	if(arg_athena) {
	    /* Athena klist style output */

	    printf("%s  %s  ", buf1, buf2);

	    if ((ct.EndTimestamp - ct.BeginTimestamp) & 1)
		printf("User's (AFS ID %d) tokens for %s", ct.ViceId, cell);
	    else
		printf("Tokens for %s", cell);

	    if (arg_verbose)
		printf(" (%d)", ct.AuthHandle);

	    putchar('\n');
	} else {
	    /* Traditional AFS output format */

	    if ((ct.EndTimestamp - ct.BeginTimestamp) & 1)
		printf("User's (AFS ID %d) tokens for afs@%s", ct.ViceId, cell);
	    else
		printf("Tokens for afs@%s", cell);

	    if (arg_verbose || tv.tv_sec < ct.EndTimestamp)
		printf(" [Expires %s]\n", buf2);
	    else
		printf(" [%s]\n", buf2);
	}
    }

    /* Ick. Deal with AFS-style output and athena style output */

    /* Skip printing this only if athena style output and have no tokens */
    if(got_anything || !(arg_athena))
	printf("   --End of list--\n");

    /* Print this if athena style output and have no tokens */
    if( !(got_anything) && (arg_athena) )
	printf("You have no AFS tokens.\n");

    /* erase the copy of the token in memory */
    memset(t, 0, sizeof(t));
}


int main(int argc, char **argv)
{
    int optind = 0;

#ifndef KERBEROS
    errx (1, "kerberos support isn't compiled in");
#endif

     if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	 warnx ("Bad argument: %s", argv[optind]);
	 do_help(1);
     }

     if (arg_help)
	 do_help(0);
     
     if (arg_version)
	 errx (0, "part of %s-%s", PACKAGE, VERSION);
     
     if (k_hasafs())
	 display_tokens();
     else
	 errx (1, "You don't seem to have AFS running");
     
     if (!got_tokens)
         return 1;
     
     return 0;
}
