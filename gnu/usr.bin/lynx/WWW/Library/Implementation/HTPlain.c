/*		Plain text object		HTWrite.c
**		=================
**
**	This version of the stream object just writes to a socket.
**	The socket is assumed open and left open.
**
**	Bugs:
**		strings written must be less than buffer size.
*/

#include <HTUtils.h>
#include <LYCharVals.h>  /* S/390 -- gil -- 0288 */

#include <HTPlain.h>

#include <HTChunk.h>
#include <HText.h>
#include <HTStyle.h>
#define Lynx_HTML_Handler
#include <HTML.h>		/* styles[] */

#define BUFFER_SIZE 4096;	/* Tradeoff */

#include <HTMLDTD.h>
#include <HTCJK.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <UCAux.h>

#include <LYCharSets.h>
#include <LYLeaks.h>

extern BOOL HTPassEightBitRaw;
extern BOOL HTPassHighCtrlRaw;
extern HTCJKlang HTCJK;

PUBLIC int HTPlain_lastraw = -1;

/*		HTML Object
**		-----------
*/
struct _HTStream {
    CONST HTStreamClass *	isa;
    HText *			text;
    /*
    **	The node_anchor UCInfo and handle for the input (PARSER) stage. - FM
    */
    LYUCcharset 	*	inUCI;
    int 			inUCLYhndl;
    /*
    **	The node_anchor UCInfo and handle for the output (HTEXT) stage. - FM
    */
    LYUCcharset	*		outUCI;
    int outUCLYhndl;
    /*
    **	Counter, value, buffer and pointer for UTF-8 handling. - FM
    */
    char			utf_count;
    UCode_t			utf_char;
    char			utf_buf[8];
    char *			utf_buf_p;
    /*
    **	The charset transformation structure. - FM
    */
    UCTransParams		T;
};

PRIVATE char replace_buf [64];	      /* buffer for replacement strings */

PRIVATE void HTPlain_getChartransInfo ARGS2(
	HTStream *,		me,
	HTParentAnchor *,	anchor)
{
    if (me->inUCLYhndl < 0) {
	HTAnchor_copyUCInfoStage(anchor, UCT_STAGE_PARSER, UCT_STAGE_MIME,
					 UCT_SETBY_PARSER);
	me->inUCLYhndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_PARSER);
    }
    if (me->outUCLYhndl < 0) {
	int chndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_HTEXT);
	if (chndl < 0) {
	    chndl = current_char_set;
	    HTAnchor_setUCInfoStage(anchor, chndl,
				    UCT_STAGE_HTEXT, UCT_SETBY_DEFAULT);
	}
	HTAnchor_setUCInfoStage(anchor, chndl,
				UCT_STAGE_HTEXT, UCT_SETBY_DEFAULT);
	me->outUCLYhndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_HTEXT);
    }
    me->inUCI = HTAnchor_getUCInfoStage(anchor, UCT_STAGE_PARSER);
    me->outUCI = HTAnchor_getUCInfoStage(anchor, UCT_STAGE_HTEXT);
}

/*	Write the buffer out to the socket
**	----------------------------------
*/

/*_________________________________________________________________________
**
**			A C T I O N	R O U T I N E S
*/

PRIVATE void HTPlain_write PARAMS((
	HTStream *		me,
	CONST char *		s,
	int			l));

