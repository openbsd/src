/*			MIME Message Parse			HTMIME.c
**			==================
**
**	This is RFC 1341-specific code.
**	The input stream pushed into this parser is assumed to be
**	stripped on CRs, ie lines end with LF, not CR LF.
**	(It is easy to change this except for the body part where
**	conversion can be slow.)
**
** History:
**	   Feb 92	Written Tim Berners-Lee, CERN
**
*/
#include "HTUtils.h"
#include "HTMIME.h"		/* Implemented here */
#include "HTAlert.h"
#include "HTCJK.h"
#include "UCMap.h"
#include "UCDefs.h"
#include "UCAux.h"

#include "LYCharSets.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

extern CONST char *LYchar_set_names[];
extern BOOLEAN LYRawMode;
extern BOOL HTPassEightBitRaw;
extern HTCJKlang HTCJK;

extern void LYSetCookie PARAMS((
	CONST char *	SetCookie,
	CONST char *	SetCookie2,
	CONST char *	address));
extern time_t LYmktime PARAMS((char *string, BOOL absolute));


/*		MIME Object
**		-----------
*/

typedef enum _MIME_state {
	MIME_TRANSPARENT,	/* put straight through to target ASAP! */
	miBEGINNING_OF_LINE,	/* first character and not a continuation */
	miA,
	miACCEPT_RANGES,
	miAGE,
	miAL,
	miALLOW,
	miALTERNATES,
	miC,
	miCACHE_CONTROL,
	miCO,
	miCOOKIE,
	miCON,
	miCONNECTION,
	miCONTENT_,
	miCONTENT_BASE,
	miCONTENT_DISPOSITION,
	miCONTENT_ENCODING,
	miCONTENT_FEATURES,
	miCONTENT_L,
	miCONTENT_LANGUAGE,
	miCONTENT_LENGTH,
	miCONTENT_LOCATION,
	miCONTENT_MD5,
	miCONTENT_RANGE,
	miCONTENT_T,
	miCONTENT_TRANSFER_ENCODING,
	miCONTENT_TYPE,
	miDATE,
	miE,
	miETAG,
	miEXPIRES,
	miKEEP_ALIVE,
	miL,
	miLAST_MODIFIED,
	miLINK,
	miLOCATION,
	miP,
	miPR,
	miPRAGMA,
	miPROXY_AUTHENTICATE,
	miPUBLIC,
	miRETRY_AFTER,
	miS,
	miSAFE,
	miSE,
	miSERVER,
	miSET_COOKIE,
	miSET_COOKIE1,
	miSET_COOKIE2,
	miT,
	miTITLE,
	miTRANSFER_ENCODING,
	miU,
	miUPGRADE,
	miURI,
	miV,
	miVARY,
	miVIA,
	miW,
	miWARNING,
	miWWW_AUTHENTICATE,
	miSKIP_GET_VALUE,	/* Skip space then get value */
	miGET_VALUE,		/* Get value till white space */
	miJUNK_LINE,		/* Ignore the rest of this folded line */
	miNEWLINE,		/* Just found a LF .. maybe continuation */
	miCHECK,		/* check against check_pointer */
	MIME_NET_ASCII, 	/* Translate from net ascii */
	MIME_IGNORE		/* Ignore entire file */
	/* TRANSPARENT and IGNORE are defined as stg else in _WINDOWS */
} MIME_state;

#define VALUE_SIZE 1024 	/* @@@@@@@ Arbitrary? */
struct _HTStream {
	CONST HTStreamClass *	isa;

	BOOL			net_ascii;	/* Is input net ascii? */
	MIME_state		state;		/* current state */
	MIME_state		if_ok;		/* got this state if match */
	MIME_state		field;		/* remember which field */
	MIME_state		fold_state;	/* state on a fold */
	CONST char *		check_pointer;	/* checking input */

	char *			value_pointer;	/* storing values */
	char			value[VALUE_SIZE];

	HTParentAnchor *	anchor; 	/* Given on creation */
	HTStream *		sink;		/* Given on creation */

	char *			boundary;	/* For multipart */
	char *			set_cookie;	/* Set-Cookie */
	char *			set_cookie2;	/* Set-Cookie2 */

	HTFormat		encoding;	/* Content-Transfer-Encoding */
	char *			compression_encoding;
	HTFormat		format; 	/* Content-Type */
	HTStream *		target; 	/* While writing out */
	HTStreamClass		targetClass;

	HTAtom *		targetRep;	/* Converting into? */
};

/*
**  This function is for trimming off any paired
**  open- and close-double quotes from header values.
**  It does not parse the string for embedded quotes,
**  and will not modify the string unless both the
**  first and last characters are double-quotes. - FM
*/
PUBLIC void HTMIME_TrimDoubleQuotes ARGS1(
	char *, 	value)
{
    int i;
    char *cp = value;

    if (!(cp && *cp) || *cp != '\"')
	return;

    i = strlen(cp);
    if (cp[(i - 1)] != '\"')
	return;
    else
	cp[(i - 1)] = '\0';

    for (i = 0; value[i]; i++)
	value[i] = cp[(i +1)];
}

/*_________________________________________________________________________
**
**			A C T I O N	R O U T I N E S
*/

