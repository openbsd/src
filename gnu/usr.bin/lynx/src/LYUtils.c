#include <HTUtils.h>
#include <HTTCP.h>
#include <HTParse.h>
#include <HTAccess.h>
#include <HTCJK.h>
#include <HTAlert.h>

#ifdef __MINGW32__
int kbhit(void);		/* FIXME: use conio.h */

#ifdef UNIX
#undef UNIX
#endif /* UNIX */
#endif /* __MINGW32__ */

#include <LYCurses.h>
#include <LYHistory.h>
#include <LYStrings.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYSignal.h>
#include <GridText.h>
#include <LYClean.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>

#include <LYMainLoop.h>
#include <LYKeymap.h>

#ifdef __DJGPP__
#include <go32.h>
#include <sys/exceptn.h>
#endif /* __DJGPP__ */

#ifndef NO_GROUPS
#include <HTFile.h>
#endif

#ifdef _WINDOWS			/* 1998/04/30 (Thu) 19:04:25 */
#define GETPID()	(getpid() & 0xffff)
#else
#define GETPID()	getpid()
#endif /* _WINDOWS */

#ifdef DJGPP_KEYHANDLER
#include <bios.h>
#endif /* DJGPP_KEYHANDLER */

#ifdef __EMX__
#  define BOOLEAN OS2_BOOLEAN	/* Conflicts, but is used */
#  undef HT_ERROR		/* Conflicts too */
#  define INCL_PM		/* I want some PM functions.. */
#  define INCL_DOSPROCESS	/* TIB PIB. */
#  include <os2.h>
#  undef BOOLEAN
#endif

#ifdef VMS
#include <descrip.h>
#include <libclidef.h>
#include <lib$routines.h>
#endif /* VMS */

#ifdef HAVE_UTMP
#include <pwd.h>
#ifdef UTMPX_FOR_UTMP
#include <utmpx.h>
#define utmp utmpx
#ifdef UTMPX_FILE
#ifdef UTMP_FILE
#undef UTMP_FILE
#endif /* UTMP_FILE */
#define UTMP_FILE UTMPX_FILE
#else
#ifdef __UTMPX_FILE
#define UTMP_FILE __UTMPX_FILE	/* at least in OS/390  S/390 -- gil -- 2100 */
#else
#define UTMP_FILE "/var/adm/utmpx"	/* Digital Unix 4.0 */
#endif
#endif /* UTMPX_FILE */
#else
#include <utmp.h>
#endif /* UTMPX_FOR_UTMP */
#endif /* HAVE_UTMP */

#ifdef NEED_PTEM_H
/* they neglected to define struct winsize in termios.h -- it's only in
 * termio.h and ptem.h (the former conflicts with other definitions).
 */
#include	<sys/stream.h>
#include	<sys/ptem.h>
#endif

#include <LYLeaks.h>

#ifdef USE_COLOR_STYLE
#include <AttrList.h>
#include <LYHash.h>
#include <LYStyle.h>
#endif

#ifdef SVR4_BSDSELECT
extern int BSDselect(int nfds, fd_set * readfds, fd_set * writefds,
		     fd_set * exceptfds, struct timeval *timeout);

#ifdef select
#undef select
#endif /* select */
#define select BSDselect
#ifdef SOCKS
#ifdef Rselect
#undef Rselect
#endif /* Rselect */
#define Rselect BSDselect
#endif /* SOCKS */
#endif /* SVR4_BSDSELECT */

#ifdef __DJGPP__
#undef select			/* defined to select_s in www_tcp.h */
#endif

#ifndef UTMP_FILE
#if defined(__FreeBSD__) || defined(__bsdi__)
#define UTMP_FILE _PATH_UTMP
#else
#define UTMP_FILE "/etc/utmp"
#endif /* __FreeBSD__ || __bsdi__ */
#endif /* !UTMP_FILE */

/*
 * experimental - make temporary filenames random to make the scheme less
 * obvious.  However, as noted by KW, there are instances (such as the
 * 'O'ption page, for which Lynx will store a temporary filename even when
 * it no longer applies, since it will reuse that filename at a later time.
 */
#ifdef EXP_RAND_TEMPNAME
#if defined(LYNX_RAND_MAX)
#define USE_RAND_TEMPNAME 1
#define MAX_TEMPNAME 10000
#ifndef BITS_PER_CHAR
#define BITS_PER_CHAR 8
#endif
#endif
#endif

#define COPY_COMMAND "%s %s %s"

static HTList *localhost_aliases = NULL;	/* Hosts to treat as local */
static char *HomeDir = NULL;	/* HOME directory */

HTList *sug_filenames = NULL;	/* Suggested filenames   */

/*
 * Maintain a list of all of the temp-files we create so that we can remove
 * them during the cleanup.
 */
typedef struct _LYTemp {
    struct _LYTemp *next;
    char *name;
    BOOLEAN outs;
    FILE *file;
} LY_TEMP;

static LY_TEMP *ly_temp;

static LY_TEMP *FindTempfileByName(const char *name)
{
    LY_TEMP *p;

    for (p = ly_temp; p != 0; p = p->next) {
	if (!strcmp(p->name, name)) {
	    break;
	}
    }
    return p;
}

static LY_TEMP *FindTempfileByFP(FILE *fp)
{
    LY_TEMP *p;

    for (p = ly_temp; p != 0; p = p->next) {
	if (p->file == fp) {
	    break;
	}
    }
    return p;
}

/*
 * Get an environment variable, rejecting empty strings
 */
char *LYGetEnv(const char *name)
{
    char *result = getenv(name);

    return non_empty(result) ? result : 0;
}

/*
 * ascii versions of locale sensitive functions needed because in
 * Turkish locales tolower("I") is not "i". That's fatal for case
 * sensitive operations with charset names, HTML tags etc.
 */
#ifdef EXP_ASCII_CTYPES
int ascii_tolower(int i)
{
    if (91 > i && i > 64)
	return (i + 32);
    else
	return i;
}

int ascii_toupper(int i)
{
    if (123 > i && i > 96)
	return (i - 32);
    else
	return i;
}

int ascii_isupper(int i)
{
    if (91 > i && i > 64)
	return 1;
    else
	return 0;
}
#endif /* EXP_ASCII_CTYPES */

/*
 * Check for UTF-8 data, returning the length past the first character.
 * Return zero if we found an ordinary character rather than UTF-8.
 */
size_t utf8_length(BOOL utf_flag,
		   const char *data)
{
    size_t utf_extra = 0;

    if (utf_flag && is8bits(*data)) {
	if ((*data & 0xe0) == 0xc0) {
	    utf_extra = 1;
	} else if ((*data & 0xf0) == 0xe0) {
	    utf_extra = 2;
	} else if ((*data & 0xf8) == 0xf0) {
	    utf_extra = 3;
	} else if ((*data & 0xfc) == 0xf8) {
	    utf_extra = 4;
	} else if ((*data & 0xfe) == 0xfc) {
	    utf_extra = 5;
	} else {
	    /*
	     * Garbage.
	     */
	    utf_extra = 0;
	}
	if (strlen(data + 1) < utf_extra) {
	    /*
	     * Shouldn't happen.
	     */
	    utf_extra = 0;
	}
    }
    return utf_extra;
}

/*
 * Free storage used for the link-highlighting.
 */
void LYFreeHilites(int first, int last)
{
    int i;

    for (i = first; i < last; i++) {
	LYSetHilite(i, NULL);
	FREE(links[i].lname);
    }
}

#define LXP (links[cur].lx)
#define LYP (links[cur].ly)

/*
 * Set the initial highlight information for a given link.
 */
void LYSetHilite(int cur,
		 const char *text)
{
    links[cur].list.hl_base.hl_text = (char *) text;
    links[cur].list.hl_len = (text != NULL) ? 1 : 0;
    FREE(links[cur].list.hl_info);
}

/*
 * Add highlight information for the next line of a link.
 */
void LYAddHilite(int cur,
		 char *text,
		 int x)
{
    HiliteList *list = &(links[cur].list);
    HiliteInfo *have = list->hl_info;
    unsigned need = (list->hl_len - 1);
    unsigned want = (list->hl_len += 1);

    if (have != NULL) {
	have = typeRealloc(HiliteInfo, have, want);
    } else {
	have = typeMallocn(HiliteInfo, want);
    }
    list->hl_info = have;
    have[need].hl_text = text;
    have[need].hl_x = x;
}

/*
 * Get the highlight text, counting from zero.
 */
const char *LYGetHiliteStr(int cur,
			   int count)
{
    const char *result;

    if (count >= links[cur].list.hl_len)
	result = NULL;
    else if (count > 0)
	result = links[cur].list.hl_info[count - 1].hl_text;
    else
	result = links[cur].list.hl_base.hl_text;
    return result;
}

/*
 * Get the X-ordinate at which to draw the corresponding highlight-text
 */
int LYGetHilitePos(int cur,
		   int count)
{
    int result;

    if (count >= links[cur].list.hl_len)
	result = -1;
    else if (count > 0)
	result = links[cur].list.hl_info[count - 1].hl_x;
    else
	result = LXP;
    return result;
}

#ifdef SHOW_WHEREIS_TARGETS

#define SKIP_GLYPHS(theFlag, theData, theOffset) \
	(theFlag \
	    ? LYmbcs_skip_glyphs(theData, (theOffset), theFlag) \
	    : (theData + (theOffset)))

/*
 * If we have an emphasized WHEREIS hit in the highlighted text, restore the
 * emphasis.  Note that we never emphasize the first and last characters of the
 * highlighted text when we are making the link current, so the link attributes
 * for the current link will persist at the beginning and end, providing an
 * indication to the user that it has been made current.  Also note that we use
 * HText_getFirstTargetInLine() to determine if there's a hit in the HText
 * structure line containing the link, and if so, get back a copy of the line
 * starting at that first hit (which might be before or after our link), and
 * with all IsSpecial characters stripped, so we don't need to deal with them
 * here.  -FM
 */
static BOOL show_whereis_targets(int flag,
				 int cur,
				 int count,
				 const char *target,
				 BOOL TargetEmphasisON,
				 BOOL utf_flag)
{
    const char *Data = NULL;
    const char *cp;
    char *theData = NULL;
    char buffer[MAX_LINE];
    char tmp[7];
    int HitOffset;
    int LenNeeded;
    int Offset;
    int tLen;

    tmp[0] = tmp[1] = tmp[2] = '\0';

    if (non_empty(target)
	&& (links[cur].type & WWW_LINK_TYPE)
	&& non_empty(LYGetHiliteStr(cur, count))
	&& LYP + count < display_lines
	&& HText_getFirstTargetInLine(HTMainText,
				      links[cur].anchor_line_num + count,
				      utf_flag,
				      &Offset,
				      &tLen,
				      &theData,
				      target)) {
	int itmp, written, len, y, offset;
	const char *data;
	int tlen = strlen(target);
	int hlen, hLen;
	int hLine = LYP + count;
	int hoffset = LYGetHilitePos(cur, count);
	size_t utf_extra = 0;

	/*
	 * Copy into the buffer only what will fit up to the right border of
	 * the screen.  -FM
	 */
	LYmbcsstrncpy(buffer,
		      NonNull(LYGetHiliteStr(cur, count)),
		      (sizeof(buffer) - 1),
		      (LYcolLimit - LYGetHilitePos(cur, count)),
		      utf_flag);
	hlen = strlen(buffer);
	hLen = ((HTCJK != NOCJK || utf_flag) ?
		LYmbcsstrlen(buffer, utf_flag, YES) : hlen);

	/*
	 * Break out if the first hit in the line starts after this link.  -FM
	 */
	if (Offset < (hoffset + hLen)) {
	    /*
	     * Recursively skip hits that end before this link, and break out
	     * if there is no hit beyond those.  -FM
	     */
	    Data = theData;
	    while ((Offset < hoffset) &&
		   ((Offset + tLen) <= hoffset)) {
		data = (Data + tlen);
		offset = (Offset + tLen);
		if (((cp = LYno_attr_mb_strstr(data,
					       target,
					       utf_flag, YES,
					       &HitOffset,
					       &LenNeeded)) != NULL)
		    && (offset + LenNeeded) < LYcols) {
		    Data = cp;
		    Offset = (offset + HitOffset);
		} else {
		    goto highlight_search_done;
		}
	    }
	    data = buffer;
	    offset = hoffset;

	    /*
	     * If the hit starts before the hightext, and ends in or beyond the
	     * hightext, restore the emphasis, skipping the first and last
	     * characters of the hightext if we're making the link current.
	     * -FM
	     */
	    if ((Offset < offset) &&
		((Offset + tLen) > offset)) {
		itmp = 0;
		written = 0;
		len = (tlen - (offset - Offset));

		/*
		 * Go to the start of the hightext and handle its first
		 * character.  -FM
		 */
		LYmove(hLine, offset);
		tmp[0] = data[itmp];
		utf_extra = utf8_length(utf_flag, data + itmp);
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp + 1], utf_extra);
		    itmp += utf_extra;
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current.  -FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && is8bits(tmp[0])) {
		    /*
		     * For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current.  -FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current.  -FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 1));
		    }
		    written++;
		}
		itmp++;
		/*
		 * Start emphasis after the first character if we are making
		 * the link current and this is not the last character.  -FM
		 */
		if (!TargetEmphasisON &&
		    data[itmp] != '\0') {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		}

		/*
		 * Handle the remaining characters.  -FM
		 */
		for (;
		     written < len && (tmp[0] = data[itmp]) != '\0';
		     itmp++) {
		    /*
		     * Print all the other target chars, except the last
		     * character if it is also the last character of hightext
		     * and we are making the link current.  -FM
		     */
		    utf_extra = utf8_length(utf_flag, data + itmp);
		    if (utf_extra) {
			LYstrncpy(&tmp[1], &data[itmp + 1], utf_extra);
			itmp += utf_extra;
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			tmp[1] = '\0';
			written += (utf_extra + 1);
			utf_extra = 0;
		    } else if (HTCJK != NOCJK && is8bits(tmp[0])) {
			/*
			 * For CJK strings, by Masanobu Kimura.
			 */
			tmp[1] = data[++itmp];
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			tmp[1] = '\0';
			written += 2;
		    } else {
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			written++;
		    }
		}

		/*
		 * Stop the emphasis if we haven't already, then reset the
		 * offset to our current position in the line, and if that is
		 * beyond the link, or or we are making the link current and it
		 * is the last character of the hightext, we are done.  -FM
		 */
		if (TargetEmphasisON) {
		    LYstopTargetEmphasis();
		    TargetEmphasisON = FALSE;
		}
		LYGetYX(y, offset);
		if (offset < (hoffset + (flag == ON ? (hLen - 1) : hLen))
		/*
		 * See if we have another hit that starts within the
		 * hightext.  -FM
		 */
		    && ((cp =
			 LYno_attr_mb_strstr(data = SKIP_GLYPHS(utf_flag,
								Data,
								offset - Offset),
					     target,
					     utf_flag, YES,
					     &HitOffset,
					     &LenNeeded)) != NULL)
		    && (offset + LenNeeded) < LYcols
		/*
		 * If the hit starts after the end of the hightext, or we
		 * are making the link current and the hit starts at its
		 * last character, we are done.  -FM
		 */
		    && (HitOffset + offset) <
		    (hoffset +
		     (flag == ON ? (hLen - 1) : hLen))) {
		    /*
		     * Set up the data and offset for the hit, and let the code
		     * for within hightext hits handle it.  -FM
		     */
		    Data = cp;
		    Offset = (offset + HitOffset);
		    data = buffer;
		    offset = hoffset;
		    goto highlight_hit_within_hightext;
		}
		goto highlight_search_done;
	    }

	  highlight_hit_within_hightext:
	    /*
	     * If we get to here, the hit starts within the hightext.  If we
	     * are making the link current and it's the last character in the
	     * hightext, we are done.  Otherwise, move there and start
	     * restoring the emphasis.  -FM
	     */
	    if ((Offset - offset) <= (flag == ON ? (hLen - 1) : hLen)) {
		data = SKIP_GLYPHS(utf_flag, data, Offset - offset);
		if (utf_flag) {
		    LYrefresh();
		}
		offset = Offset;
		itmp = 0;
		written = 0;
		len = tlen;

		/*
		 * Go to the start of the hit and handle its first character.
		 * -FM
		 */
		LYmove(hLine, offset);
		tmp[0] = data[itmp];
		utf_extra = utf8_length(utf_flag, data + itmp);
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp + 1], utf_extra);
		    itmp += utf_extra;
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current, or we are making it current but this is not
		     * the first or last character of the hightext.  -FM
		     */
		    if (flag != ON ||
			(offset > hoffset && data[itmp + 1] != '\0')) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && is8bits(tmp[0])) {
		    /*
		     * For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current, or we are making it current but this is not
		     * the first or last character of the hightext.  -FM
		     */
		    if (flag != ON ||
			(offset > hoffset && data[itmp + 1] != '\0')) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 2));
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     * Start emphasis immediately if we are making the link
		     * non-current, or we are making it current but this is not
		     * the first or last character of the hightext.  -FM
		     */
		    if (flag != ON ||
			(offset > hoffset && data[itmp + 1] != '\0')) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			LYaddstr(tmp);
		    } else {
			LYmove(hLine, (offset + 1));
		    }
		    written++;
		}
		itmp++;
		/*
		 * Start emphasis after the first character if we are making
		 * the link current and this is not the last character.  -FM
		 */
		if (!TargetEmphasisON &&
		    data[itmp] != '\0') {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		}

		for (;
		     written < len && (tmp[0] = data[itmp]) != '\0';
		     itmp++) {
		    /*
		     * Print all the other target chars, except the last
		     * character if it is also the last character of hightext
		     * and we are making the link current.  -FM
		     */
		    utf_extra = utf8_length(utf_flag, data + itmp);
		    if (utf_extra) {
			LYstrncpy(&tmp[1], &data[itmp + 1], utf_extra);
			itmp += utf_extra;
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			tmp[1] = '\0';
			written += (utf_extra + 1);
			utf_extra = 0;
		    } else if (HTCJK != NOCJK && is8bits(tmp[0])) {
			/*
			 * For CJK strings, by Masanobu Kimura.
			 */
			tmp[1] = data[++itmp];
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			tmp[1] = '\0';
			written += 2;
		    } else {
			/*
			 * Make sure we don't restore emphasis to the last
			 * character of hightext if we are making the link
			 * current.  -FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    LYmove(hLine, (offset + 1));
			} else {
			    LYaddstr(tmp);
			}
			written++;
		    }
		}

		/*
		 * Stop the emphasis if we haven't already, then reset the
		 * offset to our current position in the line, and if that is
		 * beyond the link, or we are making the link current and it is
		 * the last character in the hightext, we are done.  -FM
		 */
		if (TargetEmphasisON) {
		    LYstopTargetEmphasis();
		    TargetEmphasisON = FALSE;
		}
		LYGetYX(y, offset);
		if (offset < (hoffset + (flag == ON ? (hLen - 1) : hLen))
		/*
		 * See if we have another hit that starts within the
		 * hightext.  -FM
		 */
		    && ((cp =
			 LYno_attr_mb_strstr(data = SKIP_GLYPHS(utf_flag,
								Data,
								offset - Offset),
					     target,
					     utf_flag, YES,
					     &HitOffset,
					     &LenNeeded)) != NULL)
		    && (offset + LenNeeded) < LYcols
		/*
		 * If the hit starts after the end of the hightext, or we
		 * are making the link current and the hit starts at its
		 * last character, we are done.  -FM
		 */
		    && (HitOffset + offset) <
		    (hoffset + (flag == ON ? (hLen - 1) : hLen))) {
		    /*
		     * If the target extends beyond our buffer, emphasize
		     * everything in the hightext starting at this hit.
		     * Otherwise, set up the data and offsets, and loop back.
		     * -FM
		     */
		    if ((HitOffset + (offset + tLen)) >= (hoffset + hLen)) {
			offset = (HitOffset + offset);
			data = SKIP_GLYPHS(utf_flag, Data, offset - hoffset);
			if (utf_flag) {
			    LYrefresh();
			}
			LYmove(hLine, offset);
			itmp = 0;
			written = 0;
			len = strlen(data);

			/*
			 * Turn the emphasis back on.  -FM
			 */
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			for (;
			     written < len && (tmp[0] = data[itmp]) != '\0';
			     itmp++) {
			    /*
			     * Print all the other target chars, except the
			     * last character if it is also the last character
			     * of hightext and we are making the link current.
			     * -FM
			     */
			    utf_extra = utf8_length(utf_flag, data + itmp);
			    if (utf_extra) {
				LYstrncpy(&tmp[1], &data[itmp + 1], utf_extra);
				itmp += utf_extra;
				/*
				 * Make sure we don't restore emphasis to the
				 * last character of hightext if we are making
				 * the link current.  -FM
				 */
				if (flag == ON && data[(itmp + 1)] == '\0') {
				    LYstopTargetEmphasis();
				    TargetEmphasisON = FALSE;
				    LYGetYX(y, offset);
				    LYmove(hLine, (offset + 1));
				} else {
				    LYaddstr(tmp);
				}
				tmp[1] = '\0';
				written += (utf_extra + 1);
				utf_extra = 0;
			    } else if (HTCJK != NOCJK && is8bits(tmp[0])) {
				/*
				 * For CJK strings, by Masanobu Kimura.
				 */
				tmp[1] = data[++itmp];
				/*
				 * Make sure we don't restore emphasis to the
				 * last character of hightext if we are making
				 * the link current.  -FM
				 */
				if (flag == ON && data[(itmp + 1)] == '\0') {
				    LYstopTargetEmphasis();
				    TargetEmphasisON = FALSE;
				} else {
				    LYaddstr(tmp);
				}
				tmp[1] = '\0';
				written += 2;
			    } else {
				/*
				 * Make sure we don't restore emphasis to the
				 * last character of hightext if we are making
				 * the link current.  -FM
				 */
				if (flag == ON && data[(itmp + 1)] == '\0') {
				    LYstopTargetEmphasis();
				    TargetEmphasisON = FALSE;
				} else {
				    LYaddstr(tmp);
				}
				written++;
			    }
			}
			/*
			 * Turn off the emphasis if we haven't already, and
			 * then we're done.  -FM
			 */
			if (TargetEmphasisON) {
			    LYstopTargetEmphasis();
			}
		    } else {
			Data = cp;
			Offset = (offset + HitOffset);
			data = buffer;
			offset = hoffset;
			goto highlight_hit_within_hightext;
		    }
		}
	    }
	}
    }
  highlight_search_done:
    FREE(theData);
    return TargetEmphasisON;
}
#endif /* SHOW_WHEREIS_TARGETS */

#ifdef USE_COLOR_STYLE
static int find_cached_style(int cur,
			     int flag)
{
    int s = s_alink;

#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
    if (textfields_need_activation
	&& links[cur].type == WWW_FORM_LINK_TYPE
	&& F_TEXTLIKE(links[cur].l_form->type))
	s = s_curedit;
#endif

    if (flag != ON) {
	int x;

	/*
	 * This is where we try to restore the original style when a link is
	 * unhighlighted.  The purpose of cached_styles[][] is to save the
	 * original style just for this case.  If it doesn't have a color
	 * change saved at just the right position, we look at preceding
	 * positions in the same line until we find one.
	 */
	if (CACHE_VALIDATE_YX(LYP, LXP)) {
	    CTRACE2(TRACE_STYLE,
		    (tfp, "STYLE.highlight.off: cached style @(%d,%d): ",
		     LYP, LXP));
	    s = cached_styles[LYP][LXP];
	    if (s == 0) {
		for (x = LXP - 1; x >= 0; x--) {
		    if (cached_styles[LYP][x]) {
			if (cached_styles[LYP][x] > 0) {
			    s = cached_styles[LYP][x];
			    cached_styles[LYP][LXP] = s;
			}
			CTRACE((tfp, "found %u, x_offset=%d.\n",
				cached_styles[LYP][x], (int) x - LXP));
			break;
		    }
		}
		if (s == 0) {
		    CTRACE((tfp, "not found, assume <a>.\n"));
		    s = s_a;
		}
	    } else {
		CTRACE((tfp, "found %d.\n", s));
	    }
	} else {
	    CTRACE2(TRACE_STYLE,
		    (tfp, "STYLE.highlight.off: can't use cache.\n"));
	    s = s_a;
	}
    } else {
	CTRACE2(TRACE_STYLE, (tfp, "STYLE.highlight.on: @(%d,%d).\n", LYP, LXP));
    }
    return s;
}
#endif /* USE_COLOR_STYLE */

/*
 * Highlight (or unhighlight) a given link.
 */
void LYhighlight(int flag,
		 int cur,
		 const char *target)
{
    char buffer[MAX_LINE];
    int i;
    int hi_count;
    int hi_offset;
    int title_adjust = (no_title ? -TITLE_LINES : 0);
    char tmp[7];
    const char *hi_string;

#ifdef SHOW_WHEREIS_TARGETS
    BOOL TargetEmphasisON = FALSE;
    BOOL target1_drawn = NO;
#endif
    BOOL utf_flag = (BOOL) (LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8);
    BOOL hl1_drawn = NO;

#ifdef USE_COLOR_STYLE
    BOOL hl2_drawn = FALSE;	/* whether links[cur].l_hightext2 is already drawn

				   properly */
#endif
    tmp[0] = tmp[1] = tmp[2] = '\0';

    /*
     * Bugs in the history code might cause -1 to be sent for cur, which yields
     * a crash when LYstrncpy() is called with a nonsense pointer.  As far as I
     * know, such bugs have been squashed, but if they should reappear, this
     * works around them.  -FM
     */
    if (cur < 0) {
	CTRACE((tfp, "LYhighlight cur %d (bug workaround)\n", cur));
	cur = 0;
    }

    CTRACE((tfp, "LYhighlight %s %d [%d]:%s\n",
	    (flag
	     ? "on"
	     : "off"),
	    cur,
	    links[cur].anchor_number,
	    NONNULL(target)));

#if defined(TEXTFIELDS_MAY_NEED_ACTIVATION) && defined(INACTIVE_INPUT_STYLE_VH)
    if (flag == OFF)
	textinput_redrawn = FALSE;
#endif

    if (nlinks > 0) {
#ifdef USE_COLOR_STYLE
	if (flag == ON || links[cur].type == WWW_FORM_LINK_TYPE) {
	    LYmove(LYP + title_adjust, LXP);
	    LynxChangeStyle(find_cached_style(cur, flag), STACK_ON);
	}
#else
	if (links[cur].type == WWW_FORM_LINK_TYPE
	    || LYGetHiliteStr(cur, 0) == NULL) {
	    LYMoveToLink(cur, target, NULL,
			 flag, links[cur].inUnderline, utf_flag);
	    lynx_start_link_color(flag == ON, links[cur].inUnderline);
	} else {
	    LYMoveToLink(cur, target, LYGetHiliteStr(cur, 0),
			 flag, links[cur].inUnderline, utf_flag);
	    hl1_drawn = YES;
#ifdef SHOW_WHEREIS_TARGETS
	    target1_drawn = YES;
#endif
	}
#endif

	if (links[cur].type == WWW_FORM_LINK_TYPE) {
	    int len;
	    int avail_space = (LYcolLimit - LXP) + (LYcolLimit * (LYlines - LYP));
	    const char *text = LYGetHiliteStr(cur, 0);

	    if (avail_space > links[cur].l_form->size)
		avail_space = links[cur].l_form->size;
	    if (avail_space > (int) sizeof(buffer) - 1)
		avail_space = (int) sizeof(buffer) - 1;

	    LYstrncpy(buffer, NonNull(text), avail_space);
	    len = strlen(buffer);
	    while (len < avail_space) {
		buffer[len++] = '_';
	    }
	    buffer[len] = 0;
	    LYaddstr(buffer);

#ifdef USE_COLOR_STYLE
	} else if (flag == OFF) {
	    hl2_drawn = TRUE;
	    redraw_lines_of_link(cur);
	    CTRACE2(TRACE_STYLE,
		    (tfp, "STYLE.highlight.off: NOFIX branch @(%d,%d).\n",
		     LYP, LXP));
#endif
	} else if (!hl1_drawn) {
	    /*
	     * Copy into the buffer only what will fit within the width of the
	     * screen.
	     */
	    LYmbcsstrncpy(buffer,
			  NonNull(LYGetHiliteStr(cur, 0)),
			  (sizeof(buffer) - 1),
			  (LYcolLimit - LXP),
			  utf_flag);
	    LYaddstr(buffer);
	}

	/*
	 * Display a second line as well.
	 */
#ifdef USE_COLOR_STYLE
	if (hl2_drawn == FALSE)
#endif
	{
	    for (hi_count = 1;
		 (hi_string = LYGetHiliteStr(cur, hi_count)) != NULL
		 && LYP + hi_count <= display_lines;
		 ++hi_count) {
		int row = LYP + hi_count + title_adjust;

		hi_offset = LYGetHilitePos(cur, hi_count);
		lynx_stop_link_color(flag == ON, links[cur].inUnderline);
		LYmove(row, hi_offset);

#ifdef USE_COLOR_STYLE
		CTRACE2(TRACE_STYLE,
			(tfp, "STYLE.highlight.line2: @(%d,%d), style=%d.\n",
			 row, hi_offset,
			 flag == ON ? s_alink : s_a));
		LynxChangeStyle(flag == ON ? s_alink : s_a, ABS_ON);
#else
		lynx_start_link_color(flag == ON, links[cur].inUnderline);
#endif

		for (i = 0; (tmp[0] = hi_string[i]) != '\0'
		     && (i + hi_offset) < LYcols; i++) {
		    if (!IsSpecialAttrChar(hi_string[i])) {
			/*
			 * For CJK strings, by Masanobu Kimura.
			 */
			if (HTCJK != NOCJK && is8bits(tmp[0])) {
			    tmp[1] = hi_string[++i];
			    LYaddstr(tmp);
			    tmp[1] = '\0';
			} else {
			    LYaddstr(tmp);
			}
		    }
		}
	    }
	    lynx_stop_link_color(flag == ON, links[cur].inUnderline);
	}
#ifdef SHOW_WHEREIS_TARGETS
	for (hi_count = target1_drawn ? 1 : 0;
	     LYGetHiliteStr(cur, hi_count) != NULL;
	     hi_count++) {
	    TargetEmphasisON = show_whereis_targets(flag,
						    cur,
						    hi_count,
						    target,
						    TargetEmphasisON,
						    utf_flag);
	}

	if (!LYShowCursor)
	    /*
	     * Get cursor out of the way.
	     */
	    LYHideCursor();
	else
#endif /* SHOW_WHEREIS_TARGETS */
	    /*
	     * Never hide the cursor if there's no FANCY CURSES or SLANG.
	     */
	    LYmove(LYP + title_adjust, ((LXP > 0) ? (LXP - 1) : 0));

	if (flag)
	    LYrefresh();
    }
    return;
}