/*	Character handling
**	------------------
*/
PRIVATE void HTPlain_put_character ARGS2(
	HTStream *,		me,
	char,			c)
{
#ifdef REMOVE_CR_ONLY
    /*
    **	Throw away \r's.
    */
    if (c != '\r') {
       HText_appendCharacter(me->text, c);
    }
#else
    /*
    **	See HTPlain_write() for explanations of the following code
    **	(we've been called via HTPlain_put_string() to do for each
    **	character of a terminated string what HTPlain_write() does
    **	via a while loop for each character in a stream of given
    **	length). - FM
    */
    if ((HTPlain_lastraw == '\r') && c == '\n') {
	HTPlain_lastraw = -1;
	return;
    }
    HTPlain_lastraw = c;
    if (c == '\r') {
	HText_appendCharacter(me->text, '\n');
    } else if (TOASCII((unsigned char)c) >= 127) {  /* S/390 -- gil -- 0305 */
	/*
	**  For now, don't repeat everything here
	**  that has been done below - KW
	*/
	HTPlain_write(me, &c, 1);
    } else if (HTCJK != NOCJK) {
	HText_appendCharacter(me->text, c);
    } else if (TOASCII((unsigned char)c) >= 127 && TOASCII((unsigned char)c) < 161 &&
	       HTPassHighCtrlRaw) {
	HText_appendCharacter(me->text, c);
    } else if ((unsigned char)c == CH_NBSP) { /* S/390 -- gil -- 0341 */
	HText_appendCharacter(me->text, ' ');
    } else if ((unsigned char)c == CH_SHY) {
	return;
    } else if (((unsigned char)c >= ' ' && TOASCII((unsigned char)c) < 127) ||
	       c == '\n' || c == '\t') {
	HText_appendCharacter(me->text, c);
    } else if (TOASCII((unsigned char)c) > 160) {
	if (!HTPassEightBitRaw &&
	    !((me->outUCLYhndl == LATIN1) ||
	      (me->outUCI->enc & (UCT_CP_SUPERSETOF_LAT1)))) {
	    int len, high, low, i, diff = 1;
	    CONST char * name;
	    UCode_t value = (UCode_t)FROMASCII((TOASCII((unsigned char)c) - 160));

	    name = HTMLGetEntityName(value);
	    len =  strlen(name);
	    for (low = 0, high = HTML_dtd.number_of_entities;
		high > low;
		diff < 0 ? (low = i+1) : (high = i)) {
		/* Binary search */
		i = (low + (high-low)/2);
		diff = AS_ncmp(HTML_dtd.entity_names[i], name, len);
		if (diff == 0) {
		    HText_appendText(me->text,
				     LYCharSets[me->outUCLYhndl][i]);
		    break;
		}
	    }
	    if (diff) {
		HText_appendCharacter(me->text, c);
	    }
	} else {
	    HText_appendCharacter(me->text, c);
	}
    }
#endif /* REMOVE_CR_ONLY */
}


/*	String handling
**	---------------
**
*/
PRIVATE void HTPlain_put_string ARGS2(HTStream *, me, CONST char*, s)
{
#ifdef REMOVE_CR_ONLY
    HText_appendText(me->text, s);
#else
    CONST char * p;

    if (s == NULL)
	return;
    for (p = s; *p; p++) {
	HTPlain_put_character(me, *p);
    }
#endif /* REMOVE_CR_ONLY */
}