/*	Character handling
**	------------------
**
**	This is a FSM parser which is tolerant as it can be of all
**	syntax errors.	It ignores field names it does not understand,
**	and resynchronises on line beginnings.
*/
PRIVATE void HTMIME_put_character ARGS2(
	HTStream *,	me,
	char,		c)
{
    int i, j;

    if (me->state == MIME_TRANSPARENT) {
	(*me->targetClass.put_character)(me->target, c);/* MUST BE FAST */
	return;
    }

    /*
    **	This slightly simple conversion just strips CR and turns LF to
    **	newline.  On unix LF is \n but on Mac \n is CR for example.
    **	See NetToText for an implementation which preserves single CR or LF.
    */
    if (me->net_ascii) {
	c = FROMASCII(c);
	if (c == CR)
	    return;
	else if (c == LF)
	    c = '\n';
    }

    switch(me->state) {

    case MIME_IGNORE:
	return;

    case MIME_TRANSPARENT:		/* Not reached see above */
	(*me->targetClass.put_character)(me->target, c);
	return;

    case MIME_NET_ASCII:
	(*me->targetClass.put_character)(me->target, c); /* MUST BE FAST */
	return;

    case miNEWLINE:
	if (c != '\n' && WHITE(c)) {		/* Folded line */
	    me->state = me->fold_state; /* pop state before newline */
	    break;
	}

	/*	else Falls through */

    case miBEGINNING_OF_LINE:
	me->net_ascii = YES;
	switch (c) {
	case 'a':
	case 'A':
	    me->state = miA;
	    if (TRACE)
		fprintf(stderr,
		       "HTMIME: Got 'A' at beginning of line, state now A\n");
	    break;

	case 'c':
	case 'C':
	    me->state = miC;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'C' at beginning of line, state now C\n");
	    break;

	case 'd':
	case 'D':
	    me->check_pointer = "ate:";
	    me->if_ok = miDATE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf (stderr,
	      "HTMIME: Got 'D' at beginning of line, checking for 'ate:'\n");
	    break;

	case 'e':
	case 'E':
	    me->state = miE;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'E' at beginning of line, state now E\n");
	    break;

	case 'k':
	case 'K':
	    me->check_pointer = "eep-alive:";
	    me->if_ok = miKEEP_ALIVE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
	 "HTMIME: Got 'K' at beginning of line, checking for 'eep-alive:'\n");
	    break;

	case 'l':
	case 'L':
	    me->state = miL;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'L' at beginning of line, state now L\n");
	    break;

	case 'p':
	case 'P':
	    me->state = miP;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'P' at beginning of line, state now P\n");
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "etry-after:";
	    me->if_ok = miRETRY_AFTER;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
	 "HTMIME: Got 'R' at beginning of line, checking for 'etry-after'\n");
	    break;

	case 's':
	case 'S':
	    me->state = miS;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'S' at beginning of line, state now S\n");
	    break;

	case 't':
	case 'T':
	    me->state = miT;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'T' at beginning of line, state now T\n");
	    break;

	case 'u':
	case 'U':
	    me->state = miU;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'U' at beginning of line, state now U\n");
	    break;

	case 'v':
	case 'V':
	    me->state = miV;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'V' at beginning of line, state now V\n");
	    break;

	case 'w':
	case 'W':
	    me->state = miW;
	    if (TRACE)
		fprintf (stderr,
		       "HTMIME: Got 'W' at beginning of line, state now W\n");
	    break;

	case '\n':			/* Blank line: End of Header! */
	    {
		me->net_ascii = NO;
		if (strchr(HTAtom_name(me->format), ';') != NULL) {
		    char *cp = NULL, *cp1, *cp2, *cp3 = NULL, *cp4;

		    if (TRACE)
			fprintf(stderr,
				"HTMIME: Extended MIME Content-Type is %s\n",
				HTAtom_name(me->format));
		    StrAllocCopy(cp, HTAtom_name(me->format));
		    /*
		    **	Note that the Content-Type value was converted
		    **	to lower case when we loaded into me->format,
		    **	but there may have been a mixed or upper-case
		    **	atom, so we'll force lower-casing again.  We
		    **	also stripped spaces and double-quotes, but
		    **	we'll make sure they're still gone from any
		    **	charset parameter we check. - FM
		    */
		    for (i = 0; cp[i]; i++)
			cp[i] = TOLOWER(cp[i]);
		    if ((cp1 = strchr(cp, ';')) != NULL) {
			BOOL chartrans_ok = NO;
			if ((cp2 = strstr(cp1, "charset")) != NULL) {
			    int chndl;

			    cp2 += 7;
			    while (*cp2 == ' ' || *cp2 == '=' || *cp2 == '\"')
				cp2++;
			    StrAllocCopy(cp3, cp2); /* copy to mutilate more */
			    for (cp4 = cp3; (*cp4 != '\0' && *cp4 != '\"' &&
					     *cp4 != ';'  && *cp4 != ':' &&
					     !WHITE(*cp4));	cp4++)
				; /* do nothing */
			    *cp4 = '\0';
			    cp4 = cp3;
			    chndl = UCGetLYhndl_byMIME(cp3);
			    if (UCCanTranslateFromTo(chndl,
						     current_char_set)) {
				chartrans_ok = YES;
				*cp1 = '\0';
				me->format = HTAtom_for(cp);
				StrAllocCopy(me->anchor->charset, cp4);
				HTAnchor_setUCInfoStage(me->anchor, chndl,
							UCT_STAGE_MIME,
							UCT_SETBY_MIME);
			    }
			    else if (chndl < 0) {/* got something but we don't
						 recognize it */
				chndl = UCLYhndl_for_unrec;
				if (UCCanTranslateFromTo(chndl,
							 current_char_set)) {
				    chartrans_ok = YES;
				    *cp1 = '\0';
				    me->format = HTAtom_for(cp);
				    HTAnchor_setUCInfoStage(me->anchor, chndl,
							    UCT_STAGE_MIME,
							    UCT_SETBY_DEFAULT);
				}
			    }
			    FREE(cp3);
			    if (chartrans_ok) {
				LYUCcharset * p_in =
				    HTAnchor_getUCInfoStage(me->anchor,
							    UCT_STAGE_MIME);
				LYUCcharset * p_out =
				    HTAnchor_setUCInfoStage(me->anchor,
							    current_char_set,
							    UCT_STAGE_HTEXT,
							    UCT_SETBY_DEFAULT);
				if (!p_out)
				    /*
				    **	Try again.
				    */
				    p_out =
				      HTAnchor_getUCInfoStage(me->anchor,
							      UCT_STAGE_HTEXT);

				if (!strcmp(p_in->MIMEname,
					    "x-transparent")) {
				    HTPassEightBitRaw = TRUE;
				    HTAnchor_setUCInfoStage(me->anchor,
				       HTAnchor_getUCLYhndl(me->anchor,
							    UCT_STAGE_HTEXT),
							    UCT_STAGE_MIME,
							    UCT_SETBY_DEFAULT);
				}
				if (!strcmp(p_out->MIMEname,
					    "x-transparent")) {
				    HTPassEightBitRaw = TRUE;
				    HTAnchor_setUCInfoStage(me->anchor,
					 HTAnchor_getUCLYhndl(me->anchor,
							      UCT_STAGE_MIME),
							    UCT_STAGE_HTEXT,
							    UCT_SETBY_DEFAULT);
				}
				if (p_in->enc != UCT_ENC_CJK) {
				    HTCJK = NOCJK;
				    if (!(p_in->codepoints &
					  UCT_CP_SUBSETOF_LAT1) &&
					chndl == current_char_set) {
					HTPassEightBitRaw = TRUE;
				    }
				} else if (p_out->enc == UCT_ENC_CJK) {
				    if (LYRawMode) {
					if ((!strcmp(p_in->MIMEname,
						     "euc-jp") ||
					     !strcmp(p_in->MIMEname,
						     "shift_jis")) &&
					    (!strcmp(p_out->MIMEname,
						     "euc-jp") ||
					     !strcmp(p_out->MIMEname,
						     "shift_jis"))) {
					    HTCJK = JAPANESE;
					} else if (!strcmp(p_in->MIMEname,
							   "euc-cn") &&
						   !strcmp(p_out->MIMEname,
							   "euc-cn")) {
					    HTCJK = CHINESE;
					} else if (!strcmp(p_in->MIMEname,
							   "big-5") &&
						   !strcmp(p_out->MIMEname,
							   "big-5")) {
					    HTCJK = TAIPEI;
					} else if (!strcmp(p_in->MIMEname,
							   "euc-kr") &&
						   !strcmp(p_out->MIMEname,
							   "euc-kr")) {
					    HTCJK = KOREAN;
					} else {
					    HTCJK = NOCJK;
					}
				    } else {
					HTCJK = NOCJK;
				    }
				}
			    /*
			    **  Check for an iso-8859-# we don't know. - FM
			    */
			    } else if
			       (!strncmp(cp4, "iso-8859-", 9) &&
				isdigit((unsigned char)cp4[9]) &&
				!strncmp(LYchar_set_names[current_char_set],
					 "Other ISO Latin", 15)) {
				/*
				**  Hope it's a match, for now. - FM
				*/
				*cp1 = '\0';
				me->format = HTAtom_for(cp);
				cp1 = &cp4[10];
				while (*cp1 &&
				       isdigit((unsigned char)(*cp1)))
				    cp1++;
				*cp1 = '\0';
				StrAllocCopy(me->anchor->charset, cp4);
				HTPassEightBitRaw = TRUE;
				HTAlert(me->anchor->charset);
			    }
			    FREE(cp3);
			} else {
			    /*
			    **	No charset parameter is present.
			    **	Ignore all other parameters, as
			    **	we do when charset is present. - FM
			    */
			    *cp1 = '\0';
			    me->format = HTAtom_for(cp);
			}
		    }
		    FREE(cp);
		}
		/*
		**  If we have an Expires header and haven't
		**  already set the no_cache element for the
		**  anchor, check if we should set it based
		**  on that header. - FM
		*/
		if (me->anchor->no_cache == FALSE &&
		    me->anchor->expires != NULL) {
		    if (!strcmp(me->anchor->expires, "0")) {
			/*
			 *  The value is zero, which we treat as
			 *  an absolute no-cache directive. - FM
			 */
			me->anchor->no_cache = TRUE;
		    } else if (me->anchor->date != NULL) {
			/*
			**  We have a Date header, so check if
			**  the value is less than or equal to
			**  that. - FM
			*/
			if (LYmktime(me->anchor->expires, TRUE) <=
			    LYmktime(me->anchor->date, TRUE)) {
			    me->anchor->no_cache = TRUE;
			}
		    } else if (LYmktime(me->anchor->expires, FALSE) <= 0) {
			/*
			**  We don't have a Date header, and
			**  the value is in past for us. - FM
			*/
			me->anchor->no_cache = TRUE;
		    }
		}
		StrAllocCopy(me->anchor->content_type,
			     HTAtom_name(me->format));
		if (!me->compression_encoding) {
		    if (TRACE) {
			fprintf(stderr,
		    "HTMIME: MIME Content-Type is '%s', converting to '%s'\n",
			 HTAtom_name(me->format), HTAtom_name(me->targetRep));
		    }
		} else {
		    /*
		    **	Change the format to that for "www/compressed"
		    **	and set up a stream to deal with it. - FM
		    */
		    if (TRACE) {
			fprintf(stderr,
	     "HTMIME: MIME Content-Type is '%s',\n", HTAtom_name(me->format));
		    }
		    me->format = HTAtom_for("www/compressed");
		    if (TRACE) {
			fprintf(stderr,
			 "        Treating as '%s'.  Converting to '%s'\n",
			 HTAtom_name(me->format), HTAtom_name(me->targetRep));
		    }
		}
		if (me->set_cookie != NULL || me->set_cookie2 != NULL) {
		    LYSetCookie(me->set_cookie,
				me->set_cookie2,
				me->anchor->address);
		    FREE(me->set_cookie);
		    FREE(me->set_cookie2);
		}
		me->target = HTStreamStack(me->format, me->targetRep,
					   me->sink , me->anchor);
		if (!me->target) {
		    if (TRACE)
			fprintf(stderr, "HTMIME: Can't translate! ** \n");
		    me->target = me->sink;	/* Cheat */
		}
		if (me->target) {
		    me->targetClass = *me->target->isa;
		    /*
		    **	Check for encoding and select state from there,
		    **	someday, but until we have the relevant code,
		    **	from now push straight through. - FM
		    */
		    me->state = MIME_TRANSPARENT;
		} else {
		    me->state = MIME_IGNORE;	/* What else to do? */
		}
		FREE(me->compression_encoding);
	    }
	    break;

	default:
	   goto bad_field_name;
	   break;

	} /* switch on character */
	break;

    case miA:				/* Check for 'c','g' or 'l' */
	switch (c) {
	case 'c':
	case 'C':
	    me->check_pointer = "cept-ranges:";
	    me->if_ok = miACCEPT_RANGES;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		    "HTMIME: Was A, found C, checking for 'cept-ranges:'\n");
	    break;

	case 'g':
	case 'G':
	    me->check_pointer = "e:";
	    me->if_ok = miAGE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was A, found G, checking for 'e:'\n");
	    break;

	case 'l':
	case 'L':
	    me->state = miAL;
	    if (TRACE)
		fprintf(stderr, "HTMIME: Was A, found L, state now AL'\n");
	    break;

	default:
	   if (TRACE)
	       fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
		       c, "'g' or 'l'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miAL:				/* Check for 'l' or 't' */
	switch (c) {
	case 'l':
	case 'L':
	    me->check_pointer = "ow:";
	    me->if_ok = miALLOW;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		      "HTMIME: Was AL, found L, checking for 'ow:'\n");
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "ernates:";
	    me->if_ok = miALTERNATES;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was AL, found T, checking for 'ernates:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'l' or 't'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miC:				/* Check for 'a' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "che-control:";
	    me->if_ok = miCACHE_CONTROL;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		     "HTMIME: Was C, found A, checking for 'che-control:'\n");
	    break;

	case 'o':
	case 'O':
	    me->state = miCO;
	    if (TRACE)
		fprintf(stderr, "HTMIME: Was C, found O, state now CO'\n");
	    break;

	default:
	   if (TRACE)
	       fprintf(stderr,
		    "HTMIME: Bad character `%c' found where `%s' expected\n",
		       c, "'a' or 'o'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miCO:				/* Check for 'n' or 'o' */
	switch (c) {
	case 'n':
	case 'N':
	    me->state = miCON;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was CO, found N, state now CON\n");
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "kie:";
	    me->if_ok = miCOOKIE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was CO, found O, checking for 'kie:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'n' or 'o'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miCON: 			/* Check for 'n' or 't' */
	switch (c) {
	case 'n':
	case 'N':
	    me->check_pointer = "ection:";
	    me->if_ok = miCONNECTION;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		      "HTMIME: Was CON, found N, checking for 'ection:'\n");
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "ent-";
	    me->if_ok = miCONTENT_;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was CON, found T, checking for 'ent-'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'n' or 't'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miE:				/* Check for 't' or 'x' */
	switch (c) {
	case 't':
	case 'T':
	    me->check_pointer = "ag:";
	    me->if_ok = miETAG;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was E, found T, checking for 'ag:'\n");
	    break;

	case 'x':
	case 'X':
	    me->check_pointer = "pires:";
	    me->if_ok = miEXPIRES;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was E, found X, checking for 'pires:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'t' or 'x'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miL:				/* Check for 'a', 'i' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "st-modified:";
	    me->if_ok = miLAST_MODIFIED;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		     "HTMIME: Was L, found A, checking for 'st-modified:'\n");
	    break;

	case 'i':
	case 'I':
	    me->check_pointer = "nk:";
	    me->if_ok = miLINK;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		     "HTMIME: Was L, found I, checking for 'nk:'\n");
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "cation:";
	    me->if_ok = miLOCATION;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was L, found O, checking for 'cation:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a', 'i' or 'o'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miP:				/* Check for 'r' or 'u' */
	switch (c) {
	case 'r':
	case 'R':
	    me->state = miPR;
	    if (TRACE)
		fprintf(stderr, "HTMIME: Was P, found R, state now PR'\n");
	    break;

	case 'u':
	case 'U':
	    me->check_pointer = "blic:";
	    me->if_ok = miPUBLIC;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was P, found U, checking for 'blic:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'r' or 'u'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miPR:				/* Check for 'a' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "gma:";
	    me->if_ok = miPRAGMA;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was PR, found A, checking for 'gma'\n");
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "xy-authenticate:";
	    me->if_ok = miPROXY_AUTHENTICATE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		 "HTMIME: Was PR, found O, checking for 'xy-authenticate'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'o'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miS:				/* Check for 'a' or 'e' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "fe:";
	    me->if_ok = miSAFE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr, "HTMIME: Was S, found A, checking for 'fe:'\n");
	    break;

	case 'e':
	case 'E':
	    me->state = miSE;
	    if (TRACE)
		fprintf(stderr, "HTMIME: Was S, found E, state now SE'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'e'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miSE:				/* Check for 'r' or 't' */
	switch (c) {
	case 'r':
	case 'R':
	    me->check_pointer = "ver:";
	    me->if_ok = miSERVER;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was SE, found R, checking for 'ver'\n");
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "-cookie";
	    me->if_ok = miSET_COOKIE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		 "HTMIME: Was SE, found T, checking for '-cookie'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'r' or 't'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miSET_COOKIE:			/* Check for ':' or '2' */
	switch (c) {
	case ':':
	    me->field = miSET_COOKIE1;		/* remember it */
	    me->state = miSKIP_GET_VALUE;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was SET_COOKIE, found :, processing\n");
	    break;

	case '2':
	    me->check_pointer = ":";
	    me->if_ok = miSET_COOKIE2;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		 "HTMIME: Was SET_COOKIE, found 2, checking for ':'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "':' or '2'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miT:				/* Check for 'i' or 'r' */
	switch (c) {
	case 'i':
	case 'I':
	    me->check_pointer = "tle:";
	    me->if_ok = miTITLE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was T, found I, checking for 'tle:'\n");
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "ansfer-encoding:";
	    me->if_ok = miTRANSFER_ENCODING;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		 "HTMIME: Was T, found R, checking for 'ansfer-encoding'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'i' or 'r'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miU:				/* Check for 'p' or 'r' */
	switch (c) {
	case 'p':
	case 'P':
	    me->check_pointer = "grade:";
	    me->if_ok = miUPGRADE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was U, found P, checking for 'grade:'\n");
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "i:";
	    me->if_ok = miURI;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was U, found R, checking for 'i:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'p' or 'r'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miV:				/* Check for 'a' or 'i' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "ry:";
	    me->if_ok = miVARY;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was V, found A, checking for 'ry:'\n");
	    break;

	case 'i':
	case 'I':
	    me->check_pointer = "a:";
	    me->if_ok = miVIA;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was V, found I, checking for 'a:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'i'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miW:				/* Check for 'a' or 'w' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "rning:";
	    me->if_ok = miWARNING;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was W, found A, checking for 'rning:'\n");
	    break;

	case 'w':
	case 'W':
	    me->check_pointer = "w-authenticate:";
	    me->if_ok = miWWW_AUTHENTICATE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		  "HTMIME: Was W, found W, checking for 'w-authenticate:'\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'w'");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miCHECK:			/* Check against string */
	if (TOLOWER(c) == *(me->check_pointer)++) {
	    if (!*me->check_pointer)
		me->state = me->if_ok;
	} else {		/* Error */
	    if (TRACE)
		fprintf(stderr,
		    "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, me->check_pointer - 1);
	    goto bad_field_name;
	}
	break;

    case miCONTENT_:
	if (TRACE)
	   fprintf (stderr,
		 "HTMIME: in case CONTENT_\n");
	switch(c) {
	case 'b':
	case 'B':
	    me->check_pointer = "ase:";
	    me->if_ok = miCONTENT_BASE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		      "HTMIME: Was CONTENT_, found B, checking for 'ase:'\n");
	    break;

	case 'd':
	case 'D':
	    me->check_pointer = "isposition:";
	    me->if_ok = miCONTENT_DISPOSITION;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		"HTMIME: Was CONTENT_, found D, checking for 'isposition:'\n");
	    break;

	case 'e':
	case 'E':
	    me->check_pointer = "ncoding:";
	    me->if_ok = miCONTENT_ENCODING;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		  "HTMIME: Was CONTENT_, found E, checking for 'ncoding:'\n");
	    break;

	case 'f':
	case 'F':
	    me->check_pointer = "eatures:";
	    me->if_ok = miCONTENT_FEATURES;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		  "HTMIME: Was CONTENT_, found F, checking for 'eatures:'\n");
	    break;

	case 'l':
	case 'L':
	    me->state = miCONTENT_L;
	    if (TRACE)
		fprintf (stderr,
		     "HTMIME: Was CONTENT_, found L, state now CONTENT_L\n");
	    break;

	case 'm':
	case 'M':
	    me->check_pointer = "d5:";
	    me->if_ok = miCONTENT_MD5;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		      "HTMIME: Was CONTENT_, found M, checking for 'd5:'\n");
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "ange:";
	    me->if_ok = miCONTENT_RANGE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		    "HTMIME: Was CONTENT_, found R, checking for 'ange:'\n");
	    break;

	case 't':
	case 'T':
	    me->state = miCONTENT_T;
	    if (TRACE)
		fprintf(stderr,
		    "HTMIME: Was CONTENT_, found T, state now CONTENT_T\n");
	    break;

	default:
	    if (TRACE)
		fprintf(stderr,
			"HTMIME: Was CONTENT_, found nothing; bleah\n");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miCONTENT_L:
      if (TRACE)
	fprintf (stderr,
		 "HTMIME: in case CONTENT_L\n");
      switch(c) {
	case 'a':
	case 'A':
	    me->check_pointer = "nguage:";
	    me->if_ok = miCONTENT_LANGUAGE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Was CONTENT_L, found A, checking for 'nguage:'\n");
	    break;

	case 'e':
	case 'E':
	    me->check_pointer = "ngth:";
	    me->if_ok = miCONTENT_LENGTH;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Was CONTENT_L, found E, checking for 'ngth:'\n");
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "cation:";
	    me->if_ok = miCONTENT_LOCATION;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Was CONTENT_L, found O, checking for 'cation:'\n");
	    break;

	default:
	  if (TRACE)
	    fprintf (stderr,
		     "HTMIME: Was CONTENT_L, found nothing; bleah\n");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miCONTENT_T:
      if (TRACE)
	fprintf (stderr,
		 "HTMIME: in case CONTENT_T\n");
      switch(c) {
	case 'r':
	case 'R':
	    me->check_pointer = "ansfer-encoding:";
	    me->if_ok = miCONTENT_TRANSFER_ENCODING;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
	 "HTMIME: Was CONTENT_T, found R, checking for 'ansfer-encoding:'\n");
	    break;

	case 'y':
	case 'Y':
	    me->check_pointer = "pe:";
	    me->if_ok = miCONTENT_TYPE;
	    me->state = miCHECK;
	    if (TRACE)
		fprintf(stderr,
		   "HTMIME: Was CONTENT_T, found Y, checking for 'pe:'\n");
	    break;

	default:
	  if (TRACE)
	    fprintf (stderr,
		     "HTMIME: Was CONTENT_T, found nothing; bleah\n");
	    goto bad_field_name;
	    break;

	} /* switch on character */
	break;

    case miACCEPT_RANGES:
    case miAGE:
    case miALLOW:
    case miALTERNATES:
    case miCACHE_CONTROL:
    case miCOOKIE:
    case miCONNECTION:
    case miCONTENT_BASE:
    case miCONTENT_DISPOSITION:
    case miCONTENT_ENCODING:
    case miCONTENT_FEATURES:
    case miCONTENT_LANGUAGE:
    case miCONTENT_LENGTH:
    case miCONTENT_LOCATION:
    case miCONTENT_MD5:
    case miCONTENT_RANGE:
    case miCONTENT_TRANSFER_ENCODING:
    case miCONTENT_TYPE:
    case miDATE:
    case miETAG:
    case miEXPIRES:
    case miKEEP_ALIVE:
    case miLAST_MODIFIED:
    case miLINK:
    case miLOCATION:
    case miPRAGMA:
    case miPROXY_AUTHENTICATE:
    case miPUBLIC:
    case miRETRY_AFTER:
    case miSAFE:
    case miSERVER:
    case miSET_COOKIE1:
    case miSET_COOKIE2:
    case miTITLE:
    case miTRANSFER_ENCODING:
    case miUPGRADE:
    case miURI:
    case miVARY:
    case miVIA:
    case miWARNING:
    case miWWW_AUTHENTICATE:
	me->field = me->state;		/* remember it */
	me->state = miSKIP_GET_VALUE;
	/* Fall through! */

    case miSKIP_GET_VALUE:
	if (c == '\n') {
	   me->fold_state = me->state;
	   me->state = miNEWLINE;
	   break;
	}
	if (WHITE(c))
	    /*
	    **	Skip white space.
	    */
	    break;

	me->value_pointer = me->value;
	me->state = miGET_VALUE;
	/* Fall through to store first character */

    case miGET_VALUE:
	if (WHITE(c) && c != 32) {			/* End of field */
	    char *cp;
	    *me->value_pointer = '\0';
	    cp = (me->value_pointer - 1);
	    while ((cp >= me->value) && *cp == 32)
		/*
		**  Trim trailing spaces.
		*/
		*cp = '\0';
	    switch (me->field) {
	    case miACCEPT_RANGES:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Accept-Ranges: '%s'\n",
			    me->value);
		break;
	    case miAGE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Age: '%s'\n",
			    me->value);
		break;
	    case miALLOW:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Allow: '%s'\n",
			    me->value);
		break;
	    case miALTERNATES:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Alternates: '%s'\n",
			    me->value);
		break;
	    case miCACHE_CONTROL:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Cache-Control: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Convert to lowercase and indicate in anchor. - FM
		*/
		for (i = 0; me->value[i]; i++)
		    me->value[i] = TOLOWER(me->value[i]);
		StrAllocCopy(me->anchor->cache_control, me->value);
		/*
		**  Check whether to set no_cache for the anchor. - FM
		*/
		{
		    char *cp1, *cp0 = me->value;

		    while ((cp1 = strstr(cp0, "no-cache")) != NULL) {
			cp1 += 8;
			while (*cp1 != '\0' && WHITE(*cp1))
			    cp1++;
			if (*cp1 == '\0' || *cp1 == ';') {
			    me->anchor->no_cache = TRUE;
			    break;
			}
			cp0 = cp1;
		    }
		    if (me->anchor->no_cache == TRUE)
			break;
		    cp0 = me->value;
		    while ((cp1 = strstr(cp0, "max-age")) != NULL) {
			cp1 += 7;
			while (*cp1 != '\0' && WHITE(*cp1))
			    cp1++;
			if (*cp1 == '=') {
			    cp1++;
			    while (*cp1 != '\0' && WHITE(*cp1))
				cp1++;
			    if (isdigit((unsigned char)*cp1)) {
				cp0 = cp1;
				while (isdigit((unsigned char)*cp1))
				    cp1++;
				if (*cp0 == '0' && cp1 == (cp0 + 1)) {
				    me->anchor->no_cache = TRUE;
				    break;
				}
			    }
			}
			cp0 = cp1;
		    }
		}
		break;
	    case miCOOKIE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Cookie: '%s'\n",
			    me->value);
		break;
	    case miCONNECTION:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Connection: '%s'\n",
			    me->value);
		break;
	    case miCONTENT_BASE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Base: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->content_base, me->value);
		break;
	    case miCONTENT_DISPOSITION:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Disposition: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->content_disposition, me->value);
		/*
		**  It's not clear yet from existing RFCs and IDs
		**  whether we should be looking for file;, attachment;,
		**  and/or inline; before the filename=value, so we'll
		**  just search for "filename" followed by '=' and just
		**  hope we get the intended value.  It is purely a
		**  suggested name, anyway. - FM
		*/
		cp = me->anchor->content_disposition;
		while (*cp != '\0' && strncasecomp(cp, "filename", 8))
		    cp++;
		if (*cp == '\0')
		    break;
		cp += 8;
		while ((*cp != '\0') && (WHITE(*cp) || *cp == '='))
		    cp++;
		if (*cp == '\0')
		    break;
		while (*cp != '\0' && WHITE(*cp))
		    cp++;
		if (*cp == '\0')
		    break;
		StrAllocCopy(me->anchor->SugFname, cp);
		if (*me->anchor->SugFname == '\"') {
		    if ((cp = strchr((me->anchor->SugFname + 1),
				     '\"')) != NULL) {
			*(cp + 1) = '\0';
			HTMIME_TrimDoubleQuotes(me->anchor->SugFname);
		    } else {
			FREE(me->anchor->SugFname);
			break;
		    }
		}
		cp = me->anchor->SugFname;
		while (*cp != '\0' && !WHITE(*cp))
		    cp++;
		*cp = '\0';
		if (*me->anchor->SugFname == '\0')
		    FREE(me->anchor->SugFname);
		break;
	    case miCONTENT_ENCODING:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
		       "HTMIME: PICKED UP Content-Encoding: '%s'\n",
			    me->value);
		if (!(me->value && *me->value) ||
		    !strcasecomp(me->value, "identity"))
		    break;
		/*
		**  Convert to lowercase and indicate in anchor. - FM
		*/
		for (i = 0; me->value[i]; i++)
		    me->value[i] = TOLOWER(me->value[i]);
		StrAllocCopy(me->anchor->content_encoding, me->value);
		FREE(me->compression_encoding);
		if (!strcmp(me->value, "8bit") ||
		    !strcmp(me->value, "7bit") ||
		    !strcmp(me->value, "binary")) {
		    /*
		    **	Some server indicated "8bit", "7bit" or "binary"
		    **	inappropriately.  We'll ignore it. - FM
		    */
		    if (TRACE)
			fprintf(stderr,
				"                Ignoring it!\n");
		} else {
		    /*
		    **	Save it to use as a flag for setting
		    **	up a "www/compressed" target. - FM
		    */
		    StrAllocCopy(me->compression_encoding, me->value);
		}
		break;
	    case miCONTENT_FEATURES:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Features: '%s'\n",
			    me->value);
		break;
	    case miCONTENT_LANGUAGE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Language: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Convert to lowercase and indicate in anchor. - FM
		*/
		for (i = 0; me->value[i]; i++)
		    me->value[i] = TOLOWER(me->value[i]);
		StrAllocCopy(me->anchor->content_language, me->value);
		break;
	    case miCONTENT_LENGTH:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Length: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Convert to integer and indicate in anchor. - FM
		*/
		me->anchor->content_length = atoi(me->value);
		if (me->anchor->content_length < 0)
		    me->anchor->content_length = 0;
		if (TRACE)
		    fprintf(stderr,
			    "        Converted to integer: '%d'\n",
			    me->anchor->content_length);
		break;
	    case miCONTENT_LOCATION:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Location: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->content_location, me->value);
		break;
	    case miCONTENT_MD5:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-MD5: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->content_md5, me->value);
		break;
	    case miCONTENT_RANGE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Range: '%s'\n",
			    me->value);
		break;
	    case miCONTENT_TRANSFER_ENCODING:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			"HTMIME: PICKED UP Content-Transfer-Encoding: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Force the Content-Transfer-Encoding value
		**  to all lower case. - FM
		*/
		for (i = 0; me->value[i]; i++)
		    me->value[i] = TOLOWER(me->value[i]);
		me->encoding = HTAtom_for(me->value);
		break;
	    case miCONTENT_TYPE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Content-Type: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Force the Content-Type value to all lower case
		**  and strip spaces and double-quotes. - FM
		*/
		for (i = 0, j = 0; me->value[i]; i++) {
		    if (me->value[i] != ' ' && me->value[i] != '\"') {
			me->value[j++] = TOLOWER(me->value[i]);
		    }
		}
		me->value[j] = '\0';
		me->format = HTAtom_for(me->value);
		break;
	    case miDATE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Date: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->date, me->value);
		break;
	    case miETAG:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP ETag: '%s'\n",
			    me->value);
		break;
	    case miEXPIRES:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Expires: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->expires, me->value);
		break;
	    case miKEEP_ALIVE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Keep-Alive: '%s'\n",
			    me->value);
		break;
	    case miLAST_MODIFIED:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Last-Modified: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->last_modified, me->value);
		break;
	    case miLINK:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Link: '%s'\n",
			    me->value);
		break;
	    case miLOCATION:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Location: '%s'\n",
			    me->value);
		break;
	    case miPRAGMA:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Pragma: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Check whether to set no_cache for the anchor. - FM
		*/
		if (!strcmp(me->value, "no-cache"))
		    me->anchor->no_cache = TRUE;
		break;
	    case miPROXY_AUTHENTICATE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Proxy-Authenticate: '%s'\n",
			    me->value);
		break;
	    case miPUBLIC:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Public: '%s'\n",
			    me->value);
		break;
	    case miRETRY_AFTER:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Retry-After: '%s'\n",
			    me->value);
		break;
	    case miSAFE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Safe: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor if "YES" or "TRUE". - FM
		*/
		if (!strcasecomp(me->value, "YES") ||
		    !strcasecomp(me->value, "TRUE")) {
		    me->anchor->safe = TRUE;
		}
		break;
	    case miSERVER:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Server: '%s'\n",
			    me->value);
		if (!(me->value && *me->value))
		    break;
		/*
		**  Indicate in anchor. - FM
		*/
		StrAllocCopy(me->anchor->server, me->value);
		break;
	    case miSET_COOKIE1:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Set-Cookie: '%s'\n",
			    me->value);
		if (me->set_cookie == NULL) {
		    StrAllocCopy(me->set_cookie, me->value);
		} else {
		    StrAllocCat(me->set_cookie, ", ");
		    StrAllocCat(me->set_cookie, me->value);
		}
		break;
	    case miSET_COOKIE2:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Set-Cookie2: '%s'\n",
			    me->value);
		if (me->set_cookie2 == NULL) {
		    StrAllocCopy(me->set_cookie2, me->value);
		} else {
		    StrAllocCat(me->set_cookie2, ", ");
		    StrAllocCat(me->set_cookie2, me->value);
		}
		break;
	    case miTITLE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Title: '%s'\n",
			    me->value);
		break;
	    case miTRANSFER_ENCODING:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Transfer-Encoding: '%s'\n",
			    me->value);
		break;
	    case miUPGRADE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Upgrade: '%s'\n",
			    me->value);
		break;
	    case miURI:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP URI: '%s'\n",
			    me->value);
		break;
	    case miVARY:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Vary: '%s'\n",
			    me->value);
		break;
	    case miVIA:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Via: '%s'\n",
			    me->value);
		break;
	    case miWARNING:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP Warning: '%s'\n",
			    me->value);
		break;
	    case miWWW_AUTHENTICATE:
		HTMIME_TrimDoubleQuotes(me->value);
		if (TRACE)
		    fprintf(stderr,
			    "HTMIME: PICKED UP WWW-Authenticate: '%s'\n",
			    me->value);
		break;
	    default:		/* Should never get here */
		break;
	    }
	} else {
	    if (me->value_pointer < me->value + VALUE_SIZE - 1) {
		*me->value_pointer++ = c;
		break;
	    } else {
		goto value_too_long;
	    }
	}
	/* Fall through */

    case miJUNK_LINE:
	if (c == '\n') {
	    me->state = miNEWLINE;
	    me->fold_state = me->state;
	}
	break;


    } /* switch on state*/

    return;

