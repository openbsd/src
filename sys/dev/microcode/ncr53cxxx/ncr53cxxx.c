/*	$OpenBSD: ncr53cxxx.c,v 1.4 2003/04/06 18:54:20 ho Exp $	*/

/*
 * Copyright (c) 1995 Michael L. Hitch
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael L. Hitch.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*	scc.c	- SCSI SCRIPTS Compiler		*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AMIGA
#define strcmpi	strcasecmp
#endif

#define	MAXTOKENS	16
#define	MAXINST		1024
#define	MAXSYMBOLS	128

struct {
	long	type;
	char	*name;
} tokens[MAXTOKENS];
int	ntokens;
int	tokenix;

void	f_proc (void);
void	f_pass (void);
void	f_list (void);		/* ENTRY, EXTERNAL label list */
void	f_define (void);	/* ABSOLUTE, RELATIVE label list */
void	f_move (void);
void	f_jump (void);
void	f_call (void);
void	f_return (void);
void	f_int (void);
void	f_select (void);
void	f_reselect (void);
void	f_wait (void);
void	f_disconnect (void);
void	f_set (void);
void	f_clear (void);
void	f_arch (void);

struct {
	char	*name;
	void	(*func)(void);
} directives[] = {
	"PROC",		f_proc,
	"PASS",		f_pass,
	"ENTRY",	f_list,
	"ABSOLUTE",	f_define,
	"EXTERN",	f_list,
	"EXTERNAL",	f_list,
	"RELATIVE",	f_define,
	"MOVE",		f_move,
	"JUMP",		f_jump,
	"CALL",		f_call,
	"RETURN",	f_return,
	"INT",		f_int,
	"SELECT",	f_select,
	"RESELECT",	f_reselect,
	"WAIT",		f_wait,
	"DISCONNECT",	f_disconnect,
	"SET",		f_set,
	"CLEAR",	f_clear,
	"ARCH",		f_arch,
	NULL};

unsigned long script[MAXINST];
int	dsps;
char	*script_name = "SCRIPT";
unsigned long	inst0, inst1, inst2;
unsigned long	ninsts;
unsigned long	npatches;

struct patchlist {
	struct patchlist *next;
	unsigned	offset;
};

#define	S_LABEL		0x0000
#define	S_ABSOLUTE	0x0001
#define	S_RELATIVE	0x0002
#define	S_EXTERNAL	0x0003
#define	F_DEFINED	0x0001
#define	F_ENTRY		0x0002
struct {
	short	type;
	short	flags;
	unsigned long value;
	struct patchlist *patchlist;
	char	*name;
} symbols[MAXSYMBOLS];
int nsymbols;

char	*stypes[] = {"Label", "Absolute", "Relative", "External"};

char	*phases[] = {
	"data_out", "data_in", "cmd", "status",
	"res4", "res5", "msg_out", "msg_in"
};

char	*regs710[] = {
	"scntl0",	"scntl1",	"sdid",		"sien",
	"scid",		"sxfer",	"sodl",		"socl",
	"sfbr",		"sidl",		"sbdl",		"sbcl",
	"dstat",	"sstat0",	"sstat1",	"sstat2",
	"dsa0",		"dsa1",		"dsa2",		"dsa3",
	"ctest0",	"ctest1",	"ctest2",	"ctest3",
	"ctest4",	"ctest5",	"ctest6",	"ctest7",
	"temp0",	"temp1",	"temp2",	"temp3",
	"dfifo",	"istat",	"ctest8",	"lcrc",
	"dbc0",		"dbc1",		"dbc2",		"dcmd",
	"dnad0",	"dnad1",	"dnad2",	"dnad3",
	"dsp0",		"dsp1",		"dsp2",		"dsp3",
	"dsps0",	"dsps1",	"dsps2",	"dsps3",
	"scratch0",	"scratch1",	"scratch2",	"scratch3",
	"dmode",	"dien",		"dwt",		"dcntl",
	"addr0",	"addr1",	"addr2",	"addr3"
};