/*
**	Entry function for displayed text/plain and WWW_SOURCE strings. - FM
**	---------------------------------------------------------------
*/
PRIVATE void HTPlain_write ARGS3(HTStream *, me, CONST char*, s, int, l)
{
    CONST char * p;
    CONST char * e = s+l;
    char c;
    unsigned c_unsign;
    BOOL chk;
    UCode_t code, uck = -1;
    char saved_char_in = '\0';

    for (p = s; p < e; p++) {
#ifdef REMOVE_CR_ONLY
	/*
	**  Append the whole string, but remove any \r's. - FM
	*/
	if (*p != '\r') {
	    HText_appendCharacter(me->text, *p);
	}
#else
	/*
	**  Try to handle lone LFs, CRLFs and lone CRs
	**  as newline, and to deal with control, ASCII,
	**  and 8-bit characters based on best guesses
	**  of what's appropriate. - FM
	*/
	if ((HTPlain_lastraw == '\r') && *p == '\n') {
	    HTPlain_lastraw = -1;
	    continue;
	}
	HTPlain_lastraw = *p;
	if (*p == '\r') {
	    HText_appendCharacter(me->text, '\n');
	    continue;
	}
	/*
	**  Make sure the character is handled as Unicode
	**  whenever that's appropriate.  - FM
	*/
	c = *p;
	c_unsign = (unsigned char)c;
	code = (UCode_t)c_unsign;
	saved_char_in = '\0';
	/*
	**  Combine any UTF-8 multibytes into Unicode
	**  to check for special characters. - FM
	*/
	if (me->T.decode_utf8) {
	    /*
	    **	Combine UTF-8 into Unicode.
	    **	Incomplete characters silently ignored.
	    **	from Linux kernel's console.c - KW
	    */
	    if (TOASCII(c_unsign) > 127) {  /* S/390 -- gil -- 0371 */
		/*
		**  We have an octet from a multibyte character. - FM
		*/
		if (me->utf_count > 0 && (c & 0xc0) == 0x80) {
		    /*
		    **	Adjust the UCode_t value, add the octet
		    **	to the buffer, and decrement the byte
		    **	count. - FM
		    */
		    me->utf_char = (me->utf_char << 6) | (c & 0x3f);
		    me->utf_count--;
		    *(me->utf_buf_p) = c;
		    (me->utf_buf_p)++;
		    if (me->utf_count == 0) {
			/*
			**  Got a complete multibyte character.
			*/
			*(me->utf_buf_p) = '\0';
			code = me->utf_char;
			if (code > 0 && code < 256) {
			    c = FROMASCII((char)code);
			    c_unsign = (unsigned char)c;
			}
		    } else {
			/*
			**  Get the next byte. - FM
			*/
			continue;
		    }
		} else {
		    /*
		    **	Start handling a new multibyte character. - FM
		    */
		    me->utf_buf_p[0] = c;
		    me->utf_buf_p = &me->utf_buf[1];
		    if ((*p & 0xe0) == 0xc0) {
			me->utf_count = 1;
			me->utf_char = (c & 0x1f);
		    } else if ((*p & 0xf0) == 0xe0) {
			me->utf_count = 2;
			me->utf_char = (c & 0x0f);
		    } else if ((*p & 0xf8) == 0xf0) {
			me->utf_count = 3;
			me->utf_char = (c & 0x07);
		    } else if ((*p & 0xfc) == 0xf8) {
			me->utf_count = 4;
			me->utf_char = (c & 0x03);
		    } else if ((*p & 0xfe) == 0xfc) {
			me->utf_count = 5;
			me->utf_char = (c & 0x01);
		    } else {
			/*
			 *  We got garbage, so ignore it. - FM
			 */
			me->utf_count = 0;
			me->utf_buf_p[0] = '\0';
			me->utf_buf_p = me->utf_buf;
		    }
		    /*
		    **	Get the next byte. - FM
		    */
		    continue;
		}
	    } else if (me->utf_count > 0) {
		/*
		**  Got an ASCII character when expecting
		**  UTF-8 multibytes, so ignore the buffered
		**  multibye characters and fall through with
		**  the current ASCII character. - FM
		*/
		me->utf_count = 0;
		me->utf_buf[0] = '\0';
		me->utf_buf_p = me->utf_buf;
		code = (UCode_t)c_unsign;
	    } else {
		/*
		**  Got a valid ASCII character, so fall
		**  through with it. - FM
		*/
		code = (UCode_t)c_unsign;
	    }
	}
	/*
	**  Convert characters from non-UTF-8 charsets
	**  to Unicode (if appropriate). - FM
	*/
	if (!(me->T.decode_utf8 &&
	      (unsigned char)(*p) > 127)) {
#ifdef NOTDEFINED
	    if (me->T.strip_raw_char_in)
		saved_char_in = c;
#endif /* NOTDEFINED */
	if (me->T.trans_to_uni &&
	    (TOASCII(code) >= LYlowest_eightbit[me->inUCLYhndl] ||  /* S/390 -- gil -- 0389 */
	     (code < ' ' && code != 0 &&
	     me->T.trans_C0_to_uni))) {
		/*
		**  Convert the octet to Unicode. - FM
		*/
	    code = (UCode_t)UCTransToUni(c, me->inUCLYhndl);
	    if (code > 0) {
		    saved_char_in = c;
		if (code < 256) {
			c = FROMASCII((char)code);
			c_unsign = (unsigned char)c;
		}
	    }
	    } else if (code < 32 && code != 0 &&
		       me->T.trans_C0_to_uni) {
		/*
		**  Quote from SGML.c:
		**  	"This else if may be too ugly to keep. - KW"
		*/
		if (me->T.trans_from_uni &&
		    (((code = UCTransToUni(c, me->inUCLYhndl)) >= 32) ||
		     (me->T.transp &&
		      (code = UCTransToUni(c, me->inUCLYhndl)) > 0))) {
		    saved_char_in = c;
		    if (code < 256) {
			c = FROMASCII((char)code);
			c_unsign = (unsigned char)c;
		    }
		} else {
		    uck = -1;
		    if (me->T.transp) {
			uck = UCTransCharStr(replace_buf, 60, c,
					     me->inUCLYhndl,
					     me->inUCLYhndl, NO);
		    }
		    if (!me->T.transp || uck < 0) {
			uck = UCTransCharStr(replace_buf, 60, c,
					     me->inUCLYhndl,
					     me->outUCLYhndl, YES);
		    }
		    if (uck == 0) {
			continue;
		    } else if (uck < 0) {
			me->utf_buf[0] = '\0';
			code = (unsigned char)c;
		    } else {
			c = replace_buf[0];
			if (c && replace_buf[1]) {
			    HText_appendText(me->text, replace_buf);
			    continue;
			}
		    }
		    me->utf_buf[0] = '\0';
		    code = (unsigned char)c;
		} /*  Next line end of ugly stuff for C0. - KW */
	    } else {
		me->utf_buf[0] = '\0';
		code = (unsigned char)c;
	    }
	}
	/*
	**  At this point we have either code in Unicode
	**  (and c in latin1 if code is in the latin1 range),
	**  or code and c will have to be passed raw.
	*/

	/*
	**  If CJK mode is on, we'll assume the document matches
	**  the user's display character set, and if not, the
	**  user should toggle off raw/CJK mode to reload. - FM
	*/
	if (HTCJK != NOCJK) {
	    HText_appendCharacter(me->text, c);

#define PASSHICTRL (me->T.transp || \
		    code >= LYlowest_eightbit[me->inUCLYhndl])
#define PASS8859SPECL me->T.pass_160_173_raw
#define PASSHI8BIT (HTPassEightBitRaw || \
		    (me->T.do_8bitraw && !me->T.trans_from_uni))
	/*
	**  If HTPassHighCtrlRaw is set (e.g., for KOI8-R) assume the
	**  document matches and pass 127-160 8-bit characters.  If it
	**  doesn't match, the user should toggle raw/CJK mode off. - FM
	*/
	} else if (TOASCII(code) >= 127 && TOASCII(code) < 161 &&  /* S/390 -- gil -- 0427 */
		   PASSHICTRL && PASS8859SPECL) {
	    HText_appendCharacter(me->text, c);
	} else if (code == CH_SHY && PASS8859SPECL) {
	    HText_appendCharacter(me->text, c);
	/*
	**  If neither HTPassHighCtrlRaw nor CJK is set, play it safe
	**  and treat 160 (nbsp) as an ASCII space (32). - FM
	*/
	} else if (code == CH_NBSP) {
	    HText_appendCharacter(me->text, ' ');
	/*
	**  If neither HTPassHighCtrlRaw nor CJK is set, play it safe
	**  and ignore 173 (shy). - FM
	*/
	} else if (code == CH_SHY) {
	    continue;
	/*
	**  If we get to here, pass the displayable ASCII characters. - FM
	*/
	} else if ((code >= ' ' && TOASCII(code) < 127) ||
		   (PASSHI8BIT &&
		    c >= LYlowest_eightbit[me->outUCLYhndl]) ||
		   *p == '\n' || *p == '\t') {
	    HText_appendCharacter(me->text, c);
	/*
	**  Use an ASCII space (32) for ensp, emsp or thinsp. - FM
	*/
	} else if (code == 8194 || code == 8195 || code == 8201) {
	    HText_appendCharacter(me->text, ' ');
	/*
	**  If we want the raw character, pass it now. - FM
	*/
	} else if (me->T.use_raw_char_in && saved_char_in) {
	    HText_appendCharacter(me->text, saved_char_in);
/******************************************************************
 *   I. LATIN-1 OR UCS2  TO  DISPLAY CHARSET
 ******************************************************************/
	} else if ((chk = (me->T.trans_from_uni && code >= 160)) &&
		   (uck = UCTransUniChar(code,
					 me->outUCLYhndl)) >= ' ' &&  /* S/390 -- gil -- 0464 */
		   uck < 256) {
	    CTRACE(tfp, "UCTransUniChar returned 0x%.2lX:'%c'.\n",
			uck, FROMASCII((char)uck));
	    HText_appendCharacter(me->text, ((char)(uck & 0xff)));
	} else if (chk &&
		   (uck == -4 ||
		    (me->T.repl_translated_C0 && uck > 0 && uck < ' ')) &&  /* S/390 -- gil -- 0481 */
		   /*
		   **  Not found; look for replacement string.
		   */
		   (uck = UCTransUniCharStr(replace_buf, 60, code,
					    me->outUCLYhndl, 0) >= 0)) {
	    /*
	    **	No further tests for valididy - assume that whoever
	    **	defined replacement strings knew what she was doing.
	    */
	    HText_appendText(me->text, replace_buf);
	/*
	**  If we get to here, and should have translated,
	**  translation has failed so far.
	*/
	} else if (chk && TOASCII(code) > 127 && me->T.output_utf8) {  /* S/390 -- gil -- 0498 */
	    /*
	    **	We want UTF-8 output, so do it now. - FM
	    */
	    if (*me->utf_buf) {
		HText_appendText(me->text, me->utf_buf);
		me->utf_buf[0] = '\0';
		me->utf_buf_p = me->utf_buf;
	    } else if (UCConvertUniToUtf8(code, replace_buf)) {
		HText_appendText(me->text, replace_buf);
	    } else {
		/*
		**  Out of luck, so use the UHHH notation (ugh). - gil
		*/  /* S/390 -- gil -- 0517 */
		sprintf(replace_buf, "U%.2lX", TOASCII(code));
		HText_appendText(me->text, replace_buf);
	    }
#ifdef NOTDEFINED
	} else if (me->T.strip_raw_char_in &&
		   (unsigned char)*p >= 192 &&
		   (unsigned char)*p < 255) {
	    /*
	    **	KOI special: strip high bit, gives
	    **	(somewhat) readable ASCII.
	    */
	    HText_appendCharacter(me->text, (char)(*p & 0x7f));
#endif /* NOTDEFINED */
	   /*
	   **  If we don't actually want the character,
	   **  make it safe and output that now. - FM
	   */
	   } else if ((c_unsign > 0 &&
		      (int) c_unsign < LYlowest_eightbit[me->outUCLYhndl]) ||
		      (me->T.trans_from_uni && !HTPassEightBitRaw)) {
	    /*
	    **	If we do not have the "7-bit approximations" as our
	    **	output character set (in which case we did it already)
	    **	seek a translation for that.  Otherwise, or if the
	    **	translation fails, use UHHH notation. - FM
	    */
	    if ((chk = (me->outUCLYhndl !=
			UCGetLYhndl_byMIME("us-ascii"))) &&
		   (uck = UCTransUniChar(code,
					 UCGetLYhndl_byMIME("us-ascii")))
				      >= ' ' && TOASCII(uck) < 127) {  /* S/390 -- gil -- 0535 */
		/*
		**  Got an ASCII character (yippey). - FM
		*/
	    c = FROMASCII((char)uck);
	    HText_appendCharacter(me->text, c);
	} else if ((chk && uck == -4) &&
		       (uck = UCTransUniCharStr(replace_buf,
						60, code,
						UCGetLYhndl_byMIME("us-ascii"),
						0) >= 0)) {
		/*
		**  Got a repacement string (yippey). - FM
		*/
		HText_appendText(me->text, replace_buf);
	    } else if (code == 8204 || code == 8205) {
		/*
		**	Ignore 8204 (zwnj) or 8205 (zwj), if we get to here. - FM
		*/
		CTRACE(tfp, "HTPlain_write: Ignoring '%ld'.\n", code);
	    } else if (code == 8206 || code == 8207) {
		/*
		**	Ignore 8206 (lrm) or 8207 (rlm), if we get to here. - FM
		*/
		CTRACE(tfp, "HTPlain_write: Ignoring '%ld'.\n", code);
	    } else {
		/*
		**  Out of luck, so use the UHHH notation (ugh). - FM
		*/
			/* do not print UHHH for now
			sprintf(replace_buf, "U%.2lX", code);
			HText_appendText(me->text, replace_buf);
			*/
		}
		/*
		**  If we get to here and have a monobyte character,
		**  pass it. - FM
		*/
	} else if (c_unsign != 0 && c_unsign < 256) {
		HText_appendCharacter(me->text, c);
	    }