value_too_long:
    if (TRACE)
	fprintf(stderr, "HTMIME: *** Syntax error. (string too long)\n");

bad_field_name: 			/* Ignore it */
    me->state = miJUNK_LINE;
    return;

}



/*	String handling
**	---------------
**
**	Strings must be smaller than this buffer size.
*/
PRIVATE void HTMIME_put_string ARGS2(
	HTStream *,	me,
	CONST char *,	s)
{
    CONST char * p;

    if (me->state == MIME_TRANSPARENT) {	/* Optimisation */
	(*me->targetClass.put_string)(me->target,s);

    } else if (me->state != MIME_IGNORE) {
	if (TRACE)
	    fprintf(stderr, "HTMIME:  %s\n", s);

	for (p=s; *p; p++)
	    HTMIME_put_character(me, *p);
    }
}


/*	Buffer write.  Buffers can (and should!) be big.
**	------------
*/
PRIVATE void HTMIME_write ARGS3(
	HTStream *,	me,
	CONST char *,	s,
	int,		l)
{
    CONST char * p;

    if (me->state == MIME_TRANSPARENT) {	/* Optimisation */
	(*me->targetClass.put_block)(me->target, s, l);

    } else {
	if (TRACE)
	    fprintf(stderr, "HTMIME:  %.*s\n", l, s);

	for (p = s; p < s+l; p++)
	    HTMIME_put_character(me, *p);
    }
}