char	*regs720[] = {
	"scntl0",	"scntl1",	"scntl2",	"scntl3",
	"scid",		"sxfer",	"sdid",		"gpreg",
	"sfbr",		"socl",		"ssid",		"sbcl",
	"dstat",	"sstat0",	"sstat1",	"sstat2",
	"dsa0",		"dsa1",		"dsa2",		"dsa3",
	"istat",	"",		"",		"",
	"ctest0",	"ctest1",	"ctest2",	"ctest3",
	"temp0",	"temp1",	"temp2",	"temp3",
	"dfifo",	"ctest4",	"ctest5",	"ctest6",
	"dbc0",		"dbc1",		"dbc2",		"dcmd",
	"dnad0",	"dnad1",	"dnad2",	"dnad3",
	"dsp0",		"dsp1",		"dsp2",		"dsp3",
	"dsps0",	"dsps1",	"dsps2",	"dsps3",
	"scratcha0",	"scratcha1",	"scratcha2",	"scratcha3",
	"dmode",	"dien",		"dwt",		"dcntl",
	"addr0",	"addr1",	"addr2",	"addr3",
	"sien0",	"sien1",	"sist0",	"sist1",
	"slpar",	"swide",	"macntl",	"gpcntl",
	"stime0",	"stime1",	"respid0",	"respid1",
	"stest0",	"stest1",	"stest2",	"stest3",
	"sidl0",	"sidl1",	"",		"",
	"sodl0",	"sodl1",	"",		"",
	"sbdl0",	"sbdl1",	"",		"",
	"scratchb0",	"scratchb1",	"scratchb2",	"scratchb3",
};

int	lineno;
int	err_listed;
int	arch;

char	inbuf[128];

char	*sourcefile;
char	*outputfile;
char	*listfile;
char	*errorfile;

FILE	*infp;
FILE	*outfp;
FILE	*listfp;
FILE	*errfp;

void	parse (void);
void	process (void);
void	emit_symbols (void);
void	list_symbols (void);
void	errout (char *);
void	define_symbol (char *, unsigned long, short, short);
void	close_script (void);
void	new_script (char *);
void	store_inst (void);
int	expression (int *);
int	evaluate (int);
int	number (char *);
int	lookup (char *);
int	reserved (char *, int);
int	CheckPhase (int);
int	CheckRegister (int);
void	transfer (int, int);
void	select_reselect (int);
void	set_clear (unsigned long);
void	block_move (void);
void	register_write (void);
void	memory_to_memory (void);
void	error_line(void);
char	*makefn(char *, char *);
void	usage(void);

