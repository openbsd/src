/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
   unlog.c - a version of 'unlog' for Arla

   Written by Chris Wing - wingc@engin.umich.edu
   based on examples of AFS code in klist.c and libkafs (KTH-KRB)

   This is a reimplementation of unlog from AFS.
*/

#include "appl_locl.h"

RCSID("$arla: unlog.c,v 1.10 2002/02/07 17:58:09 lha Exp $");

#include "unlog.h"


/*
 * State variables
 */

/* Holding space for saved tokens */
struct token *saved_tokens;

/* Number of saved tokens */
static int numtokens;

/* Did we lose any tokens when trying to restore? */
static int lost_tokens = 0;

static int   unlog_version = 0;
static int   unlog_help = 0;
static agetarg_strings unlog_cells, unlog_cells_no_argument;

/*
 * Various helper functions that we call
 */

/* Get token # 'tnum' and store it into buffer pointed at by 'tok'
   Return pointer to cell name for convenience */

char *gettoken(int tnum, struct token *tok)
{
    uint32_t i;
    struct ViceIoctl parms;

    int32_t zero_length = 0;

    i = tnum;

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)tok->tbuf;
    parms.out_size = 128 * sizeof(char);

    if(k_pioctl(NULL, VIOCGETTOK, &parms, 0) != 0)
	return(NULL);
    else {
        int32_t size_secret_tok, size_public_tok, size_cell_name;
	unsigned char *r = tok->tbuf;

	/* get length of secret token */
	memcpy(&size_secret_tok, r, sizeof(size_secret_tok));

	/* skip over secret token + length */
	r += size_secret_tok + sizeof(size_secret_tok);
	/* get length of public token */
	memcpy(&size_public_tok, r, sizeof(size_public_tok));

	r += size_public_tok + sizeof(size_public_tok);

	/* get length of cell name */
	memcpy(&size_cell_name, r, sizeof(size_cell_name));

	/* FIXME: it seems we must set this value to 0 in order
	   'not to mark this as the primary cell' when we re-store
	   the token. Is this right? */
	memcpy(r, &zero_length, sizeof(zero_length));

	r += sizeof(size_cell_name);

	tok->size = size_secret_tok + size_public_tok + size_cell_name + 1
            + 3 * sizeof(int32_t);

	tok->cell = (char *)r;

	/* handle overflows of cellname */
	if(tok->size > 128) {
	    tok->size = 128;
	    tok->tbuf[127] = '\0';
	    fprintf(stderr, "Cell name overflowed: %s\n", tok->cell);
	}

	return(tok->cell);
    }
}


/* Restore token from buffer 'tok' */

void restoretoken(struct token *tok)
{
    struct ViceIoctl parms;
    int ret;
    
    parms.in = tok->tbuf;
    parms.in_size = tok->size;
    parms.out = 0;
    parms.out_size = 0;
    ret = k_pioctl(0, VIOCSETTOK, &parms, 0);

    if(ret) {
	fprintf(stderr, "Lost token for cell: %s\n", tok->cell);
	lost_tokens = 1;
    }

    return;
}

/*
 * options to program
 */

struct agetargs args[] = {
    { "cell", 0, aarg_strings, &unlog_cells,
      "only remove tokens for this cell or cells", 
      "AFS cell name(s)", aarg_optional},
    { NULL, 0, aarg_generic_string, &unlog_cells_no_argument,
      "only remove tokens for this cell or cells",
      "AFS cell name(s)", aarg_optional},
    { "help", 0, aarg_flag, &unlog_help, "help",
      NULL, aarg_optional},
    { "version", 0, aarg_flag, &unlog_version, "print version",
      NULL, aarg_optional},
    { NULL, 0, aarg_end, NULL, NULL }
};

/*
 * Print out a help message and exit 
 */

static void
do_help(int exitval)
{
    aarg_printusage(args, NULL, NULL, AARG_AFSSTYLE);
    exit(exitval);
}

/*
 * Main program code
 */

int
main(int argc, char **argv)
{
    int i, j, optind = 0;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
        warnx("Bad argument: %s", argv[optind]);
	do_help(1);
    }
    
    if (unlog_help)
	do_help(0);

    if (unlog_version)
	errx (0, "part of %s-%s", PACKAGE, VERSION);

    if(!k_hasafs())
        errx (1, "You don't seem to have AFS running");
 
    if(unlog_cells_no_argument.num_strings)
	unlog_cells = unlog_cells_no_argument;

    /* Save tokens if need be */
    if(unlog_cells.num_strings) {
	char *cell;
	struct token *save;
	int token_overflow = 0;

	/* allocate space for 1 more token in case of overflow */
	saved_tokens = malloc( (MAX_TOKENS + 1) * sizeof(struct token));

	if(!saved_tokens) {
	    fprintf(stderr, "Can't malloc space to store tokens!\n");
	    exit(1);
	}

	numtokens = 0;
	i=0;

	save = saved_tokens;
	while((cell = gettoken(i, &saved_tokens[numtokens]))) {
	    int keep_this_token = 1;

	    i++;

	    if(token_overflow) {
		fprintf(stderr, 
			"Ran out of space to save token in cell %s\n",
			cell);
		lost_tokens = 1;
	    }

	    for (j = 0 ; j < unlog_cells.num_strings; j++) 
		if (strcmp (unlog_cells.strings[j], cell) == 0)
		    keep_this_token = 0;

	    if(keep_this_token) {
		numtokens++;
		save++;
	    }

	    if(numtokens == MAX_TOKENS) {
		numtokens = MAX_TOKENS-1;
		token_overflow = 1;
	    }
	}
    }

    /* Now destroy all tokens */
    if(k_unlog() != 0) {
	fprintf(stderr, "pioctl(VIOCUNLOG) returned error!\n");

	/* make sure the program exits with status 1 */
	lost_tokens = 1;
    }

    /* Now restore tokens we want to keep */
    if(numtokens) {
	for(i=0; i<numtokens; i++) {
	    restoretoken(&saved_tokens[i]);
	}

	/* Erase the copy of all saved tokens in the process's
	   memory image! */
	memset(saved_tokens, 0, MAX_TOKENS * sizeof(struct token));

	/* Return an error code if we lost some tokens somehow, or if the
	   unlog pioctl failed */
	exit(lost_tokens);
    }
    return 0;
}