/*	Free an HTML object
**	-------------------
**
*/
PRIVATE void HTMIME_free ARGS1(
	HTStream *,	me)
{
    if (me->target)
	(*me->targetClass._free)(me->target);
    FREE(me);
}

/*	End writing
*/
PRIVATE void HTMIME_abort ARGS2(
	HTStream *,	me,
	HTError,	e)
{
    if (me->target)
	(*me->targetClass._abort)(me->target, e);
    FREE(me);
}


/*	Structured Object Class
**	-----------------------
*/
PRIVATE CONST HTStreamClass HTMIME =
{
	"MIMEParser",
	HTMIME_free,
	HTMIME_abort,
	HTMIME_put_character,
	HTMIME_put_string,
	HTMIME_write
};


/*	Subclass-specific Methods
**	-------------------------
*/
PUBLIC HTStream* HTMIMEConvert ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    HTStream* me;

    me = (HTStream *)calloc(1, sizeof(*me));
    if (me == NULL)
	outofmem(__FILE__, "HTMIMEConvert");
    me->isa	=	&HTMIME;
    me->sink	=	sink;
    me->anchor	=	anchor;
    me->anchor->safe = FALSE;
    me->anchor->no_cache = FALSE;
    FREE(me->anchor->cache_control);
    FREE(me->anchor->SugFname);
    FREE(me->anchor->charset);
    FREE(me->anchor->content_language);
    FREE(me->anchor->content_encoding);
    FREE(me->anchor->content_base);
    FREE(me->anchor->content_disposition);
    FREE(me->anchor->content_location);
    FREE(me->anchor->content_md5);
    me->anchor->content_length = 0;
    FREE(me->anchor->date);
    FREE(me->anchor->expires);
    FREE(me->anchor->last_modified);
    FREE(me->anchor->server);
    me->target	=	NULL;
    me->state	=	miBEGINNING_OF_LINE;
    /*
     *	Sadly enough, change this to always default to WWW_HTML
     *	to parse all text as HTML for the users.
     *	GAB 06-30-94
     *	Thanks to Robert Rowland robert@cyclops.pei.edu
     *
     *	After discussion of the correct handline, should be application/octet-
     *		stream or unknown; causing servers to send a correct content
     *		type.
     *
     *	The consequence of using WWW_UNKNOWN is that you end up downloading
     *	as a binary file what 99.9% of the time is an HTML file, which should
     *	have been rendered or displayed.  So sadly enough, I'm changing it
     *	back to WWW_HTML, and it will handle the situation like Mosaic does,
     *	and as Robert Rowland suggested, because being functionally correct
     *	99.9% of the time is better than being technically correct but
     *	functionally nonsensical. - FM
     *//***
    me->format	  =	WWW_UNKNOWN;
	***/
    me->format	  =	WWW_HTML;
    me->targetRep =	pres->rep_out;
    me->boundary  =	NULL;		/* Not set yet */
    me->set_cookie  =	NULL;		/* Not set yet */
    me->set_cookie2  =	NULL;		/* Not set yet */
    me->encoding  =	0;		/* Not set yet */
    me->compression_encoding = NULL;	/* Not set yet */
    me->net_ascii =	NO;		/* Local character set */
    HTAnchor_setUCInfoStage(me->anchor, current_char_set,
			    UCT_STAGE_STRUCTURED,
			    UCT_SETBY_DEFAULT);
    HTAnchor_setUCInfoStage(me->anchor, current_char_set,
			    UCT_STAGE_HTEXT,
			    UCT_SETBY_DEFAULT);
    return me;
}

