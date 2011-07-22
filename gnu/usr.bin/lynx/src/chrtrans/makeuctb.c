/*
 * $LynxId: makeuctb.c,v 1.39 2009/01/01 17:01:15 tom Exp $
 *
 *  makeuctb.c, derived from conmakehash.c   - kw
 *
 *    Original comments from conmakehash.c:
 *
 *  Create arrays for initializing the kernel folded tables (using a hash
 *  table turned out to be to limiting...)  Unfortunately we can't simply
 *  preinitialize the tables at compile time since kfree() cannot accept
 *  memory not allocated by kmalloc(), and doing our own memory management
 *  just for this seems like massive overkill.
 *
 *  Copyright (C) 1995 H. Peter Anvin
 *
 *  This program is a part of the Linux kernel, and may be freely
 *  copied under the terms of the GNU General Public License (GPL),
 *  version 2, or at your option any later version.
 */

#ifndef HAVE_CONFIG_H
/* override HTUtils.h fallbacks for cross-compiling */
#undef HAVE_LSTAT
#undef NO_FILIO_H
#define HAVE_LSTAT 1
#define NO_FILIO_H 1
#endif

#define DONT_USE_GETTEXT
#define DONT_USE_SOCKS5
#include <UCDefs.h>
#include <UCkd.h>
#include <LYUtils.h>

/*
 *  Don't try to use LYexit() since this is a standalone file.
 */
#ifdef exit
#undef exit
#endif /* exit */

#define MAX_FONTLEN 256

/*
 *  We don't deal with UCS4 here. - KW
 */
typedef u16 unicode;

static FILE *chdr = 0;

/*
 * Since we may be writing the formatted file to stdout, ensure that we flush
 * everything before leaving, since some old (and a few not-so-old) platforms
 * do not properly implement POSIX 'exit()'.
 */
static void done(int code) GCC_NORETURN;

static void done(int code)
{
    if (chdr != 0) {
	fflush(chdr);
	fclose(chdr);
    }
    fflush(stderr);
    exit(code);
}

static void usage(void)
{
    static const char *tbl[] =
    {
	"Usage: makeuctb [parameters]",
	"",
	"Utility to convert .tbl into .h files for Lynx compilation.",
	"",
	"Parameters (all are optional):",
	"  1: the input file (normally {filename}.tbl, but \"-\" for stdin",
	"  2: the output file (normally {filename}.tbl but \"-\" for stdout",
	"  3: charset mime name",
	"  4: charset display name"
    };
    unsigned n;

    for (n = 0; n < TABLESIZE(tbl); n++) {
	fprintf(stderr, "%s\n", tbl[n]);
    };
    done(EX_USAGE);
}

#ifdef EXP_ASCII_CTYPES
int ascii_tolower(int i)
{
    if (91 > i && i > 64)
	return (i + 32);
    else
	return i;
}
#endif

/* copied from HTString.c, not everybody has strncasecmp */
int strncasecomp(const char *a, const char *b, int n)
{
    const char *p = a;
    const char *q = b;

    for (p = a, q = b;; p++, q++) {
	int diff;

	if (p == (a + n))
	    return 0;		/*   Match up to n characters */
	if (!(*p && *q))
	    return (*p - *q);
	diff = TOLOWER(*p) - TOLOWER(*q);
	if (diff)
	    return diff;
    }
    /*NOTREACHED */
}

static int getunicode(char **p0)
{
    char *p = *p0;

    while (*p == ' ' || *p == '\t')
	p++;

    if (*p == '-') {
	return -2;
    } else if (*p != 'U' || p[1] != '+' ||
	       !isxdigit(UCH(p[2])) ||
	       !isxdigit(UCH(p[3])) ||
	       !isxdigit(UCH(p[4])) ||
	       !isxdigit(UCH(p[5])) ||
	       isxdigit(UCH(p[6]))) {
	return -1;
    }
    *p0 = p + 6;
    return strtol((p + 2), 0, 16);
}

/*
 *  Massive overkill, but who cares?
 */
static unicode unitable[MAX_FONTLEN][255];
static int unicount[MAX_FONTLEN];

static struct unimapdesc_str themap_str =
{0, NULL, 0, 0};

static const char *tblname;
static const char *hdrname;

static int RawOrEnc = 0;
static int Raw_found = 0;	/* whether explicit R directive found */
static int CodePage = 0;

