/*
**  This file contains code for changing the Linux console mode.
**  Currently some names for font files are hardwired in here.
**  You have to change this code if it needs accomodation for your
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

#include "HTUtils.h"
#include "tcp.h"

#include "UCMap.h"
#include "UCDefs.h"
#include "UCAuto.h"
#include "LYGlobalDefs.h"
#include "LYClean.h"
#include "LYUtils.h"

#ifdef EXP_CHARTRANS_AUTOSWITCH

#ifdef VMS
#define DISPLAY "DECW$DISPLAY"
#else
#define DISPLAY "DISPLAY"
#endif /* VMS */

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

static char T_font_fn[100] = "\0";
static char T_umap_fn[100] = "\0";
static char T_setfont_cmd[200] = "\0";
#define SETFONT "setfont"
#define NOOUTPUT "2>/dev/null >/dev/null"

PRIVATE void call_setfont ARGS3(
	char *, 	font,
	char *, 	fnsuffix,
	char *, 	umap)
{
    if (font && *font && umap && *umap &&
	!strcmp(font, T_font_fn) && !strcmp(umap, T_umap_fn)) {
	/*
	 *  No need to repeat.
	 */
	return;
    }
    if (font)
	strcpy(T_font_fn, font);
    if (umap)
	strcpy(T_umap_fn, umap);

    if (!*fnsuffix)
	fnsuffix = "";

    if (umap &&*umap && font && *font) {
	sprintf(T_setfont_cmd, "%s %s%s -u %s %s",
		SETFONT, font, fnsuffix,	umap,	NOOUTPUT);
    } else if (font && *font) {
	sprintf(T_setfont_cmd, "%s %s%s %s",
		SETFONT, font, fnsuffix,		NOOUTPUT);
    } else if (umap && *umap) {
	sprintf(T_setfont_cmd, "%s -u %s %s",
		SETFONT,			umap,	NOOUTPUT);
    } else {
	*T_setfont_cmd = '\0';
    }

    if (*T_setfont_cmd) {
	if (TRACE) {
	    fprintf(stderr, "Executing setfont: '%s'\n", T_setfont_cmd);
	}
	system(T_setfont_cmd);
    }
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
	    (sb.st_mode & S_IFMT) == S_IFREG &&
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

    static char *old_font;
    static char *old_umap;

    CONST char * name;
    TTransT_t TransT = GN_dunno;
    TGen_state_t Utf = Dunno;
    TGen_state_t HasUmap = Dunno;

    char tmpbuf1[100], tmpbuf2[20];
    char *cp;

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
		    sprintf(tmpbuf1, "%s %s -u %s %s",
			    SETFONT, old_font, old_umap, NOOUTPUT);
		} else {
		    sprintf(tmpbuf1, "%s %s %s",
			    SETFONT, old_font, NOOUTPUT);
		}
		system(tmpbuf1);
	    }

	    remove(old_font);
	    free(old_font);
	    old_font = 0;

	    remove(old_umap);
	    free(old_umap);
	    old_umap = 0;
	}
	return;
    } else if (lastcs < 0 && old_umap == 0 && old_font == 0) {
	old_umap = tempnam((char *)0, "umap");
	old_font = tempnam((char *)0, "font");
	sprintf(tmpbuf1, "%s -o %s -ou %s %s",
		SETFONT, old_font, old_umap, NOOUTPUT);
	system(tmpbuf1);
    }

    name = p->MIMEname;

    /*
     *	Font sizes are currently hardwired here.
     */
#define SUFF1 ".f16"
#define SUFF2 "-16.psf"
#define SUFF3 "-8x16"
#define SUFF4 "8x16"

    /*
     *	Use this for output of escape sequences.
     */
    if ((display != NULL) ||
	((cp = getenv(DISPLAY)) != NULL && *cp != '\0')) {
	/*
	 *  We won't do anything in an xterm.  Better that way...
	 */
	return;
    }

    if (!strcmp(name, "iso-8859-10")) {
	call_setfont("iso10", SUFF1, "iso10.uni");
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strncmp(name, "iso-8859-1", 10)) {
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
	    call_setfont("lat1u", SUFF2, "lat1u.uni");
	    HasUmap = Is_Set;
	    if (lastTransT != GN_Blat1) {
		TransT = GN_Blat1;
	    }
	}
	Utf = Is_Unset;
    } else if (!strcmp(name, "iso-8859-2")) {
#ifdef NOTDEFINED
	/*
	 *  "setfont lat2-16.psf -u lat2.uni"
	 */
	call_setfont("lat2", SUFF2, "lat2.uni");  */
#endif /* NOTDEFINED */
	/*
	 *  "setfont iso02.f16 -u iso02.uni"
	 */
	call_setfont("iso02", SUFF1, "iso02.uni");
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strncmp(name, "iso-8859-", 9)) {
	sprintf(tmpbuf1, "iso0%s", &name[9]);
	sprintf(tmpbuf2, "iso0%s%s", &name[9],".uni");
	/*
	 *  "setfont iso0N.f16 -u iso0N.uni"
	 */
	call_setfont(tmpbuf1, SUFF1, tmpbuf2);
	TransT = GN_Kuser;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "koi8-r")) {
	/*
	 *  "setfont koi8-8x16"
	 */
	call_setfont("koi8", SUFF3, NULL);
	TransT = GN_Kuser;
	HasUmap = Is_Unset;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp437")) {
	/*
	 *  "setfont default8x16 -u cp437.uni"
	 */
	call_setfont("default", SUFF4, "cp437.uni");
	if (TransT == GN_Kuser || TransT == GN_Ucp437)
	    TransT = GN_dontCare;
	else
	    TransT = GN_Ucp437;
	HasUmap = Is_Set;
	Utf = Is_Unset;
    } else if (!strcmp(name, "cp850")) {
	/*
	 *  "setfont cp850-8x16 -u cp850.uni"
	 */
	call_setfont("cp850", SUFF3, "cp850.uni");
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
    if (TRACE) {
	fprintf(stderr,
		"UCChangeTerminalCodepage: Called, but not implemented!");
    }
}
#endif /* LINUX */

#else /* EXP_CHARTRANS_AUTOSWITCH not defined: */
/*
 *  This is the thing that actually gets called from display_page().
 */
PUBLIC void UCChangeTerminalCodepage ARGS2(
	int,		newcs GCC_UNUSED,
	LYUCcharset *,	p GCC_UNUSED)
{
    if (TRACE) {
	fprintf(stderr,
		"UCChangeTerminalCodepage: Called, but not implemented!");
    }
}
#endif /* EXP_CHARTRANS_AUTOSWITCH */