PUBLIC HTStream* HTNetMIME ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    HTStream* me = HTMIMEConvert(pres,anchor, sink);
    if (!me)
	return NULL;

    me->net_ascii = YES;
    return me;
}

/*		Japanese header handling functions
**		==================================
**
**	K&Rized and added 07-Jun-96 by FM, based on:
**
////////////////////////////////////////////////////////////////////////
**
**	ISO-2022-JP handling routines
**			&
**	MIME decode routines (quick hack just for ISO-2022-JP)
**
**		Thu Jan 25 10:11:42 JST 1996
**
**  Copyright (C) 1994, 1995, 1996
**  Shuichi Ichikawa (ichikawa@nuee.nagoya-u.ac.jp)
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either versions 2, or (at your option)
**  any later version.
**
**  This program is distributed in the hope that it will be useful
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with SKK, see the file COPYING.  If not, write to the Free
**  Software Foundation Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* #include <stdio.h> */	/* Included via previous headers. - FM */
/* #include <string.h> */	/* Included via previous headers. - FM */

/*
**  MIME decoding routines
**
**	Written by S. Ichikawa,
**	partially inspired by encdec.c of <jh@efd.lth.se>.
*/
#define BUFLEN	1024
#ifdef ESC
#undef ESC
#endif /* ESC */
#define ESC	'\033'