/*
 * free_and_clear will free a pointer if it is non-zero and then set it to
 * zero.
 */
void free_and_clear(char **pointer)
{
    if (*pointer) {
	FREE(*pointer);
	*pointer = 0;
    }
    return;
}

/*
 * Convert single or serial newlines to single spaces throughout a string
 * (ignore newlines if the preceding character is a space) and convert tabs to
 * single spaces.  Don't ignore any explicit tabs or spaces if the condense
 * argument is FALSE, otherwise, condense any serial spaces or tabs to one
 * space.  - FM
 */
void convert_to_spaces(char *string,
		       BOOL condense)
{
    char *s = string;
    char *ns;
    BOOL last_is_space = FALSE;

    if (!s)
	return;

    for (; (*s && !isspace(*s)); s++) ;
    ns = s;

    while (*s) {
	switch (*s) {
	case ' ':
	case '\t':
	    if (!(condense && last_is_space))
		*(ns++) = ' ';
	    last_is_space = TRUE;
	    break;

	case '\r':
	case '\n':
	    if (!last_is_space) {
		*(ns++) = ' ';
		last_is_space = TRUE;
	    }
	    break;

	default:
	    *(ns++) = *s;
	    last_is_space = FALSE;
	    break;
	}
	s++;
    }
    *ns = '\0';
    return;
}

/*
 * Strip trailing slashes from directory paths.
 */
char *strip_trailing_slash(char *dirname)
{
    int i;

    i = strlen(dirname) - 1;
    while (i >= 0 && dirname[i] == '/')
	dirname[i--] = '\0';
    return (dirname);
}

/*
 * Remove most blanks, but restore one trailing blank to make prompts nicer.
 */
static void remove_most_blanks(char *buffer)
{
    int length = strlen(buffer);
    BOOL trailing = (length != 0) && (buffer[length - 1] == ' ');

    LYReduceBlanks(buffer);
    if (trailing)
	strcat(buffer, " ");
}

/*
 * Display (or hide) the status line.
 */
BOOLEAN mustshow = FALSE;

void statusline(const char *text)
{
    char buffer[MAX_LINE];
    unsigned char *temp = NULL;
    int max_length, len, i, j;
    int at_lineno;
    unsigned char k;
    char *p;
    char text_buff[MAX_LINE];

    if (text == NULL)
	return;

    /*
     * Don't print statusline messages if dumping to stdout.
     */
    if (dump_output_immediately)
	return;

    /*
     * Don't print statusline message if turned off.
     */
    if (mustshow != TRUE) {
	if (no_statusline == TRUE) {
	    return;
	}
    }
    mustshow = FALSE;

    /* "LYNXDOWNLOAD://Method=-1/File=%s/SugFile=%s%s\">Save to disk</a>\n" */
    LYstrncpy(text_buff, text, sizeof(text_buff) - 1);
    p = strchr(text_buff, '\n');
    if (p)
	p = '\0';

    /*
     * Deal with any CJK escape sequences and Kanji if we have a CJK character
     * set selected, otherwise, strip any escapes.  Also, make sure text is not
     * longer than the statusline window.  - FM
     */
    max_length = (((LYcolLimit - 1) < (int) sizeof(buffer))
		  ? (LYcolLimit - 1)
		  : (int) sizeof(buffer) - 1);
    if ((text_buff[0] != '\0') &&
	(LYHaveCJKCharacterSet)) {
	/*
	 * Translate or filter any escape sequences.  - FM
	 */
	if ((temp = typecallocn(unsigned char, strlen(text_buff) + 1)) == NULL)
	      outofmem(__FILE__, "statusline");

	if (kanji_code == EUC) {
	    TO_EUC((const unsigned char *) text_buff, temp);
	} else if (kanji_code == SJIS) {
#ifdef KANJI_CODE_OVERRIDE
	    if (!LYRawMode || last_kcode == SJIS)
		strcpy(temp, text_buff);
	    else
		TO_SJIS((const unsigned char *) text_buff, temp);
#else
	    strcpy((char *) temp, text_buff);
#endif
	} else {
	    for (i = 0, j = 0; text_buff[i]; i++) {
		if (text_buff[i] != CH_ESC) {	/* S/390 -- gil -- 2119 */
		    temp[j++] = text_buff[i];
		}
	    }
	    temp[j] = '\0';
	}

	/*
	 * Deal with any newlines or tabs in the string.  - FM
	 */
	remove_most_blanks((char *) temp);

	/*
	 * Handle the Kanji, making sure the text is not longer than the
	 * statusline window.  - FM
	 */
	for (i = 0, j = 0, len = 0, k = '\0';
	     temp[i] != '\0' && len < max_length; i++) {
	    if (k != '\0') {
		buffer[j++] = k;
		buffer[j++] = temp[i];
		k = '\0';
		len += 2;
	    } else if ((temp[i] & 0200) != 0) {
		k = temp[i];
	    } else {
		buffer[j++] = temp[i];
		len++;
	    }
	}
	buffer[j] = '\0';
	FREE(temp);
    } else {
	/*
	 * Deal with any newlines or tabs in the string.  - FM
	 */
	remove_most_blanks(text_buff);
#ifdef WIDEC_CURSES
	len = strlen(text_buff);
	if (len >= (int) (sizeof(buffer) - 1))
	    len = (int) (sizeof(buffer) - 1);
	strncpy(buffer, text_buff, len)[len] = '\0';
	/* FIXME: a binary search might be faster */
	while (len > 0 && LYstrExtent(buffer, len, len) > max_length)
	    buffer[--len] = '\0';
#else
	/*
	 * Strip any escapes, and shorten text if necessary.  Note that we
	 * don't deal with the possibility of UTF-8 characters in the string. 
	 * This is unlikely, but if strings with such characters are used in
	 * LYMessages_en.h, a compilation symbol of HAVE_UTF8_STATUSLINES could
	 * be added there, and code added here for determining the displayed
	 * string length, as we do above for CJK.  - FM
	 */
	for (i = 0, len = 0; text_buff[i] != '\0' && len < max_length; i++) {
	    if (text_buff[i] != CH_ESC) {	/* S/390 -- gil -- 2119 */
		buffer[len++] = text_buff[i];
	    }
	}
	buffer[len] = '\0';
#endif
    }

    /*
     * Move to the desired statusline window and output the text highlighted. 
     * - FM
     */
    if (LYStatusLine >= 0) {
	if (LYStatusLine < LYlines - 1) {
	    at_lineno = LYStatusLine;
	} else {
	    at_lineno = LYlines - 1;
	}
    } else if (user_mode == NOVICE_MODE) {
	at_lineno = LYlines - 3;
    } else {
	at_lineno = LYlines - 1;
    }
    LYmove(at_lineno, 0);
    LYclrtoeol();

    if (non_empty(buffer)) {
	BOOLEAN has_CJK = FALSE;

	if (HTCJK != NOCJK) {
	    for (i = 0; buffer[i] != '\0'; i++) {
		if (buffer[i] & 0x80) {
		    has_CJK = TRUE;
		    break;
		}
	    }
	}

	if (has_CJK
#ifdef HAVE_UTF8_STATUSLINES
	    || (LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8)
#endif
	    ) {
	    LYrefresh();
	}
#ifndef USE_COLOR_STYLE
	lynx_start_status_color();
	LYaddstr(buffer);
	lynx_stop_status_color();
#else
	/* draw the status bar in the STATUS style */
	{
	    int y, x;
	    int a = ((strncmp(buffer, ALERT_FORMAT, ALERT_PREFIX_LEN)
		      || !hashStyles[s_alert].name)
		     ? s_status
		     : s_alert);

	    LynxChangeStyle(a, STACK_ON);
	    LYaddstr(buffer);
	    wbkgdset(LYwin,
		     ((lynx_has_color && LYShowColor >= SHOW_COLOR_ON)
		      ? (chtype) hashStyles[a].color
		      : A_NORMAL) | ' ');
	    LYGetYX(y, x);
	    if (y == at_lineno) {
		LYclrtoeol();
	    }
	    if (!(lynx_has_color && LYShowColor >= SHOW_COLOR_ON))
		wbkgdset(LYwin, A_NORMAL | ' ');
	    else if (s_normal != NOSTYLE)
		wbkgdset(LYwin, hashStyles[s_normal].color | ' ');
	    else
		wbkgdset(LYwin, displayStyles[DSTYLE_NORMAL].color | ' ');
	    LynxChangeStyle(a, STACK_OFF);
	}
#endif
    }
    LYrefresh();

    return;
}

static const char *novice_lines(int lineno)
{
    switch (lineno) {
    case 0:
	return NOVICE_LINE_TWO_A;
    case 1:
	return NOVICE_LINE_TWO_B;
    case 2:
	return NOVICE_LINE_TWO_C;
    default:
	return "";
    }
}

static int lineno = 0;

void toggle_novice_line(void)
{
    lineno++;
    if (*novice_lines(lineno) == '\0')
	lineno = 0;
    return;
}

void noviceline(int more_flag GCC_UNUSED)
{
    if (dump_output_immediately)
	return;

    LYmove(LYlines - 2, 0);
    LYclrtoeol();
    LYaddstr(NOVICE_LINE_ONE);

    LYmove(LYlines - 1, 0);
    LYclrtoeol();
#if defined(DIRED_SUPPORT ) && defined(OK_OVERRIDE)
    if (lynx_edit_mode && !no_dired_support)
	LYaddstr(DIRED_NOVICELINE);
    else
#endif /* DIRED_SUPPORT && OK_OVERRIDE */

    if (LYUseNoviceLineTwo)
	LYaddstr(NOVICE_LINE_TWO);
    else
	LYaddstr(novice_lines(lineno));

    LYrefresh();
    return;
}

#if defined(MISC_EXP) || defined(TTY_DEVICE) || defined(HAVE_TTYNAME)
/*
 * If the standard input is not a tty, and Lynx is really reading from the
 * standard input, attempt to reopen it, pointing to a real tty.  Normally
 * this would happen if the user pipes data to Lynx and wants to run
 * interactively after that.
 *
 * Returns:
 *     1  if successfully reopened
 *    -1  if we cannot reopen
 *     0  if we do not have to reopen
 */
int LYReopenInput(void)
{
    int result = 0;
    int fd;

    if ((fd = fileno(stdin)) == 0
	&& !isatty(fd)
	&& LYConsoleInputFD(FALSE) == fd) {
	char *term_name = NULL;
	int new_fd = -1;

#ifdef HAVE_TTYNAME
	if (isatty(fileno(stdout)) &&
	    (term_name = ttyname(fileno(stdout))) != NULL)
	    new_fd = open(term_name, O_RDONLY);

	if (new_fd == -1 &&
	    isatty(fileno(stderr)) &&
	    (term_name = ttyname(fileno(stderr))) != NULL)
	    new_fd = open(term_name, O_RDONLY);
#endif

#ifdef HAVE_CTERMID
	if (new_fd == -1 &&
	    (term_name = ctermid(NULL)) != NULL)
	    new_fd = open(term_name, O_RDONLY);
#endif

#ifdef TTY_DEVICE
	if (new_fd == -1)
	    new_fd = open(term_name = TTY_DEVICE, O_RDONLY);
#endif

	CTRACE((tfp, "LYReopenInput open(%s) returned %d.\n", term_name, new_fd));
	if (new_fd >= 0) {
	    FILE *frp;

	    close(new_fd);
	    frp = freopen(term_name, "r", stdin);
	    CTRACE((tfp,
		    "LYReopenInput freopen(%s,\"r\",stdin) returned %p, stdin is now %p with fd %d.\n",
		    term_name, frp, stdin, fileno(stdin)));
	    result = 1;
	} else {
	    result = -1;
	}
    }
    return result;
}
#endif

#if defined(NSL_FORK) || defined(MISC_EXP) || defined (TTY_DEVICE) || defined(HAVE_TTYNAME)
/*
 * Returns the file descriptor from which keyboard input is expected, or INVSOC
 * (-1) if not available.  If need_selectable is true, returns non-INVSOC fd
 * only if select() is possible - actually, currently only checks if fd is
 * connected to a tty.  - kw
 */
int LYConsoleInputFD(BOOLEAN need_selectable)
{
    int fd = INVSOC;

#ifdef USE_SLANG
    if (!LYCursesON)
	fd = fileno(stdin);
#if ((SLANG_VERSION >= 9919) && defined(REAL_UNIX_SYSTEM) && !defined(__CYGWIN__))
    /* SLang_TT_Read_FD introduced in slang 0.99.19, from its changelog: 
     * SLang_TT_Read_FD variable is now available for unix.  This is the file
     * descriptor used by SLang_getkey.  */
    else
	fd = SLang_TT_Read_FD;
#endif /* SLANG_VERSION >= 9919 */
#else /* !USE_SLANG */
    fd = fileno(stdin);
#endif /* !USE_SLANG */

    if (need_selectable && fd != INVSOC) {
	if (isatty(fd)) {
	    return fd;
	} else {
	    return INVSOC;
	}
    }
    return fd;
}
#endif /* NSL_FORK || MISC_EXP */

static int fake_zap = 0;

void LYFakeZap(BOOL set)
{
    if (set && fake_zap < 1) {
	CTRACE((tfp, "\r *** Set simulated 'Z'"));
	if (fake_zap)
	    CTRACE((tfp, ", %d pending", fake_zap));
	CTRACE((tfp, " ***\n"));
	fake_zap++;
    } else if (!set && fake_zap) {
	CTRACE((tfp, "\r *** Unset simulated 'Z'"));
	CTRACE((tfp, ", %d pending", fake_zap));
	CTRACE((tfp, " ***\n"));
	fake_zap = 0;
    }

}

static int DontCheck(void)
{
    static long last;
    long next;

    /** Curses or slang setup was not invoked **/
    if (dump_output_immediately)
	return (TRUE);

    if (LYHaveCmdScript())	/* we may be running from a script */
	return (TRUE);

#ifdef MISC_EXP
    if (LYNoZapKey)
	return (TRUE);
#endif
    /*
     * Avoid checking interrupts more than one per second, since it is a slow
     * and expensive operation - TD
     */
#ifdef HAVE_GETTIMEOFDAY
#undef timezone			/* U/Win defines a conflicting macro */
    {
	struct timeval tv;

	gettimeofday(&tv, (struct timezone *) 0);
	next = tv.tv_usec / 100000L;	/* 0.1 seconds is a compromise */
    }
#else
    next = time((time_t *) 0);
#endif
    if (next == last)
	return (TRUE);

    last = next;
    return FALSE;
}

int HTCheckForInterrupt(void)
{
    int c;
    int cmd;

    if (fake_zap > 0) {
	fake_zap--;
	CTRACE((tfp, "\r *** Got simulated 'Z' ***\n"));
	CTRACE_FLUSH(tfp);
	CTRACE_SLEEP(AlertSecs);
	return ((int) TRUE);
    }

    /** Curses or slang setup was not invoked **/
    if (DontCheck())
	return ((int) FALSE);

#ifndef VMS			/* UNIX stuff: */

#if !defined(_WINDOWS) || defined(__MINGW32__)

    /*
     * First, check if there is a character.
     */
#ifdef USE_SLANG
    /** No keystroke was entered
	Note that this isn't taking possible SOCKSification
	and the socks_flag into account, and may fail on the
	slang library's select() when SOCKSified. - FM **/
#ifdef DJGPP_KEYHANDLER
    if (0 == _bios_keybrd(_NKEYBRD_READY))
	return (FALSE);
#else
    if (0 == SLang_input_pending(0))
	return (FALSE);
#endif /* DJGPP_KEYHANDLER */

#else /* Unix curses: */
    {
	struct timeval socket_timeout;
	int ret = 0;
	fd_set readfds;

	socket_timeout.tv_sec = 0;
	socket_timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);
#ifdef SOCKS
	if (socks_flag)
	    ret = Rselect(1, &readfds, NULL, NULL, &socket_timeout);
	else
#endif /* SOCKS */
	    ret = select(1, &readfds, NULL, NULL, &socket_timeout);

	/** Suspended? **/
	if ((ret == -1) && (SOCKET_ERRNO == EINTR))
	    return ((int) FALSE);

	/** No keystroke was entered? **/
	if (!FD_ISSET(0, &readfds))
	    return ((int) FALSE);
    }
#endif /* USE_SLANG */

#endif /* !_WINDOWS */

    /*
     * Now, read the character.
     */
#if defined(USE_CURSES_NODELAY)
    nodelay(LYwin, TRUE);
    c = LYgetch();
    nodelay(LYwin, FALSE);
#elif defined(USE_SLANG) && defined(_WINDOWS)
    if (!SLang_input_pending(0))
	return ((int) FALSE);
    c = LYgetch();
#else
    c = LYgetch();
#endif

#else /* VMS: */
    extern int typeahead(void);

    /** Control-C or Control-Y and a 'N'o reply to exit query **/
    if (HadVMSInterrupt) {
	HadVMSInterrupt = FALSE;
	return ((int) TRUE);
    }

    c = typeahead();

#endif /* !VMS */

    /*
     * 'c' contains whatever character we're able to read from keyboard
     */

    /** Keyboard 'Z' or 'z', or Control-G or Control-C **/
    if (LYCharIsINTERRUPT(c))
	return ((int) TRUE);

    /* There is a subset of mainloop() actions available at this stage:  no new
     * getfile() cycle is possible until the previous finished.  Currently we
     * have scrolling in partial mode, toggling of trace log, and pasting. 
     * User search now in progress...
     */
    cmd = (LKC_TO_LAC(keymap, c));
    switch (cmd) {
    case LYK_TRACE_TOGGLE:	/*  Toggle TRACE mode. */
	handle_LYK_TRACE_TOGGLE();
	break;
#ifdef CAN_CUT_AND_PASTE
    case LYK_TO_CLIPBOARD:{	/* ^S */
	    const char *s = LYDownLoadAddress();

	    if (!s || !*s || put_clip(s))
		HTInfoMsg(gettext("Copy to clipboard failed."));
	    else
		HTInfoMsg(gettext("Download document URL put to clipboard."));
	    break;
	}
#endif /* defined CAN_CUT_AND_PASTE */
    default:
#ifdef DISP_PARTIAL
	/* OK, we got several lines from new document and want to scroll... */
	if (display_partial && (NumOfLines_partial > 2)) {
	    BOOLEAN do_refresh;
	    int res;
	    int Newline_partial = LYGetNewline();

	    switch (cmd) {
	    case LYK_WHEREIS:	/* search within the document */
	    case LYK_NEXT:	/* search for the next occurrence in the document */
	    case LYK_PREV:	/* search for the previous occurrence in the document */
		handle_LYK_WHEREIS(cmd, &do_refresh);
		if (www_search_result != -1) {
		    Newline_partial = www_search_result;
		    www_search_result = -1;	/* reset */
		}
		break;

	    case LYK_FASTBACKW_LINK:
		if (Newline_partial <= (display_lines) + 1) {
		    Newline_partial -= display_lines;
		} else if ((res =
			    HTGetLinkOrFieldStart(-1,
						  &Newline_partial, NULL,
						  -1, TRUE)) == LINK_LINE_FOUND) {
		    Newline_partial++;
		} else if (res == LINK_DO_ARROWUP) {
		    Newline_partial -= display_lines;
		}
		break;
	    case LYK_FASTFORW_LINK:
		if (HText_canScrollDown()) {
		    /* This is not an exact science... - kw */
		    if ((res =
			 HTGetLinkOrFieldStart(HText_LinksInLines(HTMainText,
								  Newline_partial,
								  display_lines)
					       - 1,
					       &Newline_partial, NULL,
					       1, TRUE)) == LINK_LINE_FOUND) {
			Newline_partial++;
		    }
		}
		break;
	    case LYK_PREV_PAGE:
		if (Newline_partial > 1)
		    Newline_partial -= display_lines;
		break;
	    case LYK_NEXT_PAGE:
		if (HText_canScrollDown())
		    Newline_partial += display_lines;
		break;
	    case LYK_UP_HALF:
		if (Newline_partial > 1)
		    Newline_partial -= (display_lines / 2);
		break;
	    case LYK_DOWN_HALF:
		if (HText_canScrollDown())
		    Newline_partial += (display_lines / 2);
		break;
	    case LYK_UP_TWO:
		if (Newline_partial > 1)
		    Newline_partial -= 2;
		break;
	    case LYK_DOWN_TWO:
		if (HText_canScrollDown())
		    Newline_partial += 2;
		break;
	    case LYK_HOME:
		if (Newline_partial > 1)
		    Newline_partial = 1;
		break;
	    case LYK_END:
		if (HText_canScrollDown())
		    Newline_partial = HText_getNumOfLines() - display_lines + 1;
		/* calculate for "current" bottom value */
		break;
	    case LYK_REFRESH:
		break;
	    default:
		/** Other or no keystrokes **/
		return ((int) FALSE);
	    }			/* end switch */
	    if (Newline_partial < 1)
		Newline_partial = 1;
	    if (LYMainLoop_pageDisplay(Newline_partial))
		NumOfLines_partial = HText_getNumOfLines();
	}
#endif /* DISP_PARTIAL */
	break;
    }				/* end switch */
    /** Other or no keystrokes **/
    return ((int) FALSE);
}

/*
 * Check if the given filename looks like it's an absolute pathname, i.e.,
 * references a directory.
 */
BOOLEAN LYisAbsPath(const char *path)
{
    BOOLEAN result = FALSE;

    if (non_empty(path)) {
#ifdef VMS
	result = TRUE;
#else
#if defined(USE_DOS_DRIVES)
	result = (BOOL) (LYIsPathSep(path[0])
			 || (LYIsDosDrive(path)
			     && LYIsPathSep(path[2])));
#else
	result = (LYIsPathSep(path[0]));
#endif /* USE_DOS_DRIVES */
#endif
    }
    return result;
}

/*
 * Check if the given filename is the root path, e.g., "/" on Unix.
 */
BOOLEAN LYisRootPath(const char *path)
{
#if defined(USE_DOS_DRIVES)
    if (strlen(path) == 3
	&& LYIsDosDrive(path)
	&& LYIsPathSep(path[2]))
	return TRUE;
#endif
    return (BOOL) ((strlen(path) == 1) && LYIsPathSep(path[0]));
}

/*
 * A file URL for a remote host is an obsolete ftp URL.
 * Return YES only if we're certain it's a local file.  - FM
 */
BOOLEAN LYisLocalFile(const char *filename)
{
    char *host = NULL;
    char *acc_method = NULL;
    char *cp;

    if (!filename)
	return NO;
    if (!(host = HTParse(filename, "", PARSE_HOST)))
	return NO;
    if (!*host) {
	FREE(host);
	return NO;
    }

    if ((cp = strchr(host, ':')) != NULL)
	*cp = '\0';

    if ((acc_method = HTParse(filename, "", PARSE_ACCESS))) {
	if (0 == strcmp("file", acc_method) &&
	    (0 == strcmp(host, "localhost") ||
	     LYSameFilename(host, HTHostName()))) {
	    FREE(host);
	    FREE(acc_method);
	    return YES;
	}
    }

    FREE(host);
    FREE(acc_method);
    return NO;
}

/*
 * Utility for checking URLs with a host field.  Return YES only if we're
 * certain it's the local host.  - FM
 */
BOOLEAN LYisLocalHost(const char *filename)
{
    char *host = NULL;
    char *cp;

    if (!filename)
	return NO;
    if (!(host = HTParse(filename, "", PARSE_HOST)))
	return NO;
    if (!*host) {
	FREE(host);
	return NO;
    }

    if ((cp = strchr(host, ':')) != NULL)
	*cp = '\0';

    if ((LYSameFilename(host, "localhost") ||
	 LYSameFilename(host, LYHostName) ||
	 LYSameFilename(host, HTHostName()))) {
	FREE(host);
	return YES;
    }

    FREE(host);
    return NO;
}

/*
 * Free an HTList that contains strings.
 */
void LYFreeStringList(HTList *list)
{
    if (list != NULL) {
	char *argument;
	HTList *cur = list;

	while (NULL != (argument = (char *) HTList_nextObject(cur))) {
	    FREE(argument);
	}
	HTList_delete(list);
    }
}

/*
 * Utility for freeing the list of local host aliases.  - FM
 */
void LYLocalhostAliases_free(void)
{
    LYFreeStringList(localhost_aliases);
    localhost_aliases = NULL;
}

/*
 * Utility for listing hosts to be treated as local aliases.  - FM
 */
void LYAddLocalhostAlias(char *alias)
{
    char *LocalAlias = NULL;

    if (!non_empty(alias))
	return;

    if (!localhost_aliases) {
	localhost_aliases = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(LYLocalhostAliases_free);
#endif
    }

    StrAllocCopy(LocalAlias, alias);
    HTList_addObject(localhost_aliases, LocalAlias);

    return;
}

/*
 * Utility for checking URLs with a host field.  Return YES only if we've
 * listed the host as a local alias.  - FM
 */
BOOLEAN LYisLocalAlias(const char *filename)
{
    char *host = NULL;
    char *alias;
    char *cp;
    HTList *cur = localhost_aliases;

    if (!cur || !filename)
	return NO;
    if (!(host = HTParse(filename, "", PARSE_HOST)))
	return NO;
    if (!(*host)) {
	FREE(host);
	return NO;
    }

    if ((cp = strchr(host, ':')) != NULL)
	*cp = '\0';

    while (NULL != (alias = (char *) HTList_nextObject(cur))) {
	if (LYSameFilename(host, alias)) {
	    FREE(host);
	    return YES;
	}
    }

    FREE(host);
    return NO;
}

/*
 *  This function checks for a URL with an unknown scheme,
 *  but for which proxying has been set up, and if so,
 *  returns PROXY_URL_TYPE. - FM
 *
 *  If a colon is present but the string segment which
 *  precedes it is not being proxied, and we can be sure
 *  that what follows the colon is not a port field,
 *  it returns UNKNOWN_URL_TYPE.  Otherwise, it returns
 *  0 (not a URL). - FM
 */