main (int argc, char *argv[])
{
	int	i;

	if (argc < 2 || argv[1][0] == '-')
		usage();
	sourcefile = argv[1];
	infp = fopen (sourcefile, "r");
	if (infp == NULL) {
		perror ("open source");
		fprintf (stderr, "scc: error opening source file %s\n", argv[1]);
		exit (1);
	}
	/*
	 * process options
	 * -l [listfile]
	 * -o [outputfile]
	 * -z [debugfile]
	 * -e [errorfile]
	 * -a arch
	 * -v
	 * -u
	 */
	for (i = 2; i < argc; ++i) {
		if (argv[i][0] != '-')
			usage();
		switch (argv[i][1]) {
		case 'o':
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				outputfile = makefn (sourcefile, "out");
			else {
				outputfile = argv[i + 1];
				++i;
			}
			break;
		case 'l':
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				listfile = makefn (sourcefile, "lis");
			else {
				listfile = argv[i + 1];
				++i;
			}
			break;
		case 'e':
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				errorfile = makefn (sourcefile, "err");
			else {
				errorfile = argv[i + 1];
				++i;
			}
			break;
		case 'a':
			if (i + 1 == argc)
				usage();
			arch = 0;
			arch = atoi(argv[i +1]);
			if(arch != 720 && arch != 710) {
				fprintf(stderr,"%s: bad arch '%s'\n",
					argv[0], argv[i +1]);
				exit(1);
			}
			++i;
			break;
		default:
			fprintf (stderr, "scc: unrecognized option '%c'\n",
			    argv[i][1]);
			usage();
		}
	}
	if (outputfile)
		outfp = fopen (outputfile, "w");
	if (listfile)
		listfp = fopen (listfile, "w");
	if (errorfile)
		errfp = fopen (errorfile, "w");
	else
		errfp = stderr;

	while (fgets (inbuf, sizeof (inbuf), infp)) {
		++lineno;
		if (listfp)
			fprintf (listfp, "%3d:  %s", lineno, inbuf);
		err_listed = 0;
		parse ();
		if (ntokens) {
#ifdef DUMP_TOKENS
			int	i;

			fprintf (listfp, "      %d tokens\n", ntokens);
			for (i = 0; i < ntokens; ++i) {
				fprintf (listfp, "      %d: ", i);
				if (tokens[i].type)
					fprintf (listfp,"'%c'\n", tokens[i].type);
				else
					fprintf (listfp, "%s\n", tokens[i].name);
			}
#endif
			if (ntokens >= 2 && tokens[0].type == 0 &&
			    tokens[1].type == ':') {
			    	define_symbol (tokens[0].name, dsps, S_LABEL, F_DEFINED);
				tokenix += 2;
			}
			if (tokenix < ntokens)
				process ();
		}

	}
	close_script ();
	emit_symbols ();
	if (outfp) {
		fprintf (outfp, "\nunsigned long INSTRUCTIONS = 0x%08x;\n", ninsts);
		fprintf (outfp, "unsigned long PATCHES = 0x%08x;\n", npatches);
	}
	list_symbols ();
}

void emit_symbols ()
{
	int	i;
	struct	patchlist *p;

	if (nsymbols == 0 || outfp == NULL)
		return;

	for (i = 0; i < nsymbols; ++i) {
		char	*code;
		if (symbols[i].type == S_ABSOLUTE)
			code = "A_";
		else if (symbols[i].type == S_RELATIVE)
			code = "R_";
		else if (symbols[i].type == S_EXTERNAL)
			code = "E_";
		else if (symbols[i].flags & F_ENTRY)
			code = "Ent_";
		else
			continue;
		fprintf (outfp, "#define\t%s%s\t0x%08x\n", code, symbols[i].name,
			symbols[i].value);
		if (symbols[i].flags & F_ENTRY || symbols[i].patchlist == NULL)
			continue;
		fprintf (outfp, "unsigned long %s%s_Used[] = {\n", code, symbols[i].name);
#if 1
		p = symbols[i].patchlist;
		while (p) {
			fprintf (outfp, "\t%08x,\n", p->offset / 4);
			p = p->next;
		}
#endif
		fprintf (outfp, "};\n\n");
	}
	/* patches ? */
}

void list_symbols ()
{
	int	i;

	if (nsymbols == 0 || listfp == NULL)
		return;
	fprintf (listfp, "\n\nValue     Type     Symbol\n");
	for (i = 0; i < nsymbols; ++i) {
		fprintf (listfp, "%08x: %-8s %s\n", symbols[i].value,
			stypes[symbols[i].type], symbols[i].name);
	}
}

void errout (char *text)
{
	error_line();
	fprintf (errfp, "*** %s ***\n", text);
}

