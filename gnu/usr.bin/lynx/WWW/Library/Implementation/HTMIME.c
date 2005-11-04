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
#include <HTUtils.h>
#include <HTMIME.h>		/* Implemented here */
#include <HTTP.h>		/* for redirecting_url */
#include <HTAlert.h>
#include <HTCJK.h>
#include <UCMap.h>
#include <UCDefs.h>
#include <UCAux.h>

#include <LYCookie.h>
#include <LYCharSets.h>
#include <LYCharUtils.h>
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYLeaks.h>

/*		MIME Object
**		-----------
*/

typedef enum {
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
	miR,
	miRE,
	miREFRESH,
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
	MIME_NET_ASCII,		/* Translate from net ascii */
	MIME_IGNORE		/* Ignore entire file */
	/* TRANSPARENT and IGNORE are defined as stg else in _WINDOWS */
} MIME_state;

#define VALUE_SIZE 5120		/* @@@@@@@ Arbitrary? */
struct _HTStream {
	CONST HTStreamClass *	isa;

	BOOL			net_ascii;	/* Is input net ascii? */
	MIME_state		state;		/* current state */
	MIME_state		if_ok;		/* got this state if match */
	MIME_state		field;		/* remember which field */
	MIME_state		fold_state;	/* state on a fold */
	BOOL			head_only;	/* only parsing header */
	BOOL			pickup_redirection; /* parsing for location */
	BOOL			no_streamstack; /* use sink directly */
	CONST char *		check_pointer;	/* checking input */

	char *			value_pointer;	/* storing values */
	char			value[VALUE_SIZE];

	HTParentAnchor *	anchor;		/* Given on creation */
	HTStream *		sink;		/* Given on creation */

	char *			boundary;	/* For multipart */
	char *			set_cookie;	/* Set-Cookie */
	char *			set_cookie2;	/* Set-Cookie2 */
	char *			location;	/* Location */

	char *			refresh_url;	/* "Refresh:" URL */

	HTFormat		encoding;	/* Content-Transfer-Encoding */
	char *			compression_encoding;
	HTFormat		format;		/* Content-Type */
	HTStream *		target;		/* While writing out */
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
	char *,		value)
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

PRIVATE BOOL content_is_compressed ARGS1(HTStream *, me)
{
    char *encoding = me->anchor->content_encoding;

    return encoding != 0
        && strcmp(encoding, "8bit") != 0
	&& strcmp(encoding, "7bit") != 0
	&& strcmp(encoding, "binary") != 0;
}

/*
 * Strip quotes from a refresh-URL.
 */
PRIVATE void dequote ARGS1(char *, url)
{
    int len;

    len = strlen(url);
    if (*url == '\'' && len > 1 && url[len-1] == url[0]) {
	url[len-1] = '\0';
	while ((url[0] = url[1]) != '\0') {
	    ++url;
	}
    }
}

