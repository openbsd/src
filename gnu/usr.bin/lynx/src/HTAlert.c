/*	Displaying messages and getting input for Lynx Browser
**	==========================================================
**
**	REPLACE THIS MODULE with a GUI version in a GUI environment!
**
** History:
**	   Jun 92 Created May 1992 By C.T. Barker
**	   Feb 93 Simplified, portablised TBL
**
*/

#include <HTUtils.h>
#include <HTAlert.h>
#include <LYGlobalDefs.h>
#include <LYCurses.h>
#include <LYStrings.h>
#include <LYUtils.h>
#include <LYClean.h>
#include <GridText.h>
#include <LYCookie.h>
#include <LYHistory.h> /* store statusline messages */

#include <LYLeaks.h>

/*	Issue a message about a problem.		HTAlert()
**	--------------------------------
*/
PUBLIC void HTAlert ARGS1(
	CONST char *,	Msg)
{
    CTRACE(tfp, "\nAlert!: %s\n\n", Msg);
    CTRACE_FLUSH(tfp);
    _user_message(ALERT_FORMAT, Msg);
    LYstore_message2(ALERT_FORMAT, Msg);

    sleep(AlertSecs);
}

PUBLIC void HTAlwaysAlert ARGS2(
	CONST char *,	extra_prefix,
	CONST char *,	Msg)
{
    if (!dump_output_immediately && LYCursesON) {
	HTAlert(Msg);
    } else {
	if (extra_prefix) {
	    fprintf(((TRACE) ? stdout : stderr),
		    "%s %s!\n",
		    extra_prefix, Msg);
	    fflush(stdout);
	    sleep(AlertSecs);
	} else {
	    fprintf(((TRACE) ? stdout : stderr),
		    ALERT_FORMAT,
		    (Msg == 0) ? "" : Msg);
	    fflush(stdout);
	    sleep(AlertSecs);
	    fprintf(((TRACE) ? stdout : stderr), "\n");
	}
	CTRACE(tfp, "\nAlert!: %s\n\n", Msg);
	CTRACE_FLUSH(tfp);
    }
}

/*	Issue an informational message.			HTInfoMsg()
**	--------------------------------
*/
PUBLIC void HTInfoMsg ARGS1(
	CONST char *,	Msg)
{
    _statusline(Msg);
    if (Msg && *Msg) {
	CTRACE(tfp, "Info message: %s\n", Msg);
	LYstore_message(Msg);
	sleep(InfoSecs);
    }
}

/*	Issue an important message.			HTUserMsg()
**	--------------------------------
*/
PUBLIC void HTUserMsg ARGS1(
	CONST char *,	Msg)
{
    _statusline(Msg);
    if (Msg && *Msg) {
	CTRACE(tfp, "User message: %s\n", Msg);
	LYstore_message(Msg);
	sleep(MessageSecs);
    }
}

PUBLIC void HTUserMsg2 ARGS2(
	CONST char *,	Msg2,
	CONST char *,	Arg)
{
    _user_message(Msg2, Arg);
    if (Msg2 && *Msg2) {
	CTRACE(tfp, "User message: ");
	CTRACE(tfp, Msg2, Arg);
	CTRACE(tfp, "\n");
	LYstore_message2(Msg2, Arg);
	sleep(MessageSecs);
    }
}

/*	Issue a progress message.			HTProgress()
**	-------------------------
*/
PUBLIC void HTProgress ARGS1(
	CONST char *,	Msg)
{
    statusline(Msg);
    LYstore_message(Msg);
    CTRACE(tfp, "%s\n", Msg);
}

