#include "HTUtils.h"
#include "tcp.h"
#include <ctype.h>
#include "HTParse.h"
#include "HTAccess.h"
#include "HTCJK.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYUtils.h"
#include "LYStrings.h"
#include "LYGlobalDefs.h"
#include "LYSignal.h"
#include "GridText.h"
#include "LYCharSets.h"

#ifdef DOSPATH
#include "HTDOS.h"
#endif
#ifdef VMS
#include <descrip.h>
#include <libclidef.h>
#include <lib$routines.h>
#include "HTVMSUtils.h"
#endif /* VMS */

#if HAVE_UTMP
#include <pwd.h>
#ifdef UTMPX_FOR_UTMP
#include <utmpx.h>
#define utmp utmpx
#ifdef UTMP_FILE
#undef UTMP_FILE
#endif /* UTMP_FILE */
#define UTMP_FILE UTMPX_FILE
#else
#include <utmp.h>
#endif /* UTMPX_FOR_UTMP */
#endif /* HAVE_UTMP */

#if NEED_PTEM_H
/* they neglected to define struct winsize in termios.h -- it's only in
 * termio.h and ptem.h (the former conflicts with other definitions).
 */
#include	<sys/stream.h>
#include	<sys/ptem.h>
#endif

#include "LYLeaks.h"

#ifdef USE_COLOR_STYLE
#include "AttrList.h"
#include "LYHash.h"
#include "LYStyle.h"
#endif

#undef hline   /* FIXME: this is a curses feature used as a variable here */

#ifdef SVR4_BSDSELECT
extern int BSDselect PARAMS((int nfds, fd_set * readfds, fd_set * writefds,
			     fd_set * exceptfds, struct timeval * timeout));
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

#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif /* !FD_SETSIZE */

#ifndef UTMP_FILE
#if defined(__FreeBSD__) || defined(__bsdi__)
#define UTMP_FILE _PATH_UTMP
#else
#define UTMP_FILE "/etc/utmp"
#endif /* __FreeBSD__ || __bsdi__ */
#endif /* !UTMP_FILE */

#define FREE(x) if (x) {free(x); x = NULL;}

extern HTkcode kanji_code;
extern BOOLEAN LYHaveCJKCharacterSet;
extern HTCJKlang HTCJK;

PRIVATE HTList * localhost_aliases = NULL;	/* Hosts to treat as local */
PRIVATE char *HomeDir = NULL;			/* HOME directory */
PUBLIC	HTList * sug_filenames = NULL;		/* Suggested filenames	 */

/*
 *  Highlight (or unhighlight) a given link.
 */