PRIVATE int pumpData ARGS1(HTStream *, me)
{
    if (strchr(HTAtom_name(me->format), ';') != NULL) {
	char *cp = NULL, *cp1, *cp2, *cp3 = NULL, *cp4;

	CTRACE((tfp, "HTMIME: Extended MIME Content-Type is %s\n",
		HTAtom_name(me->format)));
	StrAllocCopy(cp, HTAtom_name(me->format));
	/*
	** Note that the Content-Type value was converted
	** to lower case when we loaded into me->format,
	** but there may have been a mixed or upper-case
	** atom, so we'll force lower-casing again.  We
	** also stripped spaces and double-quotes, but
	** we'll make sure they're still gone from any
	** charset parameter we check.  - FM
	*/
	LYLowerCase(cp);
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
		    if (chndl < 0)
			/*
			**  UCLYhndl_for_unrec not defined :-(
			**  fallback to UCLYhndl_for_unspec
			**  which always valid.
			*/
			chndl = UCLYhndl_for_unspec;  /* always >= 0 */
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
			Set_HTCJK(p_in->MIMEname, p_out->MIMEname);
		    }
		} else {
		    /*
		    **  Cannot translate.
		    **  If according to some heuristic the given
		    **  charset and the current display character
		    **  both are likely to be like ISO-8859 in
		    **  structure, pretend we have some kind
		    **  of match.
		    */
		    BOOL given_is_8859
			= (BOOL) (!strncmp(cp4, "iso-8859-", 9) &&
				  isdigit(UCH(cp4[9])));
		    BOOL given_is_8859like
			= (BOOL) (given_is_8859 ||
				  !strncmp(cp4, "windows-", 8) ||
				  !strncmp(cp4, "cp12", 4) ||
				  !strncmp(cp4, "cp-12", 5));
		    BOOL given_and_display_8859like
			= (BOOL) (given_is_8859like &&
				  (strstr(LYchar_set_names[current_char_set],
					  "ISO-8859") ||
				   strstr(LYchar_set_names[current_char_set],
					  "windows-")));

		    if (given_and_display_8859like) {
			*cp1 = '\0';
			me->format = HTAtom_for(cp);
		    }
		    if (given_is_8859) {
			cp1 = &cp4[10];
			while (*cp1 &&
			       isdigit(UCH(*cp1)))
			    cp1++;
			*cp1 = '\0';
		    }
		    if (given_and_display_8859like) {
			StrAllocCopy(me->anchor->charset, cp4);
			HTPassEightBitRaw = TRUE;
		    }
		    HTAlert(*cp4 ? cp4 : me->anchor->charset);
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
	} else if (LYmktime(me->anchor->expires, FALSE) == 0) {
	    /*
	    **  We don't have a Date header, and
	    **  the value is in past for us. - FM
	    */
	    me->anchor->no_cache = TRUE;
	}
    }
    StrAllocCopy(me->anchor->content_type,
		 HTAtom_name(me->format));

    if (me->set_cookie != NULL || me->set_cookie2 != NULL) {
	LYSetCookie(me->set_cookie,
		    me->set_cookie2,
		    me->anchor->address);
	FREE(me->set_cookie);
	FREE(me->set_cookie2);
    }
    if (me->pickup_redirection) {
	if (me->location && *me->location) {
	    redirecting_url = me->location;
	    me->location = NULL;
	    if (me->targetRep != WWW_DEBUG || me->sink)
		me->head_only = YES;

	} else {
	    permanent_redirection = FALSE;
	    if (me->location) {
		CTRACE((tfp, "HTTP: 'Location:' is zero-length!\n"));
		HTAlert(REDIRECTION_WITH_BAD_LOCATION);
	    }
	    CTRACE((tfp, "HTTP: Failed to pick up location.\n"));
	    if (me->location) {
		FREE(me->location);
	    } else {
		HTAlert(REDIRECTION_WITH_NO_LOCATION);
	    }
	}
    }
    if (me->head_only) {
	/* We are done! - kw */
	me->state = MIME_IGNORE;
	return HT_OK;
    }

    if (me->no_streamstack) {
	me->target = me->sink;
    } else {
	if (!me->compression_encoding) {
	    CTRACE((tfp, "HTMIME: MIME Content-Type is '%s', converting to '%s'\n",
		    HTAtom_name(me->format), HTAtom_name(me->targetRep)));
	} else {
	    /*
	    **	Change the format to that for "www/compressed"
	    **	and set up a stream to deal with it. - FM
	    */
	    CTRACE((tfp, "HTMIME: MIME Content-Type is '%s',\n", HTAtom_name(me->format)));
	    me->format = HTAtom_for("www/compressed");
	    CTRACE((tfp, "        Treating as '%s'.  Converting to '%s'\n",
		    HTAtom_name(me->format), HTAtom_name(me->targetRep)));
	    FREE(me->compression_encoding);
	}
	me->target = HTStreamStack(me->format, me->targetRep,
				   me->sink , me->anchor);
	if (!me->target) {
	    CTRACE((tfp, "HTMIME: Can't translate! ** \n"));
	    me->target = me->sink;	/* Cheat */
	}
    }
    if (me->target) {
	me->targetClass = *me->target->isa;
	/*
	**	Check for encoding and select state from there,
	**	someday, but until we have the relevant code,
	**	from now push straight through. - FM
	*/
	me->state = MIME_TRANSPARENT;	/* Pump rest of data right through */
    } else {
	me->state = MIME_IGNORE;	/* What else to do? */
    }
    if (me->refresh_url != NULL && !content_is_compressed(me)) {
	char *url = NULL;
	char *num = NULL;
	char *txt = NULL;
	char *base = "";	/* FIXME: refresh_url may be relative to doc */

	LYParseRefreshURL(me->refresh_url, &num, &url);
	if (url != NULL && me->format == WWW_HTML) {
	    CTRACE((tfp, "Formatting refresh-url as first line of result\n"));
	    HTSprintf0(&txt, gettext("Refresh: "));
	    HTSprintf(&txt, gettext("%s seconds "), num);
	    dequote(url);
	    HTSprintf(&txt, "<a href=\"%s%s\">%s</a><br>", base, url, url);
	    CTRACE((tfp, "URL %s%s\n", base, url));
	    (me->isa->put_string)(me, txt);
	    free(txt);
	}
	FREE(num);
	FREE(url);
    }
    return HT_OK;
}