UrlTypes LYCheckForProxyURL(char *filename)
{
    char *cp = filename;
    char *cp1;
    char *cp2 = NULL;

    /*
     * Don't crash on an empty argument.
     */
    if (isEmpty(cp))
	return (NOT_A_URL_TYPE);

    /* kill beginning spaces */
    cp = LYSkipBlanks(cp);

    /*
     * Check for a colon, and if present,
     * see if we have proxying set up.
     */
    if ((cp1 = strchr((cp + 1), ':')) != NULL) {
	if ((cp2 = strchr((cp + 1), '/')) != NULL && cp2 < cp1)
	    return (NOT_A_URL_TYPE);
	*cp1 = '\0';
	cp2 = NULL;
	StrAllocCopy(cp2, cp);
	*cp1 = ':';
	StrAllocCat(cp2, "_proxy");
	if (LYGetEnv(cp2) != NULL) {
	    FREE(cp2);
	    return (PROXY_URL_TYPE);
	}
	FREE(cp2);
#if defined (USE_DOS_DRIVES)
	if (LYIsDosDrive(cp))
	    return (NOT_A_URL_TYPE);
#endif
	cp1++;
	if (!*cp) {
	    return (NOT_A_URL_TYPE);
	} else if (isdigit(UCH(*cp1))) {
	    while (*cp1 && isdigit(UCH(*cp1)))
		cp1++;
	    if (*cp1 && !LYIsHtmlSep(*cp1))
		return (UNKNOWN_URL_TYPE);
	} else {
	    return (UNKNOWN_URL_TYPE);
	}
    }

    return (NOT_A_URL_TYPE);
}

/*
 * Compare a "type:" string, replacing it by the comparison-string if it
 * matches (and return true in that case).
 */
static BOOLEAN compare_type(char *tst,
			    const char *cmp,
			    size_t len)
{
    if (!strncasecomp(tst, cmp, len)) {
	if (strncmp(tst, cmp, len)) {
	    size_t i;

	    for (i = 0; i < len; i++)
		tst[i] = cmp[i];
	}
	return TRUE;
    }
    return FALSE;
}

#define DoubleHtmlSep(s) (LYIsHtmlSep((s)[0]) && LYIsHtmlSep((s)[1]))
#define compare_two(tst,cmp,len,limit) \
	((len + 2) <= limit \
	&& DoubleHtmlSep(tst + len) \
	&& compare_type(tst, cmp, len))

/*
 *  Must recognize a URL and return the type.
 *  If recognized, based on a case-insensitive
 *  analysis of the scheme field, ensures that
 *  the scheme field has the expected case.
 *
 *  Returns 0 (not a URL) for a NULL argument,
 *  one which lacks a colon.
 *
 *  Chains to LYCheckForProxyURL() if a colon
 *  is present but the type is not recognized.
 */
UrlTypes is_url(char *filename)
{
    char *cp = filename;
    char *cp1;
    UrlTypes result = NOT_A_URL_TYPE;
    int len;
    int limit;

    /*
     * Don't crash on an empty argument.
     */
    if (isEmpty(cp))
	return (result);

    /*
     * Can't be a URL if it lacks a colon.
     */
    if (NULL == strchr(cp, ':'))
	return (result);

    /*
     * Kill beginning spaces.
     */
    cp = LYSkipBlanks(cp);

    /*
     * Can't be a URL if it starts with a slash.  So return immediately for
     * this common case, also to avoid false positives if there was a colon
     * later in the string.  Also can't be a URL if it starts with a colon.  -
     * KW
     */
    if (*cp == ':' || LYIsHtmlSep(*cp)) {
	result = NOT_A_URL_TYPE;

    } else {
	limit = strlen(cp);
	switch (*cp) {
	case 'L':
	case 'l':
	    /*
	     * Lynx internal pages ("LYNXfoo:" or "lynxfoo:") start with 'l' or
	     * 'L', other URLs aren't.
	     */
	    if (compare_type(cp, STR_LYNXEXEC, LEN_LYNXEXEC)) {
		/*
		 * Special External Lynx type to handle execution of commands
		 * or scripts which require a pause to read the screen upon
		 * completion.
		 */
		result = LYNXEXEC_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXPROG, LEN_LYNXPROG)) {
		/*
		 * Special External Lynx type to handle execution of commands,
		 * scripts or programs with do not require a pause to read
		 * screen upon completion.
		 */
		result = LYNXPROG_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXCGI, LEN_LYNXCGI)) {
		/*
		 * Special External Lynx type to handle cgi scripts.
		 */
		result = LYNXCGI_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXPRINT, LEN_LYNXPRINT)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXPRINT_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXOPTIONS, LEN_LYNXOPTIONS)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXOPTIONS_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXCFG, LEN_LYNXCFG)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXCFG_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXMESSAGES, LEN_LYNXMESSAGES)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXMESSAGES_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXCFLAGS, LEN_LYNXCFLAGS)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXCOMPILE_OPTS_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXDOWNLOAD, LEN_LYNXDOWNLOAD)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXDOWNLOAD_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXDIRED, LEN_LYNXDIRED)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXDIRED_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXHIST, LEN_LYNXHIST)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXHIST_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXKEYMAP, LEN_LYNXKEYMAP)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXKEYMAP_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXIMGMAP, LEN_LYNXIMGMAP)) {
		/*
		 * Special Internal Lynx type.
		 */
		/* force lower/uppercase of next part */
		(void) is_url(&cp[LEN_LYNXIMGMAP]);
		result = LYNXIMGMAP_URL_TYPE;

	    } else if (compare_type(cp, STR_LYNXCOOKIE, LEN_LYNXCOOKIE)) {
		/*
		 * Special Internal Lynx type.
		 */
		result = LYNXCOOKIE_URL_TYPE;
	    }
	    break;
#ifndef DISABLE_NEWS
	    /*
	     * NEWSfoo:  schemes -
	     */
	case 'N':
	case 'n':
	    if (compare_type(cp, STR_NEWS_URL, LEN_NEWS_URL)) {
		result = NEWS_URL_TYPE;

	    } else if (compare_type(cp, STR_NNTP_URL, LEN_NNTP_URL)) {
		result = NNTP_URL_TYPE;

	    } else if (compare_type(cp, "newspost:", 9)) {
		/*
		 * Special Lynx type to handle news posts.
		 */
		result = NEWSPOST_URL_TYPE;

	    } else if (compare_type(cp, "newsreply:", 10)) {
		/*
		 * Special Lynx type to handle news replies (followups).
		 */
		result = NEWSREPLY_URL_TYPE;
	    }
	    break;

	    /*
	     * SNEWSfoo:  schemes -
	     */
	case 'S':
	case 's':
	    if (compare_type(cp, STR_SNEWS_URL, LEN_SNEWS_URL)) {
		result = SNEWS_URL_TYPE;

	    } else if (compare_type(cp, "snewspost:", 10)) {
		/*
		 * Special Lynx type to handle snews posts.
		 */
		result = NEWSPOST_URL_TYPE;

	    } else if (compare_type(cp, "snewsreply:", 11)) {
		/*
		 * Special Lynx type to handle snews replies (followups).
		 */
		result = NEWSREPLY_URL_TYPE;
	    }
	    break;
#endif
	case 'M':
	case 'm':
	    if (compare_type(cp, STR_MAILTO_URL, LEN_MAILTO_URL)) {
		result = MAILTO_URL_TYPE;
	    }
	    break;

	case 'F':
	case 'f':
	    if (compare_type(cp, STR_FILE_URL, len = LEN_FILE_URL)) {
		if (LYisLocalFile(cp)) {
		    result = FILE_URL_TYPE;
		} else if (DoubleHtmlSep(cp + len)) {
		    result = FTP_URL_TYPE;
		}
	    }
#ifndef DISABLE_FTP
	    else if (compare_two(cp, STR_FTP_URL, LEN_FTP_URL, limit)) {
		result = FTP_URL_TYPE;
	    }
#endif
#ifndef DISABLE_FINGER
	    else if (compare_two(cp, STR_FINGER_URL, LEN_FINGER_URL, limit)) {
		result = FINGER_URL_TYPE;
	    }
#endif
	    break;

	case 'B':
	case 'b':
#ifndef DISABLE_BIBP
	    if (compare_type(cp, STR_BIBP_URL, LEN_BIBP_URL)) {
		result = BIBP_URL_TYPE;
	    }
#endif
	    break;

	case 'D':
	case 'd':
	    if (compare_type(cp, "data:", 5)) {
		result = DATA_URL_TYPE;
	    }
	    break;

	default:
	    if (limit >= 3
		&& ((cp1 = strchr(cp + 3, ':')) == NULL
		    || !DoubleHtmlSep(cp1 + 1))) {
		/*
		 * If it doesn't contain "://", and it's not one of the the
		 * above, it can't be a URL with a scheme we know, so check if
		 * it's an unknown scheme for which proxying has been set up.
		 * - FM
		 */
		if (cp1 != NULL
		    && (cp1 - cp) > 1	/* exclude DOS-style device:/path */
		    && LYisAbsPath(cp1 + 1)) {
		    result = NCFTP_URL_TYPE;
		}

	    } else {
		switch (*cp) {
		case 'H':
		case 'h':
		    if (compare_type(cp, STR_HTTP_URL, LEN_HTTP_URL)) {
			result = HTTP_URL_TYPE;

		    } else if (compare_type(cp, STR_HTTPS_URL, LEN_HTTPS_URL)) {
			result = HTTPS_URL_TYPE;
		    }
		    break;

#ifndef DISABLE_GOPHER
		case 'G':
		case 'g':
		    if (compare_type(cp, STR_GOPHER_URL, LEN_GOPHER_URL)) {
			if (strlen(cp) >= 11
			    && (cp1 = strchr(cp + 11, '/')) != NULL) {

			    if (TOUPPER(*(cp1 + 1)) == 'H' || *(cp1 + 1) == 'w')
				/* if this is a gopher html type */
				result = HTML_GOPHER_URL_TYPE;
			    else if (*(cp1 + 1) == 'T' || *(cp1 + 1) == '8')
				result = TELNET_GOPHER_URL_TYPE;
			    else if (*(cp1 + 1) == '7')
				result = INDEX_GOPHER_URL_TYPE;
			    else
				result = GOPHER_URL_TYPE;
			} else {
			    result = GOPHER_URL_TYPE;
			}
		    }
		    break;
#endif
		case 'W':
		case 'w':
		    if (compare_type(cp, STR_WAIS_URL, LEN_WAIS_URL)) {
			result = WAIS_URL_TYPE;
		    }
		    break;

		case 'T':
		case 't':
		    if (compare_type(cp, STR_TELNET_URL, LEN_TELNET_URL)) {
			result = TELNET_URL_TYPE;

		    } else if (compare_type(cp, STR_TN3270_URL, LEN_TN3270_URL)) {
			result = TN3270_URL_TYPE;
		    }
		    break;

		case 'R':
		case 'r':
		    if (compare_type(cp, STR_RLOGIN_URL, LEN_RLOGIN_URL)) {
			result = RLOGIN_URL_TYPE;
		    }
		    break;

		case 'C':
		case 'c':
		    if (compare_type(cp, STR_CSO_URL, LEN_CSO_URL)) {
			result = CSO_URL_TYPE;
		    }
		    break;

		case 'A':
		case 'a':
		    if (compare_type(cp, "afs:", 4)) {
			result = AFS_URL_TYPE;
		    }
		    break;

		case 'P':
		case 'p':
		    if (compare_type(cp, "prospero:", 9)) {
			result = PROSPERO_URL_TYPE;
		    }
		    break;
		}
	    }
	}
	/*
	 * Check if it is an unknown scheme for which proxying has been set up.
	 */
	if (result == NOT_A_URL_TYPE)
	    result = LYCheckForProxyURL(filename);
    }
    return result;
}

/*
 * Sometimes it is just expected that curses is on when an alert or other
 * statusline message needs to be shown and we are not just dumping
 * immediately.  Calling this will 'fix' it, but may not always be appropriate. 
 * - kw
 */
void LYFixCursesOn(const char *reason)
{
    if (dump_output_immediately || LYCursesON)
	return;
    if (reason) {
	CTRACE((tfp, "Forcing curses on to %s\n", reason));
    }
    start_curses();
}

/*
 * Most protocol modules called through HTLoad* expect that curses is on unless
 * dump_output_immediately is set, so that statusline messages can be shown. 
 * Some protocols expect the opposite, namely telnet and friends.  This
 * function should be called after the 'physical' URL for accessing addr has
 * been established.  It does the right thing to the degree that curses is
 * turned on for known problem cases.  In any normal circumstances this should
 * never apply, but proxying or rule substitution is not prevented for
 * telnet-like URLs, and this 'fix' avoids some crashes that can otherwise
 * occur.  - kw
 */
BOOLEAN LYFixCursesOnForAccess(const char *addr,
			       const char *physical)
{
    /*
     * If curses is off when maybe it shouldn't...
     */
    if (!dump_output_immediately && !LYCursesON && physical) {
	char *cp1;

	/*
	 * If requested resource wants to be accessed with curses off, and
	 * getfile() would indeed have turned curses off for it...
	 */
	if (strstr(addr, "://") != NULL &&
	    (isTELNET_URL(addr) ||
	     isRLOGIN_URL(addr) ||
	     isTN3270_URL(addr) ||
	     (!isGOPHER_URL(addr) &&
	      (cp1 = strchr(addr + 11, '/')) != NULL &&
	      (*(cp1 + 1) == 'T' || *(cp1 + 1) == '8')))) {
	    /*
	     * If actual access that will be done is ok with curses off, then
	     * do nothing special, else force curses on.  - kw
	     */
	    if (!isTELNET_URL(physical) &&
		!isRLOGIN_URL(physical) &&
		!isTN3270_URL(physical)) {
		start_curses();
		HTAlert(gettext("Unexpected access protocol for this URL scheme."));
		return TRUE;
	    }
	}
    }
    return FALSE;
}

/*
 * Determine whether we allow HEAD and related flags for a URL.  - kw
 */
BOOLEAN LYCanDoHEAD(const char *address)
{
    char *temp0 = NULL;
    int isurl;

    if (!non_empty(address))
	return FALSE;
    if (!strncmp(address, "http", 4))
	return TRUE;
    /* Make copy for is_url() since caller may not care for case changes */
    StrAllocCopy(temp0, address);
    isurl = is_url(temp0);
    if (!isurl) {
	FREE(temp0);
	return FALSE;
    }
    if (isurl == LYNXCGI_URL_TYPE) {
	FREE(temp0);
#if defined(LYNXCGI_LINKS) && !defined(VMS)
	return TRUE;
#else
	return FALSE;
#endif
    }
    /*
     * The idea of the following is to allow HEAD for news URLs that identify
     * single articles, not those that identify ranges of articles or groups or
     * a list of groups.  - kw
     */
    if (isurl == NEWS_URL_TYPE || isurl == NNTP_URL_TYPE) {
	char *temp = HTParse(address, "", PARSE_PATH);
	char *cp = strrchr(temp, '/');

	if (strchr((cp ? cp : temp), '@') != NULL) {
	    FREE(temp0);
	    FREE(temp);
	    return TRUE;
	}
	if (cp && isdigit(UCH(cp[1])) && strchr(cp, '-') == NULL) {
	    FREE(temp0);
	    FREE(temp);
	    return TRUE;
	}
	FREE(temp);
    }
#define ALLOW_PROXY_HEAD
/* If defined, also allow head requests for URLs proxied through the "http" or
 * "lynxcgi" protocols, which understand HEAD.  Only the proxy environment
 * variables are checked, not the HTRules system.  - kw
 */
#ifdef ALLOW_PROXY_HEAD
    if (isurl != FILE_URL_TYPE) {
	char *acc_method = HTParse(temp0, "", PARSE_ACCESS);

	if (non_empty(acc_method)) {
	    char *proxy;

	    StrAllocCat(acc_method, "_proxy");
	    proxy = LYGetEnv(acc_method);
	    if (proxy && (isHTTP_URL(proxy) ||
			  isLYNXCGI(proxy)) &&
		!override_proxy(temp0)) {
		FREE(temp0);
		FREE(acc_method);
		return TRUE;
	    }
	}
	FREE(acc_method);
    }
#endif /* ALLOW_PROXY_HEAD */

    FREE(temp0);
    return FALSE;
}

/*
 * Close an input file.
 */