void parse ()
{
	char *p = inbuf;
	char c;
	char string[64];
	char *s;
	size_t len; 

	ntokens = tokenix = 0;
	while (1) {
		while ((c = *p++) && c != '\n' && c <= ' ' || c == '\t')
			;
		if (c == '\n' || c == 0 || c == ';')
			break;
		if (ntokens >= MAXTOKENS) {
			errout ("Token table full");
			break;
		}
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
		    (c >= 'A' && c <= 'Z') || c == '$' || c == '_') {
		    	s = string;
		    	*s++ = c;
		    	while (((c = *p) >= '0' && c <= '9') ||
		    	    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    	    c == '_' || c == '$') {
		    	    	*s++ = *p++;
		    	}
		    	*s = 0;
			len = strlen (string) + 1; 
		    	tokens[ntokens].name = malloc (len);
		    	strlcpy (tokens[ntokens].name, string, len);
		    	tokens[ntokens].type = 0;
		}
		else {
			tokens[ntokens].type = c;
		}
		++ntokens;
	}
	return;
}

void	process ()
{
	int	i;

	if (tokens[tokenix].type) {
		error_line();
		fprintf (errfp, "Error: expected directive, found '%c'\n",
			tokens[tokenix].type);
		return;
	}
	for (i = 0; directives[i].name; ++i) {
		if (strcmpi (directives[i].name, tokens[tokenix].name) == 0)
			break;
	}
	if (directives[i].name == NULL) {
		error_line();
		fprintf (errfp, "Error: expected directive, found \"%s\"\n",
			tokens[tokenix].name);
		return;
	}
	if (directives[i].func == NULL) {
		error_line();
		fprintf (errfp, "No function for directive \"%s\"\n", tokens[tokenix].name);
	} else {
#if 0
		fprintf (listfp, "Processing directive \"%s\"\n", directives[i].name);
#endif
		++tokenix;
		(*directives[i].func) ();
	}
}

void define_symbol (char *name, unsigned long value, short type, short flags)
{
	int	i;
	struct patchlist *p;
	size_t	len;

	for (i = 0; i < nsymbols; ++i) {
		if (symbols[i].type == type && strcmp (symbols[i].name, name) == 0) {
			if (symbols[i].flags & F_DEFINED) {
				error_line();
				fprintf (errfp, "*** Symbol \"%s\" multiply defined\n",
					name);
			} else {
				symbols[i].flags |= flags;
				symbols[i].value = value;
				p = symbols[i].patchlist;
				while (p) {
					if (p->offset > dsps)
						errout ("Whoops\007");
					else
						script[p->offset / 4] = dsps - p->offset - 4;
					p = p->next;
				}
			}
			return;
		}
	}
	if (nsymbols >= MAXSYMBOLS) {
		errout ("Symbol table full");
		return;
	}
	symbols[nsymbols].type = type;
	symbols[nsymbols].flags = flags;
	symbols[nsymbols].value = value;
	symbols[nsymbols].patchlist = NULL;
	len = strlen (name) + 1; 
	symbols[nsymbols].name = malloc (len);
	strlcpy (symbols[nsymbols].name, name, len);
	++nsymbols;
}

void close_script ()
{
	int	i;

	if (dsps == 0)
		return;
	if (outfp) {
		fprintf (outfp, "unsigned long %s[] = {\n", script_name);
		for (i = 0; i < dsps / 4; i += 2) {
			fprintf (outfp, "\t0x%08x, 0x%08x", script[i],
				script[i + 1]);
			/* check for memory move instruction */
			if (script[i] >> 30 == 3)
				fprintf (outfp, ", 0x%08x,", script[i + 2]);
			else
				if ((i + 2) <= dsps / 4) fprintf (outfp, ",\t\t");
			fprintf (outfp, "\t/* %03x - %3d */\n", i * 4, i * 4);
			if (script[i] >> 30 == 3)
				++i;
		}
		fprintf (outfp, "};\n\n");
	}
	dsps = 0;
}

void new_script (char *name)
{
	size_t len = strlen (name) + 1;

	close_script ();
	script_name = malloc (len);
	strlcpy (script_name, name, len);
}

int	reserved (char *string, int t)
{
	if (tokens[t].type == 0 && strcmpi (tokens[t].name, string) == 0)
		return (1);
	return (0);
}

