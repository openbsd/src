/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#include "vldb_locl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

RCSID("$arla: vled.c,v 1.7 2002/10/01 23:35:35 lha Exp $");

struct ed_context {
    long begin;
    long end;
    long current;
    struct ed_command *cmds;
};

typedef int (*ed_pcmd)(struct ed_context *context,
		       const char *str, size_t len);


struct ed_command {
    char ed_c;
    ed_pcmd ed_cmd;
    char *ed_help;
};

typedef enum { ED_NOERR = 0, ED_SYNTAX, ED_NOCMD, ED_FATAL, ED_EOF,
	       ED_EXIT } ed_ret;
typedef enum { ED_RANGE_END = -1 } ed_val;

static ed_ret get_string (FILE *f, char *buf, size_t len);
static ed_ret findexec_cmd (struct ed_command *cmds, const char *buf,
			    size_t len, struct ed_context *context);
static ed_ret parse_addr (struct ed_context *context, char **p, size_t *len);


int ed_loop (struct ed_command *cmds);
int ed_help (struct ed_context *context, const char *str, size_t len);

/*
 *
 */

static int
local_common (struct ed_context *context,
	      const char *str, size_t len,
	      int (*func) (disk_vlentry *, long num, const char *, 
			   size_t, void *),
	      void *ptr)
{
    disk_vlentry vlentry;
    int ret;
    long i;

    if (context->begin == ED_RANGE_END || context->begin > HASHSIZE)
	context->begin = HASHSIZE;
    if (context->end == ED_RANGE_END || context->end > HASHSIZE-1)
	context->end = HASHSIZE;
    else
	context->end++;

    for (i = context->begin ; i < context->end ; i++) {
	ret = vldb_get_first_id_entry(i, 0, &vlentry);
	if (ret == 0) {
	    context->current = i;
	    if ((*func) (&vlentry, i, str, len, ptr))
		return 0;
	} else if (ret == VL_NOENT)
	    continue;
	else
	    err (1, "got strange return-code from "
		 "vldb_get_first_id_entry: %d\n", ret);
    }

    return 0;
}

static int
print_func (disk_vlentry *entry, long num,
	    const char *str, size_t len, void *ptr)
{
    printf ("pos: %ld\n  ", num);
    vldb_print_entry (entry, 1);
    return 0;
}

static int
local_print (struct ed_context *context,
	     const char *str, size_t len)
{
    return local_common (context, str, len, print_func, NULL);
}

static int
search_func (disk_vlentry *entry, long num,
	     const char *str, size_t len, void *ptr)
{
    if (strstr (entry->name, str) == NULL)
	return 0;
    printf ("pos: %ld\n  ", num);
    vldb_print_entry (entry, 0);
    return 0;
}

static int
local_search (struct ed_context *context,
	      const char *str, size_t len)
{
    if (len == 0) {
	context->begin = context->current;
    } else {
	context->begin = 0;
	context->end = ED_RANGE_END;
    }
    return local_common (context, str, len, search_func, NULL);
}