#endif /* REMOVE_CR_ONLY */
    }
}

/*	Free an HTML object
**	-------------------
**
**	Note that the SGML parsing context is freed, but the created object is
**	not, as it takes on an existence of its own unless explicitly freed.
*/
PRIVATE void HTPlain_free ARGS1(
	HTStream *,	me)
{
    FREE(me);
}

/*	End writing
*/
PRIVATE void HTPlain_abort ARGS2(
	HTStream *,	me,
	HTError,	e GCC_UNUSED)
{
    HTPlain_free(me);
}

/*		Structured Object Class
**		-----------------------
*/
PUBLIC CONST HTStreamClass HTPlain =
{
	"PlainPresenter",
	HTPlain_free,
	HTPlain_abort,
	HTPlain_put_character,	HTPlain_put_string, HTPlain_write,
};

/*		New object
**		----------
*/
PUBLIC HTStream* HTPlainPresent ARGS3(
	HTPresentation *,	pres GCC_UNUSED,
	HTParentAnchor *,	anchor,
	HTStream *,		sink GCC_UNUSED)
{

    HTStream* me = (HTStream*)malloc(sizeof(*me));
    if (me == NULL)
	outofmem(__FILE__, "HTPlain_new");
    me->isa = &HTPlain;

    HTPlain_lastraw = -1;

    me->utf_count = 0;
    me->utf_char = 0;
    me->utf_buf[0] = me->utf_buf[6] =me->utf_buf[7] = '\0';
    me->utf_buf_p = me->utf_buf;
    me->outUCLYhndl = HTAnchor_getUCLYhndl(anchor,UCT_STAGE_HTEXT);
    me->inUCLYhndl = HTAnchor_getUCLYhndl(anchor,UCT_STAGE_PARSER);
    HTPlain_getChartransInfo(me, anchor);
    UCSetTransParams(&me->T,
		     me->inUCLYhndl, me->inUCI,
		     me->outUCLYhndl,
		     HTAnchor_getUCInfoStage(anchor,UCT_STAGE_HTEXT));

    me->text = HText_new(anchor);
    HText_setStyle(me->text, LYstyles(HTML_XMP) );
    HText_beginAppend(me->text);

    return (HTStream*) me;
}