/*	Issue a read-progress message.			HTReadProgress()
**	------------------------------
*/
PUBLIC void HTReadProgress ARGS2(
	long,		bytes,
	long,		total)
{
    static long kb_units = 1024;
    static time_t first, last;
    static long bytes_last;
    static long transfer_rate = 0;
    long divisor;
    char line[80];
    time_t now = time((time_t *)0);  /* once per second */
    static char *units = "bytes";

    if (bytes == 0) {
	first = last = now;
	bytes_last = bytes;
    } else if ((bytes > 0) &&
	       (now != first))
		/* 1 sec delay for transfer_rate calculation :-( */ {
	if (transfer_rate <= 0)    /* the very first time */
	    transfer_rate = (bytes) / (now - first);   /* bytes/sec */

	/*
	 * Optimal refresh time:  every 0.2 sec, use interpolation.  Transfer
	 * rate is not constant when we have partial content in a proxy, so
	 * interpolation lies - will check every second at least for sure.
	 */
	if (((bytes - bytes_last) > (transfer_rate / 5)) || (now != last)) {

	    bytes_last += (transfer_rate / 5);	/* until we got next second */

	    if (now != last) {
		last = now;
		bytes_last = bytes;
		transfer_rate = (bytes_last) / (last - first); /* more accurate here */
	    }

	    units = gettext("bytes");
	    divisor = 1;
	    if (LYshow_kb_rate
	      && (total >= kb_units || bytes >= kb_units)) {
		units = gettext("KB");
		divisor = kb_units;
		bytes /= divisor;
		if (total > 0) total /= divisor;
	    }

	    if (total >  0)
		sprintf (line, gettext("Read %ld of %ld %s of data"), bytes, total, units);
	    else
		sprintf (line, gettext("Read %ld %s of data"), bytes, units);
	    if ((transfer_rate > 0)
		  && (!LYshow_kb_rate || (bytes * divisor >= kb_units)))
		sprintf (line + strlen(line), gettext(", %ld %s/sec."), transfer_rate / divisor, units);
	    else
		sprintf (line + strlen(line), ".");
	    if (total <  0) {
		if (total < -1)
		    strcat(line, gettext(" (Press 'z' to abort)"));
	    }

	    /* do not store the message for history page. */
	    statusline(line);
	    CTRACE(tfp, "%s\n", line);
	}
    }
}

PRIVATE BOOL conf_cancelled = NO; /* used by HTConfirm only - kw */

PUBLIC BOOL HTLastConfirmCancelled NOARGS
{
    if (conf_cancelled) {
	conf_cancelled = NO;	/* reset */
	return(YES);
    } else {
	return(NO);
    }
}

#define DFT_CONFIRM ~(YES|NO)

/*	Seek confirmation.				HTConfirm()
**	------------------
*/
PUBLIC BOOL HTConfirmDefault ARGS2(CONST char *, Msg, int, Dft)
{
    char *msg_yes = gettext("yes");
    char *msg_no  = gettext("no");
    int result = -1;

    conf_cancelled = NO;
    if (dump_output_immediately) { /* Non-interactive, can't respond */
	result = NO;
    } else {
	char *msg = NULL;

	if (Dft == DFT_CONFIRM)
	    HTSprintf0(&msg, "%s (%c/%c) ", Msg, *msg_yes, *msg_no);
	else
	    HTSprintf0(&msg, "%s (%c) ", Msg, (Dft == YES) ? *msg_yes : *msg_no);
	_statusline(msg);
	FREE(msg);

	while (result < 0) {
	    int c = LYgetch();
#ifdef VMS
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		c = *msg_no;
	    }
#endif /* VMS */
	    if (c == 7 || c == 3) { /* remember we had ^G or ^C */
		conf_cancelled = YES;
		result = NO;
	    } else if (TOUPPER(c) == TOUPPER(*msg_yes)) {
		result = YES;
	    } else if (TOUPPER(c) == TOUPPER(*msg_no)) {
		return(NO);
	    } else if (Dft != DFT_CONFIRM) {
		return(Dft);
	    }
	}
    }
    return (result);
}

PUBLIC BOOL HTConfirm ARGS1(CONST char *, Msg)
{
    return HTConfirmDefault(Msg, DFT_CONFIRM);
}

/*	Prompt for answer and get text back.		HTPrompt()
**	------------------------------------
*/
PUBLIC char * HTPrompt ARGS2(
	CONST char *,	Msg,
	CONST char *,	deflt)
{
    char * rep = NULL;
    char Tmp[200];

    Tmp[0] = '\0';
    Tmp[sizeof(Tmp)-1] = '\0';

    _statusline(Msg);
    if (deflt)
	strncpy(Tmp, deflt, sizeof(Tmp)-1);

    if (!dump_output_immediately)
	LYgetstr(Tmp, VISIBLE, sizeof(Tmp), NORECALL);

    StrAllocCopy(rep, Tmp);

    return rep;
}