int	CheckPhase (int t)
{
	int	i;

	for (i = 0; i < 8; ++i) {
		if (reserved (phases[i], t)) {
			inst0 |= i << 24;
			return (1);
		}
	}
	return (0);
}

int	CheckRegister (int t)
{
	int	i;

	if(arch == 710) {
		for (i = 0; i < 64; ++i)
			if (reserved (regs710[i], t))
				return i;
	}
	else if (arch == 720) {
		for (i = 0; i < 96; ++i)
			if (reserved (regs720[i], t))
				return i;
	}
	else {
		errout("'ARCH' statement missing");
	}
	return (-1);
}

int	expression (int *t)
{
	int	value;
	int	i = *t;

	value = evaluate (i++);
	while (i < ntokens) {
		if (tokens[i].type == '+')
			value += evaluate (i + 1);
		else if (tokens[i].type == '-')
			value -= evaluate (i + 1);
		else
			errout ("Unknown identifier");
		i += 2;
	}
	*t = i;
	return (value);
}

int	evaluate (t)
{
	int	value;
	char	*name;

	if (tokens[t].type) {
		errout ("Expected an identifier");
		return (0);
	}
	name = tokens[t].name;
	if (*name >= '0' && *name <= '9')
		value = number (name);
	else
		value = lookup (name);
	return (value);
}

int	number (char *s)
{
	int	value;
	int	n;
	int	radix;

	radix = 10;
	if (*s == '0') {
		++s;
		radix = 8;
		switch (*s) {
		case 'x':
		case 'X':
			radix = 16;
			break;
		case 'b':
		case 'B':
			radix = 2;
		}
		if (radix != 8)
			++s;
	}
	value = 0;
	while (*s) {
		n = *s++;
		if (n >= '0' && n <= '9')
			n -= '0';
		else if (n >= 'a' && n <= 'f')
			n -= 'a' - 10;
		else if (n >= 'A' && n <= 'F')
			n -= 'A' - 10;
		else {
			error_line();
			fprintf (errfp, "*** Expected digit\n", n = 0);
		}
		if (n >= radix)
			errout ("Expected digit");
		else
			value = value * radix + n;
	}
	return (value);
}

int	lookup (char *name)
{
	int	i;
	struct patchlist *p;
	size_t	len;

	for (i = 0; i < nsymbols; ++i) {
		if (strcmp (name, symbols[i].name) == 0) {
			if ((symbols[i].flags & F_DEFINED) == 0) {
				p = (struct patchlist *) &symbols[i].patchlist;
				while (p->next)
					p = p->next;
				p->next = (struct patchlist *) malloc (sizeof (struct patchlist));
				p = p->next;
				p->next = NULL;
				p->offset = dsps + 4;
			}
			return ((int) symbols[i].value);
		}
	}
	if (nsymbols >= MAXSYMBOLS) {
		errout ("Symbol table full");
		return (0);
	}
	symbols[nsymbols].type = S_LABEL;	/* assume forward reference */
	symbols[nsymbols].flags = 0;
	symbols[nsymbols].value = 0;
	p = (struct patchlist *) malloc (sizeof (struct patchlist));
	symbols[nsymbols].patchlist = p;
	p->next = NULL;
	p->offset = dsps + 4;
	len = strlen (name) + 1;
	symbols[nsymbols].name = malloc (len);
	strlcpy (symbols[nsymbols].name, name, len);
	++nsymbols;
	return (0);
}

void	f_arch (void)
{
	int i, archsave;

	i = tokenix;

	archsave = arch;
	arch = 0;
	arch = atoi(tokens[i].name);
	if( arch != 710 && arch != 720) {
		errout("Unrecognized ARCH");
		arch = archsave;
	}
}

void	f_proc (void)
{
	if (tokens[tokenix].type != 0 || tokens[tokenix + 1].type != ':')
		errout ("Invalid PROC statement");
	else
		new_script (tokens[tokenix].name);
}

