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

RCSID("$arla: tokens.c,v 1.10 2001/01/21 15:33:36 lha Exp $");

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


/*
 *
 */

static int
print_token (const char *secret, size_t secret_sz, 
	     const struct ClearToken *ct, 
	     const char *cell, void *arg)
{
    static int did_banner = 0;
    struct timeval tv;
    char start_time[20];
    char end_time[20];

    got_anything = 1;
    
    if ( (did_banner == 0) && (arg_athena) ) {
	printf("\nTokens held by Arla:\n\n");
	printf("  Issued           Expires          Principal\n");
	
	did_banner = 1;
    }
    
#ifdef HAVE_KRB_KDCTIMEOFDAY	
    krb_kdctimeofday (&tv);
#else
    gettimeofday (&tv, NULL);
#endif
    strlcpy (start_time, short_date(ct->BeginTimestamp), sizeof(start_time));
    if (arg_verbose || tv.tv_sec < ct->EndTimestamp)
	strlcpy (end_time, short_date(ct->EndTimestamp), sizeof(end_time));
    else
	strlcpy (end_time, ">>> Expired <<<", sizeof(end_time));
    
    /* only return success if we have non-expired tokens */
    if (tv.tv_sec < ct->EndTimestamp)
	got_tokens = 1;
    
    if(arg_athena) {
	/* Athena klist style output */
	
	printf("%s  %s  ", start_time, end_time);
	
	if ((ct->EndTimestamp - ct->BeginTimestamp) & 1)
	    printf("User's (AFS ID %d) tokens for %s", ct->ViceId, cell);
	else
	    printf("Tokens for %s", cell);
	
	    if (arg_verbose)
		printf(" (%d)", ct->AuthHandle);
	    
	    putchar('\n');
    } else {
	/* Traditional AFS output format */
	
	if ((ct->EndTimestamp - ct->BeginTimestamp) & 1)
	    printf("User's (AFS ID %d) tokens for afs@%s", ct->ViceId, cell);
	else
	    printf("Tokens for afs@%s", cell);
	
	if (arg_verbose || tv.tv_sec < ct->EndTimestamp)
	    printf(" [Expires %s]\n", end_time);
	else
	    printf(" [%s]\n", end_time);
    }
    return 0;
}
    

/* Display list of tokens */

void
display_tokens(void)
{
    /* AFS-style always displays the banner */
    if(!arg_athena)
	printf("\nTokens held by Arla:\n\n");

    arlalib_token_iter (NULL, print_token, NULL);

    /* Ick. Deal with AFS-style output and athena style output */

    if (got_anything) {
	if (!arg_athena)
	    printf("   --End of list--\n");
    } else {
	if (arg_athena)
	    printf("You have no AFS tokens.\n");
    }
}


int
main(int argc, char **argv)
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