/*
**	Prompt for password without echoing the reply.	HTPromptPassword()
**	----------------------------------------------
*/
PUBLIC char * HTPromptPassword ARGS1(
	CONST char *,	Msg)
{
    char *result = NULL;
    char pw[120];

    pw[0] = '\0';

    if (!dump_output_immediately) {
	_statusline(Msg ? Msg : PASSWORD_PROMPT);
	LYgetstr(pw, HIDDEN, sizeof(pw), NORECALL); /* hidden */
	StrAllocCopy(result, pw);
    } else {
	printf("\n%s\n", PASSWORD_REQUIRED);
	StrAllocCopy(result, "");
    }
    return result;
}

/*	Prompt both username and password.	 HTPromptUsernameAndPassword()
**	----------------------------------
**
**  On entry,
**	Msg		is the prompting message.
**	*username and
**	*password	are char pointers which contain default
**			or zero-length strings; they are changed
**			to point to result strings.
**	IsProxy 	should be TRUE if this is for
**			proxy authentication.
**
**			If *username is not NULL, it is taken
**			to point to a default value.
**			Initial value of *password is
**			completely discarded.
**
**  On exit,
**	*username and *password point to newly allocated
**	strings -- original strings pointed to by them
**	are NOT freed.
**
*/
PUBLIC void HTPromptUsernameAndPassword ARGS4(
	CONST char *,	Msg,
	char **,	username,
	char **,	password,
	BOOL,		IsProxy)
{
    if ((IsProxy == FALSE &&
	 authentication_info[0] && authentication_info[1]) ||
	(IsProxy == TRUE &&
	 proxyauth_info[0] && proxyauth_info[1])) {
	/*
	**  The -auth or -pauth parameter gave us both the username
	**  and password to use for the first realm or proxy server,
	**  respectively, so just use them without any prompting. - FM
	*/
	StrAllocCopy(*username, (IsProxy ?
		       proxyauth_info[0] : authentication_info[0]));
	if (IsProxy) {
	    FREE(proxyauth_info[0]);
	} else {
	    FREE(authentication_info[0]);
	}
	StrAllocCopy(*password, (IsProxy ?
		       proxyauth_info[1] : authentication_info[1]));
	if (IsProxy) {
	    FREE(proxyauth_info[1]);
	} else {
	    FREE(authentication_info[1]);
	}
    } else if (dump_output_immediately) {
	/*
	 *  We are not interactive and don't have both the
	 *  username and password from the command line,
	 *  but might have one or the other. - FM
	 */
	if ((IsProxy == FALSE && authentication_info[0]) ||
	    (IsProxy == TRUE && proxyauth_info[0])) {
	    /*
	    **	Use the command line username. - FM
	    */
	    StrAllocCopy(*username, (IsProxy ?
			   proxyauth_info[0] : authentication_info[0]));
	    if (IsProxy) {
		FREE(proxyauth_info[0]);
	    } else {
		FREE(authentication_info[0]);
	    }
	} else {
	    /*
	    **	Default to "WWWuser". - FM
	    */
	    StrAllocCopy(*username, "WWWuser");
	}
	if ((IsProxy == FALSE && authentication_info[1]) ||
	    (IsProxy == TRUE && proxyauth_info[1])) {
	    /*
	    **	Use the command line password. - FM
	    */
	    StrAllocCopy(*password, (IsProxy ?
			   proxyauth_info[1] : authentication_info[1]));
	    if (IsProxy) {
		FREE(proxyauth_info[1]);
	    } else {
		FREE(authentication_info[1]);
	    }
	} else {
	    /*
	    **	Default to a zero-length string. - FM
	    */
	    StrAllocCopy(*password, "");
	}
	printf("\n%s\n", USERNAME_PASSWORD_REQUIRED);

    } else {
	/*
	 *  We are interactive and don't have both the
	 *  username and password from the command line,
	 *  but might have one or the other. - FM
	 */
	if ((IsProxy == FALSE && authentication_info[0]) ||
	    (IsProxy == TRUE && proxyauth_info[0])) {
	    /*
	    **	Offer the command line username in the
	    **	prompt for the first realm. - FM
	    */
	    StrAllocCopy(*username, (IsProxy ?
			   proxyauth_info[0] : authentication_info[0]));
	    if (IsProxy) {
		FREE(proxyauth_info[0]);
	    } else {
		FREE(authentication_info[0]);
	    }
	}
	/*
	 *  Prompt for confirmation or entry of the username. - FM
	 */
	if (Msg != NULL) {
	    *username = HTPrompt(Msg, *username);
	} else {
	    *username = HTPrompt(USERNAME_PROMPT, *username);
	}
	if ((IsProxy == FALSE && authentication_info[1]) ||
	    (IsProxy == TRUE && proxyauth_info[1])) {
	    /*
	    **	Use the command line password for the first realm. - FM
	    */
	    StrAllocCopy(*password, (IsProxy ?
			   proxyauth_info[1] : authentication_info[1]));
	    if (IsProxy) {
		FREE(proxyauth_info[1]);
	    } else {
		FREE(authentication_info[1]);
	    }
	} else if (*username != NULL && *username[0] != '\0') {
	    /*
	    **	We have a non-zero length username,
	    **	so prompt for the password. - FM
	    */
	    *password = HTPromptPassword(PASSWORD_PROMPT);
	} else {
	    /*
	    **	Return a zero-length password. - FM
	    */
	    StrAllocCopy(*password, "");
	}
    }
}