PUBLIC void highlight ARGS3(
	int,		flag,
	int,		cur,
	char *, 	target)
{
    char buffer[200];
    int i;
    char tmp[7], *cp;
    char *theData = NULL;
    char *Data = NULL;
    int Offset, HitOffset, tLen;
    int LenNeeded;
    BOOL TargetEmphasisON = FALSE;
    BOOL utf_flag = (LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8);

    tmp[0] = tmp[1] = tmp[2] = '\0';

    /*
     *	Bugs in the history code might cause -1 to be sent for cur, which
     *	yields a crash when LYstrncpy() is called with a nonsense pointer.
     *	As far as I know, such bugs have been squashed, but if they should
     *	reappear, this works around them. - FM
     */
    if (cur < 0)
	cur = 0;

    if (nlinks > 0) {
#ifdef USE_COLOR_STYLE
#define LXP (links[cur].lx)
#define LYP (links[cur].ly)
#endif
	move(links[cur].ly, links[cur].lx);
#ifndef USE_COLOR_STYLE
	lynx_start_link_color (flag == ON, links[cur].inUnderline);
#else
	if (flag == ON) {
	    LynxChangeStyle(s_alink, ABS_ON, 0);
	} else {
		/* the logic is flawed here - no provision is made for links that
		** aren't coloured as [s_a] by default - rjp
		*/
	    if (LYP >= 0 && LYP < CACHEH && LXP >= 0 && LXP < CACHEW &&
		cached_styles[LYP][LXP]) {
		LynxChangeStyle(cached_styles[LYP][LXP], ABS_ON, 0);
	    }
	    else {
		LynxChangeStyle(s_a, ABS_ON, 0);
	    }
	}
#endif

	if (links[cur].type == WWW_FORM_LINK_TYPE) {
	    int len;
	    int avail_space = (LYcols - links[cur].lx) - 1;

	    LYstrncpy(buffer,
		      (links[cur].hightext ?
		       links[cur].hightext : ""),
		      (avail_space > links[cur].form->size ?
				      links[cur].form->size : avail_space));
	    addstr(buffer);

	    len = strlen(buffer);
	    for (; len < links[cur].form->size && len < avail_space; len++)
		addch('_');

	} else {
	    /*
	     *	Copy into the buffer only what will fit
	     *	within the width of the screen.
	     */
	    LYmbcsstrncpy(buffer,
			  (links[cur].hightext ?
			   links[cur].hightext : ""),
			  (sizeof(buffer) - 1),
			  ((LYcols - 1) - links[cur].lx),
			  utf_flag);
	    addstr(buffer);
	}

	/*
	 *  Display a second line as well.
	 */
	if (links[cur].hightext2 && links[cur].ly < display_lines) {
	    lynx_stop_link_color (flag == ON, links[cur].inUnderline);
	    move((links[cur].ly + 1), links[cur].hightext2_offset);
#ifndef USE_COLOR_STYLE
	    lynx_start_link_color (flag == ON, links[cur].inUnderline);
#else
	    LynxChangeStyle(flag == ON ? s_alink : s_a, ABS_ON, 0);
#endif

	    for (i = 0; (tmp[0] = links[cur].hightext2[i]) != '\0' &&
			i+links[cur].hightext2_offset < LYcols; i++) {
		if (!IsSpecialAttrChar(links[cur].hightext2[i])) {
		    /*
		     *	For CJK strings, by Masanobu Kimura.
		     */
		    if (HTCJK != NOCJK && !isascii(tmp[0])) {
			tmp[1] = links[cur].hightext2[++i];
			addstr(tmp);
			tmp[1] = '\0';
		    } else {
			addstr(tmp);
		    }
		 }
	    }
	}
	lynx_stop_link_color (flag == ON, links[cur].inUnderline);

#if defined(FANCY_CURSES) || defined(USE_SLANG)
	/*
	 *  If we have an emphasized WHEREIS hit in the highlighted
	 *  text, restore the emphasis.  Note that we never emphasize
	 *  the first and last characters of the highlighted text when
	 *  we are making the link current, so the link attributes for
	 *  the current link will persist at the beginning and end,
	 *  providing an indication to the user that it has been made
	 *  current.   Also note that we use HText_getFirstTargetInLine()
	 *  to determine if there's a hit in the HText structure line
	 *  containing the link, and if so, get back a copy of the line
	 *  starting at that first hit (which might be before or after
	 *  our link), and with all IsSpecial characters stripped, so we
	 *  don't need to deal with them here. - FM
	 */
	if (target && *target && (links[cur].type & WWW_LINK_TYPE) &&
	    links[cur].hightext && *links[cur].hightext &&
	    HText_getFirstTargetInLine(HTMainText,
				       links[cur].anchor_line_num,
				       utf_flag,
				       (int *)&Offset,
				       (int *)&tLen,
				       (char **)&theData,
				       target)) {
	    int itmp, written, len, y, offset;
	    char *data;
	    int tlen = strlen(target);
	    int hlen, hLen;
	    int hLine = links[cur].ly, hoffset = links[cur].lx;
	    size_t utf_extra = 0;

	    /*
	     *	Copy into the buffer only what will fit
	     *	up to the right border of the screen. - FM
	     */
	    LYmbcsstrncpy(buffer,
			  (links[cur].hightext ?
			   links[cur].hightext : ""),
			  (sizeof(buffer) - 1),
			  ((LYcols - 1) - links[cur].lx),
			  utf_flag);
	    hlen = strlen(buffer);
	    hLen = ((HTCJK != NOCJK || utf_flag) ?
		  LYmbcsstrlen(buffer, utf_flag) : hlen);

	    /*
	     *	Break out if the first hit in the line
	     *	starts after this link. - FM
	     */
	    if (Offset >= (hoffset + hLen)) {
		goto highlight_search_hightext2;
	    }

	    /*
	     *	Recursively skip hits that end before this link, and
	     *	break out if there is no hit beyond those. - FM
	     */
	    Data = theData;
	    while ((Offset < hoffset) &&
		   ((Offset + tLen) <= hoffset)) {
		data = (Data + tlen);
		offset = (Offset + tLen);
		if ((case_sensitive ?
		     (cp = LYno_attr_mbcs_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL :
		     (cp = LYno_attr_mbcs_case_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL) &&
		    (offset + LenNeeded) < LYcols) {
		    Data = cp;
		    Offset = (offset + HitOffset);
		} else {
		    goto highlight_search_hightext2;
		}
	    }
	    data = buffer;
	    offset = hoffset;

	    /*
	     *	If the hit starts before the hightext, and ends
	     *	in or beyond the hightext, restore the emphasis,
	     *	skipping the first and last characters of the
	     *	hightext if we're making the link current. - FM
	     */
	    if ((Offset < offset) &&
		((Offset + tLen) > offset)) {
		itmp = 0;
		written = 0;
		len = (tlen - (offset - Offset));

		/*
		 *  Go to the start of the hightext and
		 *  handle its first character. - FM
		 */
		move(hLine, offset);
		tmp[0] = data[itmp];
		if (utf_flag && !isascii(tmp[0])) {
		    if ((*tmp & 0xe0) == 0xc0) {
			utf_extra = 1;
		    } else if ((*tmp & 0xf0) == 0xe0) {
			utf_extra = 2;
		    } else if ((*tmp & 0xf8) == 0xf0) {
			utf_extra = 3;
		    } else if ((*tmp & 0xfc) == 0xf8) {
			utf_extra = 4;
		    } else if ((*tmp & 0xfe) == 0xfc) {
			utf_extra = 5;
		    } else {
			/*
			 *  Garbage.
			 */
			utf_extra = 0;
		    }
		    if (strlen(&data[itmp+1]) < utf_extra) {
			/*
			 *  Shouldn't happen.
			 */
			utf_extra = 0;
		    }
		}
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		    itmp += utf_extra;
		    /*
		     *	Start emphasis immediately if we are
		     *	making the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		    /*
		     *	For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     *	Start emphasis immediately if we are
		     *	making the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     *	Start emphasis immediately if we are making
		     *	the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    written++;
		}
		itmp++;
		/*
		 *  Start emphasis after the first character
		 *  if we are making the link current and this
		 *  is not the last character. - FM
		 */
		if (!TargetEmphasisON &&
		    data[itmp] != '\0') {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		}

		/*
		 *  Handle the remaining characters. - FM
		 */
		for (;
		     written < len && (tmp[0] = data[itmp]) != '\0';
		     itmp++)  {
		    /*
		     *	Print all the other target chars, except
		     *	the last character if it is also the last
		     *	character of hightext and we are making
		     *	the link current. - FM
		     */
		    if (utf_flag && !isascii(tmp[0])) {
			if ((*tmp & 0xe0) == 0xc0) {
			    utf_extra = 1;
			} else if ((*tmp & 0xf0) == 0xe0) {
			    utf_extra = 2;
			} else if ((*tmp & 0xf8) == 0xf0) {
			    utf_extra = 3;
			} else if ((*tmp & 0xfc) == 0xf8) {
			    utf_extra = 4;
			} else if ((*tmp & 0xfe) == 0xfc) {
			    utf_extra = 5;
			} else {
			    /*
			     *	Garbage.
			     */
			    utf_extra = 0;
			}
			if (strlen(&data[itmp+1]) < utf_extra) {
			    /*
			     *	Shouldn't happen.
			     */
			    utf_extra = 0;
			}
		    }
		    if (utf_extra) {
			LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
			itmp += utf_extra;
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			tmp[1] = '\0';
			written += (utf_extra + 1);
			utf_extra = 0;
		    } else if (HTCJK != NOCJK && !isascii(tmp[0])) {
			/*
			 *  For CJK strings, by Masanobu Kimura.
			 */
			tmp[1] = data[++itmp];
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			tmp[1] = '\0';
			written += 2;
		    } else {
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			written++;
		    }
		}

		/*
		 *  Stop the emphasis if we haven't already, then
		 *  reset the offset to our current position in
		 *  the line, and if that is beyond the link, or
		 *  or we are making the link current and it is
		 *  the last character of the hightext, we are
		 *  done. - FM
		 */
		if (TargetEmphasisON) {
		    LYstopTargetEmphasis();
		    TargetEmphasisON = FALSE;
		}
		LYGetYX(y, offset);
		if (offset >=
		    (hoffset +
		     (flag == ON ? (hLen - 1) : hLen)))  {
		    goto highlight_search_hightext2;
		}

		/*
		 *  See if we have another hit that starts
		 *  within the hightext. - FM
		 */
		data = (Data + (offset - Offset));
		if (!utf_flag) {
		    data = Data + (offset - Offset);
		} else {
		    data = LYmbcs_skip_glyphs(Data,
					      (offset - Offset),
					      utf_flag);
		}
		if ((case_sensitive ?
		     (cp = LYno_attr_mbcs_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL :
		     (cp = LYno_attr_mbcs_case_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL) &&
		    (offset + LenNeeded) < LYcols) {
		    /*
		     *	If the hit starts after the end of the hightext,
		     *	or we are making the link current and the hit
		     *	starts at its last character, we are done. - FM
		     */
		    if ((HitOffset + offset) >=
			(hoffset +
			 (flag == ON ? (hLen - 1) : hLen)))  {
			goto highlight_search_hightext2;
		    }

		    /*
		     *	Set up the data and offset for the hit, and let
		     *	the code for within hightext hits handle it. - FM
		     */
		    Data = cp;
		    Offset = (offset + HitOffset);
		    data = buffer;
		    offset = hoffset;
		    goto highlight_hit_within_hightext;
		}
		goto highlight_search_hightext2;
	    }

highlight_hit_within_hightext:
	    /*
	     *	If we get to here, the hit starts within the
	     *	hightext.  If we are making the link current
	     *	and it's the last character in the hightext,
	     *	we are done.  Otherwise, move there and start
	     *	restoring the emphasis. - FM
	     */
	    if ((Offset - offset) >
		(flag == ON ? (hLen - 1) : hLen))  {
		goto highlight_search_hightext2;
	    }
	    if (!utf_flag) {
		data += (Offset - offset);
	    } else {
		refresh();
		data = LYmbcs_skip_glyphs(data,
					  (Offset - offset),
					  utf_flag);
	    }
	    offset = Offset;
	    itmp = 0;
	    written = 0;
	    len = tlen;

	    /*
	     *	Go to the start of the hit and
	     *	handle its first character. - FM
	     */
	    move(hLine, offset);
	    tmp[0] = data[itmp];
	    if (utf_flag && !isascii(tmp[0])) {
		if ((*tmp & 0xe0) == 0xc0) {
		    utf_extra = 1;
		} else if ((*tmp & 0xf0) == 0xe0) {
		    utf_extra = 2;
		} else if ((*tmp & 0xf8) == 0xf0) {
		    utf_extra = 3;
		} else if ((*tmp & 0xfc) == 0xf8) {
		    utf_extra = 4;
		} else if ((*tmp & 0xfe) == 0xfc) {
		    utf_extra = 5;
		} else {
		    /*
		     *	Garbage.
		     */
		    utf_extra = 0;
		}
		if (strlen(&data[itmp+1]) < utf_extra) {
		    /*
		     *	Shouldn't happen.
		     */
		    utf_extra = 0;
		}
	    }
	    if (utf_extra) {
		LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		itmp += utf_extra;
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		tmp[1] = '\0';
		written += (utf_extra + 1);
		utf_extra = 0;
	    } else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		/*
		 *  For CJK strings, by Masanobu Kimura.
		 */
		tmp[1] = data[++itmp];
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		tmp[1] = '\0';
		written += 2;
	    } else {
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		written++;
	    }
	    itmp++;
	    /*
	     *	Start emphasis after the first character
	     *	if we are making the link current and this
	     *	is not the last character. - FM
	     */
	    if (!TargetEmphasisON &&
		data[itmp] != '\0') {
		LYstartTargetEmphasis();
		TargetEmphasisON = TRUE;
	    }

	    for (;
		 written < len && (tmp[0] = data[itmp]) != '\0';
		 itmp++)  {
		/*
		 *  Print all the other target chars, except
		 *  the last character if it is also the last
		 *  character of hightext and we are making
		 *  the link current. - FM
		 */
		if (utf_flag && !isascii(tmp[0])) {
		    if ((*tmp & 0xe0) == 0xc0) {
			utf_extra = 1;
		    } else if ((*tmp & 0xf0) == 0xe0) {
			utf_extra = 2;
		    } else if ((*tmp & 0xf8) == 0xf0) {
			utf_extra = 3;
		    } else if ((*tmp & 0xfc) == 0xf8) {
			utf_extra = 4;
		    } else if ((*tmp & 0xfe) == 0xfc) {
			utf_extra = 5;
		    } else {
			/*
			 *  Garbage.
			 */
			utf_extra = 0;
		    }
		    if (strlen(&data[itmp+1]) < utf_extra) {
			/*
			 *  Shouldn't happen.
			 */
			utf_extra = 0;
		    }
		}
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		    itmp += utf_extra;
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		    /*
		     *	For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    written++;
		}
	    }

	    /*
	     *	Stop the emphasis if we haven't already, then reset
	     *	the offset to our current position in the line, and
	     *	if that is beyond the link, or we are making the link
	     *	current and it is the last character in the hightext,
	     *	we are done. - FM
	     */
	    if (TargetEmphasisON) {
		LYstopTargetEmphasis();
		TargetEmphasisON = FALSE;
	    }
	    LYGetYX(y, offset);
	    if (offset >=
		(hoffset + (flag == ON ? (hLen - 1) : hLen))) {
		goto highlight_search_hightext2;
	    }

	    /*
	     *	See if we have another hit that starts
	     *	within the hightext. - FM
	     */
	    if (!utf_flag) {
		data = Data + (offset - Offset);
	    } else {
		data = LYmbcs_skip_glyphs(Data,
					  (offset - Offset),
					  utf_flag);
	    }
	    if ((case_sensitive ?
		 (cp = LYno_attr_mbcs_strstr(data,
					     target,
					     utf_flag,
					     &HitOffset,
					     &LenNeeded)) != NULL :
		 (cp = LYno_attr_mbcs_case_strstr(data,
					     target,
					     utf_flag,
					     &HitOffset,
					     &LenNeeded)) != NULL) &&
		(offset + LenNeeded) < LYcols) {
		/*
		 *  If the hit starts after the end of the hightext,
		 *  or we are making the link current and the hit
		 *  starts at its last character, we are done. - FM
		 */
		if ((HitOffset + offset) >=
		    (hoffset +
		     (flag == ON ? (hLen - 1) : hLen)))  {
		    goto highlight_search_hightext2;
		}

		/*
		 *  If the target extends beyond our buffer, emphasize
		 *  everything in the hightext starting at this hit.
		 *  Otherwise, set up the data and offsets, and loop
		 *  back. - FM
		 */
		if ((HitOffset + (offset + tLen)) >=
		    (hoffset + hLen)) {
		    offset = (HitOffset + offset);
		    if (!utf_flag) {
			data = buffer + (offset - hoffset);
		    } else {
			refresh();
			data = LYmbcs_skip_glyphs(buffer,
						  (offset - hoffset),
						  utf_flag);
		    }
		    move(hLine, offset);
		    itmp = 0;
		    written = 0;
		    len = strlen(data);

		    /*
		     *	Turn the emphasis back on. - FM
		     */
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    for (;
			 written < len && (tmp[0] = data[itmp]) != '\0';
			 itmp++)  {
			/*
			 *  Print all the other target chars, except
			 *  the last character if it is also the last
			 *  character of hightext and we are making
			 *  the link current. - FM
			 */
			if (utf_flag && !isascii(tmp[0])) {
			    if ((*tmp & 0xe0) == 0xc0) {
				utf_extra = 1;
			    } else if ((*tmp & 0xf0) == 0xe0) {
				utf_extra = 2;
			    } else if ((*tmp & 0xf8) == 0xf0) {
				utf_extra = 3;
			    } else if ((*tmp & 0xfc) == 0xf8) {
				utf_extra = 4;
			    } else if ((*tmp & 0xfe) == 0xfc) {
				utf_extra = 5;
			    } else {
				/*
				 *  Garbage.
				 */
				utf_extra = 0;
			    }
			    if (strlen(&data[itmp+1]) < utf_extra) {
				/*
				 *  Shouldn't happen.
				 */
				utf_extra = 0;
			    }
			}
			if (utf_extra) {
			    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
			    itmp += utf_extra;
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
				LYGetYX(y, offset);
				move(hLine, (offset + 1));
			    } else {
				addstr(tmp);
			    }
			    tmp[1] = '\0';
			    written += (utf_extra + 1);
			    utf_extra = 0;
			} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
			    /*
			     *	For CJK strings, by Masanobu Kimura.
			     */
			    tmp[1] = data[++itmp];
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
			    } else {
				addstr(tmp);
			    }
			    tmp[1] = '\0';
			    written += 2;
			} else {
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
			    } else {
				addstr(tmp);
			    }
			    written++;
			}
		    }
		    /*
		     *	Turn off the emphasis if we haven't already,
		     *	and then we're done. - FM
		     */
		    if (TargetEmphasisON) {
			LYstopTargetEmphasis();
		    }
		    goto highlight_search_hightext2;
		} else {
		    Data = cp;
		    Offset = (offset + HitOffset);
		    data = buffer;
		    offset = hoffset;
		    goto highlight_hit_within_hightext;
		}
	    }
	    goto highlight_search_hightext2;
	}
highlight_search_hightext2:
	if (target && *target && (links[cur].type & WWW_LINK_TYPE) &&
	    links[cur].hightext2 && *links[cur].hightext2 &&
	    links[cur].ly < display_lines &&
	    HText_getFirstTargetInLine(HTMainText,
				       (links[cur].anchor_line_num + 1),
				       utf_flag,
				       (int *)&Offset,
				       (int *)&tLen,
				       (char **)&theData,
				       target)) {
	    int itmp, written, len, y, offset;
	    char *data;
	    int tlen = strlen(target);
	    int hlen, hLen;
	    int hLine = (links[cur].ly + 1);
	    int hoffset = links[cur].hightext2_offset;
	    size_t utf_extra = 0;

	    /*
	     *	Copy into the buffer only what will fit
	     *	up to the right border of the screen. - FM
	     */
	    LYmbcsstrncpy(buffer,
			  (links[cur].hightext2 ?
			   links[cur].hightext2 : ""),
			  (sizeof(buffer) - 1),
			  ((LYcols - 1) - links[cur].hightext2_offset),
			  utf_flag);
	    hlen = strlen(buffer);
	    hLen = ((HTCJK != NOCJK || utf_flag) ?
		  LYmbcsstrlen(buffer, utf_flag) : hlen);

	    /*
	     *	Break out if the first hit in the line
	     *	starts after this link. - FM
	     */
	    if (Offset >= (hoffset + hLen)) {
		goto highlight_search_done;
	    }

	    /*
	     *	Recursively skip hits that end before this link, and
	     *	break out if there is no hit beyond those. - FM
	     */
	    Data = theData;
	    while ((Offset < hoffset) &&
		   ((Offset + tLen) <= hoffset)) {
		data = (Data + tlen);
		offset = (Offset + tLen);
		if ((case_sensitive ?
		     (cp = LYno_attr_mbcs_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL :
		     (cp = LYno_attr_mbcs_case_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL) &&
		    (offset + LenNeeded) < LYcols) {
		    Data = cp;
		    Offset = (offset + HitOffset);
		} else {
		    goto highlight_search_done;
		}
	    }
	    data = buffer;
	    offset = hoffset;

	    /*
	     *	If the hit starts before the hightext2, and ends
	     *	in or beyond the hightext2, restore the emphasis,
	     *	skipping the first and last characters of the
	     *	hightext2 if we're making the link current. - FM
	     */
	    if ((Offset < offset) &&
		((Offset + tLen) > offset)) {
		itmp = 0;
		written = 0;
		len = (tlen - (offset - Offset));

		/*
		 *  Go to the start of the hightext2 and
		 *  handle its first character. - FM
		 */
		move(hLine, offset);
		tmp[0] = data[itmp];
		if (utf_flag && !isascii(tmp[0])) {
		    if ((*tmp & 0xe0) == 0xc0) {
			utf_extra = 1;
		    } else if ((*tmp & 0xf0) == 0xe0) {
			utf_extra = 2;
		    } else if ((*tmp & 0xf8) == 0xf0) {
			utf_extra = 3;
		    } else if ((*tmp & 0xfc) == 0xf8) {
			utf_extra = 4;
		    } else if ((*tmp & 0xfe) == 0xfc) {
			utf_extra = 5;
		    } else {
			/*
			 *  Garbage.
			 */
			utf_extra = 0;
		    }
		    if (strlen(&data[itmp+1]) < utf_extra) {
			/*
			 *  Shouldn't happen.
			 */
			utf_extra = 0;
		    }
		}
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		    itmp += utf_extra;
		    /*
		     *	Start emphasis immediately if we are
		     *	making the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		    /*
		     *	For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     *	Start emphasis immediately if we are
		     *	making the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     *	Start emphasis immediately if we are making
		     *	the link non-current. - FM
		     */
		    if (flag != ON) {
			LYstartTargetEmphasis();
			TargetEmphasisON = TRUE;
			addstr(tmp);
		    } else {
			move(hLine, (offset + 1));
		    }
		    written++;
		}
		itmp++;
		/*
		 *  Start emphasis after the first character
		 *  if we are making the link current and this
		 *  is not the last character. - FM
		 */
		if (!TargetEmphasisON &&
		    data[itmp] != '\0') {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		}

		/*
		 *  Handle the remaining characters. - FM
		 */
		for (;
		     written < len && (tmp[0] = data[itmp]) != '\0';
		     itmp++)  {
		    /*
		     *	Print all the other target chars, except
		     *	the last character if it is also the last
		     *	character of hightext2 and we are making
		     *	the link current. - FM
		     */
		    if (utf_flag && !isascii(tmp[0])) {
			if ((*tmp & 0xe0) == 0xc0) {
			    utf_extra = 1;
			} else if ((*tmp & 0xf0) == 0xe0) {
			    utf_extra = 2;
			} else if ((*tmp & 0xf8) == 0xf0) {
			    utf_extra = 3;
			} else if ((*tmp & 0xfc) == 0xf8) {
			    utf_extra = 4;
			} else if ((*tmp & 0xfe) == 0xfc) {
			    utf_extra = 5;
			} else {
			    /*
			     *	Garbage.
			     */
			    utf_extra = 0;
			}
			if (strlen(&data[itmp+1]) < utf_extra) {
			    /*
			     *	Shouldn't happen.
			     */
			    utf_extra = 0;
			}
		    }
		    if (utf_extra) {
			LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
			itmp += utf_extra;
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext2 if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			tmp[1] = '\0';
			written += (utf_extra + 1);
			utf_extra = 0;
		    } else if (HTCJK != NOCJK && !isascii(tmp[0])) {
			/*
			 *  For CJK strings, by Masanobu Kimura.
			 */
			tmp[1] = data[++itmp];
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext2 if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			tmp[1] = '\0';
			written += 2;
		    } else {
			/*
			 *  Make sure we don't restore emphasis to
			 *  the last character of hightext2 if we
			 *  are making the link current. - FM
			 */
			if (flag == ON && data[(itmp + 1)] == '\0') {
			    LYstopTargetEmphasis();
			    TargetEmphasisON = FALSE;
			    LYGetYX(y, offset);
			    move(hLine, (offset + 1));
			} else {
			    addstr(tmp);
			}
			written++;
		    }
		}

		/*
		 *  Stop the emphasis if we haven't already, then
		 *  reset the offset to our current position in
		 *  the line, and if that is beyond the link, or
		 *  or we are making the link current and it is
		 *  the last character of the hightext2, we are
		 *  done. - FM
		 */
		if (TargetEmphasisON) {
		    LYstopTargetEmphasis();
		    TargetEmphasisON = FALSE;
		}
		LYGetYX(y, offset);
		if (offset >=
		    (hoffset +
		     (flag == ON ? (hLen - 1) : hLen)))  {
		    goto highlight_search_done;
		}

		/*
		 *  See if we have another hit that starts
		 *  within the hightext2. - FM
		 */
		if (!utf_flag) {
		    data = Data + (offset - Offset);
		} else {
		    data = LYmbcs_skip_glyphs(Data,
					      (offset - Offset),
					      utf_flag);
		}
		if ((case_sensitive ?
		     (cp = LYno_attr_mbcs_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL :
		     (cp = LYno_attr_mbcs_case_strstr(data,
						 target,
						 utf_flag,
						 &HitOffset,
						 &LenNeeded)) != NULL) &&
		    (offset + LenNeeded) < LYcols) {
		    /*
		     *	If the hit starts after the end of the hightext2,
		     *	or we are making the link current and the hit
		     *	starts at its last character, we are done. - FM
		     */
		    if ((HitOffset + offset) >=
			(hoffset +
			 (flag == ON ? (hLen - 1) : hLen)))  {
			goto highlight_search_done;
		    }

		    /*
		     *	Set up the data and offset for the hit, and let
		     *	the code for within hightext2 hits handle it. - FM
		     */
		    Data = cp;
		    Offset = (offset + HitOffset);
		    data = buffer;
		    offset = hoffset;
		    goto highlight_hit_within_hightext2;
		}
		goto highlight_search_done;
	    }

highlight_hit_within_hightext2:
	    /*
	     *	If we get to here, the hit starts within the
	     *	hightext2.  If we are making the link current
	     *	and it's the last character in the hightext2,
	     *	we are done.  Otherwise, move there and start
	     *	restoring the emphasis. - FM
	     */
	    if ((Offset - offset) >
		(flag == ON ? (hLen - 1) : hLen))  {
		goto highlight_search_done;
	    }
	    if (!utf_flag) {
		data += (Offset - offset);
	    } else {
		refresh();
		data = LYmbcs_skip_glyphs(data,
					  (Offset - offset),
					  utf_flag);
	    }
	    offset = Offset;
	    itmp = 0;
	    written = 0;
	    len = tlen;

	    /*
	     *	Go to the start of the hit and
	     *	handle its first character. - FM
	     */
	    move(hLine, offset);
	    tmp[0] = data[itmp];
	    if (utf_flag && !isascii(tmp[0])) {
		if ((*tmp & 0xe0) == 0xc0) {
		    utf_extra = 1;
		} else if ((*tmp & 0xf0) == 0xe0) {
		    utf_extra = 2;
		} else if ((*tmp & 0xf8) == 0xf0) {
		    utf_extra = 3;
		} else if ((*tmp & 0xfc) == 0xf8) {
		    utf_extra = 4;
		} else if ((*tmp & 0xfe) == 0xfc) {
		    utf_extra = 5;
		} else {
		    /*
		     *	Garbage.
		     */
		    utf_extra = 0;
		}
		if (strlen(&data[itmp+1]) < utf_extra) {
		    /*
		     *	Shouldn't happen.
		     */
		    utf_extra = 0;
		}
	    }
	    if (utf_extra) {
		LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		itmp += utf_extra;
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext2. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		tmp[1] = '\0';
		written += (utf_extra + 1);
		utf_extra = 0;
	    } else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		/*
		 *  For CJK strings, by Masanobu Kimura.
		 */
		tmp[1] = data[++itmp];
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext2. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		tmp[1] = '\0';
		written += 2;
	    } else {
		/*
		 *  Start emphasis immediately if we are making
		 *  the link non-current, or we are making it
		 *  current but this is not the first or last
		 *  character of the hightext2. - FM
		 */
		if (flag != ON ||
		    (offset > hoffset && data[itmp+1] != '\0')) {
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    addstr(tmp);
		} else {
		    move(hLine, (offset + 1));
		}
		written++;
	    }
	    itmp++;
	    /*
	     *	Start emphasis after the first character
	     *	if we are making the link current and this
	     *	is not the last character. - FM
	     */
	    if (!TargetEmphasisON &&
		data[itmp] != '\0') {
		LYstartTargetEmphasis();
		TargetEmphasisON = TRUE;
	    }

	    for (;
		 written < len && (tmp[0] = data[itmp]) != '\0';
		 itmp++)  {
		/*
		 *  Print all the other target chars, except
		 *  the last character if it is also the last
		 *  character of hightext2 and we are making
		 *  the link current. - FM
		 */
		if (utf_flag && !isascii(tmp[0])) {
		    if ((*tmp & 0xe0) == 0xc0) {
			utf_extra = 1;
		    } else if ((*tmp & 0xf0) == 0xe0) {
			utf_extra = 2;
		    } else if ((*tmp & 0xf8) == 0xf0) {
			utf_extra = 3;
		    } else if ((*tmp & 0xfc) == 0xf8) {
			utf_extra = 4;
		    } else if ((*tmp & 0xfe) == 0xfc) {
			utf_extra = 5;
		    } else {
			/*
			 *  Garbage.
			 */
			utf_extra = 0;
		    }
		    if (strlen(&data[itmp+1]) < utf_extra) {
			/*
			 *  Shouldn't happen.
			 */
			utf_extra = 0;
		    }
		}
		if (utf_extra) {
		    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
		    itmp += utf_extra;
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext2 if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    tmp[1] = '\0';
		    written += (utf_extra + 1);
		    utf_extra = 0;
		} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
		    /*
		     *	For CJK strings, by Masanobu Kimura.
		     */
		    tmp[1] = data[++itmp];
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext2 if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    tmp[1] = '\0';
		    written += 2;
		} else {
		    /*
		     *	Make sure we don't restore emphasis to
		     *	the last character of hightext2 if we
		     *	are making the link current. - FM
		     */
		    if (flag == ON && data[(itmp + 1)] == '\0') {
			LYstopTargetEmphasis();
			TargetEmphasisON = FALSE;
			LYGetYX(y, offset);
			move(hLine, (offset + 1));
		    } else {
			addstr(tmp);
		    }
		    written++;
		}
	    }

	    /*
	     *	Stop the emphasis if we haven't already, then reset
	     *	the offset to our current position in the line, and
	     *	if that is beyond the link, or we are making the link
	     *	current and it is the last character in the hightext2,
	     *	we are done. - FM
	     */
	    if (TargetEmphasisON) {
		LYstopTargetEmphasis();
		TargetEmphasisON = FALSE;
	    }
	    LYGetYX(y, offset);
	    if (offset >=
		(hoffset + (flag == ON ? (hLen - 1) : hLen))) {
		goto highlight_search_done;
	    }

	    /*
	     *	See if we have another hit that starts
	     *	within the hightext2. - FM
	     */
	    if (!utf_flag) {
		data = (Data + (offset - Offset));
	    } else {
		data = LYmbcs_skip_glyphs(Data,
					  (offset - Offset),
					  utf_flag);
	    }
	    if ((case_sensitive ?
		 (cp = LYno_attr_mbcs_strstr(data,
					     target,
					     utf_flag,
					     &HitOffset,
					     &LenNeeded)) != NULL :
		 (cp = LYno_attr_mbcs_case_strstr(data,
					     target,
					     utf_flag,
					     &HitOffset,
					     &LenNeeded)) != NULL) &&
		(offset + LenNeeded) < LYcols) {
		/*
		 *  If the hit starts after the end of the hightext2,
		 *  or we are making the link current and the hit
		 *  starts at its last character, we are done. - FM
		 */
		if ((HitOffset + offset) >=
		    (hoffset +
		     (flag == ON ? (hLen - 1) : hLen)))  {
		    goto highlight_search_done;
		}

		/*
		 *  If the target extends beyond our buffer, emphasize
		 *  everything in the hightext2 starting at this hit.
		 *  Otherwise, set up the data and offsets, and loop
		 *  back. - FM
		 */
		if ((HitOffset + (offset + tLen)) >=
		    (hoffset + hLen)) {
		    offset = (HitOffset + offset);
		    if (!utf_flag) {
			data = buffer + (offset - hoffset);
		    } else {
			refresh();
			data = LYmbcs_skip_glyphs(buffer,
						  (offset - hoffset),
						  utf_flag);
		    }
		    move(hLine, offset);
		    itmp = 0;
		    written = 0;
		    len = strlen(data);

		    /*
		     *	Turn the emphasis back on. - FM
		     */
		    LYstartTargetEmphasis();
		    TargetEmphasisON = TRUE;
		    for (;
			 written < len && (tmp[0] = data[itmp]) != '\0';
			 itmp++)  {
			/*
			 *  Print all the other target chars, except
			 *  the last character if it is also the last
			 *  character of hightext2 and we are making
			 *  the link current. - FM
			 */
			if (utf_flag && !isascii(tmp[0])) {
			    if ((*tmp & 0xe0) == 0xc0) {
				utf_extra = 1;
			    } else if ((*tmp & 0xf0) == 0xe0) {
				utf_extra = 2;
			    } else if ((*tmp & 0xf8) == 0xf0) {
				utf_extra = 3;
			    } else if ((*tmp & 0xfc) == 0xf8) {
				utf_extra = 4;
			    } else if ((*tmp & 0xfe) == 0xfc) {
				utf_extra = 5;
			    } else {
				/*
				 *  Garbage.
				 */
				utf_extra = 0;
			    }
			    if (strlen(&data[itmp+1]) < utf_extra) {
				/*
				 *  Shouldn't happen.
				 */
				utf_extra = 0;
			    }
			}
			if (utf_extra) {
			    LYstrncpy(&tmp[1], &data[itmp+1], utf_extra);
			    itmp += utf_extra;
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext2 if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
				LYGetYX(y, offset);
				move(hLine, (offset + 1));
			    } else {
				addstr(tmp);
			    }
			    tmp[1] = '\0';
			    written += (utf_extra + 1);
			    utf_extra = 0;
			} else if (HTCJK != NOCJK && !isascii(tmp[0])) {
			    /*
			     *	For CJK strings, by Masanobu Kimura.
			     */
			    tmp[1] = data[++itmp];
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext2 if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
			    } else {
				addstr(tmp);
			    }
			    tmp[1] = '\0';
			    written += 2;
			} else {
			    /*
			     *	Make sure we don't restore emphasis to
			     *	the last character of hightext2 if we
			     *	are making the link current. - FM
			     */
			    if (flag == ON && data[(itmp + 1)] == '\0') {
				LYstopTargetEmphasis();
				TargetEmphasisON = FALSE;
			    } else {
				addstr(tmp);
			    }
			    written++;
			}
		    }
		    /*
		     *	Turn off the emphasis if we haven't already,
		     *	and then we're done. - FM
		     */
		    if (TargetEmphasisON) {
			LYstopTargetEmphasis();
		    }
		    goto highlight_search_done;
		} else {
		    Data = cp;
		    Offset = (offset + HitOffset);
		    data = buffer;
		    offset = hoffset;
		    goto highlight_hit_within_hightext2;
		}
	    }
	    goto highlight_search_done;
	}