PRIVATE char HTmm64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=" ;
PRIVATE char HTmmquote[] = "0123456789ABCDEF";
PRIVATE int HTmmcont = 0;

PUBLIC void HTmmdec_base64 ARGS2(
	char *, 	t,
	char *, 	s)
{
    int   d, count, j, val;
    char  buf[BUFLEN], *bp, nw[4], *p;

    for (bp = buf; *s; s += 4) {
	val = 0;
	if (s[2] == '=')
	    count = 1;
	else if (s[3] == '=')
	    count = 2;
	else
	    count = 3;

	for (j = 0; j <= count; j++) {
		if (!(p = strchr(HTmm64, s[j]))) {
			return;
		}
		d = p - HTmm64;
		d <<= (3-j)*6;
		val += d;
	}
	for (j = 2; j >= 0; j--) {
		nw[j] = val & 255;
		val >>= 8;
	}
	if (count--)
	    *bp++ = nw[0];
	if (count--)
	    *bp++ = nw[1];
	if (count)
	    *bp++ = nw[2];
    }
    *bp = '\0';
    strcpy(t, buf);
}

PUBLIC void HTmmdec_quote ARGS2(
	char *, 	t,
	char *, 	s)
{
    char  buf[BUFLEN], cval, *bp, *p;

    for (bp = buf; *s; ) {
	if (*s == '=') {
	    cval = 0;
	    if (s[1] && (p = strchr(HTmmquote, s[1]))) {
		cval += (p - HTmmquote);
	    } else {
		*bp++ = *s++;
		continue;
	    }
	    if (s[2] && (p = strchr(HTmmquote, s[2]))) {
		cval <<= 4;
		cval += (p - HTmmquote);
		*bp++ = cval;
		s += 3;
	    } else {
		*bp++ = *s++;
	    }
	} else if (*s == '_') {
	    *bp++ = 0x20;
	    s++;
	} else {
	    *bp++ = *s++;
	}
    }
    *bp = '\0';
    strcpy(t, buf);
}

