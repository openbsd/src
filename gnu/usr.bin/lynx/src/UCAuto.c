/*
**  This file contains code for changing the Linux console mode.
**  Currently some names for font files are hardwired in here.
**  You have to change this code if it needs accommodation for your
**  system (or get the required files...).
**
**  Depending on the Display Character Set switched to, and the previous
**  one as far as it is known, system("setfont ...") and/or output of
**  escape sequences to switch console mode are done.  Curses will be
**  temporarily suspended while that happens.
**
**  NOTE that the setfont calls will also affect all other virtual consoles.
**
**  Any ideas how to do this for other systems?
*/

#include <HTUtils.h>
#include <LYUtils.h>

#include <UCMap.h>
#include <UCDefs.h>
#include <UCAuto.h>
#include <LYGlobalDefs.h>
#include <LYClean.h>
#include <LYLeaks.h>
#include <LYCharSets.h>

#ifdef EXP_CHARTRANS_AUTOSWITCH

#  ifdef CAN_SWITCH_DISPLAY_CHARSET
char *charset_switch_rules;
char *charsets_directory;
int auto_other_display_charset = -1;
int codepages[2];
int real_charsets[2] = {-1, -1};	/* Non "auto-" charsets for the cps */
int switch_display_charsets;
#  endif

#  ifdef __EMX__
/* If we "just include" <os2.h>, BOOLEAN conflicts. */
#  define BOOLEAN OS2_BOOLEAN		/* This file doesn't use it, conflicts */
#  define INCL_VIO			/* I want some Vio functions.. */
#  define INCL_DOSPROCESS			/* TIB PIB. */
#  define INCL_DOSNLS			/* DosQueryCp. */
#  include <os2.h>			/* Misc stuff.. */
#  include <os2thunk.h>			/* 32 bit to 16 bit pointer conv */
#  endif

#ifdef LINUX
typedef enum {
    Is_Unset, Is_Set, Dunno, Dont_Care
} TGen_state_t;
typedef enum {
    G0, G1
} TGNstate_t;
typedef enum {
    GN_Blat1, GN_0decgraf, GN_Ucp437, GN_Kuser, GN_dunno, GN_dontCare
} TTransT_t;

static char *T_font_fn = NULL;
static char *T_umap_fn = NULL;

#define SETFONT "setfont"
#define NOOUTPUT "2>/dev/null >/dev/null"

/*
 *  call_setfont - execute "setfont" command via system()
 *  returns:	0  ok (as far as we know)
 *	       -1  error (assume font and umap are not loaded)
 *		1  error with umap (assume font loaded but umap empty)
 */
PRIVATE int call_setfont ARGS3(
	CONST char *,	font,
	CONST char *,	fnsuffix,
	CONST char *,	umap)
{
    char *T_setfont_cmd = NULL;
    int rv;

    if ((font && T_font_fn && !strcmp(font, T_font_fn))
     && (umap && T_umap_fn && !strcmp(umap, T_umap_fn))) {
	/*
	 *  No need to repeat.
	 */
	return 0;
    }
    if (font)
	StrAllocCopy(T_font_fn, font);
    if (umap)
	StrAllocCopy(T_umap_fn, umap);

    if (!*fnsuffix)
	fnsuffix = "";

    if (umap &&*umap && font && *font) {
	HTSprintf0(&T_setfont_cmd, "%s %s%s -u %s %s",
		SETFONT, font, fnsuffix,	umap,	NOOUTPUT);
    } else if (font && *font) {
	HTSprintf0(&T_setfont_cmd, "%s %s%s %s",
		SETFONT, font, fnsuffix,		NOOUTPUT);
    } else if (umap && *umap) {
	HTSprintf0(&T_setfont_cmd, "%s -u %s %s",
		SETFONT,			umap,	NOOUTPUT);
    }

    if (T_setfont_cmd) {
	CTRACE((tfp, "Executing setfont: '%s'\n", T_setfont_cmd));
	rv = LYSystem(T_setfont_cmd);
	FREE(T_setfont_cmd);
	if (rv) {
	    CTRACE((tfp, "call_setfont: system returned %d (0x%x)!\n",
		   rv, rv));
	    if ((rv == 0x4200 || rv == 0x4100) && umap && *umap)
		/*
		 * It seems setfont returns 65 or 66 to the shell if
		 * the font was loaded ok but something was wrong with
		 * the umap file. - kw
		 */
		return 1;
	    else
		return -1;
	}
    }
    return 0;
}