highlight_search_done:
	FREE(theData);

	if (!LYShowCursor)
	    /*
	     *	Get cursor out of the way.
	     */
	    move((LYlines - 1), (LYcols - 1));
	else
#endif /* FANCY CURSES || USE_SLANG */
	    /*
	     *	Never hide the cursor if there's no FANCY CURSES or SLANG.
	     */
	    move(links[cur].ly,
		 ((links[cur].lx > 0) ? (links[cur].lx - 1) : 0));

	if (flag)
	    refresh();
    }
    return;
}

/*
 *  free_and_clear will free a pointer if it
 *  is non-zero and then set it to zero.
 */
PUBLIC void free_and_clear ARGS1(
	char **,	pointer)
{
    if (*pointer) {
	free(*pointer);
	*pointer = 0;
    }
    return;
}

/*
 *  Collapse (REMOVE) all spaces in the string.
 */
PUBLIC void collapse_spaces ARGS1(
	char *, 	string)
{
    int i=0;
    int j=0;

    if (!string)
	return;

    for (; string[i] != '\0'; i++)
	if (!isspace((unsigned char)string[i]))
	    string[j++] = string[i];

    string[j] = '\0';  /* terminate */
    return;
}

/*
 *  Convert single or serial newlines to single spaces throughout a string
 *  (ignore newlines if the preceding character is a space) and convert
 *  tabs to single spaces.  Don't ignore any explicit tabs or spaces if
 *  the condense argument is FALSE, otherwise, condense any serial spaces
 *  or tabs to one space. - FM
 */
PUBLIC void convert_to_spaces ARGS2(
	char *, 	string,
	BOOL,		condense)
{
    char *s = string;
    char *ns = string;
    BOOL last_is_space = FALSE;

    if (!string)
	return;

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
 *  Strip trailing slashes from directory paths.
 */
PUBLIC char * strip_trailing_slash ARGS1(
	char *, 	dirname)
{
    int i;

    i = strlen(dirname) - 1;
    while (i >= 0 && dirname[i] == '/')
	dirname[i--] = '\0';
    return(dirname);
}

/*
 *  Display (or hide) the status line.
 */
BOOLEAN mustshow = FALSE;

PUBLIC void statusline ARGS1(
	CONST char *,	text)
{
    char buffer[256];
    unsigned char *temp = NULL;
    int max_length, len, i, j;
    unsigned char k;

    if (text == NULL)
	return;

    /*
     *	Don't print statusline messages if dumping to stdout.
     */
    if (dump_output_immediately)
	return;

    /*
     *	Don't print statusline message if turned off.
     */
    if (mustshow != TRUE) {
	if (no_statusline == TRUE) {
	    return;
	}
    }
    mustshow = FALSE;

    /*
     *	Deal with any CJK escape sequences and Kanji if we have a CJK
     *	character set selected, otherwise, strip any escapes.  Also,
     *	make sure text is not longer than the statusline window. - FM
     */
    max_length = ((LYcols - 2) < 256) ? (LYcols - 2) : 255;
    if ((text[0] != '\0') &&
	(LYHaveCJKCharacterSet)) {
	/*
	 *  Translate or filter any escape sequences. - FM
	 */
	if ((temp = (unsigned char *)calloc(1, strlen(text) + 1)) == NULL)
	    outofmem(__FILE__, "statusline");
	if (kanji_code == EUC) {
	    TO_EUC((unsigned char *)text, temp);
	} else if (kanji_code == SJIS) {
	    TO_SJIS((unsigned char *)text, temp);
	} else {
	    for (i = 0, j = 0; text[i]; i++) {
		if (text[i] != '\033') {
		    temp[j++] = text[i];
		}
	    }
	    temp[j] = '\0';
	}

	/*
	 *  Deal with any newlines or tabs in the string. - FM
	 */
	convert_to_spaces((char *)temp, FALSE);

	/*
	 *  Handle the Kanji, making sure the text is not
	 *  longer than the statusline window. - FM
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
	 *  Strip any escapes, and shorten text if necessary.  Note
	 *  that we don't deal with the possibility of UTF-8 characters
	 *  in the string.  This is unlikely, but if strings with such
	 *  characters are used in LYMessages_en.h, a compilation
	 *  symbol of HAVE_UTF8_STATUSLINES could be added there, and
	 *  code added here for determining the displayed string length,
	 *  as we do above for CJK. - FM
	 */
	for (i = 0, len = 0; text[i] != '\0' && len < max_length; i++) {
	    if (text[i] != '\033') {
		buffer[len++] = text[i];
	    }
	}
	buffer[len] = '\0';
	/*
	 *  Deal with any newlines or tabs in the string. - FM
	 */
	convert_to_spaces(buffer, FALSE);
    }

    /*
     *	Move to the desired statusline window and
     *	output the text highlighted. - FM
     */
    if (LYStatusLine >= 0) {
	if (LYStatusLine < LYlines-1) {
	    move(LYStatusLine, 0);
	} else {
	    move(LYlines-1, 0);
	}
    } else if (user_mode == NOVICE_MODE) {
	move(LYlines-3, 0);
    } else {
	move(LYlines-1, 0);
    }
    clrtoeol();
    if (text != NULL && text[0] != '\0') {
#ifdef HAVE_UTF8_STATUSLINES
	if (LYCharSet_UC[current_char_set].enc == UCT_ENC_UTF8) {
	    refresh();
	}
#endif /* HAVE_UTF8_STATUSLINES */
#ifndef USE_COLOR_STYLE
	lynx_start_status_color ();
	addstr (buffer);
	lynx_stop_status_color ();
#else
	/* draw the status bar in the STATUS style */
	{
		int a=(strncmp(buffer, "Alert", 5) || !hashStyles[s_alert].name ? s_status : s_alert);
		LynxChangeStyle (a, ABS_ON, 1);
		addstr(buffer);
		wbkgdset(stdscr,
			 ((lynx_has_color && LYShowColor >= SHOW_COLOR_ON)
			  ? hashStyles[a].color
			  :A_NORMAL) | ' ');
		clrtoeol();
		if (s_normal != NOSTYLE)
		    wbkgdset(stdscr, hashStyles[s_normal].color | ' ');
		else
		    wbkgdset(stdscr,
			     ((lynx_has_color && LYShowColor >= SHOW_COLOR_ON)
			      ? displayStyles[DSTYLE_NORMAL].color
			      : A_NORMAL) | ' ');
		LynxChangeStyle (a, ABS_OFF, 0);
	}
#endif
    }
    refresh();

    return;
}

static char *novice_lines[] = {
#ifndef NOVICE_LINE_TWO_A
#define NOVICE_LINE_TWO_A	NOVICE_LINE_TWO
#define NOVICE_LINE_TWO_B	""
#define NOVICE_LINE_TWO_C	""
#endif /* !NOVICE_LINE_TWO_A */
  NOVICE_LINE_TWO_A,
  NOVICE_LINE_TWO_B,
  NOVICE_LINE_TWO_C,
  ""
};
static int lineno = 0;

PUBLIC void toggle_novice_line NOARGS
{
	lineno++;
	if (*novice_lines[lineno] == '\0')
		lineno = 0;
	return;
}

PUBLIC void noviceline ARGS1(
	int,		more_flag GCC_UNUSED)
{

    if (dump_output_immediately)
	return;

    move(LYlines-2,0);
    /* stop_reverse(); */
    clrtoeol();
    addstr(NOVICE_LINE_ONE);
    clrtoeol();

#if defined(DIRED_SUPPORT ) && defined(OK_OVERRIDE)
    if (lynx_edit_mode && !no_dired_support)
       addstr(DIRED_NOVICELINE);
    else
#endif /* DIRED_SUPPORT && OK_OVERRIDE */

    if (LYUseNoviceLineTwo)
	addstr(NOVICE_LINE_TWO);
    else
	addstr(novice_lines[lineno]);

#ifdef NOTDEFINED
    if (is_www_index && more_flag) {
	addstr("This is a searchable index.  Use ");
	addstr(key_for_func(LYK_INDEX_SEARCH));
	addstr(" to search:");
	stop_reverse();
	addstr("                ");
	start_reverse();
	addstr("space for more");

    } else if (is_www_index) {
	addstr("This is a searchable index.  Use ");
	addstr(key_for_func(LYK_INDEX_SEARCH));
	addstr(" to search:");
    } else {
	addstr("Type a command or ? for help:");

	if (more_flag) {
	    stop_reverse();
	    addstr("                       ");
	    start_reverse();
	    addstr("Press space for next page");
	}
    }

#endif /* NOTDEFINED */

    refresh();
    return;
}

PRIVATE int fake_zap = 0;

PUBLIC void LYFakeZap ARGS1(
    BOOL,	set)
{
    if (set && fake_zap < 1) {
	if (TRACE) {
	    fprintf(stderr, "\r *** Set simulated 'Z'");
	    if (fake_zap)
		fprintf(stderr, ", %d pending", fake_zap);
	    fprintf(stderr, " ***\n");
	}
	fake_zap++;
    } else if (!set && fake_zap) {
	if (TRACE) {
	    fprintf(stderr, "\r *** Unset simulated 'Z'");
	    fprintf(stderr, ", %d pending", fake_zap);
	    fprintf(stderr, " ***\n");
	}
	fake_zap = 0;
    }

}

PUBLIC int HTCheckForInterrupt NOARGS
{
#ifndef VMS /* UNIX stuff: */
    int c;
#ifndef USE_SLANG
    struct timeval socket_timeout;
    int ret = 0;
    fd_set readfds;
#endif /* !USE_SLANG */

    if (fake_zap > 0) {
	fake_zap--;
	if (TRACE) {
	    fprintf(stderr, "\r *** Got simulated 'Z' ***\n");
	    fflush(stderr);
	    if (!LYTraceLogFP)
		sleep(AlertSecs);
	}
	return((int)TRUE);
    }

    /** Curses or slang setup was not invoked **/
    if (dump_output_immediately)
	return((int)FALSE);

#ifdef USE_SLANG
    /** No keystroke was entered
	Note that this isn't taking possible SOCKSification
	and the socks_flag into account, and may fail on the
	slang library's select() when SOCKSified. - FM **/
    if (0 == SLang_input_pending(0))
	return(FALSE);

#else /* Unix curses: */

    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 100;
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
#ifdef SOCKS
    if (socks_flag)
	ret = Rselect(1, (void *)&readfds, NULL, NULL,
		      &socket_timeout);
    else
#endif /* SOCKS */
	ret = select(1, (void *)&readfds, NULL, NULL,
		     &socket_timeout);

    /** Suspended? **/
    if ((ret == -1) && (errno == EINTR))
	 return((int)FALSE);

    /** No keystroke was entered? **/
    if (!FD_ISSET(0,&readfds))
	 return((int)FALSE);
#endif /* USE_SLANG */

    /** Keyboard 'Z' or 'z', or Control-G or Control-C **/
#if defined (DOSPATH) && defined (NCURSES)
    nodelay(stdscr,TRUE);
#endif /* DOSPATH */
    c = LYgetch();
#if defined (DOSPATH) && defined (NCURSES)
    nodelay(stdscr,FALSE);
#endif /* DOSPATH */
    if (TOUPPER(c) == 'Z' || c == 7 || c == 3)
	return((int)TRUE);

    /** Other keystrokes **/
    return((int)FALSE);

#else /* VMS: */

    int c;
    extern BOOLEAN HadVMSInterrupt;
    extern int typeahead();

    if (fake_zap > 0) {
	fake_zap--;
	if (TRACE) {
	    fprintf(stderr, "\r *** Got simulated 'Z' ***\n");
	    fflush(stderr);
	    if (!LYTraceLogFP)
		sleep(AlertSecs);
	}
	return((int)TRUE);
    }

    /** Curses or slang setup was not invoked **/
    if (dump_output_immediately)
	  return((int)FALSE);

    /** Control-C or Control-Y and a 'N'o reply to exit query **/
    if (HadVMSInterrupt) {
	HadVMSInterrupt = FALSE;
	return((int)TRUE);
    }

    /** Keyboard 'Z' or 'z', or Control-G or Control-C **/
    c = typeahead();
    if (TOUPPER(c) == 'Z' || c == 7 || c == 3)
	return((int)TRUE);

    /** Other or no keystrokes **/
    return((int)FALSE);
#endif /* !VMS */
}

/*
 *  A file URL for a remote host is an obsolete ftp URL.
 *  Return YES only if we're certain it's a local file. - FM
 */
PUBLIC BOOLEAN LYisLocalFile ARGS1(
	char *, 	filename)
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

    if ((cp=strchr(host, ':')) != NULL)
	*cp = '\0';

    if ((acc_method = HTParse(filename, "", PARSE_ACCESS))) {
	if (0==strcmp("file", acc_method) &&
	    (0==strcmp(host, "localhost") ||
#ifdef VMS
	     0==strcasecomp(host, HTHostName())))
#else
	     0==strcmp(host, HTHostName())))