#ifdef NOTDEFINED
/*
**	Generalized HTmmdecode for chartrans - K. Weide 1997-03-06
*/
PUBLIC void HTmmdecode ARGS2(
	char *, 	trg,
	char *, 	str)
{
    char buf[BUFLEN], mmbuf[BUFLEN];
    char *s, *t, *u, *qm2;
    int  base64, quote;

    buf[0] = '\0';

    /*
    **	Encoded-words look like
    **		=?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=
    */
    for (s = str, u = buf; *s; ) {
	base64 = quote = 0;
	if (*s == '=' && s[1] == '?' &&
	    (s == str || *(s-1) == '(' || WHITE(*(s-1))))
	{ /* must be beginning of word */
	    qm2 = strchr(s+2, '?'); /* 2nd question mark */
	    if (qm2 &&
		(qm2[1] == 'B' || qm2[1] == 'b' || qm2[1] == 'Q' ||
		 qm2[1] == 'q') &&
		qm2[2] == '?') { /* 3rd question mark */
		char * qm4 = strchr(qm2 + 3, '?'); /* 4th question mark */
		if (qm4 && qm4 - s < 74 &&  /* RFC 2047 length restriction */
		    qm4[1] == '=') {
		    char *p;
		    BOOL invalid = NO;
		    for (p = s+2; p < qm4; p++)
			if (WHITE(*p)) {
			    invalid = YES;
			    break;
			}
		    if (!invalid) {
			int LYhndl;

			*qm2 = '\0';
			for (p = s+2; *p; p++)
			    *p = TOLOWER(*p);
			invalid = ((LYhndl = UCGetLYhndl_byMIME(s+2)) < 0 ||
				   UCCanTranslateFromTo(LYhndl,
						 current_char_set));
			*qm2 = '?';
		    }
		    if (!invalid) {
			if (qm2[1] == 'B' || qm2[1] == 'b')
			    base64 = 1;
			else if (qm2[1] == 'Q' || qm2[1] == 'q')
			    quote = 1;
		    }
		}
	    }
	}
	if (base64 || quote) {
	    if (HTmmcont) {
		for (t = s - 1;
		    t >= str && (*t == ' ' || *t == '\t'); t--) {
			u--;
		}
	    }
	    for (s = qm2 + 3, t = mmbuf; *s; ) {
		if (s[0] == '?' && s[1] == '=') {
		    break;
		} else {
		    *t++ = *s++;
		}
	    }
	    if (s[0] != '?' || s[1] != '=') {
		goto end;
	    } else {
		s += 2;
		*t = '\0';
	    }
	    if (base64)
		HTmmdec_base64(mmbuf, mmbuf);
	    if (quote)
		HTmmdec_quote(mmbuf, mmbuf);
	    for (t = mmbuf; *t; )
		*u++ = *t++;
	    HTmmcont = 1;
	    /* if (*s == ' ' || *s == '\t') *u++ = *s; */
	    /* for ( ; *s == ' ' || *s == '\t'; s++) ; */
	} else {
	    if (*s != ' ' && *s != '\t')
		HTmmcont = 0;
	    *u++ = *s++;
	}
    }
    *u = '\0';
end:
    strcpy(trg, buf);
}
#else
/*
**	HTmmdecode for ISO-2022-JP - FM
*/
PUBLIC void HTmmdecode ARGS2(
	char *, 	trg,
	char *, 	str)
{
    char buf[BUFLEN], mmbuf[BUFLEN];
    char *s, *t, *u;
    int  base64, quote;

    buf[0] = '\0';

    for (s = str, u = buf; *s; ) {
	if (!strncasecomp(s, "=?ISO-2022-JP?B?", 16)) {
	    base64 = 1;
	} else {
	    base64 = 0;
	}
	if (!strncasecomp(s, "=?ISO-2022-JP?Q?", 16)) {
	    quote = 1;
	} else {
	    quote = 0;
	}
	if (base64 || quote) {
	    if (HTmmcont) {
		for (t = s - 1;
		    t >= str && (*t == ' ' || *t == '\t'); t--) {
			u--;
		}
	    }
	    for (s += 16, t = mmbuf; *s; ) {
		if (s[0] == '?' && s[1] == '=') {
		    break;
		} else {
		    *t++ = *s++;
		}
	    }
	    if (s[0] != '?' || s[1] != '=') {
		goto end;
	    } else {
		s += 2;
		*t = '\0';
	    }
	    if (base64)
		HTmmdec_base64(mmbuf, mmbuf);
	    if (quote)
		HTmmdec_quote(mmbuf, mmbuf);
	    for (t = mmbuf; *t; )
		*u++ = *t++;
	    HTmmcont = 1;
	    /* if (*s == ' ' || *s == '\t') *u++ = *s; */
	    /* for ( ; *s == ' ' || *s == '\t'; s++) ; */
	} else {
	    if (*s != ' ' && *s != '\t')
		HTmmcont = 0;
	    *u++ = *s++;
	}
    }
    *u = '\0';
end:
    strcpy(trg, buf);
}
#endif /* NOTDEFINED */