PRIVATE void write_esc ARGS1(
	CONST char *,	p)
{
    int fd = open("/dev/tty", O_WRONLY);

    if (fd >= 0) {
	write(fd, p, strlen(p));
	close(fd);
    }
}

PRIVATE int nonempty_file ARGS1(
	CONST char *,	p)
{
    struct stat sb;

    return (stat(p, &sb) == 0 &&
	    S_ISREG(sb.st_mode) &&
	    (sb.st_size != 0));
}

/*
 *  This is the thing that actually gets called from display_page().
 */
PUBLIC void UCChangeTerminalCodepage ARGS2(
	int,		newcs,
	LYUCcharset *,	p)
{
    static int lastcs = -1;
    static CONST char * lastname = NULL;
    static TTransT_t lastTransT = GN_dunno;
    static TGen_state_t lastUtf = Dunno;
    static TGen_state_t lastHasUmap = Dunno;

    static char *old_font = NULL;
    static char *old_umap = NULL;

    CONST char * name;
    TTransT_t TransT = GN_dunno;
    TGen_state_t Utf = Dunno;
    TGen_state_t HasUmap = Dunno;

    char *tmpbuf1 = NULL;
    char *tmpbuf2 = NULL;
    int status = 0;

    /*
     *	Restore the original character set.
     */
    if (newcs < 0 || p == 0) {
	if (old_font && *old_font &&
	    old_umap && *old_umap) {
	    int have_font = nonempty_file(old_font);
	    int have_umap = nonempty_file(old_umap);

	    if (have_font) {
		if (have_umap) {
		    HTSprintf0(&tmpbuf1, "%s %s -u %s %s",
			    SETFONT, old_font, old_umap, NOOUTPUT);
		} else {
		    HTSprintf0(&tmpbuf1, "%s %s %s",
			    SETFONT, old_font, NOOUTPUT);
		}
		CTRACE((tfp, "Executing setfont to restore: '%s'\n", tmpbuf1));
		status = LYSystem(tmpbuf1);
		FREE(tmpbuf1);
	    }
	}
	if (newcs < 0 && p == 0) {
	    if (old_font) {
		LYRemoveTemp(old_font);
		FREE(old_font);
	    }
	    if (old_umap) {
		LYRemoveTemp(old_umap);
		FREE(old_umap);
	    }
	    if (status == 0) {
		FREE(T_font_fn);
		FREE(T_umap_fn);
	    }
	}
	return;
    } else if (lastcs < 0 && old_umap == 0 && old_font == 0) {
	FILE * fp1;
	FILE * fp2 = NULL;
	if ((old_font = typecallocn(char, LY_MAXPATH)))
	    old_umap = typecallocn(char, LY_MAXPATH);
	if ((fp1 = LYOpenTemp(old_font, ".fnt", BIN_W)))
	    fp2 = LYOpenTemp(old_umap, ".uni", BIN_W);
	if (fp1 && fp2) {
	    size_t nlen;
	    char *rp;
	    HTSprintf0(&tmpbuf1, "%s -o %s -ou %s %s",
		       SETFONT, old_font, old_umap, NOOUTPUT);
	    CTRACE((tfp, "Executing setfont to save: '%s'\n", tmpbuf1));
	    LYSystem(tmpbuf1);
	    FREE(tmpbuf1);
	    LYCloseTempFP(fp1);
	    LYCloseTempFP(fp2);
	    if ((nlen = strlen(old_font)) + 1 < LY_MAXPATH &&
		 (rp = realloc(old_font, nlen + 1)))
		old_font = rp;
	    if ((nlen = strlen(old_umap)) + 1 < LY_MAXPATH &&
		 (rp = realloc(old_umap, nlen + 1)))
		old_umap = rp;
	} else {
	    if (fp1)
		LYRemoveTemp(old_font);
	    if (fp2)
		LYRemoveTemp(old_umap);
	    FREE(old_font);
	    FREE(old_umap);
	}
    }

    name = p->MIMEname;

    /*
     *	Font sizes are currently hardwired here.
     */
#define SUFF1 ".f16"
#define SUFF2 "-16.psf"
#define SUFF3 "-8x16"
#define SUFF4 "8x16"
#define SUFF5 ".cp -16"

    /*
     *	Use this for output of escape sequences.
     */
    if ((x_display != NULL) ||
	LYgetXDisplay() != NULL) {
	/*
	 *  We won't do anything in an xterm.  Better that way...
	 */
	return;
    }

    /* NOTE: `!!umap not in kbd!!' comments below means that the *.uni file
       is not found in kbd package.  Reference Debian Package: kbd-data,
       Version: 0.96a-14.  They should be located elsewhere or generated.
       Also some cpNNN fonts used below are not in the kbd-data.  - kw
       */

    if (!strncmp(name, "iso-8859-1", 10) &&
	       (!name[10] || !isdigit(UCH(name[10])))
	) {
	if ((lastHasUmap == Is_Set) && !strcmp(lastname, "cp850")) {
	    /*
	     *	cp850 already contains all latin1 characters.
	     */
	    if (lastTransT != GN_Blat1) {
		TransT = GN_Blat1;
	    }
	} else {
	    /*
	     *	"setfont lat1u-16.psf -u lat1u.uni"
	     */
	    status = call_setfont("lat1u", SUFF2, "lat1u.uni");
	    HasUmap = Is_Set;
	    if (lastTransT != GN_Blat1) {
		TransT = GN_Blat1;
	    }
	}
	Utf = Is_Unset;
    } else if (!strcmp(name, "iso-8859-2")) {
	/*
	 *  "setfont iso02.f16 -u iso02.uni"
	 */
	status = call_setfont("iso02", SUFF1, "iso02.uni");
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "iso-8859-15")) {
	/*
	 *  "setfont lat0-16.psf"
	 */
	status = call_setfont("lat0", SUFF2, NULL);
	TransT = GN_Blat1;	/* bogus! */
	HasUmap = Dunno; /* distributed lat0 files have bogus map data! */
	Utf = Is_Unset;
    } else if (!strncmp(name, "iso-8859-", 9)) {
	if (strlen(name) <= 10 || !isdigit(UCH(name[10])))
	    HTSprintf0(&tmpbuf1, "iso0%s", &name[9]);
	else
	    HTSprintf0(&tmpbuf1, "iso%s", &name[9]);
	HTSprintf0(&tmpbuf2, "%s.uni", tmpbuf1);
	/*
	 *  "setfont iso0N.f16 -u iso0N.uni"
	 */
	status = call_setfont(tmpbuf1, SUFF1, tmpbuf2);
	FREE(tmpbuf1);
	FREE(tmpbuf2);
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "koi8-r")) {
	/*
	 *  "setfont koi8-8x16"
	 */
	status = call_setfont("koi8", SUFF3, "koi8r.uni"); /* !!umap not in kbd!! */
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp437")) {
	/*
	 *  "setfont default8x16 -u cp437.uni"
	 */
	status = call_setfont("default", SUFF4, "cp437.uni");
	if (lastTransT == GN_Kuser || lastTransT == GN_Ucp437)
	    TransT = GN_dontCare;
	else
	    TransT = GN_Ucp437;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp850")) {
	/*
	 *  "setfont cp850-8x16 -u cp850.uni"
	 */
	status = call_setfont("cp850", SUFF3, "cp850.uni"); /* !!umap not in kbd!! */
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp866") ||
	       !strcmp(name, "cp852") ||
	       !strcmp(name, "cp862")) { /* MS-Kermit has these files */
	HTSprintf0(&tmpbuf2, "%s.uni", name);
	/*
	 *  "setfont cpNNN.f16"
	 */
	status = call_setfont(name, SUFF1, tmpbuf2); /* !!umap not in kbd!! */
	FREE(tmpbuf2);
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp737")) {
	/*
	 *  "setfont cp737.cp"
	 */
	status = call_setfont("737", SUFF5, "cp737.uni"); /* !!umap not in kbd!! */
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "x-transparent")) {
	Utf = Dont_Care;
    } else if (!strcmp(name, "us-ascii")) {
	Utf = Dont_Care;
    } else if (!strncmp(name, "mnem", 4)) {
	Utf = Dont_Care;
    }

    if (status == 1)
	HasUmap = Is_Unset;
    else if (status < 0) {
	if (HasUmap == Is_Set)
	    HasUmap = Dunno;
	name = "unknown-8bit";
    }

    if (TransT != lastTransT) {
	if (TransT == GN_Blat1) {
	    /*
	     *	Switch Linux console to lat1 table.
	     */
	    write_esc("\033(B");
	} else if (TransT == GN_0decgraf) {
	    write_esc("\033(0");
	} else if (TransT == GN_Ucp437) {
	     /*
	      *  Switch Linux console to 437 table?
	      */
	    write_esc("\033(U");
	} else if (TransT == GN_Kuser) {
	     /*
	      *  Switch Linux console to user table.
	      */
	    write_esc("\033(K");
	}
	if (TransT != GN_dunno && TransT != GN_dontCare) {
	    lastTransT = TransT;
	} else {
	    TransT = lastTransT;
	}
    }

    if (HasUmap != Dont_Care && HasUmap != Dunno)
	lastHasUmap = HasUmap;

    if (p->enc == UCT_ENC_UTF8) {
	if (lastUtf != Is_Set) {
	    Utf = Is_Set;
	    /*
	     *	Turn Linux console UTF8 mode ON.
	     */
	    write_esc("\033%G");
	    lastUtf = Utf;
	}
	return;
    } else if (lastUtf == Is_Set && Utf != Dont_Care) {
	Utf = Is_Unset;
	/*
	 *  Turn Linux console UTF8 mode OFF.
	 */
	write_esc("\033%@");
	lastUtf = Utf;
    }

    if (Utf != Dont_Care && Utf != Dunno)
	lastUtf = Utf;

    lastcs = newcs;
    lastname = name;
}

