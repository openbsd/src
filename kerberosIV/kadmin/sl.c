/*	$OpenBSD: sl.c,v 1.3 1998/08/16 02:42:07 art Exp $	*/
/* $KTH: sl.c,v 1.15 1997/10/19 23:12:40 assar Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "sl_locl.h"

static SL_cmd *
sl_match (SL_cmd *cmds, char *cmd, int exactp)
{
    SL_cmd *c, *current = NULL, *partial_cmd = NULL;
    int partial_match = 0;

    for (c = cmds; c->name; ++c) {
	if (c->func)
	    current = c;
	if (strcmp (cmd, c->name) == 0)
	    return current;
	else if (strncmp (cmd, c->name, strlen(cmd)) == 0 &&
		 partial_cmd != current) {
	    ++partial_match;
	    partial_cmd = current;
	}
    }
    if (partial_match == 1 && !exactp)
	return partial_cmd;
    else
	return NULL;
}

void
sl_help (SL_cmd *cmds, int argc, char **argv)
{
    SL_cmd *c, *prev_c;

    if (argc == 1) {
	prev_c = NULL;
	for (c = cmds; c->name; ++c) {
	    if (c->func) {
		if(prev_c)
		    printf ("\n\t%s%s", prev_c->usage ? prev_c->usage : "",
			    prev_c->usage ? "\n" : "");
		prev_c = c;
		printf ("%s", c->name);
	    } else
		printf (", %s", c->name);
	}
	if(prev_c)
	    printf ("\n\t%s%s", prev_c->usage ? prev_c->usage : "",
		    prev_c->usage ? "\n" : "");
    } else { 
	c = sl_match (cmds, argv[1], 0);
	if (c == NULL)
	    printf ("No such command: %s. "
		    "Try \"help\" for a list of all commands\n",
		    argv[1]);
	else {
	    printf ("%s\t%s", c->name, c->usage);
	    if(c->help && *c->help)
		printf ("%s\n", c->help);
	    if((++c)->name && c->func == NULL) {
		printf ("\nSynonyms:");
		while (c->name && c->func == NULL)
		    printf ("\t%s", (c++)->name);
	    }
	    printf ("\n");
	}
    }
}

static char *
readline(char *prompt)
{
    char buf[BUFSIZ];
    printf ("%s", prompt);
    fflush (stdout);
    if(fgets(buf, sizeof(buf), stdin) == NULL)
	return NULL;
    if (buf[strlen(buf) - 1] == '\n')
	buf[strlen(buf) - 1] = '\0';
    return strdup(buf);
}

static void
add_history(char *p)
{
}

int
sl_command(SL_cmd *cmds, int argc, char **argv)
{
    SL_cmd *c;
    c = sl_match (cmds, argv[0], 0);
    if (c == NULL)
	return -1;
    return (*c->func)(argc, argv);
}

int
sl_loop (SL_cmd *cmds, char *prompt)
{
    unsigned max_count;
    char **ptr;
    int ret;

    max_count = 17;
    ptr = malloc(max_count * sizeof(*ptr));
    if (ptr == NULL) {
	printf ("sl_loop: failed to allocate %u bytes of memory\n",
		(int) max_count * sizeof(*ptr));
	return -1;
    }

    for (;;) {
	char *buf;
	unsigned count;
	SL_cmd *c;

	ret = 0;
	buf = readline(prompt);
	if(buf == NULL)
	    break;

	if(*buf)
	    add_history(buf);
	count = 0;
	{
	    char *foo = NULL;
	    char *p;

	    for(p = strtok_r (buf, " \t", &foo);
		p;
		p = strtok_r (NULL, " \t", &foo)) {
		if(count == max_count) {
		    char **temp;

		    max_count *= 2;
		    temp = realloc (ptr, max_count * sizeof(*ptr));
		    if (temp == NULL) {
			printf ("sl_loop: failed to allocate %u "
				"bytes of memory\n",
				(unsigned) max_count * sizeof(*ptr));

			free(ptr);
			return -1;
		    }
		    ptr = temp;
		}
		ptr[count++] = p;
	    }
	}
	if (count > 0) {
	    c = sl_match (cmds, ptr[0], 0);
	    if (c) {
		ret = (*c->func)(count, ptr);
		if (ret != 0) {
		    free (buf);
		    buf = NULL;
		    break;
		}
	    } else
		printf ("Unrecognized command: %s\n", ptr[0]);
	}
	free(buf);
	buf = NULL;
    }
    free (ptr);
    ptr = NULL;
    return 0;
}