/*	Confirm a cookie operation.			HTConfirmCookie()
**	---------------------------
**
**  On entry,
**	server			is the server sending the Set-Cookie.
**	domain			is the domain of the cookie.
**	path			is the path of the cookie.
**	name			is the name of the cookie.
**	value			is the value of the cookie.
**
**  On exit,
**	Returns FALSE on cancel,
**		TRUE if the cookie should be set.
*/
PUBLIC BOOL HTConfirmCookie ARGS4(
	void *, 	dp,
	CONST char *,	server,
	CONST char *,	name,
	CONST char *,	value)
{
    domain_entry *de;
    int ch, namelen, valuelen, space_free;

    if ((de = (domain_entry *)dp) == NULL)
	return FALSE;

#ifdef ENHANCED_COOKIES
    /*	If the user has specified a list of domains to allow or deny
    **	from the config file, then they'll already have de->bv set to
    **	ACCEPT_ALWAYS or REJECT_ALWAYS so we can relax and let the
    **	default cookie handling code cope with this fine.  I hope.
    */
#endif
    /*
    **	If the user has specified a constant action, don't prompt at all.
    */
    if (de->bv == ACCEPT_ALWAYS || de->bv == FROM_FILE)
	return TRUE;
    if (de->bv == REJECT_ALWAYS)
	return FALSE;

    if (dump_output_immediately) {
	/*
	**  Non-interactive, can't respond.  Use the LYSetCookies value
	*   based on its compilation or configuration setting, or on the
	**  command line toggle. - FM
	*/
	return LYSetCookies;
    }

    /*
    **	Estimate how much of the cookie we can show.
    */
    if (de != NULL) {
	if (de->bv == ACCEPT_ALWAYS)
	    return TRUE;
	if (de->bv == REJECT_ALWAYS)
	    return FALSE;
    }
    space_free = (((LYcols - 1)
	       - strlen(ADVANCED_COOKIE_CONFIRMATION))
	       - strlen(server));
    if (space_free < 0)
	space_free = 0;
    namelen = strlen(name);
    valuelen = strlen(value);
    if ((namelen + valuelen) > space_free) {
	/*
	**  Argh... there isn't enough space on our single line for
	**  the whole cookie.  Reduce them both by a percentage.
	**  This should be smarter.
	*/
        int percentage;  /* no float */
        percentage = (100 * space_free) / (namelen + valuelen);
        namelen = (percentage * namelen) / 100;
        valuelen = (percentage * valuelen) / 100;
    }
    if(!LYAcceptAllCookies) {
	char *message = 0;
	HTSprintf(&message, ADVANCED_COOKIE_CONFIRMATION,
		 server, namelen, name, valuelen, value);
	_statusline(message);
	FREE(message);
    }
    while (1) {
	if(!LYAcceptAllCookies) {
	    ch = LYgetch();
	} else {
	    ch = 'A';
	}
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    ch = 'N';
	}
#endif /* VMS */
	switch(TOUPPER(ch)) {
	    case 'A':
		/*
		**  Set to accept all cookies for this domain.
		*/
		de->bv = ACCEPT_ALWAYS;
		HTUserMsg2(ALWAYS_ALLOWING_COOKIES, de->domain);
		return TRUE;

	    case 'N':
	    case 7:	/* Ctrl-G */
	    case 3:	/* Ctrl-C */
		/*
		**  Reject the cookie.
		*/
		HTUserMsg(REJECTING_COOKIE);
		return FALSE;

	    case 'V':
		/*
		**  Set to reject all cookies from this domain.
		*/
		de->bv = REJECT_ALWAYS;
		HTUserMsg2(NEVER_ALLOWING_COOKIES, de->domain);
		return FALSE;

	    case 'Y':
		/*
		**  Accept the cookie.
		*/
		HTInfoMsg(ALLOWING_COOKIE);
		return TRUE;

	    default:
		continue;
	}
    }
}