#else /* Not LINUX: */
/*
 *  This is the thing that actually gets called from display_page().
 */
PUBLIC void UCChangeTerminalCodepage ARGS2(
	int,		newcs,
	LYUCcharset *,	p)
{
#ifdef __EMX__
    int res = 0;

    if (newcs < 0)
	newcs = auto_display_charset;
    res = Switch_Display_Charset(newcs, SWITCH_DISPLAY_CHARSET_REALLY);
    CTRACE((tfp, "UCChangeTerminalCodepage: Switch_Display_Charset(%d) returned %d\n", newcs, res));
#else
    CTRACE((tfp, "UCChangeTerminalCodepage: Called, but not implemented!"));
#endif
}
#endif /* LINUX */

#ifdef CAN_SWITCH_DISPLAY_CHARSET

PUBLIC int Find_Best_Display_Charset ARGS1 (int, ord)
{
    CONST char *name = LYCharSet_UC[ord].MIMEname;
    char *s = charset_switch_rules, *r;
    char buf[160];
    static int lowercase;
    int n = strlen(name), source = 1;

    if (!s || !n)
	return ord;
    if (!lowercase++)
	LYLowerCase(charset_switch_rules);
    while (1) {
	while (*s && strchr(" \t,", *s))
	    s++;			/* Go to start of a name or ':' */
	if (!*s && source)
	    return ord;			/* OK to find nothing */
	if (!*s) {
	    sprintf(buf, "No destination for '%.80s' in CHARSET_SWITCH_RULES",
		    name);
	    HTInfoMsg(buf);
	    return ord;
	}
	if (*s == ':') {
	    /* Before the replacement name */
	    while (*s && strchr(" \t:", *s))
		s++;			/* Go to the replacement */
	    /* At start of the replacement name */
	    r = s;
	    while (*s && !strchr(" \t,:", *s))
		s++;			/* Skip the replacement */
	    if (source)
		continue;
	    break;
	}
	/* At start of the source name */
	if (source && !strnicmp(name, s, n) && strchr(" \t,", s[n])) {/* Found! */
	    source = 0;
	    s += n;
	    continue;			/* Look for the replacement */
	}
	while (*s && !strchr(" \t,:", *s))
	    s++;			/* Skip the other source names */
    }
    /* Here r point to the replacement, s to the end of the replacement. */
    if (s >= r + sizeof(buf)) {
	HTInfoMsg(gettext("Charset name in CHARSET_SWITCH_RULES too long"));
	return ord;
    }
    strncpy(buf, r, s-r);
    buf[s-r] = '\0';
    n = UCGetLYhndl_byMIME(buf);
    if (n < 0) {
	sprintf(buf, "Unknown charset name '%.*s' in CHARSET_SWITCH_RULES",
		s-r, r);
	HTInfoMsg(buf);
	return ord;
    }
    return n;
}