PRIVATE int dispatchField ARGS1(HTStream *, me)
{
    int i, j;
    char *cp;

    *me->value_pointer = '\0';
    cp = me->value_pointer;
    while ((cp > me->value) && *(--cp) == ' ')  /* S/390 -- gil -- 0146 */
	/*
	**  Trim trailing spaces.
	*/
	*cp = '\0';

    switch (me->field) {
    case miACCEPT_RANGES:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Accept-Ranges: '%s'\n",
		me->value));
	break;
    case miAGE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Age: '%s'\n",
		me->value));
	break;
    case miALLOW:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Allow: '%s'\n",
		me->value));
	break;
    case miALTERNATES:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Alternates: '%s'\n",
		me->value));
	break;
    case miCACHE_CONTROL:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Cache-Control: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Convert to lowercase and indicate in anchor. - FM
	*/
	LYLowerCase(me->value);
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
		    if (isdigit(UCH(*cp1))) {
			cp0 = cp1;
			while (isdigit(UCH(*cp1)))
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
	CTRACE((tfp, "HTMIME: PICKED UP Cookie: '%s'\n",
		me->value));
	break;
    case miCONNECTION:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Connection: '%s'\n",
		me->value));
	break;
    case miCONTENT_BASE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Base: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->content_base, me->value);
	break;
    case miCONTENT_DISPOSITION:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Disposition: '%s'\n",
		me->value));
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
	CTRACE((tfp, "HTMIME: PICKED UP Content-Encoding: '%s'\n",
		me->value));
	if (!(me->value && *me->value) ||
	    !strcasecomp(me->value, "identity"))
	    break;
	/*
	**  Convert to lowercase and indicate in anchor. - FM
	*/
	LYLowerCase(me->value);
	StrAllocCopy(me->anchor->content_encoding, me->value);
	FREE(me->compression_encoding);
	if (content_is_compressed(me)) {
	    /*
	    **	Save it to use as a flag for setting
	    **	up a "www/compressed" target. - FM
	    */
	    StrAllocCopy(me->compression_encoding, me->value);
	} else {
	    /*
	    **	Some server indicated "8bit", "7bit" or "binary"
	    **	inappropriately.  We'll ignore it. - FM
	    */
	    CTRACE((tfp, "                Ignoring it!\n"));
	}
	break;
    case miCONTENT_FEATURES:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Features: '%s'\n",
		me->value));
	break;
    case miCONTENT_LANGUAGE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Language: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Convert to lowercase and indicate in anchor. - FM
	*/
	LYLowerCase(me->value);
	StrAllocCopy(me->anchor->content_language, me->value);
	break;
    case miCONTENT_LENGTH:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Length: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Convert to integer and indicate in anchor. - FM
	*/
	me->anchor->content_length = atoi(me->value);
	if (me->anchor->content_length < 0)
	    me->anchor->content_length = 0;
	CTRACE((tfp, "        Converted to integer: '%d'\n",
		me->anchor->content_length));
	break;
    case miCONTENT_LOCATION:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Location: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->content_location, me->value);
	break;
    case miCONTENT_MD5:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-MD5: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->content_md5, me->value);
	break;
    case miCONTENT_RANGE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Range: '%s'\n",
		me->value));
	break;
    case miCONTENT_TRANSFER_ENCODING:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Transfer-Encoding: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Force the Content-Transfer-Encoding value
	**  to all lower case. - FM
	*/
	LYLowerCase(me->value);
	me->encoding = HTAtom_for(me->value);
	break;
    case miCONTENT_TYPE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Content-Type: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Force the Content-Type value to all lower case
	**  and strip spaces and double-quotes. - FM
	*/
	for (i = 0, j = 0; me->value[i]; i++) {
	    if (me->value[i] != ' ' && me->value[i] != '\"') {
		me->value[j++] = (char) TOLOWER(me->value[i]);
	    }
	}
	me->value[j] = '\0';
	me->format = HTAtom_for(me->value);
	break;
    case miDATE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Date: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->date, me->value);
	break;
    case miETAG:
	/*  Do not trim double quotes:
	 *  an entity tag consists of an opaque quoted string,
	 *  possibly prefixed by a weakness indicator.
	 */
	CTRACE((tfp, "HTMIME: PICKED UP ETag: %s\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->ETag, me->value);
	break;
    case miEXPIRES:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Expires: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->expires, me->value);
	break;
    case miKEEP_ALIVE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Keep-Alive: '%s'\n",
		me->value));
	break;
    case miLAST_MODIFIED:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Last-Modified: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->last_modified, me->value);
	break;
    case miLINK:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Link: '%s'\n",
		me->value));
	break;
    case miLOCATION:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Location: '%s'\n",
		me->value));
	if (me->pickup_redirection && !me->location) {
	    StrAllocCopy(me->location, me->value);
	} else {
	    CTRACE((tfp, "HTMIME: *** Ignoring Location!\n"));
	}
	break;
    case miPRAGMA:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Pragma: '%s'\n",
		me->value));
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
	CTRACE((tfp, "HTMIME: PICKED UP Proxy-Authenticate: '%s'\n",
		me->value));
	break;
    case miPUBLIC:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Public: '%s'\n",
		me->value));
	break;
    case miREFRESH:		/* nonstandard: Netscape */
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Refresh: '%s'\n",
		me->value));
	StrAllocCopy(me->refresh_url, me->value);
	break;
    case miRETRY_AFTER:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Retry-After: '%s'\n",
		me->value));
	break;
    case miSAFE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Safe: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor if "YES" or "TRUE". - FM
	*/
	if (!strcasecomp(me->value, "YES") ||
	    !strcasecomp(me->value, "TRUE")) {
	    me->anchor->safe = TRUE;
	} else if (!strcasecomp(me->value, "NO") ||
		   !strcasecomp(me->value, "FALSE")) {
	    /*
	    **  If server explicitly tells us that it has changed
	    **  its mind, reset flag in anchor. - kw
	    */
	    me->anchor->safe = FALSE;
	}
	break;
    case miSERVER:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Server: '%s'\n",
		me->value));
	if (!(me->value && *me->value))
	    break;
	/*
	**  Indicate in anchor. - FM
	*/
	StrAllocCopy(me->anchor->server, me->value);
	break;
    case miSET_COOKIE1:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Set-Cookie: '%s'\n",
		me->value));
	if (me->set_cookie == NULL) {
	    StrAllocCopy(me->set_cookie, me->value);
	} else {
	    StrAllocCat(me->set_cookie, ", ");
	    StrAllocCat(me->set_cookie, me->value);
	}
	break;
    case miSET_COOKIE2:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Set-Cookie2: '%s'\n",
		me->value));
	if (me->set_cookie2 == NULL) {
	    StrAllocCopy(me->set_cookie2, me->value);
	} else {
	    StrAllocCat(me->set_cookie2, ", ");
	    StrAllocCat(me->set_cookie2, me->value);
	}
	break;
    case miTITLE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Title: '%s'\n",
		me->value));
	break;
    case miTRANSFER_ENCODING:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Transfer-Encoding: '%s'\n",
		me->value));
	break;
    case miUPGRADE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Upgrade: '%s'\n",
		me->value));
	break;
    case miURI:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP URI: '%s'\n",
		me->value));
	break;
    case miVARY:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Vary: '%s'\n",
		me->value));
	break;
    case miVIA:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Via: '%s'\n",
		me->value));
	break;
    case miWARNING:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP Warning: '%s'\n",
		me->value));
	break;
    case miWWW_AUTHENTICATE:
	HTMIME_TrimDoubleQuotes(me->value);
	CTRACE((tfp, "HTMIME: PICKED UP WWW-Authenticate: '%s'\n",
		me->value));
	break;
    default:		/* Should never get here */
	return HT_ERROR;
    }
    return HT_OK;
}