void	f_pass (void)
{
	errout ("PASS option not implemented");
}

/*
 *	f_list:  process list of symbols for the ENTRY and EXTERNAL directive
 */

void	f_list (void)
{
	int	i;
	short	type;
	short	flags;

	type = strcmpi (tokens[tokenix-1].name, "ENTRY") ? S_EXTERNAL : S_LABEL;
	flags = type == S_LABEL ? F_ENTRY : 0;
	for (i = tokenix; i < ntokens; ++i) {
		if (tokens[i].type != 0) {
			errout ("Expected an identifier");
			return;
		}
		define_symbol (tokens[i].name, 0, type, flags);
		if (i + 1 < ntokens) {
			if (tokens[++i].type == ',')
				continue;
			errout ("Expected a separator");
			return;
		}
	}
}

/*
 *	f_define:	process list of definitions for ABSOLUTE and RELATIVE directive
 */

void	f_define (void)
{
	int	i;
	char	*name;
	unsigned long value;
	int	type;

	type = strcmpi (tokens[tokenix-1].name, "ABSOLUTE") ? S_RELATIVE : S_ABSOLUTE;
	i = tokenix;
	while (i < ntokens) {
		if (tokens[i].type) {
			errout ("Expected an identifier");
			return;
		}
		if (tokens[i + 1].type != '=') {
			errout ("Expected a separator");
			return;
		}
		name = tokens[i].name;
		i += 2;
		value = expression (&i);
		define_symbol (name, value, type, F_DEFINED);
	}
}

void	store_inst ()
{
	int	i = dsps / 4;
	int	l = 8;

	if ((inst0 & 0xc0000000) == 0xc0000000)
		l = 12;			/* Memory to memory move is 12 bytes */
	if ((dsps + l) / 4 > MAXINST) {
		errout ("Instruction table overflow");
		return;
	}
	script[i++] = inst0;
	script[i++] = inst1;
	if (l == 12)
		script[i] = inst2;
	if (listfp) {
		fprintf (listfp, "\t%04x: %08x %08x", dsps, inst0, inst1);
		if (l == 12)
			fprintf (listfp, " %08x", inst2);
		fprintf (listfp, "\n");
	}
	dsps += l;
	inst0 = inst1 = inst2 = 0;
	++ninsts;
}

void	f_move (void)
{
	if (reserved ("memory", tokenix))
		memory_to_memory ();
	else if (reserved ("from", tokenix) || tokens[tokenix+1].type == ',')
		block_move ();
	else
		register_write ();
	store_inst ();
}

void	f_jump (void)
{
	transfer (0x80000000, 0);
}

void	f_call (void)
{
	transfer (0x88000000, 0);
}

void	f_return (void)
{
	transfer (0x90000000, 1);
}

void	f_int (void)
{
	transfer (0x98000000, 2);
}

void	f_select (void)
{
	int	t = tokenix;

	if (reserved ("atn", t)) {
		inst0 = 0x01000000;
		++t;
	}
	select_reselect (t);
}

void	f_reselect (void)
{
	select_reselect (tokenix);
}

void	f_wait (void)
{
	int	i = tokenix;

	inst1 = 0;
	if (reserved ("disconnect", i)) {
		inst0 = 0x48000000;
	}
	else {
		if (reserved ("reselect", i))
			inst0 = 0x50000000;
		else if (reserved ("select", i))
			inst0 = 0x50000000;
		else
			errout ("Expected SELECT or RESELECT");
		++i;
		if (reserved ("rel", i)) {
			i += 2;
			inst1 = evaluate (i) - dsps - 8;
			inst0 |= 0x04000000;
		}
		else
			inst1 = evaluate (i);
	}
	store_inst ();
}

void	f_disconnect (void)
{
	inst0 = 0x48000000;
	store_inst ();
}

void	f_set (void)
{
	set_clear (0x58000000);
}