#  ifdef __EMX__
/* Switch display for the best fit for LYCharSet_UC[ord].
   If really is MAYBE, the switch is tentative only, another switch may happen
   before the actual display.

   Returns the charset we switched to.  */
PRIVATE int _Switch_Display_Charset ARGS2 (int, ord, enum switch_display_charset_t, really)
{
    CONST char *name;
    unsigned short cp;
    static int font_loaded_for = -1, old_h, old_w;
    int rc, ord1;
    UCHAR msgbuf[MAXPATHLEN + 80];

    CTRACE((tfp, "_Switch_Display_Charset(cp=%d, really=%d).\n", ord, really));
    /* Do not trust current_char_set unless REALLY, we fake it if MAYBE! */
    if (ord == current_char_set && really == SWITCH_DISPLAY_CHARSET_MAYBE)
	return ord;
    if (ord == auto_other_display_charset
	|| ord == auto_display_charset || ord == font_loaded_for) {
	if (really == SWITCH_DISPLAY_CHARSET_MAYBE)
	    return ord; /* Report success, to avoid flicker, switch later */
    } else	/* Currently supports only koi8-r to cp866 translation */
	ord = Find_Best_Display_Charset(ord);

    /* Ignore sizechange unless the font is loaded */
    if (ord != font_loaded_for && really == SWITCH_DISPLAY_CHARSET_RESIZE)
	return ord;

    if (ord == real_charsets[0] || ord == real_charsets[1]) {
	ord1 = (ord == real_charsets[1]
	       ? auto_other_display_charset : auto_display_charset);
	if (really == SWITCH_DISPLAY_CHARSET_MAYBE)
	    return ord; /* Can switch later, report success to avoid flicker */
    } else
	ord1 = ord;
    if (ord == current_char_set && really == SWITCH_DISPLAY_CHARSET_MAYBE)
	return ord;

    name = LYCharSet_UC[ord1].MIMEname;
    if (ord1 == auto_other_display_charset || ord1 == auto_display_charset) {
      retry:
	rc = VioSetCp(0,codepages[ord1 == auto_other_display_charset],0);
	if (rc == 0)
	    goto report;
      err:
	sprintf(msgbuf, "Can't change to '%s': err=%#lx=%ld", name, rc, rc);
	HTInfoMsg(msgbuf);
	return -1;
    }

    /* Not a "prepared" codepage.  Need to load the user font. */
    if (charsets_directory) {
	TIB *tib;			/* Can't load font in a windowed-VIO */
	PIB *pib;
	VIOFONTINFO f[2];
	VIOFONTINFO *font;
	UCHAR b[1<<17];
	UCHAR *buf = b;
	UCHAR fnamebuf[MAXPATHLEN];
	FILE  *file;
	APIRET rc;
	long i, j;

	/* 0 means a FS protected-mode session */
	if ( font_loaded_for == -1		/* Did not try it yet */
	     && (DosGetInfoBlocks(&tib, &pib) || pib->pib_ultype != 0) ) {
	    ord = ord1 = auto_display_charset;
	    goto retry;
	}
	/* Should not cross 64K boundaries: */
	font = f;
	if (((((ULONG)(char*)f) + sizeof(*font)) & 0xFFFF) < sizeof(*font))
	    font++;
	if (((ULONG)buf) & 0xFFFF)
	    buf += 0x10000 - (((ULONG)buf) & 0xFFFF);
	font->cb = sizeof(*font);	/* How large is this structure */
	font->type=0;			/* Not the BIOS, the loaded font. */
	font->cbData = 65535;		/* How large is my buffer? */
	font->pbData = _emx_32to16(buf); /* Wants an 16:16 pointer */

	rc = VioGetFont(font,0);	/* Retrieve data for current font */
	if (rc) {
	    sprintf(msgbuf, "Can't fetch current font info: err=%#lx=%ld", rc, rc);
	    HTInfoMsg(msgbuf);
	    ord = ord1 = auto_display_charset;
	    goto retry;
	}
	if ( ord1 == font_loaded_for
	     && old_h == font->cyCell && old_w == font->cxCell ) {
	    /* The same as the previous font */
	    if ((rc = VioSetCp(0, -1, 0)))	/* -1: User font */
		goto err;
	    goto report;
	}
	sprintf(fnamebuf, "%s/%dx%d/%s.fnt",
		charsets_directory, font->cyCell, font->cxCell, name);
	file = fopen(fnamebuf, BIN_R);
	if (!file) {
	    sprintf(msgbuf, "Can't open font file '%s'", fnamebuf);
	    HTInfoMsg(msgbuf);
	    ord = ord1 = auto_display_charset;
	    goto retry;
	}
	i = ftell(file);
	fseek(file, 0, SEEK_END);
	if (ftell(file) - i != font->cbData) {
	    fclose(file);
	    sprintf(msgbuf, "Mismatch of size of font file '%s'", fnamebuf);
	    HTAlert(msgbuf);
	    ord = ord1 = auto_display_charset;
	    goto retry;
	}
	fseek(file, i, SEEK_SET);
	fread(buf, 1, font->cbData,file);
	fclose(file);
	rc = VioSetFont(font,0);	/* Put it all back.. */
	if (rc) {
	    sprintf(msgbuf, "Can't set font: err=%#lx=%ld", rc, rc);
	    HTInfoMsg(msgbuf);
	    ord = ord1 = auto_display_charset;
	    font_loaded_for = -1;
	    goto retry;
	}
	font_loaded_for = ord1;
	old_h = font->cyCell;
	old_w = font->cxCell;
    } else {
	ord = ord1 = auto_display_charset;
	goto retry;
    }
  report:
    CTRACE((tfp, "Display font set to '%s'.\n", name));
    return ord;
}
#  endif /* __EMX__ */

PUBLIC int Switch_Display_Charset ARGS2 (CONST int, ord, CONST enum switch_display_charset_t, really)
{
    int prev = current_char_set;
    int res;
    static int repeated;

    if (!switch_display_charsets
	&& (really == SWITCH_DISPLAY_CHARSET_MAYBE
	    /* The first switch is not due to an interactive action */
	    || (really == SWITCH_DISPLAY_CHARSET_REALLY
		&& !(repeated++))))
	return 0;
    res = _Switch_Display_Charset(ord, really);
    if (res < 0 || prev == res)		/* No change */
	return 0;
    /* Register the change */
    current_char_set = res;
    HTMLUseCharacterSet(current_char_set);
    return 1;
}
#endif /* CAN_SWITCH_DISPLAY_CHARSET */

#else /* EXP_CHARTRANS_AUTOSWITCH not defined: */
/*
 *  This is the thing that actually gets called from display_page().
 */
PUBLIC void UCChangeTerminalCodepage ARGS2(
	int,		newcs GCC_UNUSED,
	LYUCcharset *,	p GCC_UNUSED)
{
    CTRACE((tfp, "UCChangeTerminalCodepage: Called, but not implemented!"));
}
#endif /* EXP_CHARTRANS_AUTOSWITCH */