/*_________________________________________________________________________
**
**			A C T I O N	R O U T I N E S
*/

/*	Character handling
**	------------------
**
**	This is a FSM parser. It ignores field names it does not understand.
**	Folded header fields are recognized.  Lines without a fieldname at
**	the beginning (that are not folded continuation lines) are ignored
**	as unknown field names.  Fields with empty values are not picked up.
*/
PRIVATE void HTMIME_put_character ARGS2(
	HTStream *,	me,
	char,		c)
{
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
	/*
	** <sigh> This is evidence that at one time, this code supported
	** local character sets other than ASCII.  But there is so much
	** code in HTTP.c that depends on line_buffer's having been
	** translated to local character set that I needed to put the
	** FROMASCII translation there, leaving this translation purely
	** destructive.  -- gil
	*/  /* S/390 -- gil -- 0118 */
#ifndef   NOT_ASCII
	c = FROMASCII(c);
#endif /* NOT_ASCII */
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
	    if (me->state == miGET_VALUE &&
		me->value_pointer && me->value_pointer != me->value &&
		!WHITE(*(me->value_pointer-1))) {
		c = ' ';
		goto GET_VALUE;	/* will add space to value if it fits - kw */
	    }
	    break;
	} else if (me->fold_state == miGET_VALUE) {
	    /* Got a field, and now we know it's complete - so
	     * act on it. - kw */
	    dispatchField(me);
	}
	/* FALLTHRU */

    case miBEGINNING_OF_LINE:
	me->net_ascii = YES;
	switch (c) {
	case 'a':
	case 'A':
	    me->state = miA;
	    CTRACE((tfp, "HTMIME: Got 'A' at beginning of line, state now A\n"));
	    break;

	case 'c':
	case 'C':
	    me->state = miC;
	    CTRACE((tfp, "HTMIME: Got 'C' at beginning of line, state now C\n"));
	    break;

	case 'd':
	case 'D':
	    me->check_pointer = "ate:";
	    me->if_ok = miDATE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Got 'D' at beginning of line, checking for 'ate:'\n"));
	    break;

	case 'e':
	case 'E':
	    me->state = miE;
	    CTRACE((tfp, "HTMIME: Got 'E' at beginning of line, state now E\n"));
	    break;

	case 'k':
	case 'K':
	    me->check_pointer = "eep-alive:";
	    me->if_ok = miKEEP_ALIVE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Got 'K' at beginning of line, checking for 'eep-alive:'\n"));
	    break;

	case 'l':
	case 'L':
	    me->state = miL;
	    CTRACE((tfp, "HTMIME: Got 'L' at beginning of line, state now L\n"));
	    break;

	case 'p':
	case 'P':
	    me->state = miP;
	    CTRACE((tfp, "HTMIME: Got 'P' at beginning of line, state now P\n"));
	    break;

	case 'r':
	case 'R':
	    me->state = miR;
	    CTRACE((tfp, "HTMIME: Got 'R' at beginning of line, state now R\n"));
	    break;

	case 's':
	case 'S':
	    me->state = miS;
	    CTRACE((tfp, "HTMIME: Got 'S' at beginning of line, state now S\n"));
	    break;

	case 't':
	case 'T':
	    me->state = miT;
	    CTRACE((tfp, "HTMIME: Got 'T' at beginning of line, state now T\n"));
	    break;

	case 'u':
	case 'U':
	    me->state = miU;
	    CTRACE((tfp, "HTMIME: Got 'U' at beginning of line, state now U\n"));
	    break;

	case 'v':
	case 'V':
	    me->state = miV;
	    CTRACE((tfp, "HTMIME: Got 'V' at beginning of line, state now V\n"));
	    break;

	case 'w':
	case 'W':
	    me->state = miW;
	    CTRACE((tfp, "HTMIME: Got 'W' at beginning of line, state now W\n"));
	    break;

	case '\n':			/* Blank line: End of Header! */
	    {
		me->net_ascii = NO;
		pumpData(me);
	    }
	    break;

	default:
	   goto bad_field_name;

	} /* switch on character */
	break;

    case miA:				/* Check for 'c','g' or 'l' */
	switch (c) {
	case 'c':
	case 'C':
	    me->check_pointer = "cept-ranges:";
	    me->if_ok = miACCEPT_RANGES;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was A, found C, checking for 'cept-ranges:'\n"));
	    break;

	case 'g':
	case 'G':
	    me->check_pointer = "e:";
	    me->if_ok = miAGE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was A, found G, checking for 'e:'\n"));
	    break;

	case 'l':
	case 'L':
	    me->state = miAL;
	    CTRACE((tfp, "HTMIME: Was A, found L, state now AL'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'g' or 'l'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miAL:				/* Check for 'l' or 't' */
	switch (c) {
	case 'l':
	case 'L':
	    me->check_pointer = "ow:";
	    me->if_ok = miALLOW;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was AL, found L, checking for 'ow:'\n"));
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "ernates:";
	    me->if_ok = miALTERNATES;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was AL, found T, checking for 'ernates:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'l' or 't'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miC:				/* Check for 'a' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "che-control:";
	    me->if_ok = miCACHE_CONTROL;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was C, found A, checking for 'che-control:'\n"));
	    break;

	case 'o':
	case 'O':
	    me->state = miCO;
	    CTRACE((tfp, "HTMIME: Was C, found O, state now CO'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'o'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miCO:				/* Check for 'n' or 'o' */
	switch (c) {
	case 'n':
	case 'N':
	    me->state = miCON;
	    CTRACE((tfp, "HTMIME: Was CO, found N, state now CON\n"));
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "kie:";
	    me->if_ok = miCOOKIE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CO, found O, checking for 'kie:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'n' or 'o'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miCON:				/* Check for 'n' or 't' */
	switch (c) {
	case 'n':
	case 'N':
	    me->check_pointer = "ection:";
	    me->if_ok = miCONNECTION;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CON, found N, checking for 'ection:'\n"));
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "ent-";
	    me->if_ok = miCONTENT_;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CON, found T, checking for 'ent-'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'n' or 't'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miE:				/* Check for 't' or 'x' */
	switch (c) {
	case 't':
	case 'T':
	    me->check_pointer = "ag:";
	    me->if_ok = miETAG;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was E, found T, checking for 'ag:'\n"));
	    break;

	case 'x':
	case 'X':
	    me->check_pointer = "pires:";
	    me->if_ok = miEXPIRES;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was E, found X, checking for 'pires:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'t' or 'x'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miL:				/* Check for 'a', 'i' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "st-modified:";
	    me->if_ok = miLAST_MODIFIED;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was L, found A, checking for 'st-modified:'\n"));
	    break;

	case 'i':
	case 'I':
	    me->check_pointer = "nk:";
	    me->if_ok = miLINK;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was L, found I, checking for 'nk:'\n"));
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "cation:";
	    me->if_ok = miLOCATION;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was L, found O, checking for 'cation:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a', 'i' or 'o'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miP:				/* Check for 'r' or 'u' */
	switch (c) {
	case 'r':
	case 'R':
	    me->state = miPR;
	    CTRACE((tfp, "HTMIME: Was P, found R, state now PR'\n"));
	    break;

	case 'u':
	case 'U':
	    me->check_pointer = "blic:";
	    me->if_ok = miPUBLIC;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was P, found U, checking for 'blic:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'r' or 'u'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miPR:				/* Check for 'a' or 'o' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "gma:";
	    me->if_ok = miPRAGMA;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was PR, found A, checking for 'gma'\n"));
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "xy-authenticate:";
	    me->if_ok = miPROXY_AUTHENTICATE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was PR, found O, checking for 'xy-authenticate'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'o'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miR:				/* Check for 'e' */
	switch (c) {
	case 'e':
	case 'E':
	    me->state = miRE;
	    CTRACE((tfp, "HTMIME: Was R, found E\n"));
	    break;
	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'e'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miRE:				/* Check for 'a' or 'o' */
	switch (c) {
	case 'f':
	case 'F':			/* nonstandard: Netscape */
	    me->check_pointer = "resh:";
	    me->if_ok = miREFRESH;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was RE, found F, checking for '%s'\n", me->check_pointer));
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "ry-after:";
	    me->if_ok = miRETRY_AFTER;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was RE, found T, checking for '%s'\n", me->check_pointer));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'f' or 't'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miS:				/* Check for 'a' or 'e' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "fe:";
	    me->if_ok = miSAFE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was S, found A, checking for 'fe:'\n"));
	    break;

	case 'e':
	case 'E':
	    me->state = miSE;
	    CTRACE((tfp, "HTMIME: Was S, found E, state now SE'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'e'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miSE:				/* Check for 'r' or 't' */
	switch (c) {
	case 'r':
	case 'R':
	    me->check_pointer = "ver:";
	    me->if_ok = miSERVER;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was SE, found R, checking for 'ver'\n"));
	    break;

	case 't':
	case 'T':
	    me->check_pointer = "-cookie";
	    me->if_ok = miSET_COOKIE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was SE, found T, checking for '-cookie'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'r' or 't'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miSET_COOKIE:			/* Check for ':' or '2' */
	switch (c) {
	case ':':
	    me->field = miSET_COOKIE1;		/* remember it */
	    me->state = miSKIP_GET_VALUE;
	    CTRACE((tfp, "HTMIME: Was SET_COOKIE, found :, processing\n"));
	    break;

	case '2':
	    me->check_pointer = ":";
	    me->if_ok = miSET_COOKIE2;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was SET_COOKIE, found 2, checking for ':'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "':' or '2'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miT:				/* Check for 'i' or 'r' */
	switch (c) {
	case 'i':
	case 'I':
	    me->check_pointer = "tle:";
	    me->if_ok = miTITLE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was T, found I, checking for 'tle:'\n"));
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "ansfer-encoding:";
	    me->if_ok = miTRANSFER_ENCODING;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was T, found R, checking for 'ansfer-encoding'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'i' or 'r'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miU:				/* Check for 'p' or 'r' */
	switch (c) {
	case 'p':
	case 'P':
	    me->check_pointer = "grade:";
	    me->if_ok = miUPGRADE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was U, found P, checking for 'grade:'\n"));
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "i:";
	    me->if_ok = miURI;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was U, found R, checking for 'i:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'p' or 'r'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miV:				/* Check for 'a' or 'i' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "ry:";
	    me->if_ok = miVARY;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was V, found A, checking for 'ry:'\n"));
	    break;

	case 'i':
	case 'I':
	    me->check_pointer = "a:";
	    me->if_ok = miVIA;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was V, found I, checking for 'a:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'i'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miW:				/* Check for 'a' or 'w' */
	switch (c) {
	case 'a':
	case 'A':
	    me->check_pointer = "rning:";
	    me->if_ok = miWARNING;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was W, found A, checking for 'rning:'\n"));
	    break;

	case 'w':
	case 'W':
	    me->check_pointer = "w-authenticate:";
	    me->if_ok = miWWW_AUTHENTICATE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was W, found W, checking for 'w-authenticate:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, "'a' or 'w'"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miCHECK:			/* Check against string */
	if (TOLOWER(c) == *(me->check_pointer)++) {
	    if (!*me->check_pointer)
		me->state = me->if_ok;
	} else {		/* Error */
	    CTRACE((tfp, "HTMIME: Bad character `%c' found where `%s' expected\n",
			c, me->check_pointer - 1));
	    goto bad_field_name;
	}
	break;

    case miCONTENT_:
	CTRACE((tfp, "HTMIME: in case CONTENT_\n"));

	switch(c) {
	case 'b':
	case 'B':
	    me->check_pointer = "ase:";
	    me->if_ok = miCONTENT_BASE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found B, checking for 'ase:'\n"));
	    break;

	case 'd':
	case 'D':
	    me->check_pointer = "isposition:";
	    me->if_ok = miCONTENT_DISPOSITION;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found D, checking for 'isposition:'\n"));
	    break;

	case 'e':
	case 'E':
	    me->check_pointer = "ncoding:";
	    me->if_ok = miCONTENT_ENCODING;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found E, checking for 'ncoding:'\n"));
	    break;

	case 'f':
	case 'F':
	    me->check_pointer = "eatures:";
	    me->if_ok = miCONTENT_FEATURES;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found F, checking for 'eatures:'\n"));
	    break;

	case 'l':
	case 'L':
	    me->state = miCONTENT_L;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found L, state now CONTENT_L\n"));
	    break;

	case 'm':
	case 'M':
	    me->check_pointer = "d5:";
	    me->if_ok = miCONTENT_MD5;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found M, checking for 'd5:'\n"));
	    break;

	case 'r':
	case 'R':
	    me->check_pointer = "ange:";
	    me->if_ok = miCONTENT_RANGE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found R, checking for 'ange:'\n"));
	    break;

	case 't':
	case 'T':
	    me->state = miCONTENT_T;
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found T, state now CONTENT_T\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Was CONTENT_, found nothing; bleah\n"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miCONTENT_L:
	CTRACE((tfp, "HTMIME: in case CONTENT_L\n"));

      switch(c) {
	case 'a':
	case 'A':
	    me->check_pointer = "nguage:";
	    me->if_ok = miCONTENT_LANGUAGE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_L, found A, checking for 'nguage:'\n"));
	    break;

	case 'e':
	case 'E':
	    me->check_pointer = "ngth:";
	    me->if_ok = miCONTENT_LENGTH;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_L, found E, checking for 'ngth:'\n"));
	    break;

	case 'o':
	case 'O':
	    me->check_pointer = "cation:";
	    me->if_ok = miCONTENT_LOCATION;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_L, found O, checking for 'cation:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Was CONTENT_L, found nothing; bleah\n"));
	    goto bad_field_name;

	} /* switch on character */
	break;

    case miCONTENT_T:
	CTRACE((tfp, "HTMIME: in case CONTENT_T\n"));

      switch(c) {
	case 'r':
	case 'R':
	    me->check_pointer = "ansfer-encoding:";
	    me->if_ok = miCONTENT_TRANSFER_ENCODING;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_T, found R, checking for 'ansfer-encoding:'\n"));
	    break;

	case 'y':
	case 'Y':
	    me->check_pointer = "pe:";
	    me->if_ok = miCONTENT_TYPE;
	    me->state = miCHECK;
	    CTRACE((tfp, "HTMIME: Was CONTENT_T, found Y, checking for 'pe:'\n"));
	    break;

	default:
	    CTRACE((tfp, "HTMIME: Was CONTENT_T, found nothing; bleah\n"));
	    goto bad_field_name;

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
    case miREFRESH:
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
    GET_VALUE:
	if (c != '\n') {			/* Not end of line */
	    if (me->value_pointer < me->value + VALUE_SIZE - 1) {
		*me->value_pointer++ = c;
		break;
	    } else {
		goto value_too_long;
	    }
	}
	/* Fall through (if end of line) */

    case miJUNK_LINE:
	if (c == '\n') {
	    me->fold_state = me->state;
	    me->state = miNEWLINE;
	}
	break;


    } /* switch on state*/

    return;

value_too_long:
    CTRACE((tfp, "HTMIME: *** Syntax error. (string too long)\n"));

bad_field_name:				/* Ignore it */
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
	CTRACE((tfp, "HTMIME:  %s\n", s));

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
	CTRACE((tfp, "HTMIME:  %.*s\n", l, s));

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
    if (me) {
	FREE(me->location);
	FREE(me->compression_encoding);
	if (me->target)
	    (*me->targetClass._free)(me->target);
	FREE(me);
    }
}

/*	End writing
*/
PRIVATE void HTMIME_abort ARGS2(
	HTStream *,	me,
	HTError,	e)
{
    if (me) {
	FREE(me->location);
	FREE(me->compression_encoding);
	if (me->target)
	    (*me->targetClass._abort)(me->target, e);
	FREE(me);
    }
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

    me = typecalloc(HTStream);
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
    FREE(me->anchor->ETag);
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
    me->set_cookie =	NULL;		/* Not set yet */
    me->set_cookie2 =	NULL;		/* Not set yet */
    me->refresh_url =	NULL;		/* Not set yet */
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

PUBLIC HTStream* HTMIMERedirect ARGS3(
	HTPresentation *,	pres,
	HTParentAnchor *,	anchor,
	HTStream *,		sink)
{
    HTStream* me = HTMIMEConvert(pres,anchor, sink);
    if (!me)
	return NULL;

    me->pickup_redirection = YES;
    if (me->targetRep == WWW_DEBUG && sink)
	me->no_streamstack = YES;
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

/*
**  MIME decoding routines
**
**	Written by S. Ichikawa,
**	partially inspired by encdec.c of <jh@efd.lth.se>.
*/
#include <LYCharVals.h>  /* S/390 -- gil -- 0163 */

PRIVATE char HTmm64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=" ;
PRIVATE char HTmmquote[] = "0123456789ABCDEF";
PRIVATE int HTmmcont = 0;

PRIVATE void HTmmdec_base64 ARGS2(
	char **,	t,
	char *,		s)
{
    int   d, count, j, val;
    char *buf, *bp, nw[4], *p;

    if ((buf = malloc(strlen(s) * 3 + 1)) == 0)
	outofmem(__FILE__, "HTmmdec_base64");

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
		nw[j] = (char) (val & 255);
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
    StrAllocCopy(*t, buf);
    FREE(buf);
}

PRIVATE void HTmmdec_quote ARGS2(
	char **,	t,
	char *,		s)
{
    char *buf, cval, *bp, *p;

    if ((buf = malloc(strlen(s) + 1)) == 0)
	outofmem(__FILE__, "HTmmdec_quote");

    for (bp = buf; *s; ) {
	if (*s == '=') {
	    cval = 0;
	    if (s[1] && (p = strchr(HTmmquote, s[1]))) {
		cval += (char) (p - HTmmquote);
	    } else {
		*bp++ = *s++;
		continue;
	    }
	    if (s[2] && (p = strchr(HTmmquote, s[2]))) {
		cval <<= 4;
		cval += (char) (p - HTmmquote);
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
    StrAllocCopy(*t, buf);
    FREE(buf);
}

/*
**	HTmmdecode for ISO-2022-JP - FM
*/
PUBLIC void HTmmdecode ARGS2(
	char **,	target,
	char *,		source)
{
    char *buf;
    char *mmbuf = NULL;
    char *m2buf = NULL;
    char *s, *t, *u;
    int  base64, quote;

    if ((buf = malloc(strlen(source) + 1)) == 0)
	outofmem(__FILE__, "HTmmdecode");
  
    for (s = source, u = buf; *s;) {
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
		    t >= source && (*t == ' ' || *t == '\t'); t--) {
			u--;
		}
	    }
	    if (mmbuf == 0)	/* allocate buffer big enough for source */
		StrAllocCopy(mmbuf, source);
	    for (s += 16, t = mmbuf; *s; ) {
		if (s[0] == '?' && s[1] == '=') {
		    break;
		} else {
		    *t++ = *s++;
		    *t = '\0';
		}
	    }
	    if (s[0] != '?' || s[1] != '=') {
		goto end;
	    } else {
		s += 2;
		*t = '\0';
	    }
	    if (base64)
		HTmmdec_base64(&m2buf, mmbuf);
	    if (quote)
		HTmmdec_quote(&m2buf, mmbuf);
	    for (t = m2buf; *t; )
		*u++ = *t++;
	    HTmmcont = 1;
	} else {
	    if (*s != ' ' && *s != '\t')
		HTmmcont = 0;
	    *u++ = *s++;
	}
    }
    *u = '\0';
end:
    StrAllocCopy(*target, buf);
    FREE(m2buf);
    FREE(mmbuf);
    FREE(buf);
}

/*
**  Insert ESC where it seems lost.
**  (The author of this function "rjis" is S. Ichikawa.)
*/
PUBLIC int HTrjis ARGS2(
	char **,	t,
	char *,		s)
{
    char *p;
    char *buf = NULL;
    int kanji = 0;

    if (strchr(s, CH_ESC) || !strchr(s, '$')) {
	if (s != *t)
	    StrAllocCopy(*t, s);
	return 1;
    }

    if ((buf = malloc(strlen(s) * 2 + 1)) == 0)
	outofmem(__FILE__, "HTrjis");

    for (p = buf; *s; ) {
	if (!kanji && s[0] == '$' && (s[1] == '@' || s[1] == 'B')) {
	    if (HTmaybekanji((int)s[2], (int)s[3])) {
		kanji = 1;
		*p++ = CH_ESC;
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
	    *p++ = CH_ESC;
	    *p++ = *s++;
	    *p++ = *s++;
	    continue;
	}
	*p++ = *s++;
    }
    *p = *s;	/* terminate string */

    StrAllocCopy(*t, buf);
    FREE(buf);
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
 * $Header: /home/cvs/src/gnu/usr.bin/lynx/WWW/Library/Implementation/Attic/HTMIME.c,v 1.5 2005/11/04 04:24:03 fgsch Exp $
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