void	f_clear (void)
{
	set_clear (0x60000000);
}

void	transfer (int word0, int type)
{
	int	i;

	i = tokenix;
	inst0 = word0;
	if (type == 0 && reserved ("rel", i)) {
		inst1 = evaluate (i + 2) - dsps - 8;
		i += 3;
		inst0 |= 0x00800000;
	}
	else if (type != 1) {
		inst1 = evaluate (i);
	}
	++i;
	if (i >= ntokens) {
		inst0 |= 0x00080000;
		store_inst ();
		return;
	}
	if (tokens[i].type != ',')
		errout ("Expected a separator, ',' assumed");
	else
		++i;
	if (reserved("when", i))
		inst0 |= 0x00010000;
	else if (reserved ("if", i) == 0) {
		errout ("Expected a reserved word");
		store_inst ();
		return;
	}
	if (reserved ("not", ++i))
		++i;
	else
		inst0 |= 0x00080000;
	if (reserved ("atn", i)) {
		inst0 |= 0x00020000;
		++i;
	} else if (CheckPhase (i)) {
		inst0 |= 0x00020000;
		++i;
	}
	if (i < ntokens && tokens[i].type != ',') {
		if (inst0 & 0x00020000) {
			if (inst0 & 0x00080000 && reserved ("and", i)) {
				++i;
			}
			else if ((inst0 & 0x00080000) == 0 && reserved ("or", i)) {
				++i;
			}
			else
				errout ("Expected a reserved word");
		}
		inst0 |= 0x00040000 + (evaluate (i++) & 0xff);
	}
	if (i < ntokens) {
		if (tokens[i].type == ',')
			++i;
		else
			errout ("Expected a separator, ',' assumed");
		if (reserved ("and", i) && reserved ("mask", i + 1))
			inst0 |= ((evaluate (i + 2) & 0xff) << 8);
		else
			errout ("Expected , AND MASK");
	}
	store_inst ();
}

void 	select_reselect (int t)
{
	inst0 |= 0x40000000;		/* ATN may be set from SELECT */
	if (reserved ("from", t)) {
		++t;
		inst0 |= 0x02000000 | evaluate (t++);
	}
	else
		inst0 |= (evaluate (t++) & 0xff) << 16;
	if (tokens[t++].type == ',') {
		if (reserved ("rel", t)) {
			inst0 |= 0x04000000;
			inst1 = evaluate (t + 2) - dsps - 8;
		}
		else
			inst1 = evaluate (t);
	}
	else
		errout ("Expected separator");
	store_inst ();
}

void	set_clear (unsigned long code)
{
	int	i = tokenix;
	short	need_and = 0;

	inst0 = code;
	while (i < ntokens) {
		if (need_and) {
			if (reserved ("and", i))
				++i;
			else
				errout ("Expected AND");
		}
		if (reserved ("atn", i)) {
			inst0 |= 0x0008;
			++i;
		}
		else if (reserved ("ack", i)) {
			inst0 |= 0x0040;
			++i;
		}
		else if (reserved ("target", i)) {
			inst0 |= 0x0200;
			++i;
		}
		else
			errout ("Expected ATN, ACK, or TARGET");
		need_and = 1;
	}
	store_inst ();
}

void	block_move ()
{
	int	t;

	if (reserved ("from", tokenix)) {
		inst1 = evaluate (tokenix+1);
		inst0 |= 0x10000000 | inst1;	/*** ??? to match Zeus script */
		tokenix += 2;
	}
	else {
		inst0 |= evaluate (tokenix++);	/* count */
		tokenix++;			/* skip ',' */
		if (reserved ("ptr", tokenix)) {
			++ tokenix;
			inst0 |= 0x20000000;
		}
		inst1 = evaluate (tokenix++);	/* address */
	}
	if (tokens[tokenix].type != ',')
		errout ("Expected separator");
	if (reserved ("when", tokenix + 1)) {
		inst0 |= 0x08000000;
		CheckPhase (tokenix + 2);
	}
	else if (reserved ("with", tokenix + 1)) {
		CheckPhase (tokenix + 2);
	}
	else
		errout ("Expected WITH or WHEN");
}