static int
local_add (struct ed_context *context, const char *str, size_t len)
{
    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
local_addserver (struct ed_context *context, const char *str, size_t len)
{
    disk_vlentry vlentry;
    int ret;

    if (len == 0)
	return ED_SYNTAX;
    
    ret = vldb_get_first_id_entry(context->begin, 0, &vlentry);
    if (ret) {
	printf ("addserver: vldb_get_first_id_entry failed %d\n", ret);
	return ED_NOERR;
    }

#if 0
    vldb_print_entry (&vlentry, 1);
#endif

    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
backup_func (disk_vlentry *entry, long num,
	     const char *str, size_t len, void *ptr)
{
    int i;

    printf ("%dd\n%dd\n%dd\n", 
	    entry->volumeId[RWVOL],
	    entry->volumeId[ROVOL],
	    entry->volumeId[BACKVOL]);

    printf ("a %s %d %d %d 0x0\n",
	    entry->name,
	    entry->volumeId[RWVOL],
	    entry->volumeId[ROVOL],
	    entry->volumeId[BACKVOL]);
    for (i = 0; i < MAXNSERVERS; i++) {
	struct in_addr in;
	if (entry->serverNumber[i] == 0)
	    break;
	in.s_addr = entry->serverNumber[i];
	printf ("A %s %d 0x%x\n",
		inet_ntoa(in),
		entry->serverPartition[i],
		entry->serverFlags[i]);
    }
    printf ("f 0x%d\n", entry->flags);
    return 0;
}

static int
local_backup (struct ed_context *context, const char *str, size_t len)
{
    return local_common (context, str, len, backup_func, NULL);
}

static int
local_delete (struct ed_context *context, const char *str, size_t len)
{
    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
local_deleteserver (struct ed_context *context, const char *str, size_t len)
{
    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
local_modify (struct ed_context *context, const char *str, size_t len)
{
    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
local_rename (struct ed_context *context, const char *str, size_t len)
{
    printf ("not implemented yet\n");
    return ED_NOERR;
}

static int
quit (struct ed_context *context, const char *str, size_t len)
{
    return ED_EXIT;
}

struct ed_command vled[] = {
    { 'p', local_print,		"print vldb entry" },
    { '/', local_search,	"search vldb entry" },
    { 'a', local_add,		"add new entry"},
    { 'A', local_addserver,	"add new server to entry"},
    { 'b', local_backup,	"backup/dump a entry"},
    { 'd', local_delete,	"delete entry"},
    { 'D', local_deleteserver,	"delete server from entry"},
    { 'm', local_modify,	"modify entry"},
    { 'n', local_rename,	"rename entry"},
    { '?', ed_help,		"help" },
    { 'q', quit,		"exit" },
    { 0, NULL, NULL }
};

/*
 *
 */

static ed_ret
get_string (FILE *f, char *buf, size_t len)
{
    char *str;

    str = fgets (buf, len, f);
    if (str == NULL) {
	if (feof(f))
	    return ED_EOF;
	return ED_FATAL;
    }
    buf[strcspn(buf, "\n")] = '\0';
    return ED_NOERR;
}

static ed_ret
parse_element (char **p, size_t *len, long *raddr, long current, int first)
{
    long addr;
    char *old = *p;

    while (1) {
	if (*len < 1)
	    return ED_SYNTAX;

	switch (**p) {
	case '^':
	    *raddr = 0;
	    (*p)++;
	    *len -= 1;
	    return ED_NOERR;
	case '$':
	    *raddr = ED_RANGE_END;
	    (*p)++;
	    *len -= 1;
	    return ED_NOERR;
	case ',':
	    if (first) {
		*raddr = 0;
		return ED_NOERR;
	    }
	    (*p)++;
	    *len -= 1;
	    break;
	case '1': case '2': case '3' :case '4': case '5': 
	case '6': case '7': case '8': case '9' :case '0':
	    addr = strtol (*p, p, 0);
	    if ((addr == LONG_MAX || addr == LONG_MIN) && errno == ERANGE) {
		*len -= *p - old;
		return ED_SYNTAX;
	    }
	    *raddr = addr;
	    return ED_NOERR;
	default:
	    if (!first)
		*raddr = ED_RANGE_END;
	    else
		*raddr = current;
	    return ED_NOERR;
	}
    }
    return ED_NOERR;
}


static ed_ret
parse_addr (struct ed_context *context, char **p, size_t *len)
{
    long addr;
    int num_el = 0;
    ed_ret ret;

    context->begin = context->end = context->current;

    if (*len < 1)
	return ED_SYNTAX;

    while (num_el < 2) {
	ret = parse_element (p, len, &addr, context->current, !num_el);
	if (ret != ED_NOERR)
	    break;
	num_el++;
	
	context->begin = context->end;
	context->end = addr;

       	if (**p != ',' && **p != ';')
	    break;
	else if (**p == ';') {
	    (*p)++;
	    context->current = addr;
	}
    }
    if (num_el == 1 || context->end != addr)
	context->begin = context->end;
    return ED_NOERR;
}

static ed_ret
findexec_cmd (struct ed_command *cmds, const char *buf, size_t len,
	      struct ed_context *context)
{
    struct ed_command *c;

    if (len == 0)
	return ED_SYNTAX;

    for (c = cmds; c->ed_cmd; c++) {
	if (c->ed_c == *buf)
	    return (c->ed_cmd) (context, (buf+1), (len-1));
    }
    return ED_NOCMD;
}

#define HANDLE_ERROR(ret)			\
    switch (ret) {				\
    case ED_NOERR:				\
	break;					\
    case ED_EOF:				\
    case ED_EXIT:				\
	return ED_NOERR;			\
    case ED_FATAL:				\
	return ret;				\
    default:					\
	printf ("?\n");				\
	break;					\
    }						\
    if (ret != ED_NOERR) continue
	

int
ed_help (struct ed_context *context, const char *str, size_t len)
{
    struct ed_command *c;

    printf ("help:\n");
    for (c = context->cmds; c->ed_cmd; c++)
	printf ("  %c\t%s\n", c->ed_c, c->ed_help);
    return ED_NOERR;
}

int
ed_loop (struct ed_command *cmds)
{
    struct ed_context context = {0, 0, 0, NULL};
    size_t len;
    char buf[4*1024]; 
    char *str;
    char **p;
    ed_ret ret;

    if (cmds == NULL)
	return EINVAL;

    context.cmds = cmds;

    while (1) {
	ret = get_string (stdin, buf, sizeof(buf));
	HANDLE_ERROR(ret);

	str = buf;
	p = &str;
	len = sizeof(buf);
	ret = parse_addr (&context, p, &len);
	HANDLE_ERROR(ret);
	ret = findexec_cmd (cmds, *p, len, &context);
	HANDLE_ERROR(ret);
    }
}


int
main(int argc, char **argv)
{
    char *databasedir = NULL;
    vldb_init(databasedir);
    
    ed_loop (vled);

    return 0;
}