/*
**  Modified for Lynx-jp by Takuya ASADA (and K&Rized by FM).
*/
#if NOTDEFINED
PUBLIC int main ARGS2(
	int,		ac,
	char **,	av)
{
    FILE *fp;
    char buf[BUFLEN];
    char header = 1, body = 0, r_jis = 0;
    int  i, c;

    for (i = 1; i < ac; i++) {
	if (strcmp(av[i], "-B") == NULL)
	    body = 1;
	else if (strcmp(av[i], "-r") == NULL)
	    r_jis = 1;
	else
	    break;
    }

    if (i >= ac) {
	fp = stdin;
    } else {
	if ((fp = fopen(av[i], "r")) == NULL) {
	    fprintf(stderr, "%s: cannot open %s\n", av[0], av[i]);
	    exit(1);
	}
    }

    while (fgets(buf, BUFLEN, fp)) {
	if (buf[0] == '\n' && buf[1] == '\0')
	    header = 0;
	if (header) {
	    c = fgetc(fp);
	    if (c == ' ' || c == '\t') {
		buf[strlen(buf)-1] = '\0';
		ungetc(c, fp);
	    } else {
		ungetc(c, fp);
	    }
	}
	if (header || body)
	    HTmmdecode(buf, buf);
	if (r_jis)
	    HTrjis(buf, buf);
	fprintf(stdout, "%s", buf);
    }

    close(fp);
    exit(0);
}
#endif /* NOTDEFINED */

/*
**  Insert ESC where it seems lost.
**  (The author of this function "rjis" is S. Ichikawa.)
*/
PUBLIC int HTrjis ARGS2(
	char *, 	t,
	char *, 	s)
{
    char *p, buf[BUFLEN];
    int kanji = 0;

    if (strchr(s, ESC) || !strchr(s, '$')) {
	if (s != t)
	    strcpy(t, s);
	return 1;
    }
    for (p = buf; *s; ) {
	if (!kanji && s[0] == '$' && (s[1] == '@' || s[1] == 'B')) {
	    if (HTmaybekanji((int)s[2], (int)s[3])) {
		kanji = 1;
		*p++ = ESC;
		*p++ = *s++;
		*p++ = *s++;
		*p++ = *s++;
		*p++ = *s++;
		continue;
	    }
	    *p++ = *s++;
	    continue;
	}
	if (kanji && s[0] == '(' && (s[1] == 'J' || s[1] == 'B')) {
	    kanji = 0;
	    *p++ = ESC;
	    *p++ = *s++;
	    *p++ = *s++;
	    continue;
	}
	*p++ = *s++;
    }
    *p = *s;	/* terminate string */

    strcpy(t, buf);
    return 0;
}

/*
**  The following function "maybekanji" is derived from
**  RJIS-1.0 by Mr. Hironobu Takahashi.
**  Maybekanji() is included here under the courtesy of the author.
**  The original comment of rjis.c is also included here.
*/
/*
 * RJIS ( Recover JIS code from broken file )
 * $Header: /home/cvs/src/gnu/usr.bin/lynx/WWW/Library/Implementation/Attic/HTMIME.c,v 1.1.1.1 1998/03/11 17:47:45 maja Exp $
 * Copyright (C) 1992 1994
 * Hironobu Takahashi (takahasi@tiny.or.jp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either versions 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SKK, see the file COPYING.  If not, write to the Free
 * Software Foundation Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

PUBLIC int HTmaybekanji ARGS2(
	int,		c1,
	int,		c2)
{

    if ((c2 < 33) || (c2 > 126))
	return 0;
    if ((c1 < 33) || ((40 < c1) && (c1 < 48)) || (116 < c1))
	return 0;
    c2 -= 32;
    switch(c1-32) {
      case 2:
	if ((14 < c2) && ( c2 < 26))
	    return 0;
	if ((33 < c2) && ( c2 < 42))
	    return 0;
	if ((48 < c2) && ( c2 < 60))
	    return 0;
	if ((74 < c2) && ( c2 < 82))
	    return 0;
	if ((89 < c2) && ( c2 < 94))
	    return 0;
	break;
      case 3:
	if (c2 < 16)
	    return 0;
	if ((25 < c2) && ( c2 < 33))
	    return 0;
	if ((58 < c2) && ( c2 < 65))
	    return 0;
	if (90 < c2)
	    return 0;
	break;
      case 4:
	if (83 < c2)
	    return 0;
	break;
      case 5:
	if (86 < c2)
	    return 0;
	break;
      case 6:
	if ((24 < c2) && ( c2 < 33))
	    return 0;
	if (56 < c2)
	    return 0;
	break;
      case 7:
	if ((33 < c2) && ( c2 < 49))
	    return 0;
	if (81 < c2)
	    return 0;
	break;
      case 8:
	if (32 < c2)
	    return 0;
	break;
      case 47:
	if (51 < c2)
	    return 0;
	break;
      case 84:
	if (6 < c2)
	    return 0;
	break;
    }
    return 1;
}