void	register_write ()
{
	/*
	 * MOVE reg/data8 TO reg			register write
	 * MOVE reg <op> data8 TO reg			register write
	 */
	int	op;
	int	reg;
	int	data;

	if (reserved ("to", tokenix+1))
		op = 0;
	else if (tokens[tokenix+1].type == '|')
		op = 1;
	else if (tokens[tokenix+1].type == '&')
		op = 2;
	else if (tokens[tokenix+1].type == '+')
		op = 3;
	else if (tokens[tokenix+1].type == '-')
		op = 4;
	else
		errout ("Unknown register operator");
	if (op && reserved ("to", tokenix+3) == 0)
		errout ("Register command expected TO");
	reg = CheckRegister (tokenix);
	if (reg < 0) {			/* Not register, must be data */
		data = evaluate (tokenix);
		if (op)
			errout ("Register operator not move");
		reg = CheckRegister (tokenix+2);
		if (reg < 0)
			errout ("Expected register");
		inst0 = 0x78000000 | (data << 8) | reg;
#if 0
fprintf (listfp, "Move data to register: %02x %d\n", data, reg);
#endif
	}
	else if (op) {			/* A register read/write operator */
		data = evaluate (tokenix+2);
		if (op == 4) {
			data = -data;
			op = 3;
		}
		inst0 = (data & 0xff) << 8;
		data = CheckRegister (tokenix+4);
		if (data < 0)
			errout ("Expected register");
		if (reg != data && reg != 8 && data != 8)
			errout ("One register MUST be SBFR");
		if (reg == data) {	/* A register read/modify/write */
#if 0
fprintf (listfp, "Read/modify register: %02x %d %d\n", inst0 >> 8, op, reg);
#endif
			inst0 |= 0x78000000 | (op << 25) | (reg << 16);
		}
		else {			/* A move to/from SFBR */
			if (reg == 8) {	/* MOVE SFBR <> TO reg */
#if 0
fprintf (listfp, "Move SFBR to register: %02x %d %d\n", inst0 >> 8, op, data);
#endif
				inst0 |= 0x68000000 | (op << 25) | (data << 16);
			}
			else {
#if 0
fprintf (listfp, "Move register to SFBR: %02x %d %d\n", inst0 >> 8, op, reg);
#endif
				inst0 |= 0x70000000 | (op << 25) | (reg << 16);
			}
		}
	}
	else {				/* register to register */
		data = CheckRegister (tokenix+2);
		if (reg == 8)		/* move SFBR to reg */
			inst0 = 0x6a000000 | (data << 16);
		else if (data == 8)	/* move reg to SFBR */
			inst0 = 0x72000000 | (reg << 16);
		else
			errout ("One register must be SFBR");
	}
}

void	memory_to_memory ()
{
	inst0 = 0xc0000000 + evaluate (tokenix+1);
	inst1 = evaluate (tokenix+3);
	inst2 = evaluate (tokenix+5);
}

void	error_line()
{
	if (errfp != listfp && errfp && err_listed == 0) {
		fprintf (errfp, "%3d:  %s", lineno, inbuf);
		err_listed = 1;
	}
}

char *	makefn (base, sub)
	char *base;
	char *sub;
{
	char *fn;
	size_t len = strlen (base) + strlen (sub) + 2; 

	fn = malloc (len);
	strlcpy (fn, base, len);
	base = strrchr(fn, '.');
	if (base)
		*base = 0;
	strlcat (fn, ".", len);
	strlcat (fn, sub, len);
	return (fn);
}

void	usage()
{
	fprintf (stderr, "usage: scc sourcfile [options]\n");
	exit(1);
}