#define MAX_UNIPAIRS 4500

static void addpair_str(char *str, int un)
{
    int i = 0;

    if (un <= 0xfffe) {
	if (!themap_str.entry_ct) {
	    /*
	     *  Initialize the map for replacement strings.
	     */
	    themap_str.entries = (struct unipair_str *) malloc(MAX_UNIPAIRS
							       * sizeof(struct unipair_str));

	    if (!themap_str.entries) {
		fprintf(stderr,
			"%s: Out of memory\n", tblname);
		done(EX_DATAERR);
	    }
	} else {
	    /*
	     *  Check that it isn't a duplicate.
	     */
	    for (i = 0; i < themap_str.entry_ct; i++) {
		if (themap_str.entries[i].unicode == un) {
		    themap_str.entries[i].replace_str = str;
		    return;
		}
	    }
	}

	/*
	 *  Add to list.
	 */
	if (themap_str.entry_ct > MAX_UNIPAIRS - 1) {
	    fprintf(stderr,
		    "ERROR: Only %d unicode replacement strings permitted!\n",
		    MAX_UNIPAIRS);
	    done(EX_DATAERR);
	}
	themap_str.entries[themap_str.entry_ct].unicode = un;
	themap_str.entries[themap_str.entry_ct].replace_str = str;
	themap_str.entry_ct++;
    }
    /* otherwise: ignore */
}

static void addpair(int fp, int un)
{
    int i;

    if (!Raw_found) {		/* enc not (yet) explicitly given with 'R' */
	if (fp >= 128) {
	    if (RawOrEnc != UCT_ENC_8BIT && RawOrEnc <= UCT_ENC_8859) {
		if (fp < 160) {	/* cannot be 8859 */
		    RawOrEnc = UCT_ENC_8BIT;
		} else if (fp != 160 && fp != 173) {
		    RawOrEnc = UCT_ENC_8859;	/* hmmm.. more tests needed? */
		} else if (unicount[fp] == 0 && fp != un) {
		    /* first unicode for fp doesn't map to itself */
		    RawOrEnc = UCT_ENC_8BIT;
		} else {
		    RawOrEnc = UCT_ENC_8859;	/* hmmm.. more tests needed? */
		}
	    }
	}
    }
    if (un <= 0xfffe) {
	/*
	 *  Check that it isn't a duplicate.
	 */
	for (i = 0; i < unicount[fp]; i++) {
	    if (unitable[fp][i] == un) {
		return;
	    }
	}

	/*
	 *  Add to list.
	 */
	if (unicount[fp] > 254) {
	    fprintf(stderr, "ERROR: Only 255 unicodes/glyph permitted!\n");
	    done(EX_DATAERR);
	}
	unitable[fp][unicount[fp]] = un;
	unicount[fp]++;
    }
    /* otherwise: ignore */
}

static char this_MIMEcharset[UC_MAXLEN_MIMECSNAME + 1];
static char this_LYNXcharset[UC_MAXLEN_LYNXCSNAME + 1];
static char id_append[UC_MAXLEN_ID_APPEND + 1] = "_";
static int this_isDefaultMap = -1;
static int useDefaultMap = 1;
static int lowest_eight = 999;