/*	Confirm redirection of POST.		HTConfirmPostRedirect()
**	----------------------------
**
**  On entry,
**	Redirecting_url 	    is the Location.
**	server_status		    is the server status code.
**
**  On exit,
**	Returns 0 on cancel,
**	  1 for redirect of POST with content,
**	303 for redirect as GET without content
*/
PUBLIC int HTConfirmPostRedirect ARGS2(
	CONST char *,	Redirecting_url,
	int,		server_status)
{
    int result = -1;
    char *show_POST_url = NULL;
    char *StatusInfo = 0;
    char *url = 0;
    int on_screen = 0;	/* 0 - show menu
			 * 1 - show url
			 * 2 - menu is already on screen */

    if (server_status == 303 ||
	server_status == 302) {
	/*
	 *  HTTP.c should not have called us for either of
	 *  these because we're treating 302 as historical,
	 *  so just return 303. - FM
	 */
	return 303;
    }

    if (dump_output_immediately) {
	if (server_status == 301) {
	    /*
	    **	Treat 301 as historical, i.e., like 303 (GET
	    **	without content), when not interactive. - FM
	    */
	    return 303;
	} else {
	    /*
	    **	Treat anything else (e.g., 305, 306 or 307) as too
	    **	dangerous to redirect without confirmation, and thus
	    **	cancel when not interactive. - FM
	    */
	    return 0;
	}
    }

    if (user_mode == NOVICE_MODE) {
	on_screen = 2;
	move(LYlines-2, 0);
	HTSprintf0(&StatusInfo, SERVER_ASKED_FOR_REDIRECTION, server_status);
	addstr(StatusInfo);
	clrtoeol();
	move(LYlines-1, 0);
	HTSprintf0(&url, "URL: %.*s",
		    (LYcols < 250 ? LYcols-6 : 250), Redirecting_url);
	addstr(url);
	clrtoeol();
	if (server_status == 301) {
	    _statusline(PROCEED_GET_CANCEL);
	} else {
	    _statusline(PROCEED_OR_CANCEL);
	}
    } else {
	HTSprintf0(&StatusInfo, "%d %.*s",
			    server_status,
			    251,
			    ((server_status == 301) ?
			 ADVANCED_POST_GET_REDIRECT :
			 ADVANCED_POST_REDIRECT));
	StrAllocCopy(show_POST_url, LOCATION_HEADER);
	StrAllocCat(show_POST_url, Redirecting_url);
    }
    while (result < 0) {
	int c;

	switch (on_screen) {
	    case 0:
		_statusline(StatusInfo);
		break;
	    case 1:
		_statusline(show_POST_url);
	}
	c = LYgetch();
	switch (TOUPPER(c)) {
	    case 'P':
		/*
		**  Proceed with 301 or 307 redirect of POST
		**  with same method and POST content. - FM
		*/
		FREE(show_POST_url);
		result = 1;
		break;

	    case 7:
	    case 'C':
		/*
		**  Cancel request.
		*/
		FREE(show_POST_url);
		result = 0;
		break;

	    case 'U':
		/*
		**  Show URL for intermediate or advanced mode.
		*/
		if (user_mode != NOVICE_MODE) {
		    if (on_screen == 1) {
			on_screen = 0;
		    } else {
			on_screen = 1;
		    }
		}
		break;

	    case 'G':
		if (server_status == 301) {
		    /*
		    **	Treat as 303 (GET without content).
		    */
		    FREE(show_POST_url);
		    result = 303;
		    break;
		}
		/* fall through to default */

	    default:
		/*
		**  Get another character.
		*/
		if (on_screen == 1) {
		    on_screen = 0;
		} else {
		    on_screen = 2;
		}
	}
    }
    FREE(StatusInfo);
    FREE(url);
    return (result);
}