BOOLEAN LYCloseInput(FILE *fp)
{
    if (fp != 0) {
	int err = ferror(fp);

	fclose(fp);
	if (!err) {
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * Close an output file, reporting any problems with writing to it.
 */
BOOLEAN LYCloseOutput(FILE *fp)
{
    if (fp != 0) {
	int err = ferror(fp);

	fclose(fp);
	if (!err) {
	    return TRUE;
	}
    }
    HTAlert(CANNOT_WRITE_TO_FILE);
    return FALSE;
}

/*
 * Test if we'll be able to write a file.  If not, warn the user.
 */
BOOLEAN LYCanWriteFile(const char *filename)
{
    if (LYCloseOutput(fopen(filename, "w"))) {
	remove(filename);
	return TRUE;
    } else {
	_statusline(NEW_FILENAME_PROMPT);
	return FALSE;
    }
}

/*
 * Test if we'll be able to read a file.
 */
BOOLEAN LYCanReadFile(const char *filename)
{
    FILE *fp;

    if (!isEmpty(filename)) {
	if ((fp = fopen(filename, "r")) != 0) {
	    return LYCloseInput(fp);
	}
    }
    return FALSE;
}

/*
 * Remove backslashes from any string.
 */
void remove_backslashes(char *buf)
{
    char *cp;

    for (cp = buf; *cp != '\0'; cp++) {

	if (*cp != '\\') {	/* don't print slashes */
	    *buf = *cp;
	    buf++;
	} else if (*cp == '\\' &&	/* print one slash if there */
		   *(cp + 1) == '\\') {		/* are two in a row         */
	    *buf = *cp;
	    buf++;
	}
    }
    *buf = '\0';
    return;
}

/*
 * Checks to see if the current process is attached via a terminal in the local
 * domain.
 */
BOOLEAN inlocaldomain(void)
{
#ifdef HAVE_UTMP
    int n;
    FILE *fp;
    struct utmp me;
    char *cp, *mytty = NULL;

    if ((cp = ttyname(0)))
	mytty = strrchr(cp, '/');

    if (mytty && (fp = fopen(UTMP_FILE, "r")) != NULL) {
	mytty++;
	do {
	    n = fread((char *) &me, sizeof(struct utmp), 1, fp);
	} while (n > 0 && !STREQ(me.ut_line, mytty));
	(void) LYCloseInput(fp);

	if (n > 0 &&
	    strlen(me.ut_host) > strlen(LYLocalDomain) &&
	    STREQ(LYLocalDomain,
		  me.ut_host + strlen(me.ut_host) - strlen(LYLocalDomain)))
	    return (TRUE);
#ifdef LINUX
/* Linux fix to check for local user. J.Cullen 11Jul94		*/
	if ((n > 0) && (strlen(me.ut_host) == 0))
	    return (TRUE);
#endif /* LINUX */

    } else {
	CTRACE((tfp,
		"Could not get ttyname (returned %s) or open UTMP file %s\n",
		NONNULL(cp), UTMP_FILE));
    }

    return (FALSE);
#else
    CTRACE((tfp, "LYUtils: inlocaldomain() not support.\n"));
    return (TRUE);
#endif /* HAVE_UTMP */
}

#ifdef HAVE_SIGACTION
/*
 * An extended alternative for calling signal(), sets some flags for signal
 * handler as we want them if that functionality is available.  (We don't
 * return anything from this function since the return value would currently be
 * ignored anyway.) - kw
 */
void LYExtSignal(int sig,
		 LYSigHandlerFunc_t * handler)
{
#ifdef SIGWINCH
    /* add more cases to if(condition) if required... */
    if (sig == SIGWINCH && LYNonRestartingSIGWINCH) {
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_RESTART
	if (sig != SIGWINCH)
	    act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
	sigaction(sig, &act, NULL);
    } else
#endif /* defined(SIGWINCH) */
	signal(sig, handler);
}
#endif /* HAVE_SIGACTION */

#if defined(SIGTSTP) && !defined(USE_SLANG)
#ifdef HAVE_SIGACTION
/*
 * For switching a signal's handling between SIG_DFL and something (possibly)
 * different that may have been set up by lynx code or e.g. by curses library. 
 * Uses sigaction to preserve / restore as much state as possible.
 *
 * Second arg is where to save or restore from.
 *
 * Third arg to_dfl specifies what to do:
 *	1	Save current state in where, set handling to SIG_DFL
 *	0	Restore current state to previously saved one in where
 *
 * Currently only used for SIGTSTP without SLANG, to prevent (n)curses signal
 * handler from running while lynx is waiting in system() for an interactive
 * command like an editor.  - kw
 */
static BOOLEAN LYToggleSigDfl(int sig,
			      struct sigaction *where,
			      int to_dfl)
{
    int rv = -1;
    struct sigaction oact;

    if (to_dfl == 1) {
	rv = sigaction(sig, NULL, &oact);
	if (rv == 0) {
	    if (oact.sa_handler != SIG_DFL) {
		oact.sa_handler = SIG_DFL;
		rv = sigaction(sig, &oact, where);
	    } else if (where) {
		memcpy(where, &oact, sizeof(oact));
		rv = 0;
	    }
	}
    } else {
	rv = sigaction(sig, where, NULL);
    }
    if (rv != 0) {
	CTRACE((tfp, "Error in LYToggleSigDfl: %s\n", LYStrerror(errno)));
	return FALSE;
    } else
	return TRUE;
}
#endif /* HAVE_SIGACTION */
#endif /* SIGTSTP && !USE_SLANG */

/**************
 * This bit of code catches window size change signals
 */

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

/* For systems that have both, but both can't be included, duh (or neither) */
/* FIXME: this whole chunk may be redundant */
#ifdef TERMIO_AND_CURSES
# ifdef TERMIO_AND_TERMIOS
#  include <termio.h>
# else
#  ifdef HAVE_TERMIOS_H
#   include <termios.h>
#  else
#   ifdef HAVE_TERMIO_H
#    include <termio.h>
#   endif			/* HAVE_TERMIO_H */
#  endif			/* HAVE_TERMIOS_H */
# endif				/* TERMIO_AND_TERMIOS */
#endif /* TERMIO_AND_CURSES */

void size_change(int sig GCC_UNUSED)
{
    int old_lines = LYlines;
    int old_cols = LYcols;

#ifdef USE_SLANG
#if defined(VMS) || defined(UNIX)
    SLtt_get_screen_size();
#endif /* VMS || UNIX */
    LYlines = SLtt_Screen_Rows;
    LYcols = SLtt_Screen_Cols;
#ifdef SLANG_MBCS_HACK
    PHYSICAL_SLtt_Screen_Cols = LYcols;
#ifdef SLANG_NO_LIMIT		/* define this if slang has been fixed */
    SLtt_Screen_Cols = LYcolLimit * 6;
#else
    /* Needs to be limited: fixed buffer bugs in slang can cause crash,
       see slang's SLtt_smart_puts - kw */
    SLtt_Screen_Cols = HTMIN(LYcolLimit * 6, 255);
#endif
#endif /* SLANG_MBCS_HACK */
    if (sig == 0)
	/*
	 * Called from start_curses().
	 */
	return;
#else /* Curses: */
#ifdef HAVE_SIZECHANGE
#ifdef TIOCGSIZE
    struct ttysize win;

#else
#ifdef TIOCGWINSZ
    struct winsize win;
#endif /* TIOCGWINSZ */
#endif /* TIOCGSIZE */

#ifdef TIOCGSIZE
    if (ioctl(0, TIOCGSIZE, &win) == 0) {
	if (win.ts_lines != 0) {
	    LYlines = win.ts_lines;
	}
	if (win.ts_cols != 0) {
	    LYcols = win.ts_cols;
	}
    }
#else
#ifdef TIOCGWINSZ
    if (ioctl(0, TIOCGWINSZ, &win) == 0) {
	if (win.ws_row != 0) {
	    LYlines = win.ws_row;
	}
	if (win.ws_col != 0) {
	    LYcols = win.ws_col;
	}
    }
#endif /* TIOCGWINSZ */
#endif /* TIOCGSIZE */
#endif /* HAVE_SIZECHANGE */

#ifdef __EMX__
    {
	int scrsize[2];

	_scrsize(scrsize);
	LYcols = scrsize[0];
	LYlines = scrsize[1];
    }
#endif

    if (LYlines <= 0)
	LYlines = DFT_ROWS;
    if (LYcols <= 0)
	LYcols = DFT_COLS;
#endif /* USE_SLANG */

    /*
     * Check if the screen size has actually changed.  - AJL
     */
    if (LYlines != old_lines || LYcols != old_cols) {
	recent_sizechange = TRUE;
	CTRACE((tfp, "Window size changed from (%d,%d) to (%d,%d)\n",
		old_lines, old_cols, LYlines, LYcols));
#if defined(CAN_SWITCH_DISPLAY_CHARSET) && defined(CAN_AUTODETECT_DISPLAY_CHARSET)
	/* May need to reload the font due to different char-box size */
	if (current_char_set != auto_display_charset)
	    Switch_Display_Charset(current_char_set, SWITCH_DISPLAY_CHARSET_RESIZE);
#endif
    }
#ifdef SIGWINCH
    LYExtSignal(SIGWINCH, size_change);
#endif /* SIGWINCH */

    return;
}

/*
 * Utility for freeing the list of previous suggested filenames.  - FM
 */
void HTSugFilenames_free(void)
{
    LYFreeStringList(sug_filenames);
    sug_filenames = NULL;
}

/*
 * Utility for listing suggested filenames, making any repeated filenames the
 * most current in the list.  - FM
 */
void HTAddSugFilename(char *fname)
{
    char *tmp = NULL;
    char *old;
    HTList *cur;

    if (!non_empty(fname))
	return;

    StrAllocCopy(tmp, fname);

    if (!sug_filenames) {
	sug_filenames = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(HTSugFilenames_free);
#endif
	HTList_addObject(sug_filenames, tmp);
	return;
    }

    cur = sug_filenames;
    while (NULL != (old = (char *) HTList_nextObject(cur))) {
	if (!strcmp(old, tmp)) {
	    HTList_removeObject(sug_filenames, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(sug_filenames, tmp);

    return;
}

/*
 * CHANGE_SUG_FILENAME -- Foteos Macrides 29-Dec-1993 Upgraded for use with
 * Lynx2.2 - FM 17-Jan-1994
 */
void change_sug_filename(char *fname)
{
    const char *cp2;
    char *temp = 0, *cp, *cp1, *end;

#ifdef VMS
    char *dot;
    int j, k;
#endif /* VMS */

    /*
     * Establish the current end of fname.
     */
    end = fname + strlen(fname);

    /*
     * Unescape fname.
     */
    HTUnEscape(fname);

    /*
     * Rename any temporary files.
     */
    cp2 = wwwName(lynx_temp_space);
#ifdef FNAMES_8_3
    if (LYIsHtmlSep(*cp2)) {
	HTSprintf0(&temp, "file://localhost%s%04x", cp2, GETPID());
    } else {
	HTSprintf0(&temp, "file://localhost/%s%04x", cp2, GETPID());
    }
#else
    if (LYIsHtmlSep(*cp2)) {
	HTSprintf0(&temp, "file://localhost%s%d", cp2, (int) getpid());
    } else {
	HTSprintf0(&temp, "file://localhost/%s%d", cp2, (int) getpid());
    }
#endif
    if (!strncmp(fname, temp, strlen(temp))) {
	cp = strrchr(fname, '.');
	if (strlen(cp) > (strlen(temp) - 4))
	    cp = NULL;
	StrAllocCopy(temp, NonNull(cp));
	sprintf(fname, "temp%.*s", LY_MAXPATH - 10, temp);
    }
    FREE(temp);

    if (fname[strlen(fname) - 1] == '/')
	/*
	 * Hmm...  we have a directory name.  It is annoying to see a
	 * scheme+host+path name as a suggested one, let's remove the
	 * last_slash and go ahead like we have a file name.  - LP
	 */
	fname[strlen(fname) - 1] = '\0';

    /*
     * Remove everything up the the last_slash if there is one.
     */
    if ((cp = strrchr(fname, '/')) != NULL && strlen(cp) > 1) {
	cp1 = fname;
	/*
	 * Go past the slash.
	 */
	cp++;
	for (; *cp != '\0'; cp++, cp1++) {
	    *cp1 = *cp;
	}
	*cp1 = '\0';
    }
#ifdef _WINDOWS			/* 1998/05/05 (Tue) 10:08:05 */
    if ((cp = strrchr(fname, '=')) != NULL && strlen(cp) > 1) {
	cp1 = fname;
	/*
	 * Go past the '='.
	 */
	cp++;
	for (; *cp != '\0'; cp++, cp1++) {
	    *cp1 = *cp;
	}
	*cp1 = '\0';
    }
#endif

    /*
     * Trim off date-size suffix, if present.
     */
    if ((*(end - 1) == ']') && ((cp = strrchr(fname, '[')) != NULL) &&
	(cp > fname) && *(--cp) == ' ') {
	while (*cp == ' ') {
	    *(cp--) = '\0';
	}
    }

    /*
     * Trim off VMS device and/or directory specs, if present.
     */
    if ((cp = strchr(fname, '[')) != NULL &&
	(cp1 = strrchr(cp, ']')) != NULL && strlen(cp1) > 1) {
	cp1++;
	for (cp = fname; *cp1 != '\0'; cp1++) {
	    *(cp++) = *cp1;
	}
	*cp = '\0';
    }
#ifdef VMS
    /*
     * Replace illegal or problem characters.
     */
    dot = fname + strlen(fname);
    for (cp = fname; cp < dot; cp++) {
	/*
	 * Replace with underscores.
	 */
	if (*cp == ' ' || *cp == '/' || *cp == ':' ||
	    *cp == '[' || *cp == ']' || *cp == '&') {
	    *cp = '_';
	    /*
	     * Replace with dashes.
	     */
	} else if (*cp == '!' || *cp == '?' || *cp == '\'' ||
		   *cp == ',' || *cp == ':' || *cp == '"' ||
		   *cp == '+' || *cp == '@' || *cp == '\\' ||
		   *cp == '(' || *cp == ')' || *cp == '=' ||
		   *cp == '<' || *cp == '>' || *cp == '#' ||
		   *cp == '%' || *cp == '*' || *cp == '`' ||
		   *cp == '~' || *cp == '^' || *cp == '|' ||
		   *cp < ' ' || (UCH(*cp)) > 126) {
	    *cp = '-';
	}
    }

    /*
     * Collapse any serial underscores.
     */
    cp = fname + 1;
    j = 0;
    while (cp < dot) {
	if (fname[j] == '_' && *cp == '_') {
	    cp++;
	} else {
	    fname[++j] = *cp++;
	}
    }
    fname[++j] = '\0';

    /*
     * Collapse any serial dashes.
     */
    dot = fname + (strlen(fname));
    cp = fname + 1;
    j = 0;
    while (cp < dot) {
	if (fname[j] == '-' && *cp == '-') {
	    cp++;
	} else {
	    fname[++j] = *cp++;
	}
    }
    fname[++j] = '\0';

    /*
     * Trim any trailing or leading underscores or dashes.
     */
    cp = fname + (strlen(fname)) - 1;
    while (*cp == '_' || *cp == '-') {
	*cp-- = '\0';
    }
    if (fname[0] == '_' || fname[0] == '-') {
	dot = fname + (strlen(fname));
	cp = fname;
	while ((*cp == '_' || *cp == '-') && cp < dot) {
	    cp++;
	}
	j = 0;
	while (cp < dot) {
	    fname[j++] = *cp++;
	}
	fname[j] = '\0';
    }

    /*
     * Replace all but the last period with _'s, or second to last if last is
     * followed by a terminal Z or z, or GZ or gz,
     * e.g., convert foo.tar.Z to foo.tar_Z
     * or, convert foo.tar.gz to foo.tar-gz
     */
    j = strlen(fname) - 1;
    if ((dot = strrchr(fname, '.')) != NULL) {
	if (TOUPPER(fname[j]) == 'Z') {
	    if ((fname[j - 1] == '.') &&
		(((cp = strchr(fname, '.')) != NULL) && cp < dot)) {
		*dot = '_';
		dot = strrchr(fname, '.');
	    } else if (((TOUPPER(fname[j - 1]) == 'G') &&
			fname[j - 2] == '.') &&
		       (((cp = strchr(fname, '.')) != NULL) && cp < dot)) {
		*dot = '-';
		dot = strrchr(fname, '.');
	    }
	}
	cp = fname;
	while ((cp = strchr(cp, '.')) != NULL && cp < dot) {
	    *cp = '_';
	}

	/*
	 * But if the root is > 39 characters, move the period appropriately to
	 * the left.
	 */
	while (dot - fname > 39) {
	    *dot = '\0';
	    if ((cp = strrchr(fname, '_')) != NULL) {
		*cp = '.';
		*dot = '_';
	    } else if ((cp = strrchr(fname, '-')) != NULL) {
		*cp = '.';
		*dot = '_';
	    } else if (*(dot + 1) == '\0') {
		j = strlen(fname);
		while (j > 39) {
		    fname[j] = fname[j - 1];
		    j--;
		}
		fname[j] = '.';
	    } else {
		*dot = '.';
		j = 39;
		k = 0;
		while (dot[k] != '\0') {
		    fname[j++] = dot[k++];
		}
		fname[j] = '\0';
	    }
	    dot = strrchr(fname, '.');
	}

	/*
	 * Make sure the extension is < 40 characters.
	 */
	if ((fname + strlen(fname) - dot) > 39) {
	    *(dot + 40) = '\0';
	}

	/*
	 * Trim trailing dashes or underscores.
	 */
	j = (strlen(fname) - 1);
	while (fname[j] == '_' || fname[j] == '-') {
	    fname[j--] = '\0';
	}
    } else {
	/*
	 * No period, so put one on the end, or after the 39th character,
	 * trimming trailing dashes or underscores.
	 */
	if (strlen(fname) > 39) {
	    fname[39] = '\0';
	}
	j = (strlen(fname) - 1);
	while ((fname[j] == '_') || (fname[j] == '-')) {
	    j--;
	}
	fname[++j] = '.';
	fname[++j] = '\0';
    }

#else /* Not VMS (UNIX): */

    /*
     * Replace problem characters.
     */
    for (cp = fname; *cp != '\0'; cp++) {
	switch (*cp) {
	case '\'':
	case '"':
	case '/':
	case ' ':
	    *cp = '-';
	}
    }
#endif /* VMS (UNIX) */

    /*
     * Make sure the rest of the original string in nulled.
     */
    cp = fname + strlen(fname);
    while (cp < end) {
	*cp++ = '\0';
    }

    return;
}

/*
 * Construct a temporary-filename.  Assumes result is LY_MAXPATH chars long.
 */
static int fmt_tempname(char *result,
			const char *prefix,
			const char *suffix)
{
    int code;

#ifdef USE_RAND_TEMPNAME
#define SIZE_TEMPNAME ((MAX_TEMPNAME / BITS_PER_CHAR) + 1)
    static BOOL first = TRUE;
    static int names_used = 0;
    static unsigned char used_tempname[SIZE_TEMPNAME];
    unsigned offset, mask;
#endif
    static unsigned counter;
    char leaf[LY_MAXPATH];

    if (prefix == 0)
	prefix = "";
    if (suffix == 0)
	suffix = "";
    /*
     * Prefer a random value rather than a counter.
     */
#ifdef USE_RAND_TEMPNAME
    if (first) {
#if !defined(__OpenBSD__)
	lynx_srand((unsigned)((long)time((time_t *)0) + (long)result));
#endif
	first = FALSE;
    }

    /* We don't really need all of the bits from rand().  The high-order bits
     * are the more-random portion in any case, but limiting the width of the
     * generated name is done partly to avoid problems on systems that may not
     * support long filenames.
     */
    counter = MAX_TEMPNAME;
    if (names_used < MAX_TEMPNAME) {
	counter = (unsigned) (((float) MAX_TEMPNAME * lynx_rand()) /
			      LYNX_RAND_MAX + 1);
	/*
	 * Avoid reusing a temporary name, since there are places in the code
	 * which can refer to a temporary filename even after it has been
	 * closed and removed from the filesystem.
	 */
	do {
	    counter %= MAX_TEMPNAME;
	    offset = counter / BITS_PER_CHAR;
	    mask = 1 << (counter % BITS_PER_CHAR);
	    if ((used_tempname[offset] & mask) == 0) {
		names_used++;
		used_tempname[offset] |= mask;
		break;
	    }
	} while ((used_tempname[offset] & mask) == 0);
    }
    if (names_used >= MAX_TEMPNAME)
	HTAlert(gettext("Too many tempfiles"));
#else
    counter++;
#endif

#ifdef FNAMES_8_3
    /*
     * The 'lynx_temp_space' string ends with a '/' or '\\', so we only have to
     * limit the length of the leaf.  As received (e.g., from HTCompressed),
     * the suffix may contain more than a ".htm", e.g., "-txt.gz", so we trim
     * off from the filename portion to make room.
     */
#ifdef _WINDOWS
    sprintf(leaf, "%04x%04x", counter, (unsigned) GETPID());
#else
    sprintf(leaf, "%u%u", counter, (unsigned) getpid());
#endif
    if (strlen(leaf) > 8)
	leaf[8] = 0;
    if (strlen(suffix) > 4 || *suffix != '.') {
	const char *tail = strchr(suffix, '.');

	if (tail == 0)
	    tail = suffix + strlen(suffix);
	if (8 - (tail - suffix) >= 0)
	    leaf[8 - (tail - suffix)] = 0;
    }
    strcat(leaf, suffix);
#else
    sprintf(leaf, "L%u-%uTMP%s", (unsigned) getpid(), counter, suffix);
#endif
    /*
     * Someone could have configured the temporary pathname to be too long.
     */
    if ((strlen(prefix) + strlen(leaf)) < LY_MAXPATH) {
	sprintf(result, "%s%s", prefix, leaf);
	code = TRUE;
    } else {
	sprintf(result, "%.*s", LY_MAXPATH - 1, leaf);
	code = FALSE;
    }
    CTRACE((tfp, "-> '%s'\n", result));
    return (code);
}

/*
 * Convert 4, 6, 2, 8 to left, right, down, up, etc.
 */
int number2arrows(int number)
{
    switch (number) {
    case '1':
	number = END_KEY;
	break;
    case '2':
	number = DNARROW;
	break;
    case '3':
	number = PGDOWN;
	break;
    case '4':
	number = LTARROW;
	break;
    case '5':
	number = DO_NOTHING;
	break;
    case '6':
	number = RTARROW;
	break;
    case '7':
	number = HOME;
	break;
    case '8':
	number = UPARROW;
	break;
    case '9':
	number = PGUP;
	break;
    }

    return (number);
}

/*
 * parse_restrictions takes a string of comma-separated restrictions and sets
 * the corresponding flags to restrict the facilities available.
 */
/* The first two are special:  we want to record whether "default" or "all"
 * restrictions were applied, in addition to the detailed effects of those
 * options.  - kw
 */
/* skip the special flags when processing "all" and "default": */
#define N_SPECIAL_RESTRICT_OPTIONS 2
/* *INDENT-OFF* */
static const struct {
    const char *name;
    BOOLEAN *flag;
    BOOLEAN can;
} restrictions[] = {
    { "default",	&had_restrictions_default, TRUE },
    { "all",		&had_restrictions_all,	TRUE },
    { "inside_telnet",	&no_inside_telnet,	CAN_ANONYMOUS_INSIDE_DOMAIN_TELNET },
    { "outside_telnet",	&no_outside_telnet,	CAN_ANONYMOUS_OUTSIDE_DOMAIN_TELNET },
    { "telnet_port",	&no_telnet_port,	CAN_ANONYMOUS_GOTO_TELNET_PORT },
    { "inside_ftp",	&no_inside_ftp,		CAN_ANONYMOUS_INSIDE_DOMAIN_FTP },
    { "outside_ftp",	&no_outside_ftp,	CAN_ANONYMOUS_OUTSIDE_DOMAIN_FTP },
    { "inside_rlogin",	&no_inside_rlogin,	CAN_ANONYMOUS_INSIDE_DOMAIN_RLOGIN },
    { "outside_rlogin",	&no_outside_rlogin,	CAN_ANONYMOUS_OUTSIDE_DOMAIN_RLOGIN },
    { "suspend",	&no_suspend,		FALSE },
    { "editor",		&no_editor,		FALSE },
    { "shell",		&no_shell,		FALSE },
    { "bookmark",	&no_bookmark,		FALSE },
    { "multibook",	&no_multibook,		FALSE },
    { "bookmark_exec",	&no_bookmark_exec,	FALSE },
    { "option_save",	&no_option_save,	FALSE },
    { "print",		&no_print,		CAN_ANONYMOUS_PRINT },
    { "download",	&no_download,		FALSE },
    { "disk_save",	&no_disk_save,		FALSE },
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
    { "exec",		&no_exec,		LOCAL_EXECUTION_LINKS_ALWAYS_OFF_FOR_ANONYMOUS },
#endif
    { "lynxcgi",	&no_lynxcgi,		FALSE },
    { "exec_frozen",	&exec_frozen,		FALSE },
    { "goto",		&no_goto,		CAN_ANONYMOUS_GOTO },
    { "jump",		&no_jump,		CAN_ANONYMOUS_JUMP },
    { "file_url",	&no_file_url,		FALSE },
#ifndef DISABLE_NEWS
    { "news_post",	&no_newspost,		FALSE },
    { "inside_news",	&no_inside_news,	CAN_ANONYMOUS_INSIDE_DOMAIN_READ_NEWS },
    { "outside_news",	&no_outside_news,	CAN_ANONYMOUS_OUTSIDE_DOMAIN_READ_NEWS },
#endif
    { "mail",		&no_mail,		CAN_ANONYMOUS_MAIL },
    { "dotfiles",	&no_dotfiles,		FALSE },
    { "useragent",	&no_useragent,		FALSE },
#ifdef SUPPORT_CHDIR
    { "chdir",		&no_chdir,		FALSE },
#endif
#ifdef DIRED_SUPPORT
    { "dired_support",	&no_dired_support,	FALSE },
#ifdef OK_PERMIT
    { "change_exec_perms", &no_change_exec_perms, FALSE },
#endif /* OK_PERMIT */
#endif /* DIRED_SUPPORT */
#ifdef USE_EXTERNALS
    { "externals",	&no_externals,		FALSE },
#endif
    { "lynxcfg_info",	&no_lynxcfg_info,	CAN_ANONYMOUS_VIEW_LYNXCFG_INFO },
#ifndef NO_CONFIG_INFO
    { "lynxcfg_xinfo",	&no_lynxcfg_xinfo,	CAN_ANONYMOUS_VIEW_LYNXCFG_EXTENDED_INFO },
#ifdef HAVE_CONFIG_H
    { "compileopts_info", &no_compileopts_info,	CAN_ANONYMOUS_VIEW_COMPILEOPTS_INFO },
#endif
#endif
    /* put "goto" restrictions on the end, since they are a refinement */
#ifndef DISABLE_BIBP
    { "goto_bibp",	&no_goto_bibp,		CAN_ANONYMOUS_GOTO_BIBP	},
#endif
#ifdef HAVE_CONFIG_H
#ifndef NO_CONFIG_INFO
    { "goto_configinfo", &no_goto_configinfo,	CAN_ANONYMOUS_GOTO_CONFIGINFO },
#endif
#endif
    { "goto_cso",	&no_goto_cso,		CAN_ANONYMOUS_GOTO_CSO },
    { "goto_file",	&no_goto_file,		CAN_ANONYMOUS_GOTO_FILE },
#ifndef DISABLE_FINGER
    { "goto_finger",	&no_goto_finger,	CAN_ANONYMOUS_GOTO_FINGER },
#endif
    { "goto_ftp",	&no_goto_ftp,		CAN_ANONYMOUS_GOTO_FTP },
#ifndef DISABLE_GOPHER
    { "goto_gopher",	&no_goto_gopher,	CAN_ANONYMOUS_GOTO_GOPHER },
#endif
    { "goto_http",	&no_goto_http,		CAN_ANONYMOUS_GOTO_HTTP },
    { "goto_https",	&no_goto_https,		CAN_ANONYMOUS_GOTO_HTTPS },
    { "goto_lynxcgi",	&no_goto_lynxcgi,	CAN_ANONYMOUS_GOTO_LYNXCGI },
    { "goto_lynxexec",	&no_goto_lynxexec,	CAN_ANONYMOUS_GOTO_LYNXEXEC },
    { "goto_lynxprog",	&no_goto_lynxprog,	CAN_ANONYMOUS_GOTO_LYNXPROG },
    { "goto_mailto",	&no_goto_mailto,	CAN_ANONYMOUS_GOTO_MAILTO },
#ifndef DISABLE_NEWS
    { "goto_news",	&no_goto_news,		CAN_ANONYMOUS_GOTO_NEWS },
    { "goto_nntp",	&no_goto_nntp,		CAN_ANONYMOUS_GOTO_NNTP },
#endif
    { "goto_rlogin",	&no_goto_rlogin,	CAN_ANONYMOUS_GOTO_RLOGIN },
#ifndef DISABLE_NEWS
    { "goto_snews",	&no_goto_snews,		CAN_ANONYMOUS_GOTO_SNEWS },
#endif
    { "goto_telnet",	&no_goto_telnet,	CAN_ANONYMOUS_GOTO_TELNET },
    { "goto_tn3270",	&no_goto_tn3270,	CAN_ANONYMOUS_GOTO_TN3270 },
    { "goto_wais",	&no_goto_wais,		CAN_ANONYMOUS_GOTO_WAIS },
};
/* *INDENT-ON* */

/* This will make no difference between '-' and '_'.  It does only in/equality
 * compare.  It assumes that p2 can't contain dashes, but p1 can.  This
 * function is also used (if macro OPTNAME_ALLOW_DASHES doesn't have value of
 * zero) for compare of commandline options -VH
 */
BOOL strn_dash_equ(const char *p1,
		   const char *p2,
		   int len)
{
    while (len--) {
	if (!*p2)
	    return 0;		/* canonical name is shorter */
	switch (*p1) {
	case 0:
	    return 0;
	case '-':
	case '_':
	    if (*p2 != '_')
		return 0;
	    else
		break;
	default:
	    if (*p1 != *p2)
		return 0;
	}
	++p1;
	++p2;
    }
    return 1;
}

/* Uncomment following lines to allow only exact string matching */
/* #define RESTRICT_NM_ALLOW_DASHES 0 */

#ifndef RESTRICT_NM_ALLOW_DASHES
# define RESTRICT_NM_ALLOW_DASHES 1
#endif

#if RESTRICT_NM_ALLOW_DASHES
#	define RESTRICT_NM_EQU(a,b,len) strn_dash_equ(a,b,len)
#else
#	define RESTRICT_NM_EQU(a,b,len) STRNEQ(a,b,len)
#endif

/*
 * Returns the inx'th name from the restrictions table, or null if inx is
 * out of range.
 */
const char *index_to_restriction(int inx)
{
    if (inx >= 0 && inx < (int) TABLESIZE(restrictions))
	return restrictions[inx].name;
    return NULL;
}

/*
 * Returns the value TRUE/FALSE of a given restriction, or -1 if it is not
 * one that we recognize.
 */
int find_restriction(const char *name,
		     int len)
{
    unsigned i;

    if (len < 0)
	len = strlen(name);
    for (i = 0; i < TABLESIZE(restrictions); i++) {
	if (RESTRICT_NM_EQU(name, restrictions[i].name, len)) {
	    return (*restrictions[i].flag);
	}
    }
    return -1;
}

void parse_restrictions(const char *s)
{
    const char *p;
    const char *word;
    unsigned i;
    BOOLEAN found;

    p = s;
    while (*p) {
	p = LYSkipCBlanks(p);
	if (*p == '\0')
	    break;
	word = p;
	while (*p != ',' && *p != '\0')
	    p++;

	found = FALSE;
	if (RESTRICT_NM_EQU(word, "all", p - word)) {
	    found = TRUE;
	    for (i = N_SPECIAL_RESTRICT_OPTIONS;
		 i < TABLESIZE(restrictions);
		 i++)
		*(restrictions[i].flag) = TRUE;
	} else if (RESTRICT_NM_EQU(word, "default", p - word)) {
	    found = TRUE;
	    for (i = N_SPECIAL_RESTRICT_OPTIONS;
		 i < TABLESIZE(restrictions);
		 i++)
		*(restrictions[i].flag) = !restrictions[i].can;
	} else {
	    for (i = 0; i < TABLESIZE(restrictions); i++) {
		if (RESTRICT_NM_EQU(word, restrictions[i].name, p - word)) {
		    *(restrictions[i].flag) = TRUE;
		    found = TRUE;
		    break;
		}
	    }
	}
	if (!found) {
	    printf("%s: %.*s\n", gettext("unknown restriction"),
		   (int) (p - word), word);
	    exit_immediately(EXIT_FAILURE);
	}
	if (*p)
	    p++;
    }

    /*
     * If shell is restricted, set restrictions on related topics.
     */
    if (no_shell) {
	no_goto_lynxexec = TRUE;
	no_goto_lynxprog = TRUE;
	no_goto_lynxcgi = TRUE;
#ifdef EXEC_LINKS
	local_exec_on_local_files = TRUE;
#endif
    }
}

void print_restrictions_to_fd(FILE *fp)
{
    unsigned i, count = 0;

    for (i = 0; i < TABLESIZE(restrictions); i++) {
	if (*(restrictions[i].flag) == TRUE) {
	    count++;
	}
    }
    if (!count) {
	fprintf(fp, gettext("No restrictions set.\n"));
	return;
    }
    fprintf(fp, gettext("Restrictions set:\n"));
    for (i = 0; i < TABLESIZE(restrictions); i++) {
	if (*(restrictions[i].flag) == TRUE) {
	    /* if "goto" is restricted, don't bother tell about its
	     * refinements
	     */
	    if (strncmp(restrictions[i].name, "goto_", 5)
		|| !no_goto)
		fprintf(fp, "   %s\n", restrictions[i].name);
	}
    }
}

#ifdef VMS
#include <jpidef.h>
#include <maildef.h>
#include <starlet.h>

typedef struct _VMSMailItemList {
    short buffer_length;
    short item_code;
    void *buffer_address;
    long *return_length_address;
} VMSMailItemList;

void LYCheckMail(void)
{
    static BOOL firsttime = TRUE, failure = FALSE;
    static char user[13], dir[252];
    static long userlen = 0, dirlen;
    static time_t lastcheck = 0;
    time_t now;
    static short new, lastcount;
    long ucontext = 0, status;
    short flags = MAIL$M_NEWMSG;
    /* *INDENT-OFF* */
    VMSMailItemList
      null_list[] = {{0,0,0,0}},
      jpi_list[]  = {{sizeof(user) - 1,JPI$_USERNAME,(void *)user,&userlen},
		     {0,0,0,0}},
      uilist[]	  = {{0,MAIL$_USER_USERNAME,0,0},
		     {0,0,0,0}},
      uolist[]	  = {{sizeof(new),MAIL$_USER_NEW_MESSAGES,&new,0},
		     {sizeof(dir),MAIL$_USER_FULL_DIRECTORY,dir,&dirlen},
		     {0,0,0,0}};
    /* *INDENT-ON* */

    extern long mail$user_begin();
    extern long mail$user_get_info();
    extern long mail$user_end();

    if (failure)
	return;

    if (firsttime) {
	firsttime = FALSE;
	/* Get the username. */
	status = sys$getjpiw(0, 0, 0, jpi_list, 0, 0, 0);
	if (!(status & 1)) {
	    failure = TRUE;
	    return;
	}
	user[userlen] = '\0';
	LYTrimTrailing(user);
    }

    /* Minimum report interval is 60 sec. */
    time(&now);
    if (now - lastcheck < 60)
	return;
    lastcheck = now;

    /* Get the current newmail count. */
    status = mail$user_begin(&ucontext, null_list, null_list);
    if (!(status & 1)) {
	failure = TRUE;
	return;
    }
    uilist[0].buffer_length = strlen(user);
    uilist[0].buffer_address = user;
    status = mail$user_get_info(&ucontext, uilist, uolist);
    if (!(status & 1)) {
	failure = TRUE;
	return;
    }

    /* Should we report anything to the user? */
    if (new > 0) {
	if (lastcount == 0)
	    /* Have newmail at startup of Lynx. */
	    HTUserMsg(HAVE_UNREAD_MAIL_MSG);
	else if (new > lastcount)
	    /* Have additional mail since last report. */
	    HTUserMsg(HAVE_NEW_MAIL_MSG);
	lastcount = new;
	return;
    }
    lastcount = new;

    /* Clear the context */
    mail$user_end((long *) &ucontext, null_list, null_list);
    return;
}
#else
void LYCheckMail(void)
{
    static BOOL firsttime = TRUE;
    static char *mf;
    static time_t lastcheck;
    static time_t lasttime;
    static long lastsize;
    time_t now;
    struct stat st;

    if (firsttime) {
	mf = LYGetEnv("MAIL");
	firsttime = FALSE;
	time(&lasttime);
    }

    if (mf == NULL)
	return;

    time(&now);
    if (now - lastcheck < 60)
	return;
    lastcheck = now;

    if ((stat(mf, &st) < 0)
	|| !S_ISREG(st.st_mode)) {
	mf = NULL;
	return;
    }

    if (st.st_size > 0) {
	if (((lasttime != st.st_mtime) && (st.st_mtime > st.st_atime))
	    || ((lastsize != 0) && (st.st_size > lastsize)))
	    HTUserMsg(HAVE_NEW_MAIL_MSG);
	else if (lastsize == 0)
	    HTUserMsg(HAVE_MAIL_MSG);
    }
    lastsize = st.st_size;
    lasttime = st.st_mtime;
    return;
}
#endif /* VMS */

/*
 *  This function ensures that an href will be
 *  converted to a fully resolved, absolute URL,
 *  with guessing of the host or expansions of
 *  lead tildes via LYConvertToURL() if needed,
 *  and tweaking/simplifying via HTParse().  It
 *  is used for LynxHome, startfile, homepage,
 *  and 'g'oto entries, after they have been
 *  passed to LYFillLocalFileURL(). - FM
 *  Such URLs have no `base' reference to which they
 *  could be resolved.  LYLegitimizeHREF could not be used.
 */
void LYEnsureAbsoluteURL(char **href,
			 const char *name,
			 int fixit)
{
    char *temp = NULL;

    if (isEmpty(*href))
	return;

    /*
     * Check whether to fill in localhost.  - FM
     */
    LYFillLocalFileURL(href, "file://localhost");

    /*
     * If it is not a URL then make it one.
     */
    if (!strcasecomp(*href, STR_NEWS_URL)) {
	StrAllocCat(*href, "*");
    } else if (!strcasecomp(*href, STR_SNEWS_URL)) {
	StrAllocCat(*href, "/*");
    }

    if (!is_url(*href)) {
	CTRACE((tfp, "%s%s'%s' is not a URL\n",
		NonNull(name), (name ? " " : ""), *href));
	LYConvertToURL(href, fixit);
    }

    temp = HTParse(*href, "", PARSE_ALL);
    if (non_empty(temp))
	StrAllocCopy(*href, temp);
    FREE(temp);
}

/*
 * Rewrite and reallocate a previously allocated string as a file URL if the
 * string resolves to a file or directory on the local system, otherwise as an
 * http URL.  - FM
 */
void LYConvertToURL(char **AllocatedString,
		    int fixit)
{
    char *old_string = *AllocatedString;
    char *temp = NULL;
    char *cp = NULL;

#ifndef VMS
    struct stat st;
#endif /* !VMS */

    if (!old_string || *old_string == '\0')
	return;

#if defined(USE_DOS_DRIVES)
    {
	char *cp_url = *AllocatedString;

	for (; *cp_url != '\0'; cp_url++)
	    if (*cp_url == '\\')
		*cp_url = '/';
	cp_url--;
	if (LYIsDosDrive(*AllocatedString) && *cp_url == ':')
	    LYAddPathSep(AllocatedString);
    }
#endif /* USE_DOS_DRIVES */

    *AllocatedString = NULL;	/* so StrAllocCopy doesn't free it */
    StrAllocCopy(*AllocatedString, "file://localhost");

    if (*old_string != '/') {
	char *fragment = NULL;

#if defined(USE_DOS_DRIVES)
	StrAllocCat(*AllocatedString, "/");
#endif /* USE_DOS_DRIVES */
#ifdef VMS
	/*
	 * Not a SHELL pathspec.  Get the full VMS spec and convert it.
	 */
	char *cur_dir = NULL;
	static char url_file[LY_MAXPATH], file_name[LY_MAXPATH], dir_name[LY_MAXPATH];
	unsigned long context = 0;

	$DESCRIPTOR(url_file_dsc, url_file);
	$DESCRIPTOR(file_name_dsc, file_name);
	if (*old_string == '~') {
	    /*
	     * On VMS, we'll accept '~' on the command line as Home_Dir(), and
	     * assume the rest of the path, if any, has SHELL syntax.
	     */
	    StrAllocCat(*AllocatedString, HTVMS_wwwName(Home_Dir()));
	    if ((cp = strchr(old_string, '/')) != NULL) {
		/*
		 * Append rest of path, if present, skipping "user" if "~user"
		 * was entered, simplifying, and eliminating any residual
		 * relative elements.  - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	    goto have_VMS_URL;
	} else {
	    fragment = trimPoundSelector(old_string);
	    LYstrncpy(url_file, old_string, sizeof(url_file) - 1);
	}
	url_file_dsc.dsc$w_length = (short) strlen(url_file);
	if (1 & lib$find_file(&url_file_dsc, &file_name_dsc, &context,
			      0, 0, 0, 0)) {
	    /*
	     * We found the file.  Convert to a URL pathspec.
	     */
	    if ((cp = strchr(file_name, ';')) != NULL) {
		*cp = '\0';
	    }
	    LYLowerCase(file_name);
	    StrAllocCat(*AllocatedString, HTVMS_wwwName(file_name));
	    if ((cp = strchr(old_string, ';')) != NULL) {
		StrAllocCat(*AllocatedString, cp);
	    }
	    if (fragment != NULL) {
		restorePoundSelector(fragment);
		StrAllocCat(*AllocatedString, fragment);
		fragment = NULL;
	    }
	} else if ((NULL != getcwd(dir_name, sizeof(dir_name) - 1, 0)) &&
		   0 == chdir(old_string)) {
	    /*
	     * Probably a directory.  Try converting that.
	     */
	    StrAllocCopy(cur_dir, dir_name);
	    restorePoundSelector(fragment);
	    if (NULL != getcwd(dir_name, sizeof(dir_name) - 1, 0)) {
		/*
		 * Yup, we got it!
		 */
		LYLowerCase(dir_name);
		StrAllocCat(*AllocatedString, dir_name);
		if (fragment != NULL) {
		    StrAllocCat(*AllocatedString, fragment);
		    fragment = NULL;
		}
	    } else {
		/*
		 * Nope.  Assume it's an http URL with the "http://" defaulted,
		 * if we can't rule out a bad VMS path.
		 */
		fragment = NULL;
		if (strchr(old_string, '[') ||
		    ((cp = strchr(old_string, ':')) != NULL &&
		     !isdigit(UCH(cp[1]))) ||
		    !LYExpandHostForURL(&old_string,
					URLDomainPrefixes,
					URLDomainSuffixes)) {
		    /*
		     * Probably a bad VMS path (but can't be sure).  Use
		     * original pathspec for the error message that will
		     * result.
		     */
		    sprintf(url_file, "/%.*s", sizeof(url_file) - 2, old_string);
		    CTRACE((tfp,
			    "Can't find '%s'  Will assume it's a bad path.\n",
			    old_string));
		    StrAllocCat(*AllocatedString, url_file);
		} else {
		    /*
		     * Assume a URL is wanted, so guess the scheme with
		     * "http://" as the default.  - FM
		     */
		    if (!LYAddSchemeForURL(&old_string, "http://")) {
			StrAllocCopy(*AllocatedString, "http://");
			StrAllocCat(*AllocatedString, old_string);
		    } else {
			StrAllocCopy(*AllocatedString, old_string);
		    }
		}
	    }
	} else {
	    /*
	     * Nothing found.  Assume it's an http URL with the "http://"
	     * defaulted, if we can't rule out a bad VMS path.
	     */
	    restorePoundSelector(fragment);
	    fragment = NULL;

	    if (strchr(old_string, '[') ||
		((cp = strchr(old_string, ':')) != NULL &&
		 !isdigit(UCH(cp[1]))) ||
		!LYExpandHostForURL(&old_string,
				    URLDomainPrefixes,
				    URLDomainSuffixes)) {
		/*
		 * Probably a bad VMS path (but can't be sure).  Use original
		 * pathspec for the error message that will result.
		 */
		sprintf(url_file, "/%.*s", sizeof(url_file) - 2, old_string);
		CTRACE((tfp, "Can't find '%s'  Will assume it's a bad path.\n",
			old_string));
		StrAllocCat(*AllocatedString, url_file);
	    } else {
		/*
		 * Assume a URL is wanted, so guess the scheme with "http://"
		 * as the default.  - FM
		 */
		if (!LYAddSchemeForURL(&old_string, "http://")) {
		    StrAllocCopy(*AllocatedString, "http://");
		    StrAllocCat(*AllocatedString, old_string);
		} else {
		    StrAllocCopy(*AllocatedString, old_string);
		}
	    }
	}
	lib$find_file_end(&context);
	FREE(cur_dir);
      have_VMS_URL:
	CTRACE((tfp, "Trying: '%s'\n", *AllocatedString));
#else /* not VMS: */
#if defined(USE_DOS_DRIVES)
#ifdef _WINDOWS
	if (*old_string == '.') {
	    char fullpath[MAX_PATH + 1];
	    char *filepart = NULL;
	    DWORD chk;

	    chk = GetFullPathNameA(old_string, MAX_PATH + 1,
				   fullpath, &filepart);
	    if (chk != 0) {
		StrAllocCopy(temp, wwwName(fullpath));
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
		CTRACE((tfp, "Converted '%s' to '%s'\n",
			old_string, *AllocatedString));
	    } else {
		StrAllocCat(*AllocatedString, old_string);
	    }
	}
#else
	if (strlen(old_string) == 1 && *old_string == '.') {
	    /*
	     * They want .
	     */
	    char curdir[LY_MAXPATH];

	    StrAllocCopy(temp, wwwName(Current_Dir(curdir)));
	    StrAllocCat(*AllocatedString, temp);
	    FREE(temp);
	    CTRACE((tfp, "Converted '%s' to '%s'\n",
		    old_string, *AllocatedString));
	}
#endif
	else
#endif /* USE_DOS_DRIVES */
	if (*old_string == '~') {
	    /*
	     * On Unix, convert '~' to Home_Dir().
	     */
	    StrAllocCat(*AllocatedString, wwwName(Home_Dir()));
	    if ((cp = strchr(old_string, '/')) != NULL) {
		/*
		 * Append rest of path, if present, skipping "user" if "~user"
		 * was entered, simplifying, and eliminating any residual
		 * relative elements.  - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	    CTRACE((tfp, "Converted '%s' to '%s'\n",
		    old_string, *AllocatedString));
	} else {
	    /*
	     * Create a full path to the current default directory.
	     */
	    char curdir[LY_MAXPATH];
	    char *temp2 = NULL;
	    BOOL is_local = FALSE;

	    Current_Dir(curdir);
	    /*
	     * Concatenate and simplify, trimming any residual relative
	     * elements.  - FM
	     */
#if defined (USE_DOS_DRIVES)
	    if (old_string[1] != ':' && old_string[1] != '|') {
		StrAllocCopy(temp, wwwName(curdir));
		LYAddHtmlSep(&temp);
		LYstrncpy(curdir, temp, (sizeof(curdir) - 1));
		StrAllocCat(temp, old_string);
	    } else {
		curdir[0] = '\0';
		/* 1998/01/13 (Tue) 12:24:33 */
		if (old_string[1] == '|')
		    old_string[1] = ':';
		StrAllocCopy(temp, old_string);

		if (strlen(temp) == 2 && LYIsDosDrive(temp))
		    LYAddPathSep(&temp);
	    }
#else
	    StrAllocCopy(temp, curdir);
	    StrAllocCat(temp, "/");
	    StrAllocCat(temp, old_string);
#endif /* USE_DOS_DRIVES */
	    LYTrimRelFromAbsPath(temp);
	    CTRACE((tfp, "Converted '%s' to '%s'\n", old_string, temp));
	    if ((stat(temp, &st) > -1) ||
		LYCanReadFile(temp)) {
		/*
		 * It is a subdirectory or file on the local system.
		 */
#if defined (USE_DOS_DRIVES)
		/* Don't want to see DOS local paths like c: escaped  */
		/* especially when we really have file://localhost/   */
		/* at the beginning.  To avoid any confusion we allow */
		/* escaping the path if URL specials % or # present.  */
		if (strchr(temp, '#') == NULL && strchr(temp, '%') == NULL)
		    StrAllocCopy(cp, temp);
		else
		    cp = HTEscape(temp, URL_PATH);
#else
		cp = HTEscape(temp, URL_PATH);
#endif /* USE_DOS_DRIVES */
		StrAllocCat(*AllocatedString, cp);
		FREE(cp);
		CTRACE((tfp, "Converted '%s' to '%s'\n",
			old_string, *AllocatedString));
		is_local = TRUE;
	    } else {
		char *cp2 = NULL;

		StrAllocCopy(temp2, curdir);
		LYAddPathSep(&temp2);
		StrAllocCopy(cp, old_string);
		fragment = trimPoundSelector(cp);
		HTUnEscape(cp);	/* unescape given path without fragment */
		StrAllocCat(temp2, cp);		/* append to current dir  */
		StrAllocCopy(cp2, temp2);	/* keep a copy in cp2     */
		LYTrimRelFromAbsPath(temp2);
#ifdef WIN_EX			/* 1998/07/31 (Fri) 09:09:03 */
		HTUnEscape(temp2);	/* for LFN */
#endif

		if (strcmp(temp2, temp) != 0 &&
		    ((stat(temp2, &st) > -1) ||
		     LYCanReadFile(temp2))) {
		    /*
		     * It is a subdirectory or file on the local system with
		     * escaped characters and/or a fragment to be appended to
		     * the URL.  - FM
		     */

		    FREE(temp);
		    if (strcmp(cp2, temp2) == 0) {
			/*
			 * LYTrimRelFromAbsPath did nothing, use old_string as
			 * given.  - kw
			 */
			temp = HTEscape(curdir, URL_PATH);
			LYAddHtmlSep(&temp);
			StrAllocCat(temp, old_string);
		    } else {
			temp = HTEscape(temp2, URL_PATH);
			if (fragment != NULL) {
			    restorePoundSelector(fragment);
			    StrAllocCat(temp, fragment);
			}
		    }
		    StrAllocCat(*AllocatedString, temp);
		    CTRACE((tfp, "Converted '%s' to '%s'\n",
			    old_string, *AllocatedString));
		    is_local = TRUE;

		} else if (strchr(curdir, '#') != NULL ||
			   strchr(curdir, '%') != NULL) {
		    /*
		     * If PWD has some unusual characters, construct a filename
		     * in temp where those are escaped.  This is mostly to
		     * prevent this function from returning with some weird URL
		     * if the LYExpandHostForURL tests further down fail.  - kw
		     */
		    FREE(temp);
		    if (strcmp(cp2, temp2) == 0) {
			/*
			 * LYTrimRelFromAbsPath did nothing, use old_string as
			 * given.  - kw
			 */
			temp = HTEscape(curdir, URL_PATH);
			LYAddHtmlSep(&temp);
			StrAllocCat(temp, old_string);
		    } else {
			temp = HTEscape(temp2, URL_PATH);
			if (fragment != NULL) {
			    restorePoundSelector(fragment);
			    StrAllocCat(temp, fragment);
			}
		    }
		}
		FREE(cp);
		FREE(cp2);
	    }
	    if (is_local == FALSE) {
		/*
		 * It's not an accessible subdirectory or file on the local
		 * system, so assume it's a URL request and guess the scheme
		 * with "http://" as the default.
		 */
		CTRACE((tfp, "Can't stat() or fopen() '%s'\n",
			temp2 ? temp2 : temp));
#ifdef WIN_EX			/* 1998/01/13 (Tue) 09:07:37 */
		{
		    const char *p, *q;
		    char buff[LY_MAXPATH + 128];

		    p = Home_Dir();
		    q = temp2 ? temp2 : temp;

		    if (strlen(q) == 3 && LYIsDosDrive(q)) {
			sprintf(buff,
				"'%s' not exist, Goto LynxHome '%s'.", q, p);
			_statusline(buff);
			LYSleepAlert();
			FREE(temp);
			StrAllocCat(*AllocatedString, p);
			goto Retry;
		    }
		}
#endif
		if (LYExpandHostForURL(&old_string,
				       URLDomainPrefixes,
				       URLDomainSuffixes)) {
		    if (!LYAddSchemeForURL(&old_string, "http://")) {
			StrAllocCopy(*AllocatedString, "http://");
			StrAllocCat(*AllocatedString, old_string);
		    } else {
			StrAllocCopy(*AllocatedString, old_string);
		    }
		} else if (fixit) {
		    /* RW 1998Mar16  Restore AllocatedString to 'old_string' */
		    StrAllocCopy(*AllocatedString, old_string);
		} else {
		    /* Return file URL for the file that does not exist */
		    StrAllocCat(*AllocatedString, temp);
		}
#ifdef WIN_EX
	      Retry:
#endif
		CTRACE((tfp, "Trying: '%s'\n", *AllocatedString));
	    }
	    FREE(temp);
	    FREE(temp2);
	}
#endif /* VMS */
    } else {
	/*
	 * Path begins with a slash.  Simplify and use it.
	 */
	if (old_string[1] == '\0') {
	    /*
	     * Request for root.  Respect it on Unix, but on VMS we treat that
	     * as a listing of the login directory.  - FM
	     */
#ifdef VMS
	    StrAllocCat(*AllocatedString, HTVMS_wwwName(Home_Dir()));
#else
	    StrAllocCat(*AllocatedString, "/");
	} else if ((stat(old_string, &st) > -1) ||
		   LYCanReadFile(old_string)) {
	    /*
	     * It is an absolute directory or file on the local system.  - KW
	     */
	    StrAllocCopy(temp, old_string);
	    LYTrimRelFromAbsPath(temp);
	    CTRACE((tfp, "Converted '%s' to '%s'\n", old_string, temp));
	    cp = HTEscape(temp, URL_PATH);
	    StrAllocCat(*AllocatedString, cp);
	    FREE(cp);
	    FREE(temp);
	    CTRACE((tfp, "Converted '%s' to '%s'\n",
		    old_string, *AllocatedString));
#endif /* VMS */
	} else if (old_string[1] == '~') {
	    /*
	     * Has a Home_Dir() reference.  Handle it as if there weren't a
	     * lead slash.  - FM
	     */
	    StrAllocCat(*AllocatedString, wwwName(Home_Dir()));
	    if ((cp = strchr((old_string + 1), '/')) != NULL) {
		/*
		 * Append rest of path, if present, skipping "user" if "~user"
		 * was entered, simplifying, and eliminating any residual
		 * relative elements.  - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	} else {
	    /*
	     * Normal absolute path.  Simplify, trim any residual relative
	     * elements, and append it.  - FM
	     */
	    StrAllocCopy(temp, old_string);
	    LYTrimRelFromAbsPath(temp);
	    StrAllocCat(*AllocatedString, temp);
	    FREE(temp);
	}
	CTRACE((tfp, "Converted '%s' to '%s'\n",
		old_string, *AllocatedString));
    }
    FREE(old_string);
    /* Pause so we can read the messages before invoking curses */
    CTRACE_SLEEP(AlertSecs);
}

#if defined(_WINDOWS)		/* 1998/06/23 (Tue) 16:45:20 */

int win32_check_interrupt(void)
{
    int c;

    if (kbhit()) {
	c = LYgetch();
	/** Keyboard 'Z' or 'z', or Control-G or Control-C **/
	if (LYCharIsINTERRUPT(c) || c == 0x1b) {
	    return TRUE;
	}
    }
    return FALSE;
}

void sleep(unsigned sec)
{
    unsigned int i, j;
    int c;

    for (j = 0; j < sec; j++) {
	for (i = 0; i < 10; i++) {
	    Sleep(100);
	    if (kbhit()) {
		c = LYgetch();
		return;
	    }
	}
    }
}
#endif

/*
 * This function rewrites and reallocates a previously allocated string so that
 * the first element is a confirmed Internet host, and returns TRUE, otherwise
 * it does not modify the string and returns FALSE.  It first tries the element
 * as is, then, if the element does not end with a dot, it adds prefixes from
 * the (comma separated) prefix list argument, and, if the element does not
 * begin with a dot, suffixes from the (comma separated) suffix list arguments
 * (e.g., www.host.com, then www.host,edu, then www.host.net, then
 * www.host.org).  The remaining path, if one is present, will be appended to
 * the expanded host.  It also takes into account whether a colon is in the
 * element or suffix, and includes that and what follows as a port field for
 * the expanded host field (e.g, wfbr:8002/dir/lynx should yield
 * www.wfbr.edu:8002/dir/lynx).  The calling function should prepend the scheme
 * field (e.g., http://), or pass the string to LYAddSchemeForURL(), if this
 * function returns TRUE.  - FM
 */
BOOLEAN LYExpandHostForURL(char **AllocatedString,
			   char *prefix_list,
			   char *suffix_list)
{
    char *DomainPrefix = NULL;
    const char *StartP, *EndP;
    char *DomainSuffix = NULL;
    const char *StartS, *EndS;
    char *Str = NULL, *StrColon = NULL, *MsgStr = NULL;
    char *Host = NULL, *HostColon = NULL, *host = NULL;
    char *Path = NULL;
    char *Fragment = NULL;
    BOOLEAN GotHost = FALSE;
    BOOLEAN Startup = (BOOL) (helpfilepath == NULL);

#ifdef INET6
    struct addrinfo hints, *res;
    int error;
#endif /* INET6 */

    /*
     * If it's a NULL or zero-length string, or if it begins with a slash or
     * hash, don't continue pointlessly.  - FM
     */
    if (!(*AllocatedString) || *AllocatedString[0] == '\0' ||
	*AllocatedString[0] == '/' || *AllocatedString[0] == '#') {
	return GotHost;
    }

    /*
     * If it's a partial or relative path, don't continue pointlessly.  - FM
     */
    if (!strncmp(*AllocatedString, "..", 2) ||
	!strncmp(*AllocatedString, "./", 2)) {
	return GotHost;
    }

    /*
     * Make a clean copy of the string, and trim off the path if one is
     * present, but save the information so we can restore the path after
     * filling in the Host[:port] field.  - FM
     */
    StrAllocCopy(Str, *AllocatedString);
    if ((Path = strchr(Str, '/')) != NULL) {
	/*
	 * Have a path.  Any fragment should already be included in Path.  - FM
	 */
	*Path = '\0';
    } else {
	/*
	 * No path, so check for a fragment and trim that, to be restored after
	 * filling in the Host[:port] field.  - FM
	 */
	Fragment = trimPoundSelector(Str);
    }

    /*
     * If the potential host string has a colon, assume it begins a port field,
     * and trim it off, but save the information so we can restore the port
     * field after filling in the host field.  - FM
     */
    if ((StrColon = strrchr(Str, ':')) != NULL &&
	isdigit(UCH(StrColon[1]))) {
	if (StrColon == Str) {
	    goto cleanup;
	}
	*StrColon = '\0';
    }

    /*
     * Do a DNS test on the potential host field as presently trimmed.  - FM
     */
    StrAllocCopy(host, Str);
    HTUnEscape(host);
    if (LYCursesON) {
	StrAllocCopy(MsgStr, WWW_FIND_MESSAGE);
	StrAllocCat(MsgStr, host);
	StrAllocCat(MsgStr, FIRST_SEGMENT);
	HTProgress(MsgStr);
    } else if (Startup && !dump_output_immediately) {
	fprintf(stdout, "%s '%s'%s\r\n", WWW_FIND_MESSAGE, host, FIRST_SEGMENT);
    }
#ifdef INET6
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host, "80", &hints, &res);

    if (!error && res)
#else
    if (LYGetHostByName(host) != NULL)
#endif /* INET6 */
    {
	/*
	 * Clear any residual interrupt.  - FM
	 */
	if (LYCursesON && HTCheckForInterrupt()) {
	    CTRACE((tfp,
		    "LYExpandHostForURL: Ignoring interrupt because '%s' resolved.\n",
		    host));
	}

	/*
	 * Return success.  - FM
	 */
	GotHost = TRUE;
	goto cleanup;
    } else if (LYCursesON && (lynx_nsl_status == HT_INTERRUPTED)) {
	/*
	 * Give the user chance to interrupt lookup cycles.  - KW & FM
	 */
	CTRACE((tfp,
		"LYExpandHostForURL: Interrupted while '%s' failed to resolve.\n",
		host));

	/*
	 * Return failure.  - FM
	 */
	goto cleanup;
    }

    /*
     * Set the first prefix, making it a zero-length string if the list is NULL
     * or if the potential host field ends with a dot.  - FM
     */
    StartP = ((prefix_list && Str[strlen(Str) - 1] != '.')
	      ? prefix_list
	      : "");
    /*
     * If we have a prefix, but the allocated string is one of the common host
     * prefixes, make our prefix a zero-length string.  - FM
     */
    if (*StartP && *StartP != '.') {
	if (!strncasecomp(*AllocatedString, "www.", 4) ||
	    !strncasecomp(*AllocatedString, "ftp.", 4) ||
	    !strncasecomp(*AllocatedString, "gopher.", 7) ||
	    !strncasecomp(*AllocatedString, "wais.", 5) ||
	    !strncasecomp(*AllocatedString, "cso.", 4) ||
	    !strncasecomp(*AllocatedString, "ns.", 3) ||
	    !strncasecomp(*AllocatedString, "ph.", 3) ||
	    !strncasecomp(*AllocatedString, "finger.", 7) ||
	    !strncasecomp(*AllocatedString, "news.", 5) ||
	    !strncasecomp(*AllocatedString, "nntp.", 5)) {
	    StartP = "";
	}
    }
    while ((*StartP) && (WHITE(*StartP) || *StartP == ',')) {
	StartP++;		/* Skip whitespace and separators */
    }
    EndP = StartP;
    while (*EndP && !WHITE(*EndP) && *EndP != ',') {
	EndP++;			/* Find separator */
    }
    StrAllocCopy(DomainPrefix, StartP);
    DomainPrefix[EndP - StartP] = '\0';

    /*
     * Test each prefix with each suffix.  - FM
     */
    do {
	/*
	 * Set the first suffix, making it a zero-length string if the list is
	 * NULL or if the potential host field begins with a dot.  - FM
	 */
	StartS = ((suffix_list && *Str != '.')
		  ? suffix_list
		  : "");
	while ((*StartS) && (WHITE(*StartS) || *StartS == ',')) {
	    StartS++;		/* Skip whitespace and separators */
	}
	EndS = StartS;
	while (*EndS && !WHITE(*EndS) && *EndS != ',') {
	    EndS++;		/* Find separator */
	}
	StrAllocCopy(DomainSuffix, StartS);
	DomainSuffix[EndS - StartS] = '\0';

	/*
	 * Create domain names and do DNS tests.  - FM
	 */
	do {
	    StrAllocCopy(Host, DomainPrefix);
	    StrAllocCat(Host, ((*Str == '.') ? (Str + 1) : Str));
	    if (Host[strlen(Host) - 1] == '.') {
		Host[strlen(Host) - 1] = '\0';
	    }
	    StrAllocCat(Host, DomainSuffix);
	    if ((HostColon = strrchr(Host, ':')) != NULL &&
		isdigit(UCH(HostColon[1]))) {
		*HostColon = '\0';
	    }
	    StrAllocCopy(host, Host);
	    HTUnEscape(host);
	    if (LYCursesON) {
		StrAllocCopy(MsgStr, WWW_FIND_MESSAGE);
		StrAllocCat(MsgStr, host);
		StrAllocCat(MsgStr, GUESSING_SEGMENT);
		HTProgress(MsgStr);
	    } else if (Startup && !dump_output_immediately) {
		fprintf(stdout, "%s '%s'%s\n", WWW_FIND_MESSAGE, host, GUESSING_SEGMENT);
	    }
	    GotHost = (BOOL) (LYGetHostByName(host) != NULL);
	    if (HostColon != NULL) {
		*HostColon = ':';
	    }
	    if (GotHost == FALSE) {
		/*
		 * Give the user chance to interrupt lookup cycles.  - KW
		 */
		if (LYCursesON && (lynx_nsl_status == HT_INTERRUPTED)) {
		    CTRACE((tfp,
			    "LYExpandHostForURL: Interrupted while '%s' failed to resolve.\n",
			    host));
		    goto cleanup;	/* We didn't find a valid name. */
		}

		/*
		 * Advance to the next suffix, or end of suffix list.  - FM
		 */
		StartS = ((*EndS == '\0') ? EndS : (EndS + 1));
		while ((*StartS) && (WHITE(*StartS) || *StartS == ',')) {
		    StartS++;	/* Skip whitespace and separators */
		}
		EndS = StartS;
		while (*EndS && !WHITE(*EndS) && *EndS != ',') {
		    EndS++;	/* Find separator */
		}
		LYstrncpy(DomainSuffix, StartS, (EndS - StartS));
	    }
	} while ((GotHost == FALSE) && (*DomainSuffix != '\0'));

	if (GotHost == FALSE) {
	    /*
	     * Advance to the next prefix, or end of prefix list.  - FM
	     */
	    StartP = ((*EndP == '\0') ? EndP : (EndP + 1));
	    while ((*StartP) && (WHITE(*StartP) || *StartP == ',')) {
		StartP++;	/* Skip whitespace and separators */
	    }
	    EndP = StartP;
	    while (*EndP && !WHITE(*EndP) && *EndP != ',') {
		EndP++;		/* Find separator */
	    }
	    LYstrncpy(DomainPrefix, StartP, (EndP - StartP));
	}
    } while ((GotHost == FALSE) && (*DomainPrefix != '\0'));

    /*
     * If a test passed, restore the port field if we had one and there is no
     * colon in the expanded host, and the path if we had one, and reallocate
     * the original string with the expanded Host[:port] field included.  - FM
     */
    if (GotHost) {
	if (StrColon && strchr(Host, ':') == NULL) {
	    *StrColon = ':';
	    StrAllocCat(Host, StrColon);
	}
	if (Path) {
	    *Path = '/';
	    StrAllocCat(Host, Path);
	} else if (Fragment) {
	    StrAllocCat(Host, "/");
	    restorePoundSelector(Fragment);
	    StrAllocCat(Host, Fragment);
	}
	StrAllocCopy(*AllocatedString, Host);
    }

    /*
     * Clear any residual interrupt.  - FM
     */
    if (LYCursesON && HTCheckForInterrupt()) {
	CTRACE((tfp,
		"LYExpandHostForURL: Ignoring interrupt because '%s' %s.\n",
		host,
		(GotHost ? "resolved" : "timed out")));
    }

    /*
     * Clean up and return the last test result.  - FM
     */
  cleanup:
    FREE(DomainPrefix);
    FREE(DomainSuffix);
    FREE(Str);
    FREE(MsgStr);
    FREE(Host);
    FREE(host);
    return GotHost;
}

/*
 * This function rewrites and reallocates a previously allocated string that
 * begins with an Internet host name so that the string begins with its guess
 * of the scheme based on the first field of the host name, or the default
 * scheme if no guess was made, and returns TRUE, otherwise it does not modify
 * the string and returns FALSE.  It also returns FALSE without modifying the
 * string if the default_scheme argument was NULL or zero-length and no guess
 * was made.  - FM
 */
BOOLEAN LYAddSchemeForURL(char **AllocatedString,
			  const char *default_scheme)
{
    char *Str = NULL;
    BOOLEAN GotScheme = FALSE;

    /*
     * If we were passed a NULL or zero-length string, don't continue
     * pointlessly.  - FM
     */
    if (!(*AllocatedString) || *AllocatedString[0] == '\0') {
	return GotScheme;
    }

    /*
     * Try to guess the appropriate scheme. - FM
     */
    if (0 == strncasecomp(*AllocatedString, "www", 3)) {
	/*
	 * This could be either http or https, so check the default and
	 * otherwise use "http".  - FM
	 */
	if (default_scheme != NULL &&
	    NULL != strstr(default_scheme, "http")) {
	    StrAllocCopy(Str, default_scheme);
	} else {
	    StrAllocCopy(Str, "http://");
	}
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "ftp", 3)) {
	StrAllocCopy(Str, "ftp://");
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "gopher", 6)) {
	StrAllocCopy(Str, "gopher://");
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "wais", 4)) {
	StrAllocCopy(Str, "wais://");
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "cso", 3) ||
	       0 == strncasecomp(*AllocatedString, "ns.", 3) ||
	       0 == strncasecomp(*AllocatedString, "ph.", 3)) {
	StrAllocCopy(Str, "cso://");
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "finger", 6)) {
	StrAllocCopy(Str, "finger://");
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "news", 4)) {
	/*
	 * This could be either news, snews, or nntp, so check the default, and
	 * otherwise use news.  - FM
	 */
	if ((default_scheme != NULL) &&
	    (NULL != strstr(default_scheme, "news") ||
	     NULL != strstr(default_scheme, "nntp"))) {
	    StrAllocCopy(Str, default_scheme);
	} else {
	    StrAllocCopy(Str, "news://");
	}
	GotScheme = TRUE;

    } else if (0 == strncasecomp(*AllocatedString, "nntp", 4)) {
	StrAllocCopy(Str, "nntp://");
	GotScheme = TRUE;

    }

    /*
     * If we've make a guess, use it.  Otherwise, if we were passed a default
     * scheme prefix, use that.  - FM
     */
    if (GotScheme == TRUE) {
	StrAllocCat(Str, *AllocatedString);
	StrAllocCopy(*AllocatedString, Str);
	FREE(Str);
	return GotScheme;

    } else if (non_empty(default_scheme)) {
	StrAllocCopy(Str, default_scheme);
	GotScheme = TRUE;
	StrAllocCat(Str, *AllocatedString);
	StrAllocCopy(*AllocatedString, Str);
	FREE(Str);
	return GotScheme;
    }

    return GotScheme;
}

/*
 * This function expects an absolute Unix or VMS SHELL path spec as an
 * allocated string, simplifies it, and trims out any residual relative
 * elements.  It also checks whether the path had a terminal slash, and if it
 * didn't, makes sure that the simplified path doesn't either.  If it's a
 * directory, our convention is to exclude "Up to parent" links when a terminal
 * slash is present.  - FM
 */
void LYTrimRelFromAbsPath(char *path)
{
    char *cp;
    int i;
    BOOL TerminalSlash;

    /*
     * Make sure we have a pointer to an absolute path.  - FM
     */
    if (path == NULL || !LYIsPathSep(*path))
	return;

    /*
     * Check whether the path has a terminal slash.  - FM
     */
    TerminalSlash = (BOOL) (LYIsPathSep(path[(strlen(path) - 1)]));

    /*
     * Simplify the path and then do any necessary trimming.  - FM
     */
    HTSimplify(path);
    cp = path;
    while (cp[1] == '.') {
	if (cp[2] == '\0') {
	    /*
	     * Eliminate trailing dot.  - FM
	     */
	    cp[1] = '\0';
	} else if (LYIsPathSep(cp[2])) {
	    /*
	     * Skip over the "/." of a "/./".  - FM
	     */
	    cp += 2;
	} else if (cp[2] == '.' && cp[3] == '\0') {
	    /*
	     * Eliminate trailing dotdot.  - FM
	     */
	    cp[1] = '\0';
	} else if (cp[2] == '.' && cp[3] == '/') {
	    /*
	     * Skip over the "/.." of a "/../".  - FM
	     */
	    cp += 3;
	} else {
	    /*
	     * Done trimming.  - FM
	     */
	    break;
	}
    }

    /*
     * Load any shifts into path, and eliminate any terminal slash created by
     * HTSimplify() or our walk, but not present originally.  - FM
     */
    if (cp > path) {
	for (i = 0; cp[i] != '\0'; i++)
	    path[i] = cp[i];
	path[i] = '\0';
    }
    if (TerminalSlash == FALSE) {
	LYTrimPathSep(path);
    }
}

/*
 * Example Client-Side Include interface.
 *
 * This is called from SGML.c and simply returns markup for reporting the URL
 * of the document being loaded if a comment begins with "<!--#lynxCSI".  The
 * markup will be included as if it were in the document.  Move this function
 * to a separate module for doing this kind of thing seriously, someday.  - FM
 */
void LYDoCSI(char *url,
	     const char *comment,
	     char **csi)
{
    const char *cp = comment;

    if (cp == NULL)
	return;

    if (strncmp(cp, "!--#", 4))
	return;

    cp += 4;
    if (!strncasecomp(cp, "lynxCSI", 7)) {
	StrAllocCat(*csi, "\n<p align=\"center\">URL: ");
	StrAllocCat(*csi, url);
	StrAllocCat(*csi, "</p>\n\n");
    }

    return;
}

#ifdef VMS
/*
 * Define_VMSLogical -- Fote Macrides 04-Apr-1995
 * Define VMS logicals in the process table.
 */
void Define_VMSLogical(char *LogicalName,
		       char *LogicalValue)
{
    $DESCRIPTOR(lname, "");
    $DESCRIPTOR(lvalue, "");
    $DESCRIPTOR(ltable, "LNM$PROCESS");

    if (!LogicalName || *LogicalName == '\0')
	return;

    lname.dsc$w_length = strlen(LogicalName);
    lname.dsc$a_pointer = LogicalName;

    if (!LogicalValue || *LogicalValue == '\0') {
	lib$delete_logical(&lname, &ltable);
	return;
    }

    lvalue.dsc$w_length = strlen(LogicalValue);
    lvalue.dsc$a_pointer = LogicalValue;
    lib$set_logical(&lname, &lvalue, &ltable, 0, 0);
    return;
}
#endif /* VMS */

#ifdef LY_FIND_LEAKS
static void LYHomeDir_free(void)
{
    FREE(HomeDir);
}
#endif /* LY_FIND_LEAKS */

char *Current_Dir(char *pathname)
{
    char *result;

#ifdef HAVE_GETCWD
    result = getcwd(pathname, LY_MAXPATH);
#else
    result = getwd(pathname);
#endif /* NO_GETCWD */
    if (result == 0)
	strcpy(pathname, ".");
    return pathname;
}

/*
 * Verify that the given path refers to an existing directory, returning the
 * string if the directory exists.  If not, return null.
 */
static char *CheckDir(char *path)
{
    struct stat stat_info;

    if (!LYisAbsPath(path)
	|| (HTStat(path, &stat_info) < 0
	    || !S_ISDIR(stat_info.st_mode))) {
	path = NULL;
    }
    return path;
}

/*
 * Lookup various possibilities for $HOME, and check that the directory exists.
 */
static char *HomeEnv(void)
{
    char *result = CheckDir(LYGetEnv("HOME"));

#if defined (USE_DOS_DRIVES)
    if (result == 0) {
	char *head;
	char *leaf;
	static char *temp = NULL;

	/* Windows 2000 */
	if ((result = LYGetEnv("USERPROFILE")) != 0) {
	    HTSprintf0(&temp, "%s%sMy Documents", result, PATHSEP_STR);
	    result = CheckDir(temp);
	}
	/* NT4 */
	if (result == 0) {
	    if ((head = LYGetEnv("HOMEDRIVE")) != 0) {
		if ((leaf = LYGetEnv("HOMEPATH")) != 0) {
		    HTSprintf0(&temp, "%s%s%s", head, PATHSEP_STR, leaf);
		    result = CheckDir(temp);
		}
	    }
	}
	/* General M$ */
	if (result == 0)
	    result = CheckDir(LYGetEnv("TEMP"));
	if (result == 0)
	    result = CheckDir(LYGetEnv("TMP"));
	if (result == 0) {
	    if ((head = LYGetEnv("SystemDrive")) != 0) {
		HTSprintf0(&temp, "%s%s", head, PATHSEP_STR);
		result = CheckDir(temp);
	    }
	}
	if (result == 0)
	    result = CheckDir("C:" PATHSEP_STR);
    }
#endif

    return result;
}

const char *Home_Dir(void)
{
    static const char *homedir = NULL;
    char *cp = NULL;

    if (homedir == NULL) {
	if ((cp = HomeEnv()) == NULL) {
#ifdef VMS
	    if ((cp = LYGetEnv("SYS$LOGIN")) == NULL
		&& (cp = LYGetEnv("SYS$SCRATCH")) == NULL) {
		cp = "sys$scratch:";
	    }
	    StrAllocCopy(HomeDir, cp);
#else
#ifdef UNIX
#ifdef HAVE_UTMP
	    /*
	     * One could use getlogin() and getpwnam() here instead.
	     */
	    struct passwd *pw = getpwuid(geteuid());

	    if (pw && pw->pw_dir) {
		StrAllocCopy(HomeDir, pw->pw_dir);
	    } else
#endif
	    {
		/*
		 * Use /tmp; it should be writable.
		 */
		StrAllocCopy(HomeDir, "/tmp");
	    }
#endif
#endif /* VMS */
	} else {
	    StrAllocCopy(HomeDir, cp);
	}
	homedir = (const char *) HomeDir;
#ifdef LY_FIND_LEAKS
	atexit(LYHomeDir_free);
#endif
    }
    if (homedir == NULL) {
	printf("%s\n", gettext("Cannot find HOME directory"));
	exit_immediately(EXIT_FAILURE);
    }
    return homedir;
}

/*
 * Return a pointer to the final leaf of the given pathname, If no pathname
 * separators are found, returns the original pathname.  The leaf may be
 * empty.
 */
char *LYPathLeaf(char *pathname)
{
    char *leaf;

#ifdef UNIX
    if ((leaf = strrchr(pathname, '/')) != 0) {
	leaf++;
    }
#else
#ifdef VMS
    if ((leaf = strrchr(pathname, ']')) == 0)
	leaf = strrchr(pathname, ':');
    if (leaf != 0)
	leaf++;
#else
    int n;

    for (leaf = 0, n = strlen(pathname) - 1; n >= 0; n--) {
	if (strchr("\\/:", pathname[n]) != 0) {
	    leaf = pathname + n + 1;
	    break;
	}
    }
#endif
#endif
    return (leaf != 0) ? leaf : pathname;
}

/*
 * This function checks the acceptability of file paths that are intended to be
 * off the home directory.  The file path should be passed in fbuffer, together
 * with the size of the buffer.  The function simplifies the file path, and if
 * it is acceptable, loads it into fbuffer and returns TRUE.  Otherwise, it
 * does not modify fbuffer and returns FALSE.  If a subdirectory is present and
 * the path does not begin with "./", that is prefixed to make the situation
 * clear.  - FM
 */
BOOLEAN LYPathOffHomeOK(char *fbuffer,
			size_t fbuffer_size)
{
    char *file = NULL;
    char *cp, *cp1;

    /*
     * Make sure we have an fbuffer and a string in it.  - FM
     */
    if (!fbuffer || fbuffer_size < 2 || fbuffer[0] == '\0') {
	return (FALSE);
    }
    StrAllocCopy(file, fbuffer);
    cp = file;

    /*
     * Check for an inappropriate reference to the home directory, and correct
     * it if we can.  - FM
     */
#ifdef VMS
    if (!strncasecomp(cp, "sys$login", 9)) {
	if (*(cp + 9) == '\0') {
	    /*
	     * Reject "sys$login".  - FM
	     */
	    FREE(file);
	    return (FALSE);
	}
	if (*(cp + 9) == ':') {
	    cp += 10;
	    if (*cp == '\0') {
		/*
		 * Reject "sys$login:".  Otherwise, we have converted
		 * "sys$login:file" to "file", or have left a strange path for
		 * VMS as it was originally.  - FM
		 */
		FREE(file);
		return (FALSE);
	    }
	}
    }
#endif /* VMS */
    if (*cp == '~') {
	if (*(cp + 1) == '/') {
	    if (*(cp + 2) != '\0') {
		if ((cp1 = strchr((cp + 2), '/')) != NULL) {
		    /*
		     * Convert "~/subdir(s)/file" to "./subdir(s)/file".  - FM
		     */
		    *cp = '.';
		} else {
		    /*
		     * Convert "~/file" to "file".  - FM
		     */
		    cp += 2;
		}
	    } else {
		/*
		 * Reject "~/".  - FM
		 */
		FREE(file);
		return (FALSE);
	    }
	} else if ((*(cp + 1) != '\0') &&
		   (cp1 = strchr((cp + 1), '/')) != NULL) {
	    cp = (cp1 - 1);
	    if (*(cp + 2) != '\0') {
		if ((cp1 = strchr((cp + 2), '/')) != NULL) {
		    /*
		     * Convert "~user/subdir(s)/file" to "./subdir(s)/file". 
		     * If user is someone else, we covered a spoof.  Otherwise,
		     * we simplified.  - FM
		     */
		    *cp = '.';
		} else {
		    /*
		     * Convert "~user/file" to "file".  - FM
		     */
		    cp += 2;
		}
	    } else {
		/*
		 * Reject "~user/".  - FM
		 */
		FREE(file);
		return (FALSE);
	    }
	} else {
	    /*
	     * Reject "~user".  - FM
	     */
	    FREE(file);
	    return (FALSE);
	}
    }
#ifdef VMS
    /*
     * Check for VMS path specs, and reject if still present.  - FM
     */
    if (strchr(cp, ':') != NULL || strchr(cp, ']') != NULL) {
	FREE(file);
	return (FALSE);
    }
#endif /* VMS */

    /*
     * Check for a URL or absolute path, and reject if present.  - FM
     */
    if (is_url(cp) || LYIsPathSep(*cp)) {
	FREE(file);
	return (FALSE);
    }

    /*
     * Simplify it.  - FM
     */
    HTSimplify(cp);

    /*
     * Check if it has a pointless "./".  - FM
     */
    if (!strncmp(cp, "./", 2)) {
	if ((cp1 = strchr((cp + 2), '/')) == NULL) {
	    cp += 2;
	}
    }

    /*
     * Check for spoofing.  - FM
     */
    if (*cp == '\0'
	|| LYIsPathSep(*cp)
	|| LYIsPathSep(cp[(strlen(cp) - 1)])
	|| strstr(cp, "..") != NULL
	|| !strcmp(cp, ".")) {
	FREE(file);
	return (FALSE);
    }

    /*
     * Load what we have at this point into fbuffer, trimming if too long, and
     * claim it's OK.  - FM
     */
    if (fbuffer_size > 3 && strncmp(cp, "./", 2) && strchr(cp, '/')) {
	/*
	 * We have a subdirectory and no lead "./", so prefix it to make the
	 * situation clear.  - FM
	 */
	strcpy(fbuffer, "./");
	if (strlen(cp) > (fbuffer_size - 3))
	    cp[(fbuffer_size - 3)] = '\0';
	strcat(fbuffer, cp);
    } else {
	if (strlen(cp) > (fbuffer_size - 1))
	    cp[(fbuffer_size - 1)] = '\0';
	strcpy(fbuffer, cp);
    }
    FREE(file);
    return (TRUE);
}

/*
 * This function appends fname to the home path and returns the full path and
 * filename.  The fname string can be just a filename (e.g.,
 * "lynx_bookmarks.html"), or include a subdirectory off the home directory, in
 * which case fname should begin with "./" (e.g., ./BM/lynx_bookmarks.html) Use
 * LYPathOffHomeOK() to check and/or fix up fname before calling this function. 
 * On VMS, the resultant full path and filename are converted to VMS syntax.  -
 * FM
 */
void LYAddPathToHome(char *fbuffer,
		     size_t fbuffer_size,
		     const char *fname)
{
    char *home = NULL;
    const char *file = fname;
    int len;

    /*
     * Make sure we have a buffer.  - FM
     */
    if (!fbuffer)
	return;
    if (fbuffer_size < 2) {
	fbuffer[0] = '\0';
	return;
    }
    fbuffer[(fbuffer_size - 1)] = '\0';

    /*
     * Make sure we have a file name.  - FM
     */
    if (!file)
	file = "";

    /*
     * Set up home string and length.  - FM
     */
    StrAllocCopy(home, Home_Dir());

#ifdef VMS
#define NO_HOMEPATH "Error:"
#else
#define NO_HOMEPATH "/error"
#endif /* VMS */
    if (!non_empty(home))
	/*
	 * Home_Dir() has a bug if this ever happens.  - FM
	 */
	StrAllocCopy(home, NO_HOMEPATH);

    len = fbuffer_size - (strlen(home) + 1);
    if (len <= 0) {
	/*
	 * Buffer is smaller than or only big enough for the home path.  Load
	 * what fits of the home path and return.  This will fail, but we need
	 * something in the buffer.  - FM
	 */
	LYstrncpy(fbuffer, home, (fbuffer_size - 1));
	FREE(home);
	return;
    }
#ifdef VMS
    /*
     * Check whether we have a subdirectory path or just a filename.  - FM
     */
    if (!strncmp(file, "./", 2)) {
	/*
	 * We have a subdirectory path.  - FM
	 */
	if (home[strlen(home) - 1] == ']') {
	    /*
	     * We got the home directory, so convert it to SHELL syntax and
	     * append subdirectory path, then convert that to VMS syntax.  - FM
	     */
	    char *temp = NULL;

	    HTSprintf0(&temp, "%s%s", HTVMS_wwwName(home), (file + 1));
	    sprintf(fbuffer, "%.*s",
		    (fbuffer_size - 1), HTVMS_name("", temp));
	    FREE(temp);
	} else {
	    /*
	     * This will fail, but we need something in the buffer.  - FM
	     */
	    sprintf(fbuffer, "%s%.*s", home, len, file);
	}
    } else {
	/*
	 * We have a file in the home directory.  - FM
	 */
	sprintf(fbuffer, "%s%.*s", home, len, file);
    }
#else
    /*
     * Check whether we have a subdirectory path or just a filename.  - FM
     */
    sprintf(fbuffer, "%s/%.*s", home, len,
	    (strncmp(file, "./", 2) ? file : (file + 2)));
#endif /* VMS */
    FREE(home);
}

/*
 * Given a filename, concatenate it to the save-space pathname, unless it is
 * an absolute pathname.  If there is no save-space defined, use the home
 * directory. Return a new string with the result.
 */
char *LYAddPathToSave(char *fname)
{
    char *result = NULL;

    if (LYisAbsPath(fname)) {
	StrAllocCopy(result, fname);
    } else {
	if (lynx_save_space != NULL) {
	    StrAllocCopy(result, lynx_save_space);
	} else {
	    char temp[LY_MAXPATH];

	    LYAddPathToHome(temp, sizeof(temp), fname);
	    StrAllocCopy(result, temp);
	}
    }
    return result;
}

/*
 * This function takes a string in the format
 *	"Mon, 01-Jan-96 13:45:35 GMT" or
 *	"Mon,  1 Jan 1996 13:45:35 GMT"" or
 *	"dd-mm-yyyy"
 * as an argument, and returns its conversion to clock format (seconds since
 * 00:00:00 Jan 1 1970), or 0 if the string doesn't match the expected pattern. 
 * It also returns 0 if the time is in the past and the "absolute" argument is
 * FALSE.  It is intended for handling 'expires' strings in Version 0 cookies
 * homologously to 'max-age' strings in Version 1 cookies, for which 0 is the
 * minimum, and greater values are handled as '[max-age seconds] + time(NULL)'. 
 * If "absolute" if TRUE, we return the clock format value itself, but if
 * anything goes wrong when parsing the expected patterns, we still return 0. 
 * - FM
 */
time_t LYmktime(char *string,
		BOOL absolute)
{
    char *s;
    time_t now, clock2;
    int day, month, year, hour, minutes, seconds;
    char *start;
    char temp[8];

    /*
     * Make sure we have a string to parse.  - FM
     */
    if (!non_empty(string))
	return (0);
    s = string;
    CTRACE((tfp, "LYmktime: Parsing '%s'\n", s));

    /*
     * Skip any lead alphabetic "Day, " field and seek a numeric day field.  -
     * FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);

    /*
     * Get the numeric day and convert to an integer.  - FM
     */
    start = s;
    while (*s != '\0' && isdigit(UCH(*s)))
	s++;
    if (*s == '\0' || (s - start) > 2)
	return (0);
    LYstrncpy(temp, start, (int) (s - start));
    day = atoi(temp);
    if (day < 1 || day > 31)
	return (0);

    /*
     * Get the month string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isalnum(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);
    start = s;
    while (*s != '\0' && isalnum(UCH(*s)))
	s++;
    if ((*s == '\0') ||
	(s - start) < (isdigit(UCH(*(s - 1))) ? 2 : 3) ||
	(s - start) > (isdigit(UCH(*(s - 1))) ? 2 : 9))
	return (0);
    LYstrncpy(temp, start, (isdigit(UCH(*(s - 1))) ? 2 : 3));
    switch (TOUPPER(temp[0])) {
    case '0':
    case '1':
	month = atoi(temp);
	if (month < 1 || month > 12) {
	    return (0);
	}
	break;
    case 'A':
	if (!strcasecomp(temp, "Apr")) {
	    month = 4;
	} else if (!strcasecomp(temp, "Aug")) {
	    month = 8;
	} else {
	    return (0);
	}
	break;
    case 'D':
	if (!strcasecomp(temp, "Dec")) {
	    month = 12;
	} else {
	    return (0);
	}
	break;
    case 'F':
	if (!strcasecomp(temp, "Feb")) {
	    month = 2;
	} else {
	    return (0);
	}
	break;
    case 'J':
	if (!strcasecomp(temp, "Jan")) {
	    month = 1;
	} else if (!strcasecomp(temp, "Jun")) {
	    month = 6;
	} else if (!strcasecomp(temp, "Jul")) {
	    month = 7;
	} else {
	    return (0);
	}
	break;
    case 'M':
	if (!strcasecomp(temp, "Mar")) {
	    month = 3;
	} else if (!strcasecomp(temp, "May")) {
	    month = 5;
	} else {
	    return (0);
	}
	break;
    case 'N':
	if (!strcasecomp(temp, "Nov")) {
	    month = 11;
	} else {
	    return (0);
	}
	break;
    case 'O':
	if (!strcasecomp(temp, "Oct")) {
	    month = 10;
	} else {
	    return (0);
	}
	break;
    case 'S':
	if (!strcasecomp(temp, "Sep")) {
	    month = 9;
	} else {
	    return (0);
	}
	break;
    default:
	return (0);
    }

    /*
     * Get the numeric year string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0')
	return (0);
    start = s;
    while (*s != '\0' && isdigit(UCH(*s)))
	s++;
    if ((s - start) == 4) {
	LYstrncpy(temp, start, 4);
    } else if ((s - start) == 2) {
	now = time(NULL);
	/*
	 * Assume that received 2-digit dates >= 70 are 19xx; others
	 * are 20xx.  Only matters when dealing with broken software
	 * (HTTP server or web page) which is not Y2K compliant.  The
	 * line is drawn on a best-guess basis; it is impossible for
	 * this to be completely accurate because it depends on what
	 * the broken sender software intends.  (This totally breaks
	 * in 2100 -- setting up the next crisis...) - BL
	 */
	if (atoi(start) >= 70)
	    LYstrncpy(temp, "19", 2);
	else
	    LYstrncpy(temp, "20", 2);
	strncat(temp, start, 2);
	temp[4] = '\0';
    } else {
	return (0);
    }
    year = atoi(temp);

    /*
     * Get the numeric hour string and convert to an integer.  - FM
     */
    while (*s != '\0' && !isdigit(UCH(*s)))
	s++;
    if (*s == '\0') {
	hour = 0;
	minutes = 0;
	seconds = 0;
    } else {
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return (0);
	LYstrncpy(temp, start, (int) (s - start));
	hour = atoi(temp);

	/*
	 * Get the numeric minutes string and convert to an integer.  - FM
	 */
	while (*s != '\0' && !isdigit(UCH(*s)))
	    s++;
	if (*s == '\0')
	    return (0);
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return (0);
	LYstrncpy(temp, start, (int) (s - start));
	minutes = atoi(temp);

	/*
	 * Get the numeric seconds string and convert to an integer.  - FM
	 */
	while (*s != '\0' && !isdigit(UCH(*s)))
	    s++;
	if (*s == '\0')
	    return (0);
	start = s;
	while (*s != '\0' && isdigit(UCH(*s)))
	    s++;
	if (*s == '\0' || (s - start) > 2)
	    return (0);
	LYstrncpy(temp, start, (int) (s - start));
	seconds = atoi(temp);
    }

    /*
     * Convert to clock format (seconds since 00:00:00 Jan 1 1970), but then
     * zero it if it's in the past and "absolute" is not TRUE.  - FM
     */
    month -= 3;
    if (month < 0) {
	month += 12;
	year--;
    }
    day += (year - 1968) * 1461 / 4;
    day += ((((month * 153) + 2) / 5) - 672);
    clock2 = (time_t) ((day * 60 * 60 * 24) +
		       (hour * 60 * 60) +
		       (minutes * 60) +
		       seconds);
    if (absolute == FALSE && (long) (time((time_t *) 0) - clock2) >= 0)
	clock2 = (time_t) 0;
    if (clock2 > 0)
	CTRACE((tfp, "LYmktime: clock=%ld, ctime=%s",
		(long) clock2,
		ctime(&clock2)));

    return (clock2);
}

#if !defined(HAVE_PUTENV) && !defined(_WINDOWS)
/*
 * No putenv on the NeXT so we use this code instead!
 */

/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can  redistribute it and/or
modify it under the terms of the GNU Library General  Public License as
published by the Free Software Foundation; either  version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it  will be useful,
but WITHOUT ANY WARRANTY; without even the implied  warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library  General Public
License along with the GNU C Library; see the file  COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675  Mass Ave,
Cambridge, MA 02139, USA.  */

#if defined(STDC_HEADERS) || defined(USG)
#include <string.h>
#else /* Not (STDC_HEADERS or USG): */
#include <strings.h>
#endif /* STDC_HEADERS or USG */

#ifndef NULL
#define NULL 0
#endif /* !NULL */

extern char **environ;

/*
 * Put STRING, which is of the form "NAME=VALUE", in the environment.
 */
int putenv(const char *string)
{
    char *name_end = strchr(string, '=');
    register size_t size;
    register char **ep;

    if (name_end == NULL) {
	/* Remove the variable from the environment.  */
	size = strlen(string);
	for (ep = environ; *ep != NULL; ++ep)
	    if (!strncmp(*ep, string, size) && (*ep)[size] == '=') {
		while (ep[1] != NULL) {
		    ep[0] = ep[1];
		    ++ep;
		}
		*ep = NULL;
		return 0;
	    }
    }

    size = 0;
    for (ep = environ; *ep != NULL; ++ep)
	if (!strncmp(*ep, string, name_end - string) &&
	    (*ep)[name_end - string] == '=')
	    break;
	else
	    ++size;

    if (*ep == NULL) {
	static char **last_environ = NULL;
	char **new_environ = (char **) malloc((size + 2) * sizeof(char *));

	if (new_environ == NULL)
	    return -1;
	(void) memcpy((char *) new_environ, (char *) environ, size * sizeof(char *));

	new_environ[size] = (char *) string;
	new_environ[size + 1] = NULL;
	if (last_environ != NULL)
	    FREE(last_environ);
	last_environ = new_environ;
	environ = new_environ;
    } else
	*ep = (char *) string;

    return 0;
}
#endif /* !HAVE_PUTENV */

#ifdef NEED_REMOVE
int remove(char *name)
{
    return unlink(name);
}
#endif

#if defined(MULTI_USER_UNIX)
/*
 * Verify if this is really a file, not accessed by a link, except for the
 * special case of its directory being pointed to by a link from a directory
 * owned by root and not writable by other users.
 */
BOOL IsOurFile(const char *name)
{
    BOOL result = FALSE;
    struct stat data;

    if (lstat(name, &data) == 0
	&& S_ISREG(data.st_mode)
	&& (data.st_mode & (S_IWOTH | S_IWGRP)) == 0
	&& data.st_nlink == 1
	&& data.st_uid == getuid()) {
	int linked = FALSE;

	/*
	 * ( If this is not a single-user system, the other user is presumed by
	 * some people busy trying to use a symlink attack on our files ;-)
	 */
#if defined(HAVE_LSTAT)
	char *path = 0;
	char *leaf;

	StrAllocCopy(path, name);
	do {
	    if ((leaf = LYPathLeaf(path)) != path)
		*--leaf = '\0';	/* write a null on the '/' */
	    if (lstat(*path ? path : "/", &data) != 0) {
		break;
	    }
	    /*
	     * If we find a symbolic link, it has to be in a directory that's
	     * protected.  Otherwise someone could have switched it to point
	     * to one of the real user's files.
	     */
	    if (S_ISLNK(data.st_mode)) {
		linked = TRUE;	/* could be link-to-link; doesn't matter */
	    } else if (S_ISDIR(data.st_mode)) {
		if (linked) {
		    linked = FALSE;
		    /*
		     * We assume that a properly-configured system has the
		     * unwritable directories owned by root.  This is not
		     * necessarily so (bin, news, etc., may), but the only
		     * uid we can count on is 0.  It would be nice to add a
		     * check for the gid also, but that wouldn't be
		     * portable.
		     */
		    if (data.st_uid != 0
			|| (data.st_mode & S_IWOTH) != 0) {
			linked = TRUE;	/* force an error-return */
			break;
		    }
		}
	    } else if (linked) {
		break;
	    }
	} while (leaf != path);
	FREE(path);
#endif
	result = !linked;
    }
    CTRACE2(TRACE_CFG, (tfp, "IsOurFile(%s) %d\n", name, result));
    return result;
}

/*
 * Open a file that we don't want other users to see.
 */
static FILE *OpenHiddenFile(const char *name, const char *mode)
{
    FILE *fp = 0;
    struct stat data;
    BOOLEAN binary = strchr(mode, 'b') != 0;

#if defined(O_CREAT) && defined(O_EXCL)		/* we have fcntl.h or kindred? */
    /*
     * This is the preferred method for creating new files, since it ensures
     * that no one has an existing file or link that they happen to own.
     */
    if (*mode == 'w') {
	int fd = open(name, O_CREAT | O_EXCL | O_WRONLY, HIDE_CHMOD);

	if (fd < 0
	    && errno == EEXIST
	    && IsOurFile(name)) {
	    remove(name);
	    /* FIXME: there's a race at this point if directory is open */
	    fd = open(name, O_CREAT | O_EXCL | O_WRONLY, HIDE_CHMOD);
	}
	if (fd >= 0) {
#if defined(O_BINARY) && defined(__CYGWIN__)
	    if (binary)
		setmode(fd, O_BINARY);
#endif
	    fp = fdopen(fd, mode);
	}
    } else
#endif
    if (*mode == 'a') {
	if (IsOurFile(name)
	    && chmod(name, HIDE_CHMOD) == 0)
	    fp = fopen(name, mode);
	else if (lstat(name, &data) != 0)
	    fp = OpenHiddenFile(name, binary ? BIN_W : TXT_W);
	/*
	 * This is less stringent, but reasonably portable.  For new files, the
	 * umask will suffice; however if the file already exists we'll change
	 * permissions first, before opening it.  If the chmod fails because of
	 * some reason other than a non-existent file, there's no point in trying
	 * to open it.
	 *
	 * This won't work properly if the user is root, since the chmod succeeds.
	 */
    } else if (*mode != 'a') {
	mode_t save = umask(HIDE_UMASK);

	if (chmod(name, HIDE_CHMOD) == 0 || errno == ENOENT)
	    fp = fopen(name, mode);
	umask(save);
    }
    return fp;
}
#else
#define OpenHiddenFile(name, mode) fopen(name, mode)
#endif /* MULTI_USER_UNIX */

FILE *LYNewBinFile(const char *name)
{
#ifdef VMS
    FILE *fp = fopen(name, BIN_W, "mbc=32");

    chmod(name, HIDE_CHMOD);
#else
    FILE *fp = OpenHiddenFile(name, BIN_W);
#endif
    return fp;
}

FILE *LYNewTxtFile(const char *name)
{
    FILE *fp;

#ifdef VMS
    fp = fopen(name, TXT_W, "shr=get");
    chmod(name, HIDE_CHMOD);
#else
    SetDefaultMode(O_TEXT);

    fp = OpenHiddenFile(name, TXT_W);

    SetDefaultMode(O_BINARY);
#endif

    return fp;
}

FILE *LYAppendToTxtFile(const char *name)
{
    FILE *fp;

#ifdef VMS
    fp = fopen(name, TXT_A, "shr=get");
    chmod(name, HIDE_CHMOD);
#else
    SetDefaultMode(O_TEXT);

    fp = OpenHiddenFile(name, TXT_A);

    SetDefaultMode(O_BINARY);
#endif
    return fp;
}

#if defined(MULTI_USER_UNIX)
/*
 * Restore normal permissions to a copy of a file that we have created with
 * temp file restricted permissions.  The normal umask should apply for user
 * files.  - kw
 */
void LYRelaxFilePermissions(const char *name)
{
    mode_t mode;
    struct stat stat_buf;

    if (stat(name, &stat_buf) == 0 &&
	S_ISREG(stat_buf.st_mode) &&
	(mode = (stat_buf.st_mode & 0777)) == HIDE_CHMOD) {
	/*
	 * It looks plausible that this is a file we created with temp file
	 * paranoid permissions (and the umask wasn't even more restrictive
	 * when it was copied).  - kw
	 */
	mode_t save = umask(HIDE_UMASK);

	mode = ((mode & 0700) | 0066) & ~save;
	umask(save);
	chmod(name, mode);
    }
}
#endif

/*
 * Check if the given anchor has an associated file-cache.
 */
BOOLEAN LYCachedTemp(char *result,
		     char **cached)
{
    if (*cached) {
	LYstrncpy(result, *cached, LY_MAXPATH);
	FREE(*cached);
	if (LYCanReadFile(result)) {
	    remove(result);
	}
	return TRUE;
    }
    return FALSE;
}

#ifndef HAVE_MKDTEMP
#define mkdtemp(path) ((mktemp(path) != 0) && (mkdir(path, 0700) == 0))
#endif

/*
 * Open a temp-file, ensuring that it is unique, and not readable by other
 * users.
 *
 * The mode can be one of: "w", "a", "wb".
 */
FILE *LYOpenTemp(char *result,
		 const char *suffix,
		 const char *mode)
{
    FILE *fp = 0;
    BOOL txt = TRUE;
    char wrt = 'r';
    LY_TEMP *p;

    CTRACE((tfp, "LYOpenTemp(,%s,%s)\n", suffix, mode));
    if (result == 0)
	return 0;

    while (*mode != '\0') {
	switch (*mode++) {
	case 'w':
	    wrt = 'w';
	    break;
	case 'a':
	    wrt = 'a';
	    break;
	case 'b':
	    txt = FALSE;
	    break;
	default:
	    CTRACE((tfp, "%s @%d: BUG\n", __FILE__, __LINE__));
	    return 0;
	}
    }

    /*
     * Verify if the given space looks secure enough.  Otherwise, make a
     * secure subdirectory of that.
     */
#if defined(MULTI_USER_UNIX) && (defined(HAVE_MKTEMP) || defined(HAVE_MKDTEMP))
    if (lynx_temp_subspace == 0) {
	BOOL make_it = FALSE;
	struct stat sb;

	if (lstat(lynx_temp_space, &sb) == 0
	    && S_ISDIR(sb.st_mode)) {
	    if (sb.st_uid != getuid()
		|| (sb.st_mode & (S_IWOTH | S_IWGRP)) != 0) {
		make_it = TRUE;
		CTRACE((tfp,
			"lynx_temp_space is not our directory %s owner %d mode %03o\n",
			lynx_temp_space, (int) sb.st_uid, (int) sb.st_mode & 0777));
	    }
	} else {
	    make_it = TRUE;
	    CTRACE((tfp, "lynx_temp_space is not a directory %s\n", lynx_temp_space));
	}
	if (make_it) {
	    int old_mask = umask(HIDE_UMASK);

	    StrAllocCat(lynx_temp_space, "lynxXXXXXXXXXX");
	    if (mkdtemp(lynx_temp_space) == 0) {
		printf("%s: %s\n", lynx_temp_space, LYStrerror(errno));
		exit_immediately(EXIT_FAILURE);
	    }
	    umask(old_mask);
	    lynx_temp_subspace = 1;
	    StrAllocCat(lynx_temp_space, "/");
	    CTRACE((tfp, "made subdirectory %s\n", lynx_temp_space));
	} else {
	    lynx_temp_subspace = -1;
	}
    }
#endif

    do {
	if (!fmt_tempname(result, lynx_temp_space, suffix))
	    return 0;
	if (txt) {
	    switch (wrt) {
	    case 'w':
		fp = LYNewTxtFile(result);
		break;
	    case 'a':
		fp = LYAppendToTxtFile(result);
		break;
	    }
	} else {
	    fp = LYNewBinFile(result);
	}
	/*
	 * If we get a failure to make a temporary file, don't bother to try a
	 * different name unless the failure was because the file already
	 * exists.
	 */
#ifdef EEXIST			/* FIXME (need a better test) in fcntl.h or unistd.h */
	if ((fp == 0) && (errno != EEXIST)) {
	    CTRACE((tfp, "... LYOpenTemp(%s) failed: %s\n",
		    result, LYStrerror(errno)));
	    return 0;
	}
#endif
    } while (fp == 0);

    if ((p = typecalloc(LY_TEMP)) != 0) {
	p->next = ly_temp;
	StrAllocCopy((p->name), result);
	p->file = fp;
	p->outs = (wrt != 'r');
	ly_temp = p;
    } else {
	outofmem(__FILE__, "LYOpenTemp");
    }

    CTRACE((tfp, "... LYOpenTemp(%s)\n", result));
    return fp;
}

/*
 * Reopen a temporary file
 */
FILE *LYReopenTemp(char *name)
{
    LY_TEMP *p;
    FILE *fp = 0;

    LYCloseTemp(name);
    if ((p = FindTempfileByName(name)) != 0) {
	fp = p->file = LYAppendToTxtFile(name);
    }
    return fp;
}

/*
 * Open a temp-file for writing, possibly re-using a previously used
 * name and file.
 * If a non-empty fname is given, it is reused if it indicates a file
 * previously registered as a temp file and, in case the file still
 * exists, if it looks like we can write to it safely.  Otherwise a
 * new temp file (with new name) will be generated and returned in fname.
 *
 * File permissions are set so that the file is not readable by unprivileged
 * other users.
 *
 * Suffix is only used if fname is not being reused.
 * The mode should be "w", others are possible (they may be passed on)
 * but probably don't make sense. - kw
 */
FILE *LYOpenTempRewrite(char *fname,
			const char *suffix,
			const char *mode)
{
    FILE *fp = 0;
    BOOL txt = TRUE;
    char wrt = 'r';
    BOOL registered = NO;
    BOOL writable_exists = NO;
    BOOL is_ours = NO;
    BOOL still_open = NO;
    LY_TEMP *p;
    struct stat stat_buf;

    CTRACE((tfp, "LYOpenTempRewrite(%s,%s,%s)\n", fname, suffix, mode));
    if (*fname == '\0')		/* first time, no filename yet */
	return (LYOpenTemp(fname, suffix, mode));

    if ((p = FindTempfileByName(fname)) != 0) {
	registered = YES;
	if (p->file != 0)
	    still_open = YES;
	CTRACE((tfp, "...used before%s\n", still_open ? ", still open!" : "."));
    }

    if (registered) {
#ifndef NO_GROUPS
	writable_exists = HTEditable(fname);	/* existing, can write */
#define CTRACE_EXISTS "exists and is writable, "
#else
	writable_exists = (BOOL) (stat(fname, &stat_buf) == 0);		/* existing, assume can write */
#define CTRACE_EXISTS "exists, "
#endif

	if (writable_exists) {
	    is_ours = IsOurFile(fname);
	}
	CTRACE((tfp, "...%s%s\n",
		writable_exists ? CTRACE_EXISTS : "",
		is_ours ? "is our file." : "is NOT our file."));
    }

    /*
     * Note that in cases where LYOpenTemp is called as fallback below, we
     * don't call LYRemoveTemp first.  That may be appropriate in some cases,
     * but not trying to remove a weird existing file seems safer and could
     * help diagnose an unusual situation.  (They may be removed anyway later.)
     */
    if (still_open) {
	/*
	 * This should probably not happen.  Make a new one.
	 */
	return (LYOpenTemp(fname, suffix, mode));
    } else if (!registered) {
	/*
	 * Not registered.  It should have been registered at one point though,
	 * otherwise we wouldn't be called like this.
	 */
	return (LYOpenTemp(fname, suffix, mode));
    } else if (writable_exists && !is_ours) {
	/*
	 * File exists, writable if we checked, but something is wrong with it.
	 */
	return (LYOpenTemp(fname, suffix, mode));
#ifndef NO_GROUPS
    } else if (!is_ours && (lstat(fname, &stat_buf) == 0)) {
	/*
	 * Exists but not writable, and something is wrong with it.
	 */
	return (LYOpenTemp(fname, suffix, mode));
#endif
    }

    while (*mode != '\0') {
	switch (*mode++) {
	case 'w':
	    wrt = 'w';
	    break;
	case 'a':
	    wrt = 'a';
	    break;
	case 'b':
	    txt = FALSE;
	    break;
	default:
	    CTRACE((tfp, "%s @%d: BUG\n", __FILE__, __LINE__));
	    return fp;
	}
    }

    if (is_ours) {
	/*
	 * Yes, it exists, is writable if we checked, and everything looks ok
	 * so far.  This should be the most regular case.  - kw
	 */
#ifdef HAVE_TRUNCATE
	if (txt == TRUE) {	/* limitation of LYReopenTemp.  shrug */
	    /*
	     * We truncate and then append, this avoids having a small window
	     * in which the file doesn't exist.  - kw
	     */
	    if (truncate(fname, 0) != 0) {
		CTRACE((tfp, "... truncate(%s,0) failed: %s\n",
			fname, LYStrerror(errno)));
		return (LYOpenTemp(fname, suffix, mode));
	    } else {
		return (LYReopenTemp(fname));
	    }
	}
#endif
	remove(fname);

    }

    /* We come here in two cases:  either the file existed and was ours and we
     * just got rid of it.  Or the file did and does not exist, but is
     * registered as a temp file.  It must have been removed by some means
     * other than LYRemoveTemp.  In both cases, reuse the name!  - kw
     */

    if (txt) {
	switch (wrt) {
	case 'w':
	    fp = LYNewTxtFile(fname);
	    break;
	case 'a':
	    fp = LYAppendToTxtFile(fname);
	    break;
	}
    } else {
	fp = LYNewBinFile(fname);
    }
    p->file = fp;

    CTRACE((tfp, "... LYOpenTempRewrite(%s), %s\n", fname,
	    (fp) ? "ok" : "failed"));
    /*
     * We could fall back to trying LYOpenTemp() here in case of failure. 
     * After all the checks already done above a filure here should be pretty
     * unusual though, so maybe it's better to let the user notice that
     * something went wrong, and not try to fix it up.  - kw
     */
    return fp;
}

/*
 * Special case of LYOpenTemp, used for manipulating bookmark file, i.e., with
 * renaming.
 */
FILE *LYOpenScratch(char *result,
		    const char *prefix)
{
    FILE *fp;
    LY_TEMP *p;

    if (!fmt_tempname(result, prefix, HTML_SUFFIX))
	return 0;

    if ((fp = LYNewTxtFile(result)) != 0) {
	if ((p = typecalloc(LY_TEMP)) != 0) {
	    p->next = ly_temp;
	    StrAllocCopy((p->name), result);
	    p->file = fp;
	    ly_temp = p;
	} else {
	    outofmem(__FILE__, "LYOpenScratch");
	}
    }
    CTRACE((tfp, "LYOpenScratch(%s)\n", result));
    return fp;
}

static void LY_close_temp(LY_TEMP * p)
{
    if (p->file != 0) {
	if (p->outs) {
	    LYCloseOutput(p->file);
	} else {
	    LYCloseInput(p->file);
	}
	p->file = 0;
    }
}

/*
 * Close a temp-file, given its name
 */
void LYCloseTemp(char *name)
{
    LY_TEMP *p;

    CTRACE((tfp, "LYCloseTemp(%s)\n", name));
    if ((p = FindTempfileByName(name)) != 0) {
	CTRACE((tfp, "...LYCloseTemp(%s)%s\n", name,
		(p->file != 0) ? ", closed" : ""));
	LY_close_temp(p);
    }
}

/*
 * Close a temp-file, given its file-pointer
 */
void LYCloseTempFP(FILE *fp)
{
    LY_TEMP *p;

    CTRACE((tfp, "LYCloseTempFP\n"));
    if ((p = FindTempfileByFP(fp)) != 0) {
	LY_close_temp(p);
	CTRACE((tfp, "...LYCloseTempFP(%s)\n", p->name));
    }
}

/*
 * Close a temp-file, removing it.
 */
int LYRemoveTemp(char *name)
{
    LY_TEMP *p, *q;
    int code = -1;

    if (non_empty(name)) {
	CTRACE((tfp, "LYRemoveTemp(%s)\n", name));
	for (p = ly_temp, q = 0; p != 0; q = p, p = p->next) {
	    if (!strcmp(name, p->name)) {
		if (q != 0) {
		    q->next = p->next;
		} else {
		    ly_temp = p->next;
		}
		LY_close_temp(p);
		code = HTSYS_remove(name);
		CTRACE((tfp, "...LYRemoveTemp done(%d)%s\n", code,
			(p->file != 0) ? ", closed" : ""));
		CTRACE_FLUSH(tfp);
		FREE(p->name);
		FREE(p);
		break;
	    }
	}
    }
    return code;
}

/*
 * Remove all of the temp-files.  Note that this assumes that they are closed,
 * since some systems will not allow us to remove a file which is open.
 */
void LYCleanupTemp(void)
{
    while (ly_temp != 0) {
	LYRemoveTemp(ly_temp->name);
    }
#if defined(MULTI_USER_UNIX)
    if (lynx_temp_subspace > 0) {
	char result[LY_MAXPATH];

	LYstrncpy(result, lynx_temp_space, sizeof(result) - 1);
	LYTrimPathSep(result);
	CTRACE((tfp, "LYCleanupTemp removing %s\n", result));
	rmdir(result);
	lynx_temp_subspace = -1;
    }
#endif
}

/*
 * We renamed a temporary file.  Keep track so we can remove it on exit.
 */
void LYRenamedTemp(char *oldname,
		   char *newname)
{
    LY_TEMP *p;

    CTRACE((tfp, "LYRenamedTemp(old=%s, new=%s)\n", oldname, newname));
    if ((p = FindTempfileByName(oldname)) != 0) {
	StrAllocCopy((p->name), newname);
    }
}

#ifndef DISABLE_BIBP
/*
 * Check that bibhost defines the BibP icon.
 */
void LYCheckBibHost(void)
{
    DocAddress bibhostIcon;
    BOOLEAN saveFlag;

    bibhostIcon.address = NULL;
    StrAllocCopy(bibhostIcon.address, BibP_bibhost);
    StrAllocCat(bibhostIcon.address, "bibp1.0/bibpicon.jpg");
    bibhostIcon.post_data = NULL;
    bibhostIcon.post_content_type = NULL;
    bibhostIcon.bookmark = FALSE;
    bibhostIcon.isHEAD = FALSE;
    bibhostIcon.safe = FALSE;
    saveFlag = traversal;
    traversal = TRUE;		/* Hack to force error response. */
    BibP_bibhost_available = HTLoadAbsolute(&bibhostIcon) == YES;
    traversal = saveFlag;
    BibP_bibhost_checked = TRUE;
}
#endif /* !DISABLE_BIBP */

/*
 * Management of User Interface Pages.  - kw
 *
 * These are mostly temp files.  Pages which can be recognized by their special
 * URL (after having been loaded) need not be tracked here.
 *
 * First some private stuff:
 */
typedef struct uipage_entry {
    UIP_t type;
    unsigned flags;
    char *url;
    HTList *alturls;
    char *file;
} uip_entry;

#define UIP_F_MULTI	0x0001	/* flag: track multiple instances */
#define UIP_F_LIMIT	0x0002	/* flag: limit size of alturls list */
#define UIP_F_LMULTI   (UIP_F_MULTI | UIP_F_LIMIT)
/* *INDENT-OFF* */
static uip_entry ly_uip[] =
{
    { UIP_HISTORY		, UIP_F_LMULTI, NULL, NULL, NULL }
  , { UIP_DOWNLOAD_OPTIONS	, 0	      , NULL, NULL, NULL }
  , { UIP_PRINT_OPTIONS		, 0	      , NULL, NULL, NULL }
  , { UIP_SHOWINFO		, UIP_F_LMULTI, NULL, NULL, NULL }
  , { UIP_LIST_PAGE		, UIP_F_LMULTI, NULL, NULL, NULL }
  , { UIP_VLINKS		, UIP_F_LMULTI, NULL, NULL, NULL }
#if !defined(NO_OPTION_FORMS)
  , { UIP_OPTIONS_MENU		, UIP_F_LMULTI, NULL, NULL, NULL }
#endif
#ifdef DIRED_SUPPORT
  , { UIP_DIRED_MENU		, 0	      , NULL, NULL, NULL }
  , { UIP_PERMIT_OPTIONS	, 0	      , NULL, NULL, NULL }
  , { UIP_UPLOAD_OPTIONS	, UIP_F_LMULTI, NULL, NULL, NULL }
#endif
#ifdef EXP_ADDRLIST_PAGE
  , { UIP_ADDRLIST_PAGE		, UIP_F_LMULTI, NULL, NULL, NULL }
#endif
  , { UIP_LYNXCFG		, UIP_F_LMULTI, NULL, NULL, NULL }
#if !defined(NO_CONFIG_INFO)
  , { UIP_CONFIG_DEF		, UIP_F_LMULTI, NULL, NULL, NULL }
#endif
/* The following are not generated tempfiles: */
  , { UIP_TRACELOG		, 0	     , NULL, NULL, NULL }
#if defined(DIRED_SUPPORT) && defined(OK_INSTALL)
  , { UIP_INSTALL		, 0	     , NULL, NULL, NULL }
#endif

};
/* *INDENT-ON* */

/*  Public entry points for User Interface Page management: */

BOOL LYIsUIPage3(const char *url,
		 UIP_t type,
		 int flagparam)
{
    unsigned int i;
    size_t l;

    if (!url)
	return NO;
    for (i = 0; i < TABLESIZE(ly_uip); i++) {
	if (ly_uip[i].type == type) {
	    if (!ly_uip[i].url) {
		return NO;
	    } else if ((flagparam & UIP_P_FRAG) ?
		       (!strncmp(ly_uip[i].url, url, (l = strlen(ly_uip[i].url)))
			&& (url[l] == '\0' || url[l] == '#')) :
		       !strcmp(ly_uip[i].url, url)) {
		return YES;
	    } else if (ly_uip[i].flags & UIP_F_MULTI) {
		char *p;
		HTList *l0 = ly_uip[i].alturls;

		while ((p = (char *) HTList_nextObject(l0)) != NULL) {
		    if ((flagparam & UIP_P_FRAG) ?
			(!strncmp(p, url, (l = strlen(p)))
			 && (url[l] == '\0' || url[l] == '#')) :
			!strcmp(p, url))
			return YES;
		}
	    }
	    return NO;
	}
    }
    return NO;
}

void LYRegisterUIPage(const char *url,
		      UIP_t type)
{
    unsigned int i;

    for (i = 0; i < TABLESIZE(ly_uip); i++) {
	if (ly_uip[i].type == type) {
	    if (ly_uip[i].url && url &&
		!strcmp(ly_uip[i].url, url)) {

	    } else if (!ly_uip[i].url || !url ||
		       !(ly_uip[i].flags & UIP_F_MULTI)) {
		StrAllocCopy(ly_uip[i].url, url);

	    } else {
		char *p;
		int n = 0;
		HTList *l0 = ly_uip[i].alturls;

		while ((p = (char *) HTList_nextObject(l0)) != NULL) {
		    if (!strcmp(p, url))
			return;
		    if (!strcmp(p, ly_uip[i].url)) {
			StrAllocCopy(ly_uip[i].url, url);
			return;
		    }
		    n++;
		}
		if (!ly_uip[i].alturls)
		    ly_uip[i].alturls = HTList_new();

		if (n >= HTCacheSize && (ly_uip[i].flags & UIP_F_LIMIT))
		    HTList_removeFirstObject(ly_uip[i].alturls);
		HTList_addObject(ly_uip[i].alturls, ly_uip[i].url);
		ly_uip[i].url = NULL;
		StrAllocCopy(ly_uip[i].url, url);
	    }

	    return;
	}
    }
}

void LYUIPages_free(void)
{
    unsigned int i;

    for (i = 0; i < TABLESIZE(ly_uip); i++) {
	FREE(ly_uip[i].url);
	FREE(ly_uip[i].file);
	LYFreeStringList(ly_uip[i].alturls);
	ly_uip[i].alturls = NULL;
    }
}

/*
 * Convert local pathname to www name
 * (do not bother about file://localhost prefix at this point).
 */
const char *wwwName(const char *pathname)
{
    const char *cp = NULL;

#if defined(USE_DOS_DRIVES)
    cp = HTDOS_wwwName(pathname);
#else
#ifdef VMS
    cp = HTVMS_wwwName(pathname);
#else
    cp = pathname;
#endif /* VMS */
#endif

    return cp;
}

/*
 * Given a user-specified filename, e.g., for download or print, validate and
 * expand it.  Expand home-directory expressions in the given string.  Only
 * allow pipes if the user can spawn shell commands.
 *
 * Both strings are fixed buffer sizes, LY_MAXPATH.
 */
BOOLEAN LYValidateFilename(char *result,
			   char *given)
{
    char *cp;
    const char *cp2;

    /*
     * Cancel if the user entered "/dev/null" on Unix, or an "nl:" path on VMS. 
     * - FM
     */
    if (LYIsNullDevice(given)) {
	/* just ignore it */
	return FALSE;
    }
#ifdef HAVE_POPEN
    if (LYIsPipeCommand(given)) {
	if (no_shell) {
	    HTUserMsg(SPAWNING_DISABLED);
	    return FALSE;
	}
	LYstrncpy(result, given, LY_MAXPATH);
	return TRUE;
    }
#endif
    if ((cp = strchr(given, '~')) != 0
	&& (cp2 = wwwName(Home_Dir())) != 0
	&& strlen(cp2) + strlen(given) < LY_MAXPATH) {
	*(cp++) = '\0';
	strcpy(result, given);
	LYTrimPathSep(result);
	strcat(result, cp2);
	strcat(result, cp);
	strcpy(given, result);
    }
#ifdef VMS
    if (strchr(given, '/') != NULL) {
	strcpy(result, HTVMS_name("", given));
	strcpy(given, result);
    }
    if (given[0] != '/'
	&& strchr(given, ':') == NULL
	&& strlen(given) < LY_MAXPATH - 13) {
	strcpy(result, "sys$disk:");
	if (strchr(given, ']') == NULL)
	    strcat(result, "[]");
	strcat(result, given);
    } else {
	strcpy(result, given);
    }
#else

#ifndef __EMX__
    if (!LYisAbsPath(given)) {
#if defined(__DJGPP__) || defined(_WINDOWS)
	if (strchr(result, ':') != NULL)
	    cp = NULL;
	else
#endif /*  __DJGPP__ || _WINDOWS */
	{
#ifdef SUPPORT_CHDIR
	    static char buf[LY_MAXPATH];

	    cp = Current_Dir(buf);
#else
	    cp = original_dir;
#endif
	}
    } else
#endif /* __EMX__ */
	cp = NULL;

    *result = 0;
    if (cp) {
	LYTrimPathSep(cp);
	if (strlen(cp) >= LY_MAXPATH - 2)
	    return FALSE;
	sprintf(result, "%s/", cp);
    }
    cp = HTSYS_name(given);
    if (strlen(result) + strlen(cp) >= LY_MAXPATH - 1)
	return FALSE;
    strcat(result, cp);
#endif /* VMS */
    return TRUE;
}

/*
 * Given a valid filename, check if it exists.  If so, we'll have to worry
 * about overwriting it.
 *
 * Returns:
 *	'Y' (yes/success)
 *	'N' (no/retry)
 *	3   (cancel)
 */
int LYValidateOutput(char *filename)
{
    int c;

    /*
     * Assume we can write to a pipe
     */
#ifdef HAVE_POPEN
    if (LYIsPipeCommand(filename))
	return 'Y';
#endif

    if (no_dotfiles || !show_dotfiles) {
	if (*LYPathLeaf(filename) == '.') {
	    HTAlert(FILENAME_CANNOT_BE_DOT);
	    return 'N';
	}
    }

    /*
     * See if it already exists.
     */
    if (LYCanReadFile(filename)) {
#ifdef VMS
	c = HTConfirm(FILE_EXISTS_HPROMPT);
#else
	c = HTConfirm(FILE_EXISTS_OPROMPT);
#endif /* VMS */
	if (HTLastConfirmCancelled()) {
	    HTInfoMsg(SAVE_REQUEST_CANCELLED);
	    return 3;
	} else if (c == NO) {
	    return 'N';
	}
    }
    return 'Y';
}

/*
 * Convert a local filename to a URL
 */
void LYLocalFileToURL(char **target,
		      const char *source)
{
    const char *leaf;

    StrAllocCopy(*target, "file://localhost");

    leaf = wwwName(source);

    if (!LYisAbsPath(source)) {
	char temp[LY_MAXPATH];

	Current_Dir(temp);
	if (!LYIsHtmlSep(*temp))
	    LYAddHtmlSep(target);
	StrAllocCat(*target, temp);
    }
    if (!LYIsHtmlSep(*leaf))
	LYAddHtmlSep(target);
    StrAllocCat(*target, leaf);
}

/*
 * Open a temporary file for internal-pages, optionally reusing an existing
 * filename.
 */
FILE *InternalPageFP(char *filename,
		     int reuse_flag)
{
    FILE *fp;

    if (LYReuseTempfiles && reuse_flag) {
	fp = LYOpenTempRewrite(filename, HTML_SUFFIX, BIN_W);
    } else {
	LYRemoveTemp(filename);
	fp = LYOpenTemp(filename, HTML_SUFFIX, BIN_W);
    }
    if (fp == NULL) {
	HTAlert(CANNOT_OPEN_TEMP);
    }
    return fp;
}

/*
 * This part is shared by all internal pages.
 */
void WriteInternalTitle(FILE *fp0, const char *Title)
{
    fprintf(fp0,
	    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");

    fprintf(fp0, "<html>\n<head>\n");
    LYAddMETAcharsetToFD(fp0, -1);
    if (LYIsListpageTitle(Title)) {
	if (strchr(HTLoadedDocumentURL(), '"') == NULL) {
	    char *Address = NULL;

	    /*
	     * Insert a BASE tag so there is some way to relate the List Page
	     * file to its underlying document after we are done.  It won't be
	     * actually used for resolving relative URLs.  - kw
	     */
	    StrAllocCopy(Address, HTLoadedDocumentURL());
	    LYEntify(&Address, FALSE);
	    fprintf(fp0, "<base href=\"%s\">\n", Address);
	    FREE(Address);
	}
    }
    fprintf(fp0, "<title>%s</title>\n</head>\n<body>\n", Title);
}

/*
 * This is used to start most internal pages, except for special cases where
 * the embedded HREF's in the title differ.
 */
void BeginInternalPage(FILE *fp0, const char *Title,
		       const char *HelpURL)
{
    WriteInternalTitle(fp0, Title);

    if ((user_mode == NOVICE_MODE)
	&& LYwouldPush(Title, NULL)
	&& (HelpURL != 0)) {
	fprintf(fp0, "<h1>%s (%s%s%s), <a href=\"%s%s\">help</a></h1>\n",
		Title, LYNX_NAME, VERSION_SEGMENT, LYNX_VERSION,
		helpfilepath, HelpURL);
    } else {
	fprintf(fp0, "<h1>%s (%s%s%s)</h1>\n",
		Title, LYNX_NAME, VERSION_SEGMENT, LYNX_VERSION);
    }
}

void EndInternalPage(FILE *fp0)
{
    fprintf(fp0, "</body>\n</html>");
}

char *trimPoundSelector(char *address)
{
    char *pound = findPoundSelector(address);

    if (pound != 0)
	*pound = '\0';
    return pound;
}

/*
 * Trim a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to local filesystems.
 */
void LYTrimPathSep(char *path)
{
    size_t len;

    if (path != 0
	&& (len = strlen(path)) != 0
	&& LYIsPathSep(path[len - 1]))
	path[len - 1] = 0;
}

/*
 * Add a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to local filesystems.
 */
void LYAddPathSep(char **path)
{
    size_t len;
    char *temp;

    if ((path != 0)
	&& ((temp = *path) != 0)
	&& (len = strlen(temp)) != 0
	&& !LYIsPathSep(temp[len - 1])) {
	StrAllocCat(*path, PATHSEP_STR);
    }
}

/*
 * Add a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to local filesystems.
 */
void LYAddPathSep0(char *path)
{
    size_t len;

    if ((path != 0)
	&& (len = strlen(path)) != 0
	&& (len < LY_MAXPATH - 2)
	&& !LYIsPathSep(path[len - 1])) {
	strcat(path, PATHSEP_STR);
    }
}

/*
 * Check if a given string contains a path separator
 */
char *LYLastPathSep(const char *path)
{
    char *result;

#if defined(USE_DOS_DRIVES)
    if ((result = strrchr(path, '\\')) == 0)
	result = strrchr(path, '/');
#else
    result = strrchr(path, '/');
#endif
    return result;
}

/*
 * Trim a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to HTML paths.
 */
void LYTrimHtmlSep(char *path)
{
    size_t len;

    if (path != 0
	&& (len = strlen(path)) != 0
	&& LYIsHtmlSep(path[len - 1]))
	path[len - 1] = 0;
}

/*
 * Add a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to HTML paths.
 */
void LYAddHtmlSep(char **path)
{
    size_t len;
    char *temp;

    if ((path != 0)
	&& ((temp = *path) != 0)
	&& (len = strlen(temp)) != 0
	&& !LYIsHtmlSep(temp[len - 1])) {
	StrAllocCat(*path, "/");
    }
}

/*
 * Add a trailing path-separator to avoid confusing other programs when we concatenate
 * to it.  This only applies to HTML paths.
 */
void LYAddHtmlSep0(char *path)
{
    size_t len;

    if ((path != 0)
	&& (len = strlen(path)) != 0
	&& (len < LY_MAXPATH - 2)
	&& !LYIsHtmlSep(path[len - 1])) {
	strcat(path, "/");
    }
}

/*
 * Copy a file
 */
int LYCopyFile(char *src,
	       char *dst)
{
    int code;
    const char *program;

    if ((program = HTGetProgramPath(ppCOPY)) != NULL) {
	char *the_command = 0;

	HTAddParam(&the_command, COPY_COMMAND, 1, program);
	HTAddParam(&the_command, COPY_COMMAND, 2, src);
	HTAddParam(&the_command, COPY_COMMAND, 3, dst);
	HTEndParam(&the_command, COPY_COMMAND, 3);

	CTRACE((tfp, "command: %s\n", the_command));
	stop_curses();
	code = LYSystem(the_command);
	start_curses();

	FREE(the_command);
    } else {
	FILE *fin, *fout;
	unsigned char buff[BUFSIZ];
	int len;

	code = EOF;
	if ((fin = fopen(src, BIN_R)) != 0) {
	    if ((fout = fopen(dst, BIN_W)) != 0) {
		code = 0;
		while ((len = fread(buff, 1, sizeof(buff), fin)) > 0) {
		    fwrite(buff, 1, len, fout);
		    if (ferror(fout)) {
			code = EOF;
			break;
		    }
		}
		LYCloseOutput(fout);
	    }
	    LYCloseInput(fin);
	}
    }

    if (code) {
	HTAlert(CANNOT_WRITE_TO_FILE);
    }
    return code;
}

#ifdef __DJGPP__
static char *escape_backslashes(char *source)
{
    char *result = 0;
    int count = 0;
    int n;

    for (n = 0; source[n] != '\0'; ++n) {
	if (source[n] == '\\')
	    ++count;
    }
    if (count != 0) {
	result = malloc(count + n + 1);
	if (result != 0) {
	    int ch;
	    char *target = result;

	    while ((ch = *source++) != '\0') {
		if (ch == '\\')
		    *target++ = ch;
		*target++ = ch;
	    }
	    *target = '\0';
	}
    }
    return result;
}
#endif /* __DJGPP__ */
/*
 * Invoke a shell command, return nonzero on error.
 */
int LYSystem(char *command)
{
    int code;
    int do_free = 0;

#if defined(HAVE_SIGACTION) && defined(SIGTSTP) && !defined(USE_SLANG)
    struct sigaction saved_sigtstp_act;
    BOOLEAN sigtstp_saved = FALSE;
#endif
    int saved_errno = 0;

#ifdef __EMX__
    int scrsize[4];
#endif

    fflush(stdout);
    fflush(stderr);
    CTRACE((tfp, "LYSystem(%s)\n", command));
    CTRACE_FLUSH(tfp);

#ifdef __DJGPP__
    __djgpp_set_ctrl_c(0);
    _go32_want_ctrl_break(1);
#endif /* __DJGPP__ */

#ifdef VMS
    code = DCLsystem(command);
#else
#  ifdef __EMX__		/* FIXME: Should be LY_CONVERT_SLASH? */
    /* Configure writes commands which contain direct slashes.
       Native command-(non)-shell will not tolerate this. */
    {
	char *space = command, *slash = command;

	_scrsize(scrsize);
	while (*space && *space != ' ' && *space != '\t')
	    space++;
	while (slash < space && *slash != '/')
	    slash++;
	if (slash != space) {
	    char *old = command;

	    command = NULL;
	    StrAllocCopy(command, old);
	    do_free = 1;
	    slash = (slash - old) + command - 1;
	    space = (space - old) + command;
	    while (++slash < space)
		if (*slash == '/')
		    *slash = '\\';
	}
    }
#  endif

    /*
     * This chunk of code does not work, for two reasons:
     * a) the Cygwin system() function edits out the backslashes
     * b) it does not account for more than one parameter, e.g., +number
     */
#if defined(__CYGWIN__) && defined(DOSPATH)	/* 1999/02/26 (Fri) */
    {
	char cmd[LY_MAXPATH];
	char win32_name[LY_MAXPATH];
	char new_cmd[LY_MAXPATH];
	char new_command[LY_MAXPATH * 2 + 10];
	char *p, *q;

	p = command;
	q = cmd;
	while (*p) {
	    if (*p == ' ')
		break;
	    else
		*q = *p;
	    p++;
	    q++;
	}
	*q = '\0';

	if (cmd[0] == '/')
	    cygwin_conv_to_full_posix_path(cmd, new_cmd);
	else
	    strcpy(new_cmd, cmd);

	while (*p == ' ')
	    p++;

	if (strchr(p, '\\') == NULL) {
	    /* for Windows Application */
	    cygwin_conv_to_full_win32_path(p, win32_name);
	    sprintf(new_command, "%.*s \"%.*s\"",
		    LY_MAXPATH, new_cmd, LY_MAXPATH, win32_name);
	} else {
	    /* for DOS like editor */
	    q = win32_name;
	    while (*p) {
		if (*p == '\\') {
		    if (*(p + 1) == '\\')
			p++;
		}
		*q = *p;
		q++, p++;
	    }
	    *q = '\0';
	    sprintf(new_command, "%.*s %.*s", LY_MAXPATH, new_cmd, LY_MAXPATH, win32_name);
	}
	command = new_command;
    }
#endif

#ifdef __DJGPP__
    if (dj_is_bash) {
	char *new_command = escape_backslashes(command);

	if (new_command != 0) {
	    if (do_free)
		free(command);
	    command = new_command;
	}
    }
#endif /* __DJGPP__ */

#ifdef _WIN_CC
    code = exec_command(command, TRUE);		/* Wait exec */
#else /* !_WIN_CC */
#ifdef SIGPIPE
    if (restore_sigpipe_for_children)
	signal(SIGPIPE, SIG_DFL);	/* Some commands expect the default */
#endif
#if defined(HAVE_SIGACTION) && defined(SIGTSTP) && !defined(USE_SLANG)
    if (!dump_output_immediately && !LYCursesON && !no_suspend)
	sigtstp_saved = LYToggleSigDfl(SIGTSTP, &saved_sigtstp_act, 1);
#endif
    code = system(command);
    saved_errno = errno;
#if defined(HAVE_SIGACTION) && defined(SIGTSTP) && !defined(USE_SLANG)
    if (sigtstp_saved)
	LYToggleSigDfl(SIGTSTP, &saved_sigtstp_act, 0);
#endif
#ifdef SIGPIPE
    if (restore_sigpipe_for_children)
	signal(SIGPIPE, SIG_IGN);	/* Ignore it again - kw */
#endif
#endif
#endif

#ifdef __DJGPP__
    __djgpp_set_ctrl_c(1);
    _go32_want_ctrl_break(0);
#endif /* __DJGPP__ */

    fflush(stdout);
    fflush(stderr);

    if (do_free)
	FREE(command);
#if !defined(UCX) || !defined(VAXC)	/* errno not modifiable ?? */
    set_errno(saved_errno);	/* may have been clobbered */
#endif
#ifdef __EMX__			/* Check whether the screen size changed */
    size_change(0);
#endif
    return code;
}

/*
 * Return a string which can be used in LYSystem() for spawning a subshell
 */
#if defined(__CYGWIN__)		/* 1999/02/26 (Fri) */
int Cygwin_Shell(void)
{
    char *shell;
    int code;
    STARTUPINFO startUpInfo;
    PROCESS_INFORMATION procInfo;
    SECURITY_ATTRIBUTES sa;

    /* Set up security attributes to allow inheritance of the file handle */

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = 0;
    sa.bInheritHandle = TRUE;

    /* Init a startup structure */
    GetStartupInfo(&startUpInfo);

    shell = LYGetEnv("COMSPEC");

    /* Create the child process, specifying
       inherited handles. Pass the value of the
       handle as a command line parameter */
    code = 0;
    if (shell) {
	code = CreateProcess(0, shell, 0, 0,
			     TRUE, 0,
			     0, 0, &startUpInfo, &procInfo);

	if (!code) {
	    printf("shell = [%s], code = %ld\n", shell, GetLastError());
	}

	/* wait for the child to return (this is not a requirement
	   since the child is its own independent process) */
	WaitForSingleObject(procInfo.hProcess, INFINITE);
    }

    return code;
}
#endif

const char *LYSysShell(void)
{
    const char *shell = 0;

#ifdef DOSPATH
#ifdef WIN_EX
    shell = LYGetEnv("SHELL");
    if (shell) {
	if (access(shell, 0) != 0)
	    shell = LYGetEnv("COMSPEC");
    } else {
	shell = LYGetEnv("COMSPEC");
    }
    if (shell == NULL) {
	if (system_is_NT)
	    shell = "cmd.exe";
	else
	    shell = "command.com";
    }
#else
    shell = LYGetEnv("SHELL");
    if (shell == NULL) {
	shell = LYGetEnv("COMSPEC");
    }
    if (shell == NULL) {
	shell = "command.com";
    }
#endif /* WIN_EX */
#else
#ifdef __EMX__
    if (LYGetEnv("SHELL") != NULL) {
	shell = LYGetEnv("SHELL");
    } else {
	shell = (LYGetEnv("COMSPEC") == NULL) ? "cmd.exe" : LYGetEnv("COMSPEC");
    }
#else
#ifdef VMS
    shell = "";
#else
    shell = "exec $SHELL";
#endif /* __EMX__ */
#endif /* VMS */
#endif /* DOSPATH */
    return shell;
}

#ifdef VMS
#define DISPLAY "DECW$DISPLAY"
#else
#define DISPLAY "DISPLAY"
#endif /* VMS */

/*
 * Return the X-Window $DISPLAY string if it is nonnull/nonempty
 */
char *LYgetXDisplay(void)
{
    return LYGetEnv(DISPLAY);
}

/*
 * Set the value of the X-Window $DISPLAY variable (yes it leaks memory, but
 * that is putenv's fault).
 */
void LYsetXDisplay(char *new_display)
{
    if (new_display != 0) {
#ifdef VMS
	LYUpperCase(new_display);
	Define_VMSLogical(DISPLAY, new_display);
#else
	static char *display_putenv_command;

	HTSprintf0(&display_putenv_command, "DISPLAY=%s", new_display);
	putenv(display_putenv_command);
#endif /* VMS */
	if ((new_display = LYgetXDisplay()) != 0) {
	    StrAllocCopy(x_display, new_display);
	}
    }
}

#ifdef CAN_CUT_AND_PASTE
#ifdef __EMX__

static int proc_type = -1;
static PPIB pib;
static HAB hab;
static HMQ hmq;

static void morph_PM(void)
{
    PTIB tib;
    int first = 0;

    if (proc_type == -1) {
	DosGetInfoBlocks(&tib, &pib);
	proc_type = pib->pib_ultype;
	first = 1;
    }
    if (pib->pib_ultype != 3)	/* 2 is VIO */
	pib->pib_ultype = 3;	/* 3 is PM */
    if (first)
	hab = WinInitialize(0);
    /* 64 messages if before OS/2 3.0, ignored otherwise */
    hmq = WinCreateMsgQueue(hab, 64);
    WinCancelShutdown(hmq, 1);	/* Do not inform us on shutdown */
}

static void unmorph_PM(void)
{
    WinDestroyMsgQueue(hmq);
    pib->pib_ultype = proc_type;
}

int size_clip(void)
{
    return 8192;
}

/* Code partially stolen from FED editor. */

int put_clip(const char *s)
{
    int sz = strlen(s) + 1;
    int ret = EOF, nl = 0;
    char *pByte = 0, *s1 = s, c, *t;

    while ((c = *s1++)) {
	if (c == '\r' && *s1 == '\n')
	    s1++;
	else if (c == '\n')
	    nl++;
    }
    if (DosAllocSharedMem((PPVOID) & pByte, 0, sz + nl,
			  PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE | OBJ_GETTABLE))
	return ret;

    if (!nl)
	memcpy(pByte, s, sz);
    else {
	t = pByte;
	while ((c = *t++ = *s++))
	    if (c == '\n' && (t == pByte + 1 || t[-2] != '\r'))
		t[-1] = '\r', *t++ = '\n';
    }

    morph_PM();
    if (!hab)
	goto fail;

    WinOpenClipbrd(hab);
    WinEmptyClipbrd(hab);
    if (WinSetClipbrdData(hab, (ULONG) pByte, CF_TEXT, CFI_POINTER))
	ret = 0;
    WinCloseClipbrd(hab);
    unmorph_PM();
    if (ret == 0)
	return 0;
  fail:
    DosFreeMem((PPVOID) & pByte);
    return EOF;
}

static int clip_open;

/* get_clip_grab() returns a pointer to the string in the system area.
   get_clip_release() should be called ASAP after this. */

char *get_clip_grab(void)
{
    char *ClipData;
    ULONG ulFormat;
    int sz;

    morph_PM();
    if (!hab)
	return 0;
    if (clip_open)
	get_clip_release();

    WinQueryClipbrdFmtInfo(hab, CF_TEXT, &ulFormat);
    if (ulFormat != CFI_POINTER) {
	unmorph_PM();
	return 0;
    }
    WinOpenClipbrd(hab);
    clip_open = 1;
    ClipData = (char *) WinQueryClipbrdData(hab, CF_TEXT);
    sz = strlen(ClipData);
    if (!ClipData || !sz) {
	get_clip_release();
	return 0;
    }
    return ClipData;
}

void get_clip_release(void)
{
    if (!clip_open)
	return;
    WinCloseClipbrd(hab);
    clip_open = 0;
    unmorph_PM();
}

#else /* !( defined __EMX__ ) */

#  if !defined(WIN_EX) && defined(HAVE_POPEN)

static FILE *paste_handle = 0;
static char *paste_buf = NULL;

void get_clip_release(void)
{
    if (paste_handle != 0)
	pclose(paste_handle);
    if (paste_buf)
	FREE(paste_buf);
}

static int clip_grab(void)
{
    char *cmd = LYGetEnv("RL_PASTE_CMD");

    if (paste_handle)
	pclose(paste_handle);
    if (!cmd)
	return 0;

    paste_handle = popen(cmd, TXT_R);
    if (!paste_handle)
	return 0;
    return 1;
}

#define PASTE_BUFFER 1008
#define CF_TEXT 0		/* Not used */

char *get_clip_grab(void)
{
    int len;
    int size = PASTE_BUFFER;
    int off = 0;

    if (!clip_grab())
	return NULL;
    if (!paste_handle)
	return NULL;
    if (paste_buf)
	FREE(paste_buf);
    paste_buf = (char *) malloc(PASTE_BUFFER);
    while (1) {
	len = fread(paste_buf + off, 1, PASTE_BUFFER - 1, paste_handle);
	paste_buf[off + len] = '\0';
	if (len < PASTE_BUFFER - 1)
	    break;
	if (strchr(paste_buf + off, '\r')
	    || strchr(paste_buf + off, '\n'))
	    break;
	paste_buf = typeRealloc(char, paste_buf, size += PASTE_BUFFER - 1);

	off += len;
    }
    return paste_buf;
}

int put_clip(const char *s)
{
    char *cmd = LYGetEnv("RL_CLCOPY_CMD");
    FILE *fh;
    int l = strlen(s), res;

    if (!cmd)
	return -1;

    fh = popen(cmd, TXT_W);
    if (!fh)
	return -1;
    res = fwrite(s, 1, l, fh);
    if (pclose(fh) != 0 || res != l)
	return -1;
    return 0;
}

#  endif			/* !defined(WIN_EX) && defined(HAVE_POPEN) */

#endif /* __EMX__ */

/*
 * Sleep for a number of milli-sec.
 */
void LYmsec_delay(unsigned msec)
{
#if defined(_WINDOWS)
    Sleep(msec);

#elif defined(HAVE_NAPMS)
    napms(msec);

#elif defined(DJGPP) || defined(HAVE_USLEEP)
    usleep(1000 * msec);

#else
    struct timeval tv;
    unsigned long usec = 1000UL * msec;

    tv.tv_sec = usec / 1000000UL;
    tv.tv_usec = usec % 1000000UL;
    select(0, NULL, NULL, NULL, &tv);
#endif
}

#if defined(WIN_EX)		/* 1997/10/16 (Thu) 20:13:28 */

int put_clip(const char *szBuffer)
{
    HANDLE hWnd;
    HANDLE m_hLogData;
    LPTSTR pLogData;
    HANDLE hClip;
    int len;

    if (szBuffer == NULL)
	return EOF;

    len = strlen(szBuffer);
    if (len == 0)
	return EOF;
    else
	len++;

    m_hLogData = GlobalAlloc(GHND, len);
    if (m_hLogData == NULL) {
	return EOF;
    }

    hWnd = NULL;
    if (!OpenClipboard(hWnd)) {
	return EOF;
    }
    /* Remove the current Clipboard contents */
    if (!EmptyClipboard()) {
	GlobalFree(m_hLogData);
	return EOF;
    }

    /* Lock the global memory while we write to it. */
    pLogData = (LPTSTR) GlobalLock(m_hLogData);

    lstrcpy((LPTSTR) pLogData, szBuffer);
    GlobalUnlock(m_hLogData);

    /* If there were any lines at all then copy them to clipboard. */
    hClip = SetClipboardData(CF_TEXT, m_hLogData);
    if (!hClip) {
	/* If we couldn't clip the data then free the global handle. */
	GlobalFree(m_hLogData);
    }

    CloseClipboard();
    return 0;
}

static HANDLE m_hLogData;
static int m_locked;

/* get_clip_grab() returns a pointer to the string in the system area.
   get_clip_release() should be called ASAP after this. */

char *get_clip_grab()
{
    HANDLE hWnd;
    LPTSTR pLogData;

    hWnd = NULL;
    if (!OpenClipboard(hWnd)) {
	return 0;
    }

    m_hLogData = GetClipboardData(CF_TEXT);

    if (m_hLogData == NULL) {
	CloseClipboard();
	m_locked = 0;
	return 0;
    }
    pLogData = (LPTSTR) GlobalLock(m_hLogData);

    m_locked = 1;
    return pLogData;
}

void get_clip_release()
{
    if (!m_locked)
	return;
    GlobalUnlock(m_hLogData);
    CloseClipboard();
    m_locked = 0;
}
#endif /* WIN_EX */
#endif /* CAN_CUT_AND_PASTE */

#if defined(WIN_EX)

#ifndef WSABASEERR
#define WSABASEERR 10000
#endif

#ifdef ENABLE_IPV6
#define WSOCK_NAME  "ws2_32"
#else
#define WSOCK_NAME  "wsock32"
#endif

/*
 * Description: the windows32 version of perror()
 *
 * Returns:  a pointer to a static error
 *
 * Notes/Dependencies:  I got this from
 *	comp.os.ms-windows.programmer.win32
 */
char *w32_strerror(DWORD ercode)
{
/*  __declspec(thread) necessary if you will use multiple threads */
#ifdef __CYGWIN__
    static char msg_buff[256];

#else
    __declspec(thread) static char msg_buff[256];
#endif
    HMODULE hModule;
    int i, msg_type;
    unsigned char *p, *q, tmp_buff[256];
    DWORD rc;

    hModule = NULL;
    msg_type = FORMAT_MESSAGE_FROM_SYSTEM;
    /* Fill message buffer with a default message in
     * case FormatMessage fails
     */
    wsprintf(msg_buff, "Error %ld", ercode);

    /*
     * Special code for winsock error handling.
     */
    if (ercode > WSABASEERR) {
	hModule = GetModuleHandle(WSOCK_NAME);
	if (hModule)
	    msg_type = FORMAT_MESSAGE_FROM_HMODULE;
    }
    /*
     * message handling. If not found in module, retry from system.
     */
    rc = FormatMessage(msg_type, hModule, ercode, LANG_NEUTRAL,
		       msg_buff, sizeof(msg_buff), NULL);

    if (rc == 0 && msg_type == FORMAT_MESSAGE_FROM_HMODULE) {
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, ercode,
		      LANG_NEUTRAL, msg_buff, sizeof(msg_buff), NULL);
    }

    strcpy(tmp_buff, msg_buff);
    p = q = tmp_buff;
    i = 0;
    while (*p) {
	if (!(*p == '\n' || *p == '\r'))
	    msg_buff[i++] = *p;
	p++;
    }
    msg_buff[i] = '\0';

    return msg_buff;
}

#endif

#if defined(SYSLOG_REQUESTED_URLS)
/*
 * syslog() interface
 */
void LYOpenlog(const char *banner)
{
    if (syslog_requested_urls) {
	CTRACE((tfp, "LYOpenlog(%s)\n", NONNULL(banner)));
#if defined(DJGPP)
	openlog("lynx", LOG_PID | LOG_NDELAY, LOG_LOCAL5);
#else
	openlog("lynx", LOG_PID, LOG_LOCAL5);
#endif

	if (banner) {
	    syslog(LOG_INFO, "Session start:%s", banner);
	} else {
	    syslog(LOG_INFO, "Session start");
	}
    }
}

static BOOLEAN looks_like_password(char *first,
				   char *last)
{
    BOOLEAN result = FALSE;

    while (first <= last) {
	if (*first == '/'
	    || *first == ':') {
	    result = FALSE;
	    break;
	}
	result = TRUE;
	first++;
    }
    return result;
}

void LYSyslog(char *arg)
{
    char *colon1;
    char *colon2;
    char *atsign;

    if (syslog_requested_urls) {

	CTRACE((tfp, "LYSyslog %s\n", arg));

	if (is_url(arg)) {	/* proto://user:password@host/path:port */
	    /*      ^this colon                 */
	    if ((colon1 = strchr(arg, ':')) != 0
		&& !strncmp(colon1, "://", 3)
		&& (colon2 = strchr(colon1 + 3, ':')) != 0
		&& (atsign = strchr(colon1, '@')) != 0
		&& (colon2 < atsign)
		&& looks_like_password(colon2 + 1, atsign - 1)) {
		char *buf = NULL;

		StrAllocCopy(buf, arg);
		buf[colon2 - arg + 1] = 0;
		StrAllocCat(buf, "******");
		StrAllocCat(buf, atsign);
		syslog(LOG_INFO | LOG_LOCAL5, "%s", buf);
		CTRACE((tfp, "...alter %s\n", buf));
		FREE(buf);
		return;
	    }
	}
	syslog(LOG_INFO | LOG_LOCAL5, "%s", NONNULL(arg));
    }
}

void LYCloselog(void)
{
    if (syslog_requested_urls) {
	syslog(LOG_INFO, "Session over");
	closelog();
    }
}

#endif /* SYSLOG_REQUESTED_URLS */