int main(int argc, char **argv)
{
    static const char *first_ifdefs[] =
    {
	"/*",
	" * Compile-in this chunk of code unless we've turned it off specifically",
	" * or in general (id=%s).",
	" */",
	"",
	"#ifndef INCL_CHARSET%s",
	"#define INCL_CHARSET%s 1",
	"",
	"/*ifdef NO_CHARSET*/",
	"#ifdef  NO_CHARSET",
	"#undef  NO_CHARSET",
	"#endif",
	"#define NO_CHARSET 0 /* force default to always be active */",
	"",
	"/*ifndef NO_CHARSET%s*/",
	"#ifndef NO_CHARSET%s",
	"",
	"#if    ALL_CHARSETS",
	"#define NO_CHARSET%s 0",
	"#else",
	"#define NO_CHARSET%s 1",
	"#endif",
	"",
	"#endif /* ndef(NO_CHARSET%s) */",
	"",
	"#if NO_CHARSET%s",
	"#define UC_CHARSET_SETUP%s /*nothing*/",
	"#else"
    };
    static const char *last_ifdefs[] =
    {
	"",
	"#endif /* NO_CHARSET%s */",
	"",
	"#endif /* INCL_CHARSET%s */"
    };

    FILE *ctbl;
    char buffer[65536];
    char *outname = 0;
    unsigned n;
    int fontlen;
    int i, nuni, nent;
    int fp0 = 0, fp1 = 0, un0, un1;
    char *p, *p1;
    char *tbuf, ch;

    if (argc < 2 || argc > 5) {
	usage();
    }

    if (!strcmp(argv[1], "-")) {
	ctbl = stdin;
	tblname = "stdin";
    } else {
	ctbl = fopen(tblname = argv[1], "r");
	if (!ctbl) {
	    perror(tblname);
	    done(EX_NOINPUT);
	}
    }

    if (argc > 2) {
	if (!strcmp(argv[2], "-")) {
	    chdr = stdout;
	    hdrname = "stdout";
	} else {
	    hdrname = argv[2];
	}
    } else if (ctbl == stdin) {
	chdr = stdout;
	hdrname = "stdout";
    } else if ((outname = (char *) malloc(strlen(tblname) + 3)) != 0) {
	strcpy(outname, tblname);
	hdrname = outname;
	if ((p = strrchr(outname, '.')) == 0)
	    p = outname + strlen(outname);
	strcpy(p, ".h");
    } else {
	perror("malloc");
	done(EX_NOINPUT);
    }

    if (chdr == 0) {
	chdr = fopen(hdrname, "w");
	if (!chdr) {
	    perror(hdrname);
	    done(EX_NOINPUT);
	}
    }

    /*
     *  For now we assume the default font is always 256 characters.
     */
    fontlen = 256;

    /*
     *  Initialize table.
     */
    for (i = 0; i < fontlen; i++) {
	unicount[i] = 0;
    }

    /*
     *  Now we comes to the tricky part.  Parse the input table.
     */
    while (fgets(buffer, sizeof(buffer), ctbl) != NULL) {
	if ((p = strchr(buffer, '\n')) != NULL) {
	    *p = '\0';
	} else {
	    fprintf(stderr,
		    "%s: Warning: line too long or incomplete.\n",
		    tblname);
	}

	/*
	 *  Syntax accepted:
	 *      <fontpos>       <unicode> <unicode> ...
	 *      <fontpos>       <unicode range> <unicode range> ...
	 *      <fontpos>       idem
	 *      <range>         idem
	 *      <range>         <unicode range>
	 *      <unicode>       :<replace>
	 *      <unicode range> :<replace>
	 *      <unicode>       "<C replace>"
	 *      <unicode range> "<C replace>"
	 *
	 *  where <range> ::= <fontpos>-<fontpos>
	 *  and <unicode> ::= U+<h><h><h><h>
	 *  and <h> ::= <hexadecimal digit>
	 *  and <replace> any string not containing '\n' or '\0'
	 *  and <C replace> any string with C backslash escapes.
	 */
	p = buffer;
	while (*p == ' ' || *p == '\t') {
	    p++;
	}
	if (!(*p) || *p == '#') {
	    /*
	     *  Skip comment or blank line.
	     */
	    continue;
	}

	switch (*p) {
	    /*
	     *  Raw Unicode?  I.e. needs some special
	     *  processing.  One digit code.
	     */
	case 'R':
	    if (p[1] == 'a' || p[1] == 'A') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "RawOrEnc", 8)) {
		    p += 8;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    RawOrEnc = strtol(p, 0, 10);
	    Raw_found = 1;
	    continue;

	    /*
	     *  Is this the default table?
	     */
	case 'D':
	    if (p[1] == 'e' || p[1] == 'E') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "Default", 7)) {
		    p += 7;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    this_isDefaultMap = (*p == '1' || TOLOWER(*p) == 'y');
	    continue;

	    /*
	     *  Is this the default table?
	     */
	case 'F':
	    if (p[1] == 'a' || p[1] == 'A') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "FallBack", 8)) {
		    p += 8;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    useDefaultMap = (*p == '1' || TOLOWER(*p) == 'y');
	    continue;

	case 'M':
	    if (p[1] == 'i' || p[1] == 'I') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "MIMEName", 8)) {
		    p += 8;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    sscanf(p, "%40s", this_MIMEcharset);
	    continue;

	    /*
	     *  Display charset name for options screen.
	     */
	case 'O':
	    if (p[1] == 'p' || p[1] == 'P') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "OptionName", 10)) {
		    p += 10;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    for (i = 0; *p && i < UC_MAXLEN_LYNXCSNAME; p++, i++) {
		this_LYNXcharset[i] = *p;
	    }
	    this_LYNXcharset[i] = '\0';
	    continue;

	    /*
	     *  Codepage number.  Three or four digit code.
	     */
	case 'C':
	    if (p[1] == 'o' || p[1] == 'O') {
		buffer[sizeof(buffer) - 1] = '\0';
		if (!strncasecomp(p, "CodePage", 8)) {
		    p += 8;
		}
	    }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    CodePage = strtol(p, 0, 10);
	    continue;
	}

	if (*p == 'U') {
	    un0 = getunicode(&p);
	    if (un0 < 0) {
		fprintf(stderr, "Bad input line: %s\n", buffer);
		done(EX_DATAERR);
		fprintf(stderr,
			"%s: Bad Unicode range corresponding to font position range 0x%x-0x%x\n",
			tblname, fp0, fp1);
		done(EX_DATAERR);
	    }
	    un1 = un0;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    if (*p == '-') {
		p++;
		while (*p == ' ' || *p == '\t') {
		    p++;
		}
		un1 = getunicode(&p);
		if (un1 < 0 || un1 < un0) {
		    fprintf(stderr,
			    "%s: Bad Unicode range U+%x-U+%x\n",
			    tblname, un0, un1);
		    fprintf(stderr, "Bad input line: %s\n", buffer);
		    done(EX_DATAERR);
		}
		while (*p == ' ' || *p == '\t') {
		    p++;
		}
	    }

	    if (*p != ':' && *p != '"') {
		fprintf(stderr, "No ':' or '\"' where expected: %s\n",
			buffer);
		continue;
	    }

	    /*
	     * Allocate a string large enough for the worst-case use in the
	     * loop using sprintf.
	     */
	    tbuf = (char *) malloc(5 * strlen(p));

	    if (!(p1 = tbuf)) {
		fprintf(stderr, "%s: Out of memory\n", tblname);
		done(EX_DATAERR);
	    }
	    if (*p == '"') {
		/*
		 *  Handle "<C replace>".
		 *  Copy chars verbatim until first '"' not \-escaped or
		 *  end of buffer.
		 */
		int escaped = 0;

		for (ch = *(++p); (ch = *p) != '\0'; p++) {
		    if (escaped) {
			escaped = 0;
		    } else if (ch == '"') {
			break;
		    } else if (ch == '\\') {
			escaped = 1;
		    }
		    *p1++ = ch;
		}
		if (escaped || ch != '"') {
		    fprintf(stderr, "Warning: String not terminated: %s\n",
			    buffer);
		    if (escaped)
			*p1++ = '\n';
		}
	    } else {
		/*
		 *  We had ':'.
		 */
		for (ch = *(++p); (ch = *p) != '\0'; p++, p1++) {
		    if (UCH(ch) < 32 || ch == '\\' || ch == '\"' ||
			UCH(ch) >= 127) {
			sprintf(p1, "\\%.3o", UCH(ch));
			p1 += 3;
		    } else {
			*p1 = ch;
		    }
		}
	    }
	    *p1 = '\0';
	    for (i = un0; i <= un1; i++) {
		addpair_str(tbuf, i);
	    }
	    continue;
	}

	/*
	 *  Input line (after skipping spaces) doesn't start with one
	 *  of the specially recognized characters, so try to interpret
	 *  it as starting with a fontpos.
	 */
	fp0 = strtol(p, &p1, 0);
	if (p1 == p) {
	    fprintf(stderr, "Bad input line: %s\n", buffer);
	    done(EX_DATAERR);
	}
	p = p1;

	while (*p == ' ' || *p == '\t') {
	    p++;
	}
	if (*p == '-') {
	    p++;
	    fp1 = strtol(p, &p1, 0);
	    if (p1 == p) {
		fprintf(stderr, "Bad input line: %s\n", buffer);
		done(EX_DATAERR);
	    }
	    p = p1;
	} else {
	    fp1 = 0;
	}

	if (fp0 < 0 || fp0 >= fontlen) {
	    fprintf(stderr,
		    "%s: Glyph number (0x%x) larger than font length\n",
		    tblname, fp0);
	    done(EX_DATAERR);
	}
	if (fp1 && (fp1 < fp0 || fp1 >= fontlen)) {
	    fprintf(stderr,
		    "%s: Bad end of range (0x%x)\n",
		    tblname, fp1);
	    done(EX_DATAERR);
	}

	if (fp1) {
	    /*
	     *  We have a range; expect the word "idem"
	     *  or a Unicode range of the same length.
	     */
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    if (!strncmp(p, "idem", 4)) {
		for (i = fp0; i <= fp1; i++) {
		    addpair(i, i);
		}
		p += 4;
	    } else {
		un0 = getunicode(&p);
		while (*p == ' ' || *p == '\t') {
		    p++;
		}
		if (*p != '-') {
		    fprintf(stderr,
			    "%s: Corresponding to a range of font positions,",
			    tblname);
		    fprintf(stderr,
			    " there should be a Unicode range.\n");
		    done(EX_DATAERR);
		}
		p++;
		un1 = getunicode(&p);
		if (un0 < 0 || un1 < 0) {
		    fprintf(stderr,
			    "%s: Bad Unicode range corresponding to font position range 0x%x-0x%x\n",
			    tblname, fp0, fp1);
		    done(EX_DATAERR);
		}
		if (un1 - un0 != fp1 - fp0) {
		    fprintf(stderr,
			    "%s: Unicode range U+%x-U+%x not of the same length",
			    tblname, un0, un1);
		    fprintf(stderr,
			    " as font position range 0x%x-0x%x\n",
			    fp0, fp1);
		    done(EX_DATAERR);
		}
		for (i = fp0; i <= fp1; i++) {
		    addpair(i, un0 - fp0 + i);
		}
	    }
	} else {
	    /*
	     *  No range; expect a list of unicode values
	     *  or unicode ranges for a single font position,
	     *  or the word "idem"
	     */
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    if (!strncmp(p, "idem", 4)) {
		addpair(fp0, fp0);
		p += 4;
	    }
	    while ((un0 = getunicode(&p)) >= 0) {
		addpair(fp0, un0);
		while (*p == ' ' || *p == '\t') {
		    p++;
		}
		if (*p == '-') {
		    p++;
		    un1 = getunicode(&p);
		    if (un1 < un0) {
			fprintf(stderr,
				"%s: Bad Unicode range 0x%x-0x%x\n",
				tblname, un0, un1);
			done(EX_DATAERR);
		    }
		    for (un0++; un0 <= un1; un0++) {
			addpair(fp0, un0);
		    }
		}
	    }
	}
	while (*p == ' ' || *p == '\t') {
	    p++;
	}
	if (*p && *p != '#') {
	    fprintf(stderr, "%s: trailing junk (%s) ignored\n", tblname, p);
	}
    }

    /*
     *  Okay, we hit EOF, now output tables.
     */
    fclose(ctbl);

    /*
     *  Compute total size of Unicode list.
     */
    nuni = 0;
    for (i = 0; i < fontlen; i++) {
	nuni += unicount[i];
    }

    if (argc > 3) {
	strncpy(this_MIMEcharset, argv[3], UC_MAXLEN_MIMECSNAME);
    } else if (this_MIMEcharset[0] == '\0') {
	strncpy(this_MIMEcharset, tblname, UC_MAXLEN_MIMECSNAME);
	if ((p = strchr(this_MIMEcharset, '.')) != 0) {
	    *p = '\0';
	}
    }
    for (p = this_MIMEcharset; *p; p++) {
	*p = TOLOWER(*p);
    }
    if (argc > 4) {
	strncpy(this_LYNXcharset, argv[4], UC_MAXLEN_LYNXCSNAME);
    } else if (this_LYNXcharset[0] == '\0') {
	strncpy(this_LYNXcharset, this_MIMEcharset, UC_MAXLEN_LYNXCSNAME);
    }

    if (this_isDefaultMap == -1) {
	this_isDefaultMap = !strncmp(this_MIMEcharset, "iso-8859-1", 10);
    }
    fprintf(stderr,
	    "makeuctb: %s: %stranslation map",
	    this_MIMEcharset, (this_isDefaultMap ? "default " : ""));
    if (this_isDefaultMap == 1) {
	*id_append = '\0';
    } else {
	for (i = 0, p = this_MIMEcharset;
	     *p && (i < UC_MAXLEN_ID_APPEND - 1);
	     p++, i++) {
	    id_append[i + 1] = isalnum(UCH(*p)) ? *p : '_';
	}
	id_append[i + 1] = '\0';
    }
    fprintf(stderr, " (%s).\n", id_append);

    for (n = 0; n < TABLESIZE(first_ifdefs); n++) {
	fprintf(chdr, first_ifdefs[n], id_append);
	fprintf(chdr, "\n");
    }

    fprintf(chdr, "\n\
/*\n\
 *  uni_hash.tbl\n\
 *\n\
 *  Do not edit this file; it was automatically generated by\n\
 *\n\
 *  %s %s\n\
 *\n\
 */\n\
\n\
static const u8 dfont_unicount%s[%d] = \n\
{\n\t", argv[0], argv[1], id_append, fontlen);

    for (i = 0; i < fontlen; i++) {
	if (i >= 128 && unicount[i] > 0 && i < lowest_eight) {
	    lowest_eight = i;
	}
	fprintf(chdr, "%3d", unicount[i]);
	if (i == (fontlen - 1)) {
	    fprintf(chdr, "\n};\n");
	} else if ((i % 8) == 7) {
	    fprintf(chdr, ",\n\t");
	} else {
	    fprintf(chdr, ", ");
	}
    }

    /*
     *  If lowest_eightbit is anything else but 999,
     *  this can't be 7-bit only.
     */
    if (lowest_eight != 999 && !RawOrEnc) {
	RawOrEnc = UCT_ENC_8BIT;
    }

    if (nuni) {
	fprintf(chdr, "\nstatic const u16 dfont_unitable%s[%d] = \n{\n\t",
		id_append, nuni);
    } else {
	fprintf(chdr,
		"\nstatic const u16 dfont_unitable%s[1] = {0}; /* dummy */\n", id_append);
    }

    fp0 = 0;
    nent = 0;
    for (i = 0; i < nuni; i++) {
	while (nent >= unicount[fp0]) {
	    fp0++;
	    nent = 0;
	}
	fprintf(chdr, "0x%04x", unitable[fp0][nent++]);
	if (i == (nuni - 1)) {
	    fprintf(chdr, "\n};\n");
	} else if ((i % 8) == 7) {
	    fprintf(chdr, ",\n\t");
	} else {
	    fprintf(chdr, ", ");
	}
    }

    if (themap_str.entry_ct) {
	fprintf(chdr, "\n\
static struct unipair_str repl_map%s[%d] = \n\
{\n\t", id_append, themap_str.entry_ct);
    } else {
	fprintf(chdr, "\n\
/* static struct unipair_str repl_map%s[]; */\n", id_append);
    }

    for (i = 0; i < themap_str.entry_ct; i++) {
	fprintf(chdr, "{0x%x,\"%s\"}",
		themap_str.entries[i].unicode,
		themap_str.entries[i].replace_str);
	if (i == (themap_str.entry_ct - 1)) {
	    fprintf(chdr, "\n};\n");
	} else if ((i % 4) == 3) {
	    fprintf(chdr, ",\n\t");
	} else {
	    fprintf(chdr, ", ");
	}
    }
    if (themap_str.entry_ct) {
	fprintf(chdr, "\n\
static const struct unimapdesc_str dfont_replacedesc%s = {%d,repl_map%s,",
		id_append, themap_str.entry_ct, id_append);
    } else {
	fprintf(chdr, "\n\
static const struct unimapdesc_str dfont_replacedesc%s = {0,NULL,", id_append);
    }
    fprintf(chdr, "%d,%d};\n",
	    this_isDefaultMap ? 1 : 0,
	    (useDefaultMap && !this_isDefaultMap) ? 1 : 0
	);

    fprintf(chdr, "#define UC_CHARSET_SETUP%s UC_Charset_Setup(\
\"%s\",\\\n\"%s\",\\\n\
dfont_unicount%s,dfont_unitable%s,%d,\\\n\
dfont_replacedesc%s,%d,%d,%d)\n",
	    id_append, this_MIMEcharset, this_LYNXcharset,
	    id_append, id_append, nuni, id_append, lowest_eight, RawOrEnc, CodePage);

    for (n = 0; n < TABLESIZE(last_ifdefs); n++) {
	fprintf(chdr, last_ifdefs[n], id_append);
	fprintf(chdr, "\n");
    }

    done(EX_OK);
    return 0;
}