#endif /* VMS */
	{
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
 *  Utility for checking URLs with a host field.
 *  Return YES only if we're certain it's the local host. - FM
 */
PUBLIC BOOLEAN LYisLocalHost ARGS1(
	char *, 	filename)
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

#ifdef VMS
    if ((0==strcasecomp(host, "localhost") ||
	 0==strcasecomp(host, LYHostName) ||
	 0==strcasecomp(host, HTHostName()))) {
#else
    if ((0==strcmp(host, "localhost") ||
	 0==strcmp(host, LYHostName) ||
	 0==strcmp(host, HTHostName()))) {
#endif /* VMS */
	    FREE(host);
	    return YES;
    }

    FREE(host);
    return NO;
}

/*
 *  Utility for freeing the list of local host aliases. - FM
 */
PUBLIC void LYLocalhostAliases_free NOARGS
{
    char *alias;
    HTList *cur = localhost_aliases;

    if (!cur)
	return;

    while (NULL != (alias = (char *)HTList_nextObject(cur))) {
	FREE(alias);
    }
    HTList_delete(localhost_aliases);
    localhost_aliases = NULL;
    return;
}

/*
 *  Utility for listing hosts to be treated as local aliases. - FM
 */
PUBLIC void LYAddLocalhostAlias ARGS1(
	char *, 	alias)
{
    char *LocalAlias;

    if (!(alias && *alias))
	return;

    if (!localhost_aliases) {
	localhost_aliases = HTList_new();
	atexit(LYLocalhostAliases_free);
    }

    if ((LocalAlias = (char *)calloc(1, (strlen(alias) + 1))) == NULL)
	outofmem(__FILE__, "HTAddLocalhosAlias");
    strcpy(LocalAlias, alias);
    HTList_addObject(localhost_aliases, LocalAlias);

    return;
}

/*
 *  Utility for checking URLs with a host field.
 *  Return YES only if we've listed the host as a local alias. - FM
 */
PUBLIC BOOLEAN LYisLocalAlias ARGS1(
	char *, 	filename)
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

    while (NULL != (alias = (char *)HTList_nextObject(cur))) {
#ifdef VMS
	if (0==strcasecomp(host, alias)) {
#else
	if (0==strcmp(host, alias)) {
#endif /* VMS */
	    FREE(host);
	    return YES;
	}
    }

    FREE(host);
    return NO;
}

/*
**  This function checks for a URL with an unknown scheme,
**  but for which proxying has been set up, and if so,
**  returns PROXY_URL_TYPE. - FM
**
**  If a colon is present but the string segment which
**  precedes it is not being proxied, and we can rule
**  out that what follows the colon is not a port field,
**  it returns UNKNOWN_URL_TYPE.  Otherwise, it returns
**  0 (not a URL). - FM
*/
PUBLIC int LYCheckForProxyURL ARGS1(
	char *, 	filename)
{
    char *cp = filename;
    char *cp1;
    char *cp2 = NULL;

    /*
     *	Don't crash on an empty argument.
     */
    if (cp == NULL || *cp == '\0')
	return(0);

    /* kill beginning spaces */
    while (isspace((unsigned char)*cp))
	cp++;

    /*
     * Check for a colon, and if present,
     * see if we have proxying set up.
     */
    if ((cp1 = strchr((cp+1), ':')) != NULL) {
	*cp1 = '\0';
	StrAllocCopy(cp2, cp);
	*cp1 = ':';
	StrAllocCat(cp2, "_proxy");
	if (getenv(cp2) != NULL) {
	    FREE(cp2);
	    return(PROXY_URL_TYPE);
	}
	FREE(cp2);
	cp1++;
	if (isdigit((unsigned char)*cp1)) {
	    while (*cp1 && isdigit((unsigned char)*cp1))
		cp1++;
	    if (*cp1 && *cp1 != '/')
		return(UNKNOWN_URL_TYPE);
	}
    }

    return(0);
}

/*
**  Must recognize a URL and return the type.
**  If recognized, based on a case-insensitive
**  analyis of the scheme field, ensures that
**  the scheme field has the expected case.
**
**  Returns 0 (not a URL) for a NULL argument,
**  one which lacks a colon.
**
**  Chains to LYCheckForProxyURL() if a colon
**  is present but the type is not recognized.
*/
PUBLIC int is_url ARGS1(
	char *, 	filename)
{
    char *cp = filename;
    char *cp1;
    int i;

    /*
     *	Don't crash on an empty argument.
     */
    if (cp == NULL || *cp == '\0')
	return(0);

    /*
     *	Can't be a URL if it lacks a colon.
     */
    if (NULL == strchr(cp, ':'))
	return(0);

    /*
     *	Kill beginning spaces.
     */
    while (isspace((unsigned char)*cp))
	cp++;

    /*
     *	Can't be a URL if it starts with a slash.
     *	So return immediately for this common case,
     *	also to avoid false positives if there was
     *	a colon later in the string. - KW
     */
    if (*cp == '/')
	return(0);

#ifdef DOSPATH /* sorry! */
	if (strncmp(cp, "file:///", 8) && strlen(cp) == 19 &&
	    cp[strlen(cp)-1] == ':')
	    StrAllocCat(cp,"/");
#endif

    if (!strncasecomp(cp, "news:", 5)) {
	if (strncmp(cp, "news", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NEWS_URL_TYPE);

    } else if (!strncasecomp(cp, "nntp:", 5)) {
	if (strncmp(cp, "nntp", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NNTP_URL_TYPE);

    } else if (!strncasecomp(cp, "snews:", 6)) {
	if (strncmp(cp, "snews", 5)) {
	    for (i = 0; i < 5; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(SNEWS_URL_TYPE);

    } else if (!strncasecomp(cp, "newspost:", 9)) {
	/*
	 *  Special Lynx type to handle news posts.
	 */
	if (strncmp(cp, "newspost", 8)) {
	    for (i = 0; i < 8; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NEWSPOST_URL_TYPE);

    } else if (!strncasecomp(cp, "newsreply:", 10)) {
	/*
	 *  Special Lynx type to handle news replies (followups).
	 */
	if (strncmp(cp, "newsreply", 9)) {
	    for (i = 0; i < 9; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NEWSREPLY_URL_TYPE);

    } else if (!strncasecomp(cp, "snewspost:", 10)) {
	/*
	 *  Special Lynx type to handle snews posts.
	 */
	if (strncmp(cp, "snewspost", 9)) {
	    for (i = 0; i < 9; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NEWSPOST_URL_TYPE);

    } else if (!strncasecomp(cp, "snewsreply:", 11)) {
	/*
	 *  Special Lynx type to handle snews replies (followups).
	 */
	if (strncmp(cp, "snewsreply", 10)) {
	    for (i = 0; i < 10; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(NEWSREPLY_URL_TYPE);

    } else if (!strncasecomp(cp, "mailto:", 7)) {
	if (strncmp(cp, "mailto", 6)) {
	    for (i = 0; i < 6; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(MAILTO_URL_TYPE);

    } else if (!strncasecomp(cp, "file:", 5)) {
	if (strncmp(cp, "file", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	if (LYisLocalFile(cp)) {
	    return(FILE_URL_TYPE);
	} else if (cp[5] == '/' && cp[6] == '/') {
	    return(FTP_URL_TYPE);
	} else {
	    return(0);
	}

    } else if (!strncasecomp(cp, "data:", 5)) {
	if (strncmp(cp, "data", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(DATA_URL_TYPE);

    } else if (!strncasecomp(cp, "lynxexec:", 9)) {
	/*
	 *  Special External Lynx type to handle execution
	 *  of commands or scripts which require a pause to
	 *  read the screen upon completion.
	 */
	if (strncmp(cp, "lynxexec", 8)) {
	    for (i = 0; i < 8; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(LYNXEXEC_URL_TYPE);

    } else if (!strncasecomp(cp, "lynxprog:", 9)) {
	/*
	 *  Special External Lynx type to handle execution
	 *  of commans, sriptis or programs with do not
	 *  require a pause to read screen upon completion.
	 */
	if (strncmp(cp, "lynxprog", 8)) {
	    for (i = 0; i < 8; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(LYNXPROG_URL_TYPE);

    } else if (!strncasecomp(cp, "lynxcgi:", 8)) {
	/*
	 *  Special External Lynx type to handle cgi scripts.
	 */
	if (strncmp(cp, "lynxcgi", 7)) {
	    for (i = 0; i < 7; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(LYNXCGI_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXPRINT:", 10)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXPRINT", 9)) {
	    for (i = 0; i < 9; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXPRINT_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXDOWNLOAD:", 13)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYDOWNLOAD", 12)) {
	    for (i = 0; i < 12; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXDOWNLOAD_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXDIRED:", 10)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXDIRED", 9)) {
	    for (i = 0; i < 9; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXDIRED_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXHIST:", 9)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXHIST", 8)) {
	    for (i = 0; i < 8; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXHIST_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXKEYMAP:", 11)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXKEYMAP", 10)) {
	    for (i = 0; i < 10; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXKEYMAP_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXIMGMAP:", 11)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXIMGMAP", 10)) {
	    for (i = 0; i < 10; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	(void)is_url(&cp[11]);
	return(LYNXIMGMAP_URL_TYPE);

    } else if (!strncasecomp(cp, "LYNXCOOKIE:", 11)) {
	/*
	 *  Special Internal Lynx type.
	 */
	if (strncmp(cp, "LYNXCOOKIE", 10)) {
	    for (i = 0; i < 10; i++)
		cp[i] = TOUPPER(cp[i]);
	}
	return(LYNXCOOKIE_URL_TYPE);

    } else if (strstr((cp+3), "://") == NULL) {
	/*
	 *  If it doesn't contain "://", and it's not one of the
	 *  the above, it can't be a URL with a scheme we know,
	 *  so check if it's an unknown scheme for which proxying
	 *  has been set up. - FM
	 */
	return(LYCheckForProxyURL(filename));

    } else if (!strncasecomp(cp, "http:", 5)) {
	if (strncmp(cp, "http", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(HTTP_URL_TYPE);

    } else if (!strncasecomp(cp, "https:", 6)) {
	if (strncmp(cp, "https", 5)) {
	    for (i = 0; i < 5; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(HTTPS_URL_TYPE);

    } else if (!strncasecomp(cp, "gopher:", 7)) {
	if (strncmp(cp, "gopher", 6)) {
	    for (i = 0; i < 6; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	if ((cp1 = strchr(cp+11,'/')) != NULL) {

	    if (TOUPPER(*(cp1+1)) == 'H' || *(cp1+1) == 'w')
		/* if this is a gopher html type */
		return(HTML_GOPHER_URL_TYPE);
	    else if (*(cp1+1) == 'T' || *(cp1+1) == '8')
		return(TELNET_GOPHER_URL_TYPE);
	    else if (*(cp1+1) == '7')
		return(INDEX_GOPHER_URL_TYPE);
	    else
		return(GOPHER_URL_TYPE);
	} else {
	    return(GOPHER_URL_TYPE);
	}

    } else if (!strncasecomp(cp, "ftp:", 4)) {
	if (strncmp(cp, "ftp", 3)) {
	    for (i = 0; i < 3; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(FTP_URL_TYPE);

    } else if (!strncasecomp(cp, "wais:", 5)) {
	if (strncmp(cp, "wais", 4)) {
	    for (i = 0; i < 4; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(WAIS_URL_TYPE);

    } else if (!strncasecomp(cp, "telnet:", 7)) {
	if (strncmp(cp, "telnet", 6)) {
	    for (i = 0; i < 6; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(TELNET_URL_TYPE);

    } else if (!strncasecomp(cp, "tn3270:", 7)) {
	if (strncmp(cp, "tn", 2)) {
	    for (i = 0; i < 2; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(TN3270_URL_TYPE);

    } else if (!strncasecomp(cp, "rlogin:", 7)) {
	if (strncmp(cp, "rlogin", 6)) {
	    for (i = 0; i < 6; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(RLOGIN_URL_TYPE);

    } else if (!strncasecomp(cp, "cso:", 4)) {
	if (strncmp(cp, "cso", 3)) {
	    for (i = 0; i < 3; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(CSO_URL_TYPE);

    } else if (!strncasecomp(cp, "finger:", 7)) {
	if (strncmp(cp, "finger", 6)) {
	    for (i = 0; i < 6; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(FINGER_URL_TYPE);

    } else if (!strncasecomp(cp, "afs:", 4)) {
	if (strncmp(cp, "afs", 3)) {
	    for (i = 0; i < 3; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(AFS_URL_TYPE);

    } else if (!strncasecomp(cp, "prospero:", 9)) {
	if (strncmp(cp, "prospero", 8)) {
	    for (i = 0; i < 8; i++)
		cp[i] = TOLOWER(cp[i]);
	}
	return(PROSPERO_URL_TYPE);

    } else {
	/*
	 *  Check if it's an unknown scheme for which
	 *  proxying has been set up. - FM
	 */
	return(LYCheckForProxyURL(filename));
    }
}

/*
 *  Determine whether we allow HEAD and related flags for a URL. - kw
 */
PUBLIC BOOLEAN LYCanDoHEAD ARGS1(
    CONST char *,	address
    )
{
    char *temp0 = NULL;
    int isurl;
    if (!(address && *address))
	return FALSE;
    if (!strncmp(address, "http", 4))
	return TRUE;
    /* Make copy for is_url() since caller may not care for case changes */
    StrAllocCopy(temp0, address);
    isurl = is_url(temp0);
    FREE(temp0);
    if (!isurl)
	return FALSE;
    if (isurl == LYNXCGI_URL_TYPE) {
#if defined(LYNXCGI_LINKS) && !defined(VMS)
	return TRUE;
#else
	return FALSE;
#endif
    }
    if (isurl == NEWS_URL_TYPE || isurl == NNTP_URL_TYPE) {
	char *temp = HTParse(address, "", PARSE_PATH);
	char *cp = strrchr(temp, '/');
	if (strchr((cp ? cp : temp), '@') != NULL) {
	    FREE(temp);
	    return TRUE;
	}
	if (cp && isdigit(cp[1]) && strchr(cp, '-') == NULL) {
	    FREE(temp);
	    return TRUE;
	}
	FREE(temp);
    }
    return FALSE;
}

/*
 *  Remove backslashes from any string.
 */
PUBLIC void remove_backslashes ARGS1(
	char *, 	buf)
{
    char *cp;

    for (cp = buf; *cp != '\0' ; cp++) {

	if (*cp != '\\') { /* don't print slashes */
	    *buf = *cp;
	    buf++;
	} else if (*cp == '\\' &&	/* print one slash if there */
		   *(cp+1) == '\\') {	/* are two in a row	    */
	    *buf = *cp;
	    buf++;
	}
    }
    *buf = '\0';
    return;
}

/*
 *  Quote the path to make it safe for shell command processing.
 *
 *  We use a simple technique which involves quoting the entire
 *  string using single quotes, escaping the real single quotes
 *  with double quotes. This may be gross but it seems to work.
 */
PUBLIC char * quote_pathname ARGS1(
	char *, 	pathname)
{
    size_t i, n = 0;
    char * result;

    for (i=0; i < strlen(pathname); ++i)
	if (pathname[i] == '\'') ++n;

    result = (char *)malloc(strlen(pathname) + 5*n + 3);
    if (result == NULL)
	outofmem(__FILE__, "quote_pathname");

    result[0] = '\'';
    for (i = 0, n = 1; i < strlen(pathname); i++)
	if (pathname[i] == '\'') {
	    result[n++] = '\'';
	    result[n++] = '"';
	    result[n++] = '\'';
	    result[n++] = '"';
	    result[n++] = '\'';
	} else {
	    result[n++] = pathname[i];
	}
    result[n++] = '\'';
    result[n] = '\0';
    return result;
}

#if HAVE_UTMP
extern char *ttyname PARAMS((int fd));
#endif

/*
 *  Checks to see if the current process is attached
 *  via a terminal in the local domain.
 *
 */
PUBLIC BOOLEAN inlocaldomain NOARGS
{
#if ! HAVE_UTMP
    return(TRUE);
#else
    int n;
    FILE *fp;
    struct utmp me;
    char *cp, *mytty = NULL;

    if ((cp=ttyname(0)))
	mytty = strrchr(cp, '/');

    if (mytty && (fp=fopen(UTMP_FILE, "r")) != NULL) {
	    mytty++;
	    do {
		n = fread((char *) &me, sizeof(struct utmp), 1, fp);
	    } while (n>0 && !STREQ(me.ut_line,mytty));
	    (void) fclose(fp);

	    if (n > 0 &&
		strlen(me.ut_host) > strlen(LYLocalDomain) &&
		STREQ(LYLocalDomain,
		  me.ut_host+strlen(me.ut_host)-strlen(LYLocalDomain)) )
		return(TRUE);
#ifdef LINUX
/* Linux fix to check for local user. J.Cullen 11Jul94		*/
		if ((n > 0) && (strlen(me.ut_host) == 0))
			return(TRUE);
#endif /* LINUX */

    } else {
	if (TRACE)
	   fprintf(stderr,"Could not get ttyname or open UTMP file");
    }

    return(FALSE);
#endif /* !HAVE_UTMP */
}

/**************
** This bit of code catches window size change signals
**/

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

/* For systems that have both, but both can't be included, duh */
#ifdef TERMIO_AND_TERMIOS
# include <termio.h>
#else
# ifdef HAVE_TERMIOS_H
#  include <termios.h>
# else
#  ifdef HAVE_TERMIO_H
#   include <termio.h>
#  endif /* HAVE_TERMIO_H */
# endif /* HAVE_TERMIOS_H */
#endif	/* TERMIO_AND_TERMIOS */

PUBLIC void size_change ARGS1(
	int,		sig GCC_UNUSED)
{
    int old_lines = LYlines;
    int old_cols = LYcols;

#ifdef USE_SLANG
    SLtt_get_screen_size();
    LYlines = SLtt_Screen_Rows;
    LYcols  = SLtt_Screen_Cols;
#ifdef SLANG_MBCS_HACK
    PHYSICAL_SLtt_Screen_Cols = LYcols;
    SLtt_Screen_Cols = (LYcols * 6);
#endif /* SLANG_MBCS_HACK */
    if (sig == 0)
	/*
	 *  Called from start_curses().
	 */
	return;
#else /* Curses: */
#if HAVE_SIZECHANGE
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

    if (LYlines <= 0)
	LYlines = 24;
    if (LYcols <= 0)
	LYcols = 80;
#endif /* USE_SLANG */

    /*
     *	Check if the screen size has actually changed. - AJL
     */
    if (LYlines != old_lines || LYcols != old_cols) {
	recent_sizechange = TRUE;
    }
    if (TRACE) {
	fprintf(stderr,
		"Window size changed from (%d,%d) to (%d,%d)\n",
		old_lines, old_cols, LYlines, LYcols);
    }
#ifdef SIGWINCH
    (void)signal (SIGWINCH, size_change);
#endif /* SIGWINCH */

    return;
}

/*
 *  Utility for freeing the list of previous suggested filenames. - FM
 */
PUBLIC void HTSugFilenames_free NOARGS
{
    char *fname;
    HTList *cur = sug_filenames;

    if (!cur)
	return;

    while (NULL != (fname = (char *)HTList_nextObject(cur))) {
	FREE(fname);
    }
    HTList_delete(sug_filenames);
    sug_filenames = NULL;
    return;
}

/*
 *  Utility for listing suggested filenames, making any
 *  repeated filenanmes the most current in the list. - FM
 */
PUBLIC void HTAddSugFilename ARGS1(
	char *, 	fname)
{
    char *new;
    char *old;
    HTList *cur;

    if (!(fname && *fname))
	return;

    if ((new = (char *)calloc(1, (strlen(fname) + 1))) == NULL)
	outofmem(__FILE__, "HTAddSugFilename");
    strcpy(new, fname);

    if (!sug_filenames) {
	sug_filenames = HTList_new();
	atexit(HTSugFilenames_free);
	HTList_addObject(sug_filenames, new);
	return;
    }

    cur = sug_filenames;
    while (NULL != (old = (char *)HTList_nextObject(cur))) {
	if (!strcmp(old, new)) {
	    HTList_removeObject(sug_filenames, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(sug_filenames, new);

    return;
}

/*
 *  CHANGE_SUG_FILENAME -- Foteos Macrides 29-Dec-1993
 *	Upgraded for use with Lynx2.2 - FM 17-Jan-1994
 */
PUBLIC void change_sug_filename ARGS1(
	char *, 	fname)
{
    char *temp, *cp, *cp1, *end;
#ifdef VMS
    char *dot;
    int j, k;
#endif /* VMS */

    /*
     *	Establish the current end of fname.
     */
    end = fname + strlen(fname);

    /*
     *	Unescape fname.
     */
    HTUnEscape(fname);

    /*
     *	Rename any temporary files.
     */
    temp = (char *)calloc(1, (strlen(lynx_temp_space) + 60));
#if defined(FNAMES_8_3) && defined(DOSPATH)
    cp = HTDOS_wwwName(lynx_temp_space);
#else
    cp = lynx_temp_space;
#endif
    if (*cp == '/') {
	sprintf(temp, "file://localhost%s%d", cp, (int)getpid());
    } else {
	sprintf(temp, "file://localhost/%s%d", cp, (int)getpid());
    }
    if (!strncmp(fname, temp, strlen(temp))) {
	cp = strrchr(fname, '.');
	if (strlen(cp) > (strlen(temp) - 4))
	    cp = NULL;
	strcpy(temp, (cp ? cp : ""));
	strcpy(fname, "temp");
	strcat(fname, temp);
    }
    FREE(temp);

    /*
     *	Remove everything up the the last_slash if there is one.
     */
    if ((cp = strrchr(fname,'/')) != NULL && strlen(cp) > 1) {
	cp1 = fname;
	/*
	 *  Go past the slash.
	 */
	cp++;
	for (; *cp != '\0'; cp++, cp1++) {
	    *cp1 = *cp;
	}
	*cp1 = '\0';
    }

    /*
     *	Trim off date-size suffix, if present.
     */
    if ((*(end - 1) == ']') && ((cp = strrchr(fname, '[')) != NULL) &&
	(cp > fname) && *(--cp) == ' ') {
	while (*cp == ' ') {
	    *(cp--) = '\0';
	}
    }

    /*
     *	Trim off VMS device and/or directory specs, if present.
     */
    if ((cp = strchr(fname,'[')) != NULL &&
	(cp1 = strrchr(cp,']')) != NULL && strlen(cp1) > 1) {
	cp1++;
	for (cp=fname; *cp1 != '\0'; cp1++) {
	    *(cp++) = *cp1;
	}
	*cp = '\0';
    }

#ifdef VMS
    /*
     *	Replace illegal or problem characters.
     */
    dot = fname + strlen(fname);
    for (cp = fname; cp < dot; cp++) {
	/*
	 *  Replace with underscores.
	 */
	if (*cp == ' ' || *cp == '/' || *cp == ':' ||
	    *cp == '[' || *cp == ']' || *cp == '&') {
	    *cp = '_';
	/*
	 *  Replace with dashes.
	 */
	} else if (*cp == '!' || *cp == '?' || *cp == '\'' ||
		   *cp == ',' || *cp == ':' || *cp == '\"' ||
		   *cp == '+' || *cp == '@' || *cp == '\\' ||
		   *cp == '(' || *cp == ')' || *cp == '=' ||
		   *cp == '<' || *cp == '>' || *cp == '#' ||
		   *cp == '%' || *cp == '*' || *cp == '`' ||
		   *cp == '~' || *cp == '^' || *cp == '|' ||
		   *cp <  ' ' || ((unsigned char)*cp) > 126) {
	    *cp = '-';
	}
    }

    /*
     *	Collapse any serial underscores.
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
     *	Collapse any serial dashes.
     */
    dot = fname + (strlen(fname));
    cp = fname + 1;
    j = 0;
    while (cp < dot) {
	if (fname[j] == '-' && *cp == '-') {
	    cp++;
	}  else {
	    fname[++j] = *cp++;
	}
    }
    fname[++j] = '\0';

    /*
     *	Trim any trailing or leading
     *	underscrores or dashes.
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
     *	Replace all but the last period with _'s, or second
     *	to last if last is followed by a terminal Z or z,
     *	or GZ or gz,
     *	e.g., convert foo.tar.Z to
     *		      foo.tar_Z
     *	  or, convert foo.tar.gz to
     *		      foo.tar-gz
     */
    j = strlen(fname) - 1;
    if ((dot = strrchr(fname, '.')) != NULL) {
	if (TOUPPER(fname[j]) == 'Z') {
	    if ((fname[j-1] == '.') &&
		(((cp = strchr(fname, '.')) != NULL) && cp < dot)) {
		*dot = '_';
		dot = strrchr(fname, '.');
	    } else if (((TOUPPER(fname[j-1]) == 'G') &&
			fname[j-2] == '.') &&
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
	 *  But if the root is > 39 characters, move
	 *  the period appropriately to the left.
	 */
	while (dot - fname > 39) {
	    *dot = '\0';
	    if ((cp = strrchr(fname, '_')) != NULL) {
		*cp  = '.';
		*dot = '_';
	    } else if ((cp = strrchr(fname, '-')) != NULL) {
		*cp  = '.';
		*dot = '_';
	    } else if (*(dot + 1) == '\0') {
		j = strlen(fname);
		while (j > 39) {
		    fname[j] = fname[j-1];
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
	 *  Make sure the extension is < 40 characters.
	 */
	if ((fname + strlen(fname) - dot) > 39) {
	    *(dot + 40) = '\0';
	}

	/*
	 *  Trim trailing dashes or underscores.
	 */
	j = (strlen(fname) - 1);
	while (fname[j] == '_' || fname[j] == '-') {
	    fname[j--] = '\0';
	}
    } else {
	/*
	 *  No period, so put one on the end, or after
	 *  the 39th character, trimming trailing dashes
	 *  or underscrores.
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
     *	Replace problem characters.
     */
    for (cp = fname; *cp != '\0'; cp++) {
	switch (*cp) {
	    case '\'':
	    case '\"':
	    case '/':
	    case ' ':
		*cp = '-';
	}
    }
#endif /* VMS (UNIX) */

    /*
     *	Make sure the rest of the original string in nulled.
     */
    cp = fname + strlen(fname);
    while (cp < end) {
	*cp++ = '\0';
    }

    return;
}

/*
 *  To create standard temporary file names.
 */
PUBLIC void tempname ARGS2(
	char *, 	namebuffer,
	int,		action)
{
    static int counter = 0;
    FILE *fp = NULL;
#ifdef FNAMES_8_3
    int LYMaxTempCount = 1000; /* Arbitrary limit.  Make it configurable? */
#else
    int LYMaxTempCount = 10000; /* Arbitrary limit.  Make it configurable? */
#endif /* FNAMES_8_3 */

    if (action == REMOVE_FILES) {
	/*
	 *  Remove all temporary files with .txt or .html suffixes. - FM
	 */
	for (; counter > 0; counter--) {
#ifdef FNAMES_8_3
	    sprintf(namebuffer,
		    "%s%d%u.txt",
		    lynx_temp_space, (int)getpid(), counter-1);
	    remove(namebuffer);
	    sprintf(namebuffer,
		    "%s%d%u%s",
		    lynx_temp_space, (int)getpid(), counter-1, HTML_SUFFIX);
	    remove(namebuffer);
#else
	    sprintf(namebuffer,
		    "%sL%d-%uTMP.txt",
		    lynx_temp_space, (int)getpid(), counter-1);
	    remove(namebuffer);
	    sprintf(namebuffer,
		    "%sL%d-%uTMP%s",
		    lynx_temp_space, (int)getpid(), counter-1, HTML_SUFFIX);
	    remove(namebuffer);
#endif /* FNAMES_8_3 */
	}
    } else {
	/*
	 *  Load a tentative temporary file name into namebuffer. - FM
	 */
	while (counter < LYMaxTempCount) {
	    /*
	     *	Create names with .txt, then .bin, then
	     *	.html suffixes, and check for their prior
	     *	existence.  If any already exist, someone
	     *	might be trying to spoof us, so increment
	     *	the count and try again.  Otherwise, return
	     *	with the name which has the .html suffix
	     *	loaded in namebuffer. - FM
	     *
	     *	Some systems may use .htm instead of .html.  This
	     *	should be done consistently by always using HTML_SUFFIX
	     *	where filenames are generated for new local files. - kw
	     */
#ifdef FNAMES_8_3
	    sprintf(namebuffer,
		    "%s%d%u.txt",
		    lynx_temp_space, (int)getpid(), counter);
#else
	    sprintf(namebuffer,
		    "%sL%d-%uTMP.txt",
		    lynx_temp_space, (int)getpid(), counter);
#endif /* FNAMES_8_3 */
	    if ((fp = fopen(namebuffer, "r")) != NULL) {
		fclose(fp);
		if (TRACE)
		    fprintf(stderr,
			    "tempname: file '%s' already exists!\n",
			    namebuffer);
		counter++;
		continue;
	    }
#ifdef FNAMES_8_3
	    sprintf(namebuffer,
		    "%s%d%u.bin",
		    lynx_temp_space, (int)getpid(), counter);
#else
	    sprintf(namebuffer,
		    "%sL%d-%uTMP.bin",
		    lynx_temp_space, (int)getpid(), counter);
#endif /* FNAMES_8_3 */
	    if ((fp = fopen(namebuffer, "r")) != NULL) {
		fclose(fp);
		if (TRACE)
		    fprintf(stderr,
			    "tempname: file '%s' already exists!\n",
			    namebuffer);
		counter++;
		continue;
	    }
#ifdef FNAMES_8_3
	    sprintf(namebuffer,
		    "%s%d%u%s",
		    lynx_temp_space, (int)getpid(), counter++, HTML_SUFFIX);
#else
	    sprintf(namebuffer,
		    "%sL%d-%uTMP%s",
		    lynx_temp_space, (int)getpid(), counter++, HTML_SUFFIX);
#endif /* FNAMES_8_3 */
	    if ((fp = fopen(namebuffer, "r")) != NULL) {
		fclose(fp);
		if (TRACE)
		    fprintf(stderr,
			    "tempname: file '%s' already exists!\n",
			    namebuffer);
		continue;
	    }
	    /*
	     *	Return to the calling function, with the tentative
	     *	temporary file name loaded in namebuffer.  Note that
	     *	if the calling function will use a suffix other than
	     *	.txt, .bin, or .html, it similarly should do tests for
	     *	a spoof.  The file name can be reused if it is written
	     *	to on receipt of this name, and thereafter accessed
	     *	for reading.  Note that if writing to a file is to
	     *	be followed by reading it, as it the usual case for
	     *	Lynx, the spoof attempt will be apparent, and the user
	     *	can take appropriate action. - FM
	     */
	    return;
	}
	/*
	 *  The tempfile maximum count has been reached.
	 *  Issue a message and exit. - FM
	 */
	_statusline(MAX_TEMPCOUNT_REACHED);
	sleep(AlertSecs);
	exit(-1);
    }

    /*
     *	We were called for a clean up, and have done it. - FM
     */
    return;
}

/*
 *  Convert 4, 6, 2, 8 to left, right, down, up, etc.
 */
PUBLIC int number2arrows ARGS1(
	int,		number)
{
    switch(number) {
	case '1':
	    number=END;
	    break;
	case '2':
	    number=DNARROW;
	    break;
	case '3':
	    number=PGDOWN;
	    break;
	case '4':
	    number=LTARROW;
	    break;
	case '5':
	    number=DO_NOTHING;
	    break;
	case '6':
	    number=RTARROW;
	    break;
	case '7':
	    number=HOME;
	    break;
	case '8':
	    number=UPARROW;
	    break;
	case '9':
	    number=PGUP;
	    break;
    }

    return(number);
}

/*
 *  parse_restrictions takes a string of comma-separated restrictions
 *  and sets the corresponding flags to restrict the facilities available.
 */
PRIVATE char *restrict_name[] = {
       "inside_telnet" ,
       "outside_telnet",
       "telnet_port"   ,
       "inside_ftp"    ,
       "outside_ftp"   ,
       "inside_rlogin" ,
       "outside_rlogin",
       "suspend"       ,
       "editor"        ,
       "shell"	       ,
       "bookmark"      ,
       "multibook"     ,
       "bookmark_exec" ,
       "option_save"   ,
       "print"	       ,
       "download"      ,
       "disk_save"     ,
       "exec"	       ,
       "lynxcgi"       ,
       "exec_frozen"   ,
       "goto"	       ,
       "jump"	       ,
       "file_url"      ,
       "news_post"     ,
       "inside_news"   ,
       "outside_news"  ,
       "mail"	       ,
       "dotfiles"      ,
       "useragent"     ,
#ifdef DIRED_SUPPORT
       "dired_support" ,
#ifdef OK_PERMIT
       "change_exec_perms",
#endif /* OK_PERMIT */
#endif /* DIRED_SUPPORT */
#ifdef USE_EXTERNALS
       "externals" ,
#endif
       (char *) 0     };

	/* restrict_name and restrict_flag structure order
	 * must be maintained exactly!
	 */

PRIVATE BOOLEAN *restrict_flag[] = {
       &no_inside_telnet,
       &no_outside_telnet,
       &no_telnet_port,
       &no_inside_ftp,
       &no_outside_ftp,
       &no_inside_rlogin,
       &no_outside_rlogin,
       &no_suspend  ,
       &no_editor   ,
       &no_shell    ,
       &no_bookmark ,
       &no_multibook ,
       &no_bookmark_exec,
       &no_option_save,
       &no_print    ,
       &no_download ,
       &no_disk_save,
       &no_exec     ,
       &no_lynxcgi  ,
       &exec_frozen ,
       &no_goto     ,
       &no_jump     ,
       &no_file_url ,
       &no_newspost ,
       &no_inside_news,
       &no_outside_news,
       &no_mail     ,
       &no_dotfiles ,
       &no_useragent ,
#ifdef DIRED_SUPPORT
       &no_dired_support,
#ifdef OK_PERMIT
       &no_change_exec_perms,
#endif /* OK_PERMIT */
#endif /* DIRED_SUPPORT */
#ifdef USE_EXTERNALS
       &no_externals ,
#endif
       (BOOLEAN *) 0  };

PUBLIC void parse_restrictions ARGS1(
	char *, 	s)
{
      char *p;
      char *word;
      int i;

      if (STREQ("all", s)) {
	   /* set all restrictions */
	  for (i=0; restrict_flag[i]; i++)
	      *restrict_flag[i] = TRUE;
	  return;
      }

      if (STREQ("default", s)) {
	   /* set all restrictions */
	  for (i=0; restrict_flag[i]; i++)
	      *restrict_flag[i] = TRUE;

	     /* reset these to defaults */
	     no_inside_telnet = !(CAN_ANONYMOUS_INSIDE_DOMAIN_TELNET);
	    no_outside_telnet = !(CAN_ANONYMOUS_OUTSIDE_DOMAIN_TELNET);
	       no_inside_news = !(CAN_ANONYMOUS_INSIDE_DOMAIN_READ_NEWS);
	      no_outside_news = !(CAN_ANONYMOUS_OUTSIDE_DOMAIN_READ_NEWS);
		no_inside_ftp = !(CAN_ANONYMOUS_INSIDE_DOMAIN_FTP);
	       no_outside_ftp = !(CAN_ANONYMOUS_OUTSIDE_DOMAIN_FTP);
	     no_inside_rlogin = !(CAN_ANONYMOUS_INSIDE_DOMAIN_RLOGIN);
	    no_outside_rlogin = !(CAN_ANONYMOUS_OUTSIDE_DOMAIN_RLOGIN);
		      no_goto = !(CAN_ANONYMOUS_GOTO);
		  no_goto_cso = !(CAN_ANONYMOUS_GOTO_CSO);
		 no_goto_file = !(CAN_ANONYMOUS_GOTO_FILE);
	       no_goto_finger = !(CAN_ANONYMOUS_GOTO_FINGER);
		  no_goto_ftp = !(CAN_ANONYMOUS_GOTO_FTP);
	       no_goto_gopher = !(CAN_ANONYMOUS_GOTO_GOPHER);
		 no_goto_http = !(CAN_ANONYMOUS_GOTO_HTTP);
		no_goto_https = !(CAN_ANONYMOUS_GOTO_HTTPS);
	      no_goto_lynxcgi = !(CAN_ANONYMOUS_GOTO_LYNXCGI);
	     no_goto_lynxexec = !(CAN_ANONYMOUS_GOTO_LYNXEXEC);
	     no_goto_lynxprog = !(CAN_ANONYMOUS_GOTO_LYNXPROG);
	       no_goto_mailto = !(CAN_ANONYMOUS_GOTO_MAILTO);
		 no_goto_news = !(CAN_ANONYMOUS_GOTO_NEWS);
		 no_goto_nntp = !(CAN_ANONYMOUS_GOTO_NNTP);
	       no_goto_rlogin = !(CAN_ANONYMOUS_GOTO_RLOGIN);
		no_goto_snews = !(CAN_ANONYMOUS_GOTO_SNEWS);
	       no_goto_telnet = !(CAN_ANONYMOUS_GOTO_TELNET);
	       no_goto_tn3270 = !(CAN_ANONYMOUS_GOTO_TN3270);
		 no_goto_wais = !(CAN_ANONYMOUS_GOTO_WAIS);
	       no_telnet_port = !(CAN_ANONYMOUS_GOTO_TELNET_PORT);
		      no_jump = !(CAN_ANONYMOUS_JUMP);
		      no_mail = !(CAN_ANONYMOUS_MAIL);
		     no_print = !(CAN_ANONYMOUS_PRINT);
#if defined(EXEC_LINKS) || defined(EXEC_SCRIPTS)
		      no_exec = LOCAL_EXECUTION_LINKS_ALWAYS_OFF_FOR_ANONYMOUS;
#endif /* EXEC_LINKS || EXEC_SCRIPTS */
	  return;
      }

      p = s;
      while (*p) {
	  while (isspace((unsigned char)*p))
	      p++;
	  if (*p == '\0')
	      break;
	  word = p;
	  while (*p != ',' && *p != '\0')
	      p++;
	  if (*p)
	      *p++ = '\0';

	  for (i=0; restrict_name[i]; i++)
	     if (STREQ(word, restrict_name[i])) {
		 *restrict_flag[i] = TRUE;
		 break;
	     }
      }
      return;
}

#ifdef VMS
#include <jpidef.h>
#include <maildef.h>
#include <starlet.h>

typedef struct _VMSMailItemList
{
  short buffer_length;
  short item_code;
  void *buffer_address;
  long *return_length_address;
} VMSMailItemList;

PUBLIC int LYCheckMail NOARGS
{
    static BOOL firsttime = TRUE, failure = FALSE;
    static char user[13], dir[252];
    static long userlen = 0, dirlen;
    static time_t lastcheck = 0;
    time_t now;
    static short new, lastcount;
    long ucontext = 0, status;
    short flags = MAIL$M_NEWMSG;
    VMSMailItemList
      null_list[] = {{0,0,0,0}},
      jpi_list[]  = {{sizeof(user) - 1,JPI$_USERNAME,(void *)user,&userlen},
		     {0,0,0,0}},
      uilist[]	  = {{0,MAIL$_USER_USERNAME,0,0},
		     {0,0,0,0}},
      uolist[]	  = {{sizeof(new),MAIL$_USER_NEW_MESSAGES,&new,0},
		     {sizeof(dir),MAIL$_USER_FULL_DIRECTORY,dir,&dirlen},
		     {0,0,0,0}};
    extern long mail$user_begin();
    extern long mail$user_get_info();
    extern long mail$user_end();

    if (failure)
	return 0;

    if (firsttime) {
	firsttime = FALSE;
	/* Get the username. */
	status = sys$getjpiw(0,0,0,jpi_list,0,0,0);
	if (!(status & 1)) {
	    failure = TRUE;
	    return 0;
	}
	user[userlen] = '\0';
	while (user[0] &&
	       /*
		*  Suck up trailing spaces.
		*/
	       isspace((unsigned char)user[--userlen]))
	    user[userlen] = '\0';
    }

    /* Minimum report interval is 60 sec. */
    time(&now);
    if (now - lastcheck < 60)
	return 0;
    lastcheck = now;

    /* Get the current newmail count. */
    status = mail$user_begin(&ucontext,null_list,null_list);
    if (!(status & 1)) {
	failure = TRUE;
	return 0;
    }
    uilist[0].buffer_length = strlen(user);
    uilist[0].buffer_address = user;
    status = mail$user_get_info(&ucontext,uilist,uolist);
    if (!(status & 1)) {
	failure = TRUE;
	return 0;
    }

    /* Should we report anything to the user? */
    if (new > 0) {
	if (lastcount == 0)
	    /* Have newmail at startup of Lynx. */
	    _statusline(HAVE_UNREAD_MAIL_MSG);
	else if (new > lastcount)
	    /* Have additional mail since last report. */
	    _statusline(HAVE_NEW_MAIL_MSG);
	lastcount = new;
	return 1;
    }
    lastcount = new;

    /* Clear the context */
    mail$user_end((long *)&ucontext,null_list,null_list);
    return 0;
}
#else
PUBLIC int LYCheckMail NOARGS
{
    static BOOL firsttime = TRUE;
    static char *mf;
    static time_t lastcheck;
    static long lastsize;
    time_t now;
    struct stat st;

    if (firsttime) {
	mf = getenv("MAIL");
	firsttime = FALSE;
    }

    if (mf == NULL)
	return 0;

    time(&now);
    if (now - lastcheck < 60)
	return 0;
    lastcheck = now;

    if (stat(mf,&st) < 0) {
	mf = NULL;
	return 0;
    }

    if (st.st_size > 0) {
	if (st.st_mtime > st.st_atime ||
	    (lastsize && st.st_size > lastsize))
	    _statusline(HAVE_NEW_MAIL_MSG);
	else if (lastsize == 0)
	    _statusline(HAVE_MAIL_MSG);
	lastsize = st.st_size;
	return 1;
    }
    lastsize = st.st_size;
    return 0;
}
#endif /* VMS */

/*
**  This function ensures that an href will be
**  converted to a fully resolved, absolute URL,
**  with guessing of the host or expansions of
**  lead tildes via LYConvertToURL() if needed,
**  and tweaking/simplifying via HTParse().  It
**  is used for LynxHome, startfile, homepage,
**  an 'g'oto entries, after they have been
**  passed to LYFillLocalFileURL(). - FM
*/
PUBLIC void LYEnsureAbsoluteURL ARGS2(
	char **,	href,
	char *, 	name)
{
    char *temp = NULL;

    if (!(*href && *(*href)))
	return;

    /*
     *	If it is not a URL then make it one.
     */
    if (!strcasecomp(*href, "news:")) {
	StrAllocCat(*href, "*");
    } else if (!strcasecomp(*href, "nntp:") ||
	       !strcasecomp(*href, "snews:")) {
	StrAllocCat(*href, "/*");
    }
    if (!is_url(*href)) {
	if (TRACE)
	    fprintf(stderr, "%s%s'%s' is not a URL\n",
		    (name ? name : ""), (name ? " " : ""), *href);
	LYConvertToURL(href);
    }
    if ((temp = HTParse(*href, "", PARSE_ALL)) != NULL && *temp != '\0')
	StrAllocCopy(*href, temp);
    FREE(temp);
}

/*
 *  Rewrite and reallocate a previously allocated string
 *  as a file URL if the string resolves to a file or
 *  directory on the local system, otherwise as an
 *  http URL. - FM
 */
PUBLIC void LYConvertToURL ARGS1(
	char **,	AllocatedString)
{
    char *old_string = *AllocatedString;
    char *temp = NULL;
    char *cp = NULL;
#ifndef VMS
    struct stat st;
    FILE *fptemp = NULL;
#endif /* !VMS */

    if (!old_string || *old_string == '\0')
	return;

#ifdef DOSPATH
    {
	 char *cp_url = *AllocatedString;
	 for(; *cp_url != '\0'; cp_url++)
		if(*cp_url == '\\') *cp_url = '/';
	 cp_url--;
	 if(*cp_url == ':')
		 StrAllocCat(*AllocatedString,"/");
#ifdef NOTDEFINED
	 if(strlen(old_string) > 3 && *cp_url == '/')
		*cp_url = '\0';
#endif
    }
#endif /* DOSPATH */

    *AllocatedString = NULL;  /* so StrAllocCopy doesn't free it */
    StrAllocCopy(*AllocatedString,"file://localhost");

    if (*old_string != '/') {
	char *fragment = NULL;
#ifdef DOSPATH
	StrAllocCat(*AllocatedString,"/");
#endif /* DOSPATH */
#ifdef VMS
	/*
	 *  Not a SHELL pathspec.  Get the full VMS spec and convert it.
	 */
	char *cur_dir = NULL;
	static char url_file[256], file_name[256], dir_name[256];
	unsigned long context = 0;
	$DESCRIPTOR(url_file_dsc, url_file);
	$DESCRIPTOR(file_name_dsc, file_name);
	if (*old_string == '~') {
	    /*
	     *	On VMS, we'll accept '~' on the command line as
	     *	Home_Dir(), and assume the rest of the path, if
	     *	any, has SHELL syntax.
	     */
	    StrAllocCat(*AllocatedString, HTVMS_wwwName((char *)Home_Dir()));
	    if ((cp = strchr(old_string, '/')) != NULL) {
		/*
		 *  Append rest of path, if present, skipping "user" if
		 *  "~user" was entered, simplifying, and eliminating
		 *  any residual relative elements. - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	    goto have_VMS_URL;
	} else {
	    if ((fragment = strchr(old_string, '#')) != NULL)
		*fragment = '\0';
	    strcpy(url_file, old_string);
	}
	url_file_dsc.dsc$w_length = (short) strlen(url_file);
	if (1&lib$find_file(&url_file_dsc, &file_name_dsc, &context,
			    0, 0, 0, 0)) {
	    /*
	     *	We found the file.  Convert to a URL pathspec.
	     */
	    if ((cp = strchr(file_name, ';')) != NULL) {
		*cp = '\0';
	    }
	    for (cp = file_name; *cp; cp++) {
		*cp = TOLOWER(*cp);
	    }
	    StrAllocCat(*AllocatedString, HTVMS_wwwName(file_name));
	    if ((cp = strchr(old_string, ';')) != NULL) {
		StrAllocCat(*AllocatedString, cp);
	    }
	    if (fragment != NULL) {
		*fragment = '#';
		StrAllocCat(*AllocatedString, fragment);
		fragment = NULL;
	    }
	} else if ((NULL != getcwd(dir_name, 255, 0)) &&
		   0 == chdir(old_string)) {
	    /*
	     * Probably a directory.  Try converting that.
	     */
	    StrAllocCopy(cur_dir, dir_name);
	    if (fragment != NULL) {
		*fragment = '#';
	    }
	    if (NULL != getcwd(dir_name, 255, 0)) {
		/*
		 * Yup, we got it!
		 */
		for (cp = dir_name; *cp; cp++) {
		    *cp = TOLOWER(*cp);
		}
		StrAllocCat(*AllocatedString, dir_name);
		if (fragment != NULL) {
		    StrAllocCat(*AllocatedString, fragment);
		    fragment = NULL;
		}
	    } else {
		/*
		 *  Nope.  Assume it's an http URL with
		 *  the "http://" defaulted, if we can't
		 *  rule out a bad VMS path.
		 */
		fragment = NULL;
		if (strchr(old_string, '[') ||
		    ((cp = strchr(old_string, ':')) != NULL &&
		     !isdigit((unsigned char)cp[1])) ||
		    !LYExpandHostForURL((char **)&old_string,
					URLDomainPrefixes,
					URLDomainSuffixes)) {
		    /*
		     *	Probably a bad VMS path (but can't be
		     *	sure).	Use original pathspec for the
		     *	error message that will result.
		     */
		    strcpy(url_file, "/");
		    strcat(url_file, old_string);
		    if (TRACE) {
			fprintf(stderr,
			    "Can't find '%s'  Will assume it's a bad path.\n",
				old_string);
		    }
		    StrAllocCat(*AllocatedString, url_file);
		} else {
		    /*
		     *	Assume a URL is wanted, so guess the
		     *	scheme with "http://" as the default. - FM
		     */
		    if (!LYAddSchemeForURL((char **)&old_string, "http://")) {
			StrAllocCopy(*AllocatedString, "http://");
			StrAllocCat(*AllocatedString, old_string);
		    } else {
			StrAllocCopy(*AllocatedString, old_string);
		    }
		}
	    }
	} else {
	    /*
	     *	Nothing found.	Assume it's an http URL
	     *	with the "http://" defaulted, if we can't
	     *	rule out a bad VMS path.
	     */
	    if (fragment != NULL) {
		*fragment = '#';
		fragment = NULL;
	    }
	    if (strchr(old_string, '[') ||
		((cp = strchr(old_string, ':')) != NULL &&
		 !isdigit((unsigned char)cp[1])) ||
		!LYExpandHostForURL((char **)&old_string,
				    URLDomainPrefixes,
				    URLDomainSuffixes)) {
		/*
		 *  Probably a bad VMS path (but can't be
		 *  sure).  Use original pathspec for the
		 *  error message that will result.
		 */
		strcpy(url_file, "/");
		strcat(url_file, old_string);
		if (TRACE) {
		    fprintf(stderr,
			    "Can't find '%s'  Will assume it's a bad path.\n",
				old_string);
		}
		StrAllocCat(*AllocatedString, url_file);
	    } else {
		/*
		 *  Assume a URL is wanted, so guess the
		 *  scheme with "http://" as the default. - FM
		 */
		if (!LYAddSchemeForURL((char **)&old_string, "http://")) {
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
	if (TRACE) {
	    fprintf(stderr, "Trying: '%s'\n", *AllocatedString);
	}
#else /* Unix: */
#ifdef DOSPATH
	if (strlen(old_string) == 1 && *old_string == '.') {
	    /*
	     *	They want .
	     */
	    char curdir[DIRNAMESIZE];
	    getcwd (curdir, DIRNAMESIZE);
	    StrAllocCopy(temp, HTDOS_wwwName(curdir));
	    StrAllocCat(*AllocatedString, temp);
	    FREE(temp);
	    if (TRACE) {
		fprintf(stderr, "Converted '%s' to '%s'\n",
				old_string, *AllocatedString);
	    }
	} else
#endif /* DOSPATH */
	if (*old_string == '~') {
	    /*
	     *	On Unix, covert '~' to Home_Dir().
	     */
	    StrAllocCat(*AllocatedString, Home_Dir());
	    if ((cp = strchr(old_string, '/')) != NULL) {
		/*
		 *  Append rest of path, if present, skipping "user" if
		 *  "~user" was entered, simplifying, and eliminating
		 *  any residual relative elements. - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	    if (TRACE) {
		fprintf(stderr, "Converted '%s' to '%s'\n",
				old_string, *AllocatedString);
	    }
	} else {
	    /*
	     *	Create a full path to the current default directory.
	     */
	    char curdir[DIRNAMESIZE];
	    char *temp2 = NULL;
	    BOOL is_local = FALSE;
#if HAVE_GETCWD
	    getcwd (curdir, DIRNAMESIZE);
#else
	    getwd (curdir);
#endif /* NO_GETCWD */
	    /*
	     *	Concatenate and simplify, trimming any
	     *	residual relative elements. - FM
	     */
#ifndef DOSPATH
	    StrAllocCopy(temp, curdir);
	    StrAllocCat(temp, "/");
	    StrAllocCat(temp, old_string);
#else
	    if (old_string[1] != ':' && old_string[1] != '|') {
		StrAllocCopy(temp, HTDOS_wwwName(curdir));
		if(curdir[strlen(curdir)-1] != '/')
		    StrAllocCat(temp, "/");
		LYstrncpy(curdir, temp, (DIRNAMESIZE - 1));
		StrAllocCat(temp, old_string);
	    } else {
		curdir[0] = '\0';
		StrAllocCopy(temp, old_string);
	    }
#endif /* DOSPATH */
	    LYTrimRelFromAbsPath(temp);
	    if (TRACE) {
		fprintf(stderr, "Converted '%s' to '%s'\n", old_string, temp);
	    }
	    if ((stat(temp, &st) > -1) ||
		(fptemp = fopen(temp, "r")) != NULL) {
		/*
		 *  It is a subdirectory or file on the local system.
		 */
#ifdef DOSPATH
		/* Don't want to see DOS local paths like c: escaped */
		/* especially when we really have file://localhost/  */
		/* at the beginning. To avoid any confusion we allow */
		/* escaping the path if URL specials % or # present. */
		if (strchr(temp, '#') == NULL &&
			   strchr(temp, '%') == NULL)
		StrAllocCopy(cp, temp);
		else
#endif /* DOSPATH */
		cp = HTEscape(temp, URL_PATH);
		StrAllocCat(*AllocatedString, cp);
		FREE(cp);
		if (TRACE) {
		    fprintf(stderr, "Converted '%s' to '%s'\n",
				    old_string, *AllocatedString);
		}
		is_local = TRUE;
	    } else {
		char *cp2 = NULL;
		StrAllocCopy(temp2, curdir);
		if (curdir[0] != '\0' && curdir[strlen(curdir)-1] != '/')
		    StrAllocCat(temp2, "/");
		StrAllocCopy(cp, old_string);
		if ((fragment = strchr(cp, '#')) != NULL)
		    *fragment = '\0';	/* keep as pointer into cp string */
		HTUnEscape(cp);   /* unescape given path without fragment */
		StrAllocCat(temp2, cp); 	/* append to current dir  */
		StrAllocCopy(cp2, temp2);	/* keep a copy in cp2	  */
		LYTrimRelFromAbsPath(temp2);

		if (strcmp(temp2, temp) != 0 &&
		    ((stat(temp2, &st) > -1) ||
		     (fptemp = fopen(temp2, "r")) != NULL)) {
		    /*
		     *	It is a subdirectory or file on the local system
		     *	with escaped characters and/or a fragment to be
		     *	appended to the URL. - FM
		     */

		    FREE(temp);
		    if (strcmp(cp2, temp2) == 0) {
			/*
			 *  LYTrimRelFromAbsPath did nothing, use
			 *  old_string as given. - kw
			 */
			temp = HTEscape(curdir, URL_PATH);
			if (curdir[0] != '\0' && curdir[strlen(curdir)-1] != '/')
			    StrAllocCat(temp, "/");
			StrAllocCat(temp, old_string);
		    } else {
			temp = HTEscape(temp2, URL_PATH);
			if (fragment != NULL) {
			    *fragment = '#';
			    StrAllocCat(temp, fragment);
			}
		    }
		    StrAllocCat(*AllocatedString, temp);
		    if (TRACE) {
			fprintf(stderr, "Converted '%s' to '%s'\n",
					old_string, *AllocatedString);
		    }
		    is_local = TRUE;

		} else if (strchr(curdir, '#') != NULL ||
			   strchr(curdir, '%') != NULL) {
		    /*
		     *	If PWD has some unusual characters, construct a
		     *	filename in temp where those are escaped.  This
		     *	is mostly to prevent this function from returning
		     *	with some weird URL if the LYExpandHostForURL tests
		     *	further down fail. - kw
		     */
		    FREE(temp);
		    if (strcmp(cp2, temp2) == 0) {
			/*
			 *  LYTrimRelFromAbsPath did nothing, use
			 *  old_string as given. - kw
			 */
			temp = HTEscape(curdir, URL_PATH);
			if (curdir[0] != '\0' && curdir[strlen(curdir)-1] != '/')
			    StrAllocCat(temp, "/");
			StrAllocCat(temp, old_string);
		    } else {
			temp = HTEscape(temp2, URL_PATH);
			if (fragment != NULL) {
			    *fragment = '#';
			    StrAllocCat(temp, fragment);
			}
		    }
		}
		FREE(cp);
		FREE(cp2);
	    }
	    if (is_local == FALSE) {
		/*
		 *  It's not an accessible subdirectory or file on the
		 *  local system, so assume it's a URL request and guess
		 *  the scheme with "http://" as the default.
		 */
		if (TRACE) {
		    fprintf(stderr, "Can't stat() or fopen() '%s'\n",
			    temp2 ? temp2 : temp);
		}
		if (LYExpandHostForURL((char **)&old_string,
				       URLDomainPrefixes,
				       URLDomainSuffixes)) {
		    if (!LYAddSchemeForURL((char **)&old_string, "http://")) {
			StrAllocCopy(*AllocatedString, "http://");
			StrAllocCat(*AllocatedString, old_string);
		    } else {
			StrAllocCopy(*AllocatedString, old_string);
		    }
		} else {
		    StrAllocCat(*AllocatedString, temp);
		}
		if (TRACE) {
		    fprintf(stderr, "Trying: '%s'\n", *AllocatedString);
		}
	    }
	    FREE(temp);
	    FREE(temp2);
	    if (fptemp) {
		fclose(fptemp);
		fptemp = NULL;
	    }
	}
#endif /* VMS */
    } else {
	/*
	 *  Path begins with a slash.  Simplify and use it.
	 */
	if (old_string[1] == '\0') {
	    /*
	     *	Request for root.  Respect it on Unix, but
	     *	on VMS we treat that as a listing of the
	     *	login directory. - FM
	     */
#ifdef VMS
	    StrAllocCat(*AllocatedString, HTVMS_wwwName((char *)Home_Dir()));
#else
	    StrAllocCat(*AllocatedString, "/");
	} else if ((stat(old_string, &st) > -1) ||
		   (fptemp = fopen(old_string, "r")) != NULL) {
	    /*
	     *	It is an absolute directory or file
	     *	on the local system. - KW
	     */
	    StrAllocCopy(temp, old_string);
	    LYTrimRelFromAbsPath(temp);
	    if (TRACE) {
		fprintf(stderr, "Converted '%s' to '%s'\n", old_string, temp);
	    }
	    cp = HTEscape(temp, URL_PATH);
	    StrAllocCat(*AllocatedString, cp);
	    FREE(cp);
	    FREE(temp);
	    if (fptemp) {
		fclose(fptemp);
		fptemp = NULL;
	    }
	    if (TRACE) {
		fprintf(stderr, "Converted '%s' to '%s'\n",
			old_string, *AllocatedString);
	    }
#endif /* VMS */
	} else if (old_string[1] == '~') {
	    /*
	     *	Has a Home_Dir() reference.  Handle it
	     *	as if there weren't a lead slash. - FM
	     */
#ifdef VMS
	    StrAllocCat(*AllocatedString, HTVMS_wwwName((char *)Home_Dir()));
#else
	    StrAllocCat(*AllocatedString, Home_Dir());
#endif /* VMS */
	    if ((cp = strchr((old_string + 1), '/')) != NULL) {
		/*
		 *  Append rest of path, if present, skipping "user" if
		 *  "~user" was entered, simplifying, and eliminating
		 *  any residual relative elements. - FM
		 */
		StrAllocCopy(temp, cp);
		LYTrimRelFromAbsPath(temp);
		StrAllocCat(*AllocatedString, temp);
		FREE(temp);
	    }
	} else {
	    /*
	     *	Normal absolute path.  Simplify, trim any
	     *	residual relative elements, and append it. - FM
	     */
	    StrAllocCopy(temp, old_string);
	    LYTrimRelFromAbsPath(temp);
	    StrAllocCat(*AllocatedString, temp);
	    FREE(temp);
	}
	if (TRACE) {
	    fprintf(stderr, "Converted '%s' to '%s'\n",
			    old_string, *AllocatedString);
	}
    }
    FREE(old_string);
    if (TRACE) {
	/* Pause so we can read the messages before invoking curses */
	if (!LYTraceLogFP)
	    sleep(AlertSecs);
    }
}

/*
 *  This function rewrites and reallocates a previously allocated
 *  string so that the first element is a confirmed Internet host,
 *  and returns TRUE, otherwise it does not modify the string and
 *  returns FALSE.  It first tries the element as is, then, if the
 *  element does not end with a dot, it adds prefixes from the
 *  (comma separated) prefix list arguement, and, if the element
 *  does not begin with a dot, suffixes from the (comma separated)
 *  suffix list arguments (e.g., www.host.com, then www.host,edu,
 *  then www.host.net, then www.host.org).  The remaining path, if
 *  one is present, will be appended to the expanded host.  It also
 *  takes into account whether a colon is in the element or suffix,
 *  and includes that and what follows as a port field for the
 *  expanded host field (e.g, wfbr:8002/dir/lynx should yield
 *  www.wfbr.edu:8002/dir/lynx).  The calling function should
 *  prepend the scheme field (e.g., http://), or pass the string
 *  to LYAddSchemeForURL(), if this function returns TRUE. - FM
 */
PUBLIC BOOLEAN LYExpandHostForURL ARGS3(
	char **,	AllocatedString,
	char *, 	prefix_list,
	char *, 	suffix_list)
{
    char DomainPrefix[80], *StartP, *EndP;
    char DomainSuffix[80], *StartS, *EndS;
    char *Str = NULL, *StrColon = NULL, *MsgStr = NULL;
    char *Host = NULL, *HostColon = NULL, *host = NULL;
    char *Path = NULL;
    char *Fragment = NULL;
    struct hostent  *phost;
    BOOLEAN GotHost = FALSE;
    BOOLEAN Startup = (helpfilepath == NULL);

    /*
     *	If it's a NULL or zero-length string,
     *	or if it begins with a slash or hash,
     *	don't continue pointlessly. - FM
     */
    if (!(*AllocatedString) || *AllocatedString[0] == '\0' ||
	*AllocatedString[0] == '/' || *AllocatedString[0] == '#') {
	return GotHost;
    }

    /*
     *	If it's a partial or relative path,
     *	don't continue pointlessly. - FM
     */
    if (!strncmp(*AllocatedString, "..", 2) ||
	!strncmp(*AllocatedString, "./", 2)) {
	return GotHost;
    }

    /*
     *	Make a clean copy of the string, and trim off the
     *	path if one is present, but save the information
     *	so we can restore the path after filling in the
     *	Host[:port] field. - FM
     */
    StrAllocCopy(Str, *AllocatedString);
    if ((Path = strchr(Str, '/')) != NULL) {
	/*
	 *  Have a path.  Any fragment should
	 *  already be included in Path. - FM
	 */
	*Path = '\0';
    } else if ((Fragment = strchr(Str, '#')) != NULL) {
	/*
	 *  No path, so check for a fragment and
	 *  trim that, to be restored after filling
	 *  in the Host[:port] field. - FM
	 */
	*Fragment = '\0';
    }

    /*
     *	If the potential host string has a colon, assume it
     *	begins a port field, and trim it off, but save the
     *	information so we can restore the port field after
     *	filling in the host field. - FM
     */
    if ((StrColon = strrchr(Str, ':')) != NULL &&
	isdigit((unsigned char)StrColon[1])) {
	if (StrColon == Str) {
	    FREE(Str);
	    return GotHost;
	}
	*StrColon = '\0';
    }

    /*
     *	Do a DNS test on the potential host field
     *	as presently trimmed. - FM
     */
    StrAllocCopy(host, Str);
    HTUnEscape(host);
    if (LYCursesON) {
	StrAllocCopy(MsgStr, "Looking up ");
	StrAllocCat(MsgStr, host);
	StrAllocCat(MsgStr, " first.");
	HTProgress(MsgStr);
    } else if (Startup && !dump_output_immediately) {
	fprintf(stdout, "Looking up '%s' first.\n", host);
    }
#ifndef DJGPP
    if ((phost = gethostbyname(host)) != NULL)
#else
    if (resolve(host) != 0)
#endif /* DJGPP */
    {
	/*
	 *  Clear any residual interrupt. - FM
	 */
	if (LYCursesON && HTCheckForInterrupt()) {
	    if (TRACE) {
		fprintf(stderr,
	 "LYExpandHostForURL: Ignoring interrupt because '%s' resolved.\n",
			host);
	    }
	}

	/*
	 *  Return success. - FM
	 */
	GotHost = TRUE;
	FREE(host);
	FREE(Str);
	FREE(MsgStr);
	return GotHost;
    } else if (LYCursesON && HTCheckForInterrupt()) {
	/*
	 *  Give the user chance to interrupt lookup cycles. - KW & FM
	 */
	if (TRACE) {
	    fprintf(stderr,
	 "LYExpandHostForURL: Interrupted while '%s' failed to resolve.\n",
		    host);
	}

	/*
	 *  Return failure. - FM
	 */
	FREE(host);
	FREE(Str);
	FREE(MsgStr);
	return FALSE;
    }

    /*
     *	Set the first prefix, making it a zero-length string
     *	if the list is NULL or if the potential host field
     *	ends with a dot. - FM
     */
    StartP = ((prefix_list && Str[strlen(Str)-1] != '.') ?
					     prefix_list : "");
    /*
     *	If we have a prefix, but the allocated string is
     *	one of the common host prefixes, make our prefix
     *	a zero-length string. - FM
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
	StartP++;	/* Skip whitespace and separators */
    }
    EndP = StartP;
    while (*EndP && !WHITE(*EndP) && *EndP != ',') {
	EndP++; 	/* Find separator */
    }
    LYstrncpy(DomainPrefix, StartP, (EndP - StartP));

    /*
     *	Test each prefix with each suffix. - FM
     */
    do {
	/*
	 *  Set the first suffix, making it a zero-length string
	 *  if the list is NULL or if the potential host field
	 *  begins with a dot. - FM
	 */
	StartS = ((suffix_list && *Str != '.') ?
				   suffix_list : "");
	while ((*StartS) && (WHITE(*StartS) || *StartS == ',')) {
	    StartS++;	/* Skip whitespace and separators */
	}
	EndS = StartS;
	while (*EndS && !WHITE(*EndS) && *EndS != ',') {
	    EndS++;	/* Find separator */
	}
	LYstrncpy(DomainSuffix, StartS, (EndS - StartS));

	/*
	 *  Create domain names and do DNS tests. - FM
	 */
	do {
	    StrAllocCopy(Host, DomainPrefix);
	    StrAllocCat(Host, ((*Str == '.') ? (Str + 1) : Str));
	    if (Host[strlen(Host)-1] == '.') {
		Host[strlen(Host)-1] = '\0';
	    }
	    StrAllocCat(Host, DomainSuffix);
	    if ((HostColon = strrchr(Host, ':')) != NULL &&
		isdigit((unsigned char)HostColon[1])) {
		*HostColon = '\0';
	    }
	    StrAllocCopy(host, Host);
	    HTUnEscape(host);
	    if (LYCursesON) {
		StrAllocCopy(MsgStr, "Looking up ");
		StrAllocCat(MsgStr, host);
		StrAllocCat(MsgStr, ", guessing...");
		HTProgress(MsgStr);
	    } else if (Startup && !dump_output_immediately) {
		fprintf(stdout, "Looking up '%s', guessing...\n", host);
	    }
#ifndef DJGPP
	    GotHost = ((phost = gethostbyname(host)) != NULL);
#else
	    GotHost = (resolve(host) != 0);
#endif /* DJGPP */
	    if (HostColon != NULL) {
		*HostColon = ':';
	    }
	    if (GotHost == FALSE) {
		/*
		 *  Give the user chance to interrupt lookup cycles. - KW
		 */
		if (LYCursesON && HTCheckForInterrupt()) {
		    if (TRACE) {
			fprintf(stderr,
	 "LYExpandHostForURL: Interrupted while '%s' failed to resolve.\n",
				host);
			    }
		    FREE(Str);
		    FREE(MsgStr);
		    FREE(Host);
		    FREE(host);
		    return FALSE; /* We didn't find a valid name. */
		}

		/*
		 *  Advance to the next suffix, or end of suffix list. - FM
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
	}  while ((GotHost == FALSE) && (*DomainSuffix != '\0'));

	if (GotHost == FALSE) {
	   /*
	    *  Advance to the next prefix, or end of prefix list. - FM
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
     *	If a test passed, restore the port field if we had one
     *	and there is no colon in the expanded host, and the path
     *	if we had one, and reallocate the original string with
     *	the expanded Host[:port] field included. - FM
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
	    *Fragment = '#';
	    StrAllocCat(Host, Fragment);
	}
	StrAllocCopy(*AllocatedString, Host);
    }

    /*
     *	Clear any residual interrupt. - FM
     */
    if (LYCursesON && HTCheckForInterrupt()) {
	if (TRACE) {
	    fprintf(stderr,
	 "LYExpandHostForURL: Ignoring interrupt because '%s' %s.\n",
		    host,
		    (GotHost ? "resolved" : "timed out"));
	}
    }

    /*
     *	Clean up and return the last test result. - FM
     */
    FREE(Str);
    FREE(MsgStr);
    FREE(Host);
    FREE(host);
    return GotHost;
}

/*
 *  This function rewrites and reallocates a previously allocated
 *  string that begins with an Internet host name so that the string
 *  begins with its guess of the scheme based on the first field of
 *  the host name, or the default scheme if no guess was made, and
 *  returns TRUE, otherwise it does not modify the string and returns
 *  FALSE.  It also returns FALSE without modifying the string if the
 *  default_scheme argument was NULL or zero-length and no guess was
 *  made. - FM
  */
PUBLIC BOOLEAN LYAddSchemeForURL ARGS2(
	char **,	AllocatedString,
	char *, 	default_scheme)
{
    char *Str = NULL;
    BOOLEAN GotScheme = FALSE;

    /*
     *	If we were passed a NULL or zero-length string,
     *	don't continue pointlessly. - FM
     */
    if (!(*AllocatedString) || *AllocatedString[0] == '\0') {
	return GotScheme;
    }

    /*
     * Try to guess the appropriate scheme. - FM
     */
    if (0 == strncasecomp(*AllocatedString, "www", 3)) {
	/*
	 *  This could be either http or https, so check
	 *  the default and otherwise use "http". - FM
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
	 *  This could be either news, snews, or nntp, so
	 *  check the default, and otherwise use news. - FM
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
     *	If we've make a guess, use it.	Otherwise, if we
     *	were passed a default scheme prefix, use that. - FM
     */
    if (GotScheme == TRUE) {
	StrAllocCat(Str, *AllocatedString);
	StrAllocCopy(*AllocatedString, Str);
	FREE(Str);
	return GotScheme;

    } else if (default_scheme != NULL && *default_scheme != '\0') {
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
 *  This function expects an absolute Unix or VMS SHELL path
 *  spec as an allocated string, simplifies it, and trims out
 *  any residual relative elements.  It also checks whether
 *  the path had a terminal slash, and if it didn't, makes
 *  sure that the simplified path doesn't either.  If it's
 *  a directory, our convention is to exclude "Up to parent"
 *  links when a terminal slash is present. - FM
 */
PUBLIC void LYTrimRelFromAbsPath ARGS1(
	char *, 	path)
{
    char *cp;
    int i;
    BOOL TerminalSlash;

    /*
     *	Make sure we have a pointer to an absolute path. - FM
     */
    if (path == NULL || *path != '/')
	return;

    /*
     *	Check whether the path has a terminal slash. - FM
     */
    TerminalSlash = (path[(strlen(path) - 1)] == '/');

    /*
     *	Simplify the path and then do any necessary trimming. - FM
     */
    HTSimplify(path);
    cp = path;
    while (cp[1] == '.') {
	if (cp[2] == '\0') {
	    /*
	     *	Eliminate trailing dot. - FM
	     */
	    cp[1] = '\0';
	} else if (cp[2] == '/') {
	    /*
	     *	Skip over the "/." of a "/./". - FM
	     */
	    cp += 2;
	} else if (cp[2] == '.' && cp[3] == '\0') {
	    /*
	     *	Eliminate trailing dotdot. - FM
	     */
	    cp[1] = '\0';
	} else if (cp[2] == '.' && cp[3] == '/') {
	    /*
	     *	Skip over the "/.." of a "/../". - FM
	     */
	    cp += 3;
	} else {
	    /*
	     *	Done trimming. - FM
	     */
	    break;
	}
    }

    /*
     *	Load any shifts into path, and eliminate any
     *	terminal slash created by HTSimplify() or our
     *	walk, but not present originally. - FM
     */
    if (cp > path) {
	for (i = 0; cp[i] != '\0'; i++)
	    path[i] = cp[i];
	path[i] = '\0';
    }
    if (TerminalSlash == FALSE &&
	path[(strlen(path) - 1)] == '/') {
	path[(strlen(path) - 1)] = '\0';
    }
}

/*
 *  Example Client-Side Include interface.
 *
 *  This is called from SGML.c and simply returns markup for reporting
 *  the URL of the document being loaded if a comment begins with
 *  "<!--#lynxCSI".  The markup will be included as if it were in the
 *  document.  Move this function to a separate module for doing this
 *  kind of thing seriously, someday. - FM
 */
PUBLIC void LYDoCSI ARGS3(
	char *, 	url,
	CONST char *,	comment,
	char **,	csi)
{
    CONST char *cp = comment;

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
 *  Define_VMSLogical -- Fote Macrides 04-Apr-1995
 *	Define VMS logicals in the process table.
 */
PUBLIC void Define_VMSLogical ARGS2(
	char *, 	LogicalName,
	char *, 	LogicalValue)
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

PRIVATE void LYHomeDir_free NOARGS
{
    FREE(HomeDir);
}

PUBLIC CONST char * Home_Dir NOARGS
{
    static CONST char *homedir = NULL;
    char *cp = NULL;

    if (homedir == NULL) {
	if ((cp = getenv("HOME")) == NULL || *cp == '\0') {
#ifdef DOSPATH /* BAD!	WSB */
	    if ((cp = getenv("TEMP")) == NULL || *cp == '\0') {
		if ((cp = getenv("TMP")) == NULL || *cp == '\0') {
		    StrAllocCopy(HomeDir, "C:\\");
		} else {
		    StrAllocCopy(HomeDir, cp);
		}
	    } else {
		StrAllocCopy(HomeDir, cp);
	    }
#else
#ifdef VMS
	    if ((cp = getenv("SYS$LOGIN")) == NULL || *cp == '\0') {
		if ((cp = getenv("SYS$SCRATCH")) == NULL || *cp == '\0') {
		    StrAllocCopy(HomeDir, "sys$scratch:");
		} else {
		    StrAllocCopy(HomeDir, cp);
		}
	    } else {
		StrAllocCopy(HomeDir, cp);
	    }
#else
#if HAVE_UTMP
	    /*
	     *	One could use getlogin() and getpwnam() here instead.
	     */
	    struct passwd *pw = getpwuid(geteuid());

	    if (pw && pw->pw_dir) {
		StrAllocCopy(HomeDir, pw->pw_dir);
	    } else
#endif
	    {
		/*
		 *  Use /tmp; it should be writable.
		 */
		StrAllocCopy(HomeDir, "/tmp");
	    }
#endif /* VMS */
#endif /* DOSPATH */
	} else {
	    StrAllocCopy(HomeDir, cp);
	}
	homedir = (CONST char *)HomeDir;
	atexit(LYHomeDir_free);
    }
    return homedir;
}

/*
 *  This function checks the acceptability of file paths that
 *  are intended to be off the home directory.	The file path
 *  should be passed in fbuffer, together with the size of the
 *  buffer.  The function simplifies the file path, and if it
 *  is acceptible, loads it into fbuffer and returns TRUE.
 *  Otherwise, it does not modify fbuffer and returns FALSE.
 *  If a subdirectory is present and the path does not begin
 *  with "./", that is prefixed to make the situation clear. - FM
 */
PUBLIC BOOLEAN LYPathOffHomeOK ARGS2(
	char *, 	fbuffer,
	size_t, 	fbuffer_size)
{
    char *file = NULL;
    char *cp, *cp1;

    /*
     *	Make sure we have an fbuffer and a string in it. - FM
     */
    if (!fbuffer || fbuffer_size < 2 || fbuffer[0] == '\0') {
	return(FALSE);
    }
    StrAllocCopy(file, fbuffer);
    cp = file;

    /*
     *	Check for an inappropriate reference to the
     *	home directory, and correct it if we can. - FM
     */
#ifdef VMS
    if (!strncasecomp(cp, "sys$login", 9)) {
	if (*(cp + 9) == '\0') {
	    /*
	     *	Reject "sys$login". - FM
	     */
	    FREE(file);
	    return(FALSE);
	}
	if (*(cp + 9) == ':') {
	    cp += 10;
	    if (*cp == '\0') {
		/*
		 *  Reject "sys$login:".  Otherwise, we have
		 *  converted "sys$login:file" to "file", or
		 *  have left a strange path for VMS as it
		 *  was originally. - FM
		 */
		FREE(file);
		return(FALSE);
	    }
	}
    }
#endif /* VMS */
    if (*cp == '~') {
	if (*(cp + 1) == '/') {
	    if (*(cp + 2) != '\0') {
		if ((cp1 = strchr((cp + 2), '/')) != NULL) {
		    /*
		     *	Convert "~/subdir(s)/file"
		     *	to "./subdir(s)/file". - FM
		     */
		    *cp = '.';
		} else {
		    /*
		     *	Convert "~/file" to "file". - FM
		     */
		    cp += 2;
		}
	    } else {
		/*
		 *  Reject "~/". - FM
		 */
		FREE(file);
		return(FALSE);
	    }
	} else if ((*(cp + 1) != '\0') &&
		   (cp1 = strchr((cp + 1), '/')) != NULL) {
	    cp = (cp1 - 1) ;
	    if (*(cp + 2) != '\0') {
		if ((cp1 = strchr((cp + 2), '/')) != NULL) {
		    /*
		     *	Convert "~user/subdir(s)/file" to
		     *	"./subdir(s)/file".  If user is someone
		     *	else, we covered a spoof.  Otherwise,
		     *	we simplified. - FM
		     */
		    *cp = '.';
		} else {
		    /*
		     *	Convert "~user/file" to "file". - FM
		     */
		    cp += 2;
		}
	    } else {
		/*
		 *  Reject "~user/". - FM
		 */
		FREE(file);
		return(FALSE);
	    }
	} else {
	    /*
	     *	Reject "~user". - FM
	     */
	    FREE(file);
	    return(FALSE);
	}
    }

#ifdef VMS
    /*
     *	Check for VMS path specs, and reject if still present. - FM
     */
    if (strchr(cp, ':') != NULL || strchr(cp, ']') != NULL) {
	FREE(file);
	return(FALSE);
    }
#endif /* VMS */

    /*
     *	Check for a URL or absolute path, and reject if present. - FM
     */
    if (is_url(cp) || *cp == '/') {
	FREE(file);
	return(FALSE);
    }

    /*
     *	Simplify it. - FM
     */
    HTSimplify(cp);

    /*
     *	Check if it has a pointless "./". - FM
     */
    if (!strncmp(cp, "./", 2)) {
	if ((cp1 = strchr((cp + 2), '/')) == NULL) {
	    cp += 2;
	}
    }

    /*
     *	Check for spoofing. - FM
     */
    if (*cp == '\0' || *cp == '/' || cp[(strlen(cp) - 1)] == '/' ||
	strstr(cp, "..") != NULL || !strcmp(cp, ".")) {
	FREE(file);
	return(FALSE);
    }

    /*
     *	Load what we have at this point into fbuffer,
     *	trimming if too long, and claim it's OK. - FM
     */
    if (fbuffer_size > 3 && strncmp(cp, "./", 2) && strchr(cp, '/')) {
	/*
	 *  We have a subdirectory and no lead "./", so
	 *  prefix it to make the situation clear. - FM
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
    return(TRUE);
}

/*
 *  This function appends fname to the home path and returns
 *  the full path and filename.  The fname string can be just
 *  a filename (e.g., "lynx_bookmarks.html"), or include a
 *  subdirectory off the home directory, in which case fname
 *  should begin with "./" (e.g., ./BM/lynx_bookmarks.html)
 *  Use LYPathOffHomeOK() to check and/or fix up fname before
 *  calling this function.  On VMS, the resultant full path
 *  and filename are converted to VMS syntax. - FM
 */
PUBLIC void LYAddPathToHome ARGS3(
	char *, 	fbuffer,
	size_t, 	fbuffer_size,
	char *, 	fname)
{
    char *home = NULL;
    char *file = fname;
    int len;

    /*
     *	Make sure we have a buffer. - FM
     */
    if (!fbuffer)
	return;
    if (fbuffer_size < 2) {
	fbuffer[0] = '\0';
	return;
    }
    fbuffer[(fbuffer_size - 1)] = '\0';

    /*
     *	Make sure we have a file name. - FM
     */
    if (!file)
	file = "";

    /*
     *	Set up home string and length. - FM
     */
    StrAllocCopy(home, Home_Dir());
    if (!(home && *home))
	/*
	 *  Home_Dir() has a bug if this ever happens. - FM
	 */
#ifdef VMS
	StrAllocCopy(home, "Error:");
#else
	StrAllocCopy(home, "/error");
#endif /* VMS */
    len = fbuffer_size - (strlen(home) + 1);
    if (len <= 0) {
	/*
	 *  Buffer is smaller than or only big enough for the home path.
	 *  Load what fits of the home path and return.  This will fail,
	 *  but we need something in the buffer. - FM
	 */
	LYstrncpy(fbuffer, home, (fbuffer_size - 1));
	FREE(home);
	return;
    }

#ifdef VMS
    /*
     *	Check whether we have a subdirectory path or just a filename. - FM
     */
    if (!strncmp(file, "./", 2)) {
	/*
	 *  We have a subdirectory path. - FM
	 */
	if (home[strlen(home)-1] == ']') {
	    /*
	     *	We got the home directory, so convert it to
	     *	SHELL syntax and append subdirectory path,
	     *	then convert that to VMS syntax. - FM
	     */
	    char *temp = (char *)calloc(1,
					(strlen(home) + strlen(file) + 10));
	    sprintf(temp, "%s%s", HTVMS_wwwName(home), (file + 1));
	    sprintf(fbuffer, "%.*s",
		    (fbuffer_size - 1), HTVMS_name("", temp));
	    FREE(temp);
	} else {
	    /*
	     *	This will fail, but we need something in the buffer. - FM
	     */
	    sprintf(fbuffer, "%s%.*s", home, len, file);
	}
    } else {
	/*
	 *  We have a file in the home directory. - FM
	 */
	sprintf(fbuffer, "%s%.*s", home, len, file);
    }
#else
    /*
     *	Check whether we have a subdirectory path or just a filename. - FM
     */
    sprintf(fbuffer, "%s/%.*s", home, len,
		     (strncmp(file, "./", 2) ? file : (file + 2)));
#endif /* VMS */
    FREE(home);
}

/*
 *  This function takes a string in the format
 *	"Mon, 01-Jan-96 13:45:35 GMT" or
 *	"Mon,  1 Jan 1996 13:45:35 GMT"" or
 *	"dd-mm-yyyy"
 *  as an argument, and returns its conversion to clock format
 *  (seconds since 00:00:00 Jan 1 1970), or 0 if the string
 *  doesn't match the expected pattern.  It also returns 0 if
 *  the time is in the past and the "absolute" argument is FALSE.
 *  It is intended for handling 'expires' strings in Version 0
 *  cookies homologously to 'max-age' strings in Version 1 cookies,
 *  for which 0 is the minimum, and greater values are handled as
 *  '[max-age seconds] + time(NULL)'.	If "absolute" if TRUE, we
 *  return the clock format value itself, but if anything goes wrong
 *  when parsing the expected patterns, we still return 0. - FM
 */
PUBLIC time_t LYmktime ARGS2(
	char *, 	string,
	BOOL,		absolute)
{
    char *s;
    time_t now, clock2;
    int day, month, year, hour, minutes, seconds;
    char *start;
    char temp[8];

    /*
     *	Make sure we have a string to parse. - FM
     */
    if (!(string && *string))
	return(0);
    s = string;
    if (TRACE)
	fprintf(stderr, "LYmktime: Parsing '%s'\n", s);

    /*
     *	Skip any lead alphabetic "Day, " field and
     *	seek a numberic day field. - FM
     */
    while (*s != '\0' && !isdigit((unsigned char)*s))
	s++;
    if (*s == '\0')
	return(0);

    /*
     *	Get the numeric day and convert to an integer. - FM
     */
    start = s;
    while (*s != '\0' && isdigit((unsigned char)*s))
       s++;
    if (*s == '\0' || (s - start) > 2)
	return(0);
    LYstrncpy(temp, start, (int)(s - start));
    day = atoi(temp);
    if (day < 1 || day > 31)
	return(0);

    /*
     *	Get the month string and convert to an integer. - FM
     */
    while (*s != '\0' && !isalnum((unsigned char)*s))
	s++;
    if (*s == '\0')
	return(0);
    start = s;
    while (*s != '\0' && isalnum((unsigned char)*s))
	s++;
    if ((*s == '\0') ||
	(s - start) < (isdigit((unsigned char)*(s - 1)) ? 2 : 3) ||
	(s - start) > (isdigit((unsigned char)*(s - 1)) ? 2 : 9))
	return(0);
    LYstrncpy(temp, start, (isdigit((unsigned char)*(s - 1)) ? 2 : 3));
    switch (TOUPPER(temp[0])) {
	case '0':
	case '1':
	    month = atoi(temp);
	    if (month < 1 || month > 12) {
		return(0);
	    }
	    break;
	case 'A':
	    if (!strcasecomp(temp, "Apr")) {
		month = 4;
	    } else if (!strcasecomp(temp, "Aug")) {
		month = 8;
	    } else {
		return(0);
	    }
	    break;
	case 'D':
	    if (!strcasecomp(temp, "Dec")) {
		month = 12;
	    } else {
		return(0);
	    }
	    break;
	case 'F':
	    if (!strcasecomp(temp, "Feb")) {
		month = 2;
	    } else {
		return(0);
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
		return(0);
	    }
	    break;
	case 'M':
	    if (!strcasecomp(temp, "Mar")) {
		month = 3;
	    } else if (!strcasecomp(temp, "May")) {
		month = 5;
	    } else {
		return(0);
	    }
	    break;
	case 'N':
	    if (!strcasecomp(temp, "Nov")) {
		month = 11;
	    } else {
		return(0);
	    }
	    break;
	case 'O':
	    if (!strcasecomp(temp, "Oct")) {
		month = 10;
	    } else {
		return(0);
	    }
	    break;
	case 'S':
	    if (!strcasecomp(temp, "Sep")) {
		month = 9;
	    } else {
		return(0);
	    }
	    break;
	default:
	    return(0);
    }

    /*
     *	Get the numeric year string and convert to an integer. - FM
     */
    while (*s != '\0' && !isdigit((unsigned char)*s))
	s++;
    if (*s == '\0')
	return(0);
    start = s;
    while (*s != '\0' && isdigit((unsigned char)*s))
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
	 * the broken sender software intends.	(This totally breaks
	 * in 2100 -- setting up the next crisis...) - BL
	 */
	if (atoi(start) >= 70)
	    LYstrncpy(temp, "19", 2);
	else
	    LYstrncpy(temp, "20", 2);
	strncat(temp, start, 2);
	temp[4] = '\0';
    } else {
	return(0);
    }
    year = atoi(temp);

    /*
     *	Get the numeric hour string and convert to an integer. - FM
     */
    while (*s != '\0' && !isdigit((unsigned char)*s))
	s++;
    if (*s == '\0') {
	hour = 0;
	minutes = 0;
	seconds = 0;
    } else {
	start = s;
	while (*s != '\0' && isdigit((unsigned char)*s))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return(0);
	LYstrncpy(temp, start, (int)(s - start));
	hour = atoi(temp);

	/*
	 *  Get the numeric minutes string and convert to an integer. - FM
	 */
	while (*s != '\0' && !isdigit((unsigned char)*s))
	    s++;
	if (*s == '\0')
	    return(0);
	start = s;
	while (*s != '\0' && isdigit((unsigned char)*s))
	    s++;
	if (*s != ':' || (s - start) > 2)
	    return(0);
	LYstrncpy(temp, start, (int)(s - start));
	minutes = atoi(temp);

	/*
	 *  Get the numeric seconds string and convert to an integer. - FM
	 */
	while (*s != '\0' && !isdigit((unsigned char)*s))
	    s++;
	if (*s == '\0')
	    return(0);
	start = s;
	while (*s != '\0' && isdigit((unsigned char)*s))
	    s++;
	if (*s == '\0' || (s - start) > 2)
	    return(0);
	LYstrncpy(temp, start, (int)(s - start));
	seconds = atoi(temp);
    }

    /*
     *	Convert to clock format (seconds since 00:00:00 Jan 1 1970),
     *	but then zero it if it's in the past and "absolute" is not
     *	TRUE.  - FM
     */
    month -= 3;
    if (month < 0) {
	 month += 12;
	 year--;
    }
    day += (year - 1968)*1461/4;
    day += ((((month*153) + 2)/5) - 672);
    clock2 = (time_t)((day * 60 * 60 * 24) +
		     (hour * 60 * 60) +
		     (minutes * 60) +
		     seconds);
    if (absolute == FALSE && clock2 <= time(NULL))
	clock2 = (time_t)0;
    if (TRACE && clock2 > 0)
	fprintf(stderr,
		"LYmktime: clock=%ld, ctime=%s", (long) clock2, ctime(&clock2));

    return(clock2);
}

#if ! HAVE_PUTENV
/*
 *  No putenv on the next so we use this code instead!
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

#include <sys/types.h>
#include <errno.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern int errno;
#endif /* STDC_HEADERS */

#if defined(STDC_HEADERS) || defined(USG)
#include <string.h>
#ifdef NOTDEFINED
#define index strchr
#define bcopy(s, d, n) memcpy((d), (s), (n))
#endif /* NOTDEFINED */
#else /* Not (STDC_HEADERS or USG): */
#include <strings.h>
#endif /* STDC_HEADERS or USG */

#ifndef NULL
#define NULL 0
#endif /* !NULL */

#if !__STDC__
#define const
#endif /* !__STDC__ */

extern char **environ;

/*
 *  Put STRING, which is of the form "NAME=VALUE", in  the environment.
 */
PUBLIC int putenv ARGS1(
	CONST char *,	string)
{
  char *name_end = strchr(string, '=');
  register size_t size;
  register char **ep;

  if (name_end == NULL)
    {
      /* Remove the variable from the environment.  */
      size = strlen (string);
      for (ep = environ; *ep != NULL; ++ep)
	if (!strncmp (*ep, string, size) && (*ep)[size]  == '=')
	  {
	    while (ep[1] != NULL)
	      {
		ep[0] = ep[1];
		++ep;
	      }
	    *ep = NULL;
	    return 0;
	  }
    }

  size = 0;
  for (ep = environ; *ep != NULL; ++ep)
    if (!strncmp (*ep, string, name_end - string) && (*ep)[name_end - string] == '=')
      break;
    else
      ++size;

  if (*ep == NULL)
    {
      static char **last_environ = NULL;
      char **new_environ = (char **) malloc ((size + 2) * sizeof (char *));
      if (new_environ == NULL)
	return -1;
      (void) memcpy((char *)new_environ, (char *)environ, size * sizeof(char *));
      new_environ[size] = (char *) string;
      new_environ[size + 1] = NULL;
      if (last_environ != NULL)
	free ((char *) last_environ);
      last_environ = new_environ;
      environ = new_environ;
    }
  else
    *ep = (char *) string;

  return 0;
}
#endif /* !HAVE_PUTENV */

#ifdef NEED_REMOVE
int remove ARGS1(char *, name)
{
    return unlink(name);
}
#endif

#ifdef UNIX
/*
 * Open a file that we don't want other users to see.  For new files, the umask
 * will suffice; however if the file already exists we'll change permissions
 * first, before opening it.  If the chmod fails because of some reason other
 * than a non-existent file, there's no point in trying to open it.
 */
PRIVATE FILE *OpenHiddenFile ARGS2(char *, name, char *, mode)
{
    int save = umask(HIDE_UMASK);
    FILE *fp = 0;
    if (chmod(name, HIDE_CHMOD) == 0 || errno == ENOENT)
	fp = fopen(name, mode);
    umask(save);
    return fp;
}
#else
# ifndef VMS
#  define OpenHiddenFile(name, mode) fopen(name, mode)
# endif
#endif

PUBLIC FILE *LYNewBinFile ARGS1(char *, name)
{
#ifdef VMS
    FILE *fp = fopen (name, "wb", "mbc=32");
    chmod(name, HIDE_CHMOD);
#else
    FILE *fp = OpenHiddenFile(name, "wb");
#endif
    return fp;
}

PUBLIC FILE *LYNewTxtFile ARGS1(char *, name)
{
#ifdef VMS
    FILE *fp = fopen (name, "w", "shr=get");
    chmod(name, HIDE_CHMOD);
#else
    FILE *fp = OpenHiddenFile(name, "w");
#endif
    return fp;
}

PUBLIC FILE *LYAppendToTxtFile ARGS1(char *, name)
{
#ifdef VMS
    FILE *fp = fopen (name, "a+", "shr=get");
    chmod(name, HIDE_CHMOD);
#else
    FILE *fp = OpenHiddenFile(name, "a+");
#endif
    return fp;
}

#ifdef UNIX
/*
 *  Restore normal permisions to a copy of a file that we have created
 *  with temp file restricted permissions.  The normal umask should
 *  apply for user files. - kw
 */
PUBLIC void LYRelaxFilePermissions ARGS1(CONST char *, name)
{
    int mode;
    struct stat stat_buf;
    if (stat(name, &stat_buf) == 0 &&
	S_ISREG(stat_buf.st_mode) &&
	(mode = (stat_buf.st_mode & 0777)) == HIDE_CHMOD) {
	/*
	 *  It looks plausible that this is a file we created with
	 *  temp file paranoid permissions (and the umask wasn't even
	 *  more restrictive when it was copied). - kw
	 */
	int save = umask(HIDE_UMASK);
	mode = ((mode & 0700) | 0066) & ~save;
	umask(save);
	chmod(name, mode);
    }
}
#endif
