/*	Displaying messages and getting input for Lynx Browser
 *	==========================================================
 *
 *	REPLACE THIS MODULE with a GUI version in a GUI environment!
 *
 * History:
 *	   Jun 92 Created May 1992 By C.T. Barker
 *	   Feb 93 Simplified, portablised TBL
 *
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
#include <LYHistory.h>		/* store statusline messages */

#include <LYLeaks.h>

#include <HTParse.h>

#undef timezone			/* U/Win defines this in time.h, hides implementation detail */

#if defined(HAVE_FTIME) && defined(HAVE_SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

/*
 * 'napms()' is preferable to 'sleep()' in any case because it does not
 * interfere with output, but also because it can be less than a second.
 */
#ifdef HAVE_NAPMS
#define LYSleep(n) napms(n)
#else
#define LYSleep(n) sleep(n)
#endif

/*	Issue a message about a problem.		HTAlert()
 *	--------------------------------
 */
void HTAlert(const char *Msg)
{
    CTRACE((tfp, "\nAlert!: %s\n\n", Msg));
    CTRACE_FLUSH(tfp);
    _user_message(ALERT_FORMAT, Msg);
    LYstore_message2(ALERT_FORMAT, Msg);

    if (dump_output_immediately && dump_to_stderr) {
	fflush(stdout);
	fprintf(stderr, ALERT_FORMAT, Msg);
	fputc('\n', stderr);
	fflush(stderr);
    }

    LYSleepAlert();
}

void HTAlwaysAlert(const char *extra_prefix,
		   const char *Msg)
{
    if (!dump_output_immediately && LYCursesON) {
	HTAlert(Msg);
    } else {
	if (extra_prefix) {
	    fprintf(((TRACE) ? stdout : stderr),
		    "%s %s!\n",
		    extra_prefix, Msg);
	    fflush(stdout);
	    LYstore_message2(ALERT_FORMAT, Msg);
	    LYSleepAlert();
	} else {
	    fprintf(((TRACE) ? stdout : stderr), ALERT_FORMAT, NonNull(Msg));
	    fflush(stdout);
	    LYstore_message2(ALERT_FORMAT, Msg);
	    LYSleepAlert();
	    fprintf(((TRACE) ? stdout : stderr), "\n");
	}
	CTRACE((tfp, "\nAlert!: %s\n\n", Msg));
	CTRACE_FLUSH(tfp);
    }
}

/*	Issue an informational message.			HTInfoMsg()
 *	--------------------------------
 */
void HTInfoMsg(const char *Msg)
{
    _statusline(Msg);
    if (non_empty(Msg)) {
	CTRACE((tfp, "Info message: %s\n", Msg));
	LYstore_message(Msg);
	LYSleepInfo();
    }
}

/*	Issue an important message.			HTUserMsg()
 *	--------------------------------
 */
void HTUserMsg(const char *Msg)
{
    _statusline(Msg);
    if (non_empty(Msg)) {
	CTRACE((tfp, "User message: %s\n", Msg));
	LYstore_message(Msg);
#if !(defined(USE_SLANG) || defined(WIDEC_CURSES))
	if (HTCJK != NOCJK) {
	    clearok(curscr, TRUE);
	    LYrefresh();
	}
#endif
	LYSleepMsg();
    }
}

void HTUserMsg2(const char *Msg2, const char *Arg)
{
    _user_message(Msg2, Arg);
    if (non_empty(Msg2)) {
	CTRACE((tfp, "User message: "));
	CTRACE((tfp, Msg2, Arg));
	CTRACE((tfp, "\n"));
	LYstore_message2(Msg2, Arg);
	LYSleepMsg();
    }
}

/*	Issue a progress message.			HTProgress()
 *	-------------------------
 */
void HTProgress(const char *Msg)
{
    statusline(Msg);
    LYstore_message(Msg);
    CTRACE((tfp, "%s\n", Msg));
    LYSleepDebug();
}

const char *HTProgressUnits(int rate)
{
    static const char *bunits = 0;
    static const char *kbunits = 0;

    if (!bunits) {
	bunits = gettext("bytes");
	kbunits = gettext(LYTransferName);
    }
    return ((rate == rateKB)
#ifdef USE_READPROGRESS
	    || (rate == rateEtaKB)
#endif
	)? kbunits : bunits;
}

static const char *sprint_bytes(char *s, long n, const char *was_units)
{
    static long kb_units = 1024;
    const char *u = HTProgressUnits(LYTransferRate);

    if (LYTransferRate == rateKB || LYTransferRate == rateEtaKB_maybe) {
	if (n >= 10 * kb_units) {
	    sprintf(s, "%ld", n / kb_units);
	} else if (n > 999) {	/* Avoid switching between 1016b/s and 1K/s */
	    sprintf(s, "%.2g", ((double) n) / kb_units);
	} else {
	    sprintf(s, "%ld", n);
	    u = HTProgressUnits(rateBYTES);
	}
    } else {
	sprintf(s, "%ld", n);
    }

    if (!was_units || was_units != u)
	sprintf(s + strlen(s), " %s", u);
    return u;
}

#ifdef USE_READPROGRESS
#define TIME_HMS_LENGTH (16)
static char *sprint_tbuf(char *s, long t)
{
    if (t > 3600)
	sprintf(s, "%ldh%ldm%lds", t / 3600, (t / 60) % 60, t % 60);
    else if (t > 60)
	sprintf(s, "%ldm%lds", t / 60, t % 60);
    else
	sprintf(s, "%ld sec", t);
    return s;
}
#endif /* USE_READPROGRESS */

/*	Issue a read-progress message.			HTReadProgress()
 *	------------------------------
 */
void HTReadProgress(long bytes, long total)
{
    static long bytes_last, total_last;
    static long transfer_rate = 0;
    static char *line = NULL;
    char bytesp[80], totalp[80], transferp[80];
    int renew = 0;
    const char *was_units;

#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    double now;
    static double first, last, last_active;

    gettimeofday(&tv, (struct timezone *) 0);
    now = tv.tv_sec + tv.tv_usec / 1000000.;
#else
#if defined(HAVE_FTIME) && defined(HAVE_SYS_TIMEB_H)
    static double now, first, last, last_active;
    struct timeb tb;

    ftime(&tb);
    now = tb.time + (double) tb.millitm / 1000;
#else
    time_t now = time((time_t *) 0);	/* once per second */
    static time_t first, last, last_active;
#endif
#endif

    if (!LYShowTransferRate)
	LYTransferRate = rateOFF;

    if (bytes == 0) {
	first = last = last_active = now;
	bytes_last = bytes;
    } else if (bytes < 0) {	/* stalled */
	bytes = bytes_last;
	total = total_last;
    }
    if ((bytes > 0) &&
	(now != first))
	/* 1 sec delay for transfer_rate calculation without g-t-o-d */  {
	if (transfer_rate <= 0)	/* the very first time */
	    transfer_rate = (long) ((bytes) / (now - first));	/* bytes/sec */
	total_last = total;

	/*
	 * Optimal refresh time:  every 0.2 sec
	 */
#if defined(HAVE_GETTIMEOFDAY) || (defined(HAVE_FTIME) && defined(HAVE_SYS_TIMEB_H))
	if (now >= last + 0.2)
	    renew = 1;
#else
	/*
	 * Use interpolation.  (The transfer rate may be not constant
	 * when we have partial content in a proxy.  We adjust transfer_rate
	 * once a second to minimize interpolation error below.)
	 */
	if ((now != last) || ((bytes - bytes_last) > (transfer_rate / 5))) {
	    renew = 1;
	    bytes_last += (transfer_rate / 5);	/* until we got next second */
	}
#endif
	if (renew) {
	    if (now != last) {
		last = now;
		if (bytes_last != bytes)
		    last_active = now;
		bytes_last = bytes;
		transfer_rate = (long) (bytes / (now - first));		/* more accurate value */
	    }

	    if (total > 0)
		was_units = sprint_bytes(totalp, total, 0);
	    else
		was_units = 0;
	    sprint_bytes(bytesp, bytes, was_units);

	    if (total > 0)
		HTSprintf0(&line, gettext("Read %s of %s of data"), bytesp, totalp);
	    else
		HTSprintf0(&line, gettext("Read %s of data"), bytesp);

	    if (LYTransferRate != rateOFF
		&& transfer_rate > 0) {
		sprint_bytes(transferp, transfer_rate, 0);
		HTSprintf(&line, gettext(", %s/sec"), transferp);
	    }
#ifdef USE_READPROGRESS
	    if (LYTransferRate == rateEtaBYTES
		|| LYTransferRate == rateEtaKB) {
		char tbuf[TIME_HMS_LENGTH];

		if (now - last_active >= 5)
		    HTSprintf(&line,
			      gettext(" (stalled for %s)"),
			      sprint_tbuf(tbuf, (long) (now - last_active)));
		if (total > 0 && transfer_rate)
		    HTSprintf(&line,
			      gettext(", ETA %s"),
			      sprint_tbuf(tbuf, (long) ((total - bytes) / transfer_rate)));
	    }
#endif

	    StrAllocCat(line, ".");
	    if (total < -1)
		StrAllocCat(line, gettext(" (Press 'z' to abort)"));

	    /* do not store the message for history page. */
	    statusline(line);
	    CTRACE((tfp, "%s\n", line));
	}
	}
#ifdef LY_FIND_LEAKS
    FREE(line);
#endif
}

static BOOL conf_cancelled = NO;	/* used by HTConfirm only - kw */

BOOL HTLastConfirmCancelled(void)
{
    if (conf_cancelled) {
	conf_cancelled = NO;	/* reset */
	return (YES);
    } else {
	return (NO);
    }
}

/*
 * Prompt for yes/no response, but let a configuration variable override
 * the prompt entirely.
 */
int HTForcedPrompt(int option, const char *msg, int dft)
{
    int result = FALSE;
    const char *show = NULL;
    char *msg2 = NULL;

    if (option == FORCE_PROMPT_DFT) {
	result = HTConfirmDefault(msg, dft);
    } else {
	if (option == FORCE_PROMPT_YES) {
	    show = gettext("yes");
	    result = YES;
	} else if (option == FORCE_PROMPT_NO) {
	    show = gettext("no");
	    result = NO;
	} else {
	    return HTConfirmDefault(msg, dft);	/* bug... */
	}
	HTSprintf(&msg2, "%s %s", msg, show);
	HTUserMsg(msg2);
	free(msg2);
    }
    return result;
}

#define DFT_CONFIRM ~(YES|NO)

/*	Seek confirmation with default answer.		HTConfirmDefault()
 *	--------------------------------------
 */
int HTConfirmDefault(const char *Msg, int Dft)
{
/* Meta-note: don't move the following note from its place right
   in front of the first gettext().  As it is now, it should
   automatically appear in generated lynx.pot files. - kw
 */

/* NOTE TO TRANSLATORS:  If you provide a translation for "yes", lynx
 * will take the first byte of the translation as a positive response
 * to Yes/No questions.  If you provide a translation for "no", lynx
 * will take the first byte of the translation as a negative response
 * to Yes/No questions.  For both, lynx will also try to show the
 * first byte in the prompt as a character, instead of (y) or (n),
 * respectively.  This will not work right for multibyte charsets!
 * Don't translate "yes" and "no" for CJK character sets (or translate
 * them to "yes" and "no").  For a translation using UTF-8, don't
 * translate if the translation would begin with anything but a 7-bit
 * (US_ASCII) character.  That also means do not translate if the
 * translation would begin with anything but a 7-bit character, if
 * you use a single-byte character encoding (a charset like ISO-8859-n)
 * but anticipate that the message catalog may be used re-encoded in
 * UTF-8 form.
 * For translations using other character sets, you may also wish to
 * leave "yes" and "no" untranslated, if using (y) and (n) is the
 * preferred behavior.
 * Lynx will also accept y Y n N as responses unless there is a conflict
 * with the first letter of the "yes" or "no" translation.
 */
    const char *msg_yes = gettext("yes");
    const char *msg_no = gettext("no");
    int result = -1;

    /* If they're not really distinct in the first letter, revert to English */
    if (TOUPPER(*msg_yes) == TOUPPER(*msg_no)) {
	msg_yes = "yes";
	msg_no = "no";
    }

    conf_cancelled = NO;
    if (dump_output_immediately) {	/* Non-interactive, can't respond */
	if (Dft == DFT_CONFIRM) {
	    CTRACE((tfp, "Confirm: %s (%c/%c) ", Msg, *msg_yes, *msg_no));
	} else {
	    CTRACE((tfp, "Confirm: %s (%c) ", Msg, (Dft == YES) ? *msg_yes : *msg_no));
	}
	CTRACE((tfp, "- NO, not interactive.\n"));
	result = NO;
    } else {
	char *msg = NULL;
	char fallback_y = 'y';	/* English letter response as fallback */
	char fallback_n = 'n';	/* English letter response as fallback */

	if (fallback_y == *msg_yes || fallback_y == *msg_no)
	    fallback_y = '\0';	/* conflict or duplication, don't use */
	if (fallback_n == *msg_yes || fallback_n == *msg_no)
	    fallback_n = '\0';	/* conflict or duplication, don't use */

	if (Dft == DFT_CONFIRM)
	    HTSprintf0(&msg, "%s (%c/%c) ", Msg, *msg_yes, *msg_no);
	else
	    HTSprintf0(&msg, "%s (%c) ", Msg, (Dft == YES) ? *msg_yes : *msg_no);
	if (LYTraceLogFP) {
	    CTRACE((tfp, "Confirm: %s", msg));
	}
	_statusline(msg);
	FREE(msg);

	while (result < 0) {
	    int c = LYgetch_single();

#ifdef VMS
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		c = TOUPPER(*msg_no);
	    }
#endif /* VMS */
	    if (c == TOUPPER(*msg_yes)) {
		result = YES;
	    } else if (c == TOUPPER(*msg_no)) {
		result = NO;
	    } else if (fallback_y && c == fallback_y) {
		result = YES;
	    } else if (fallback_n && c == fallback_n) {
		result = NO;
	    } else if (LYCharIsINTERRUPT(c)) {	/* remember we had ^G or ^C */
		conf_cancelled = YES;
		result = NO;
	    } else if (Dft != DFT_CONFIRM) {
		result = Dft;
		break;
	    }
	}
	CTRACE((tfp, "- %s%s.\n",
		(result != NO) ? "YES" : "NO",
		conf_cancelled ? ", cancelled" : ""));
    }
    return (result);
}

/*	Seek confirmation.				HTConfirm()
 *	------------------
 */
BOOL HTConfirm(const char *Msg)
{
    return (BOOL) HTConfirmDefault(Msg, DFT_CONFIRM);
}

/*
 * Ask a post resubmission prompt with some indication of what would
 * be resubmitted, useful especially for going backward in history.
 * Try to use parts of the address or, if given, the title, depending
 * on how much fits on the statusline.
 * if_imgmap and if_file indicate how to handle an address that is
 * a "LYNXIMGMAP:", or a "file:" URL (presumably the List Page file),
 * respectively:  0:  auto-deny, 1:  auto-confirm, 2:  prompt.
 * - kw
 */

BOOL confirm_post_resub(const char *address,
			const char *title,
			int if_imgmap,
			int if_file)
{
    size_t len1;
    const char *msg = CONFIRM_POST_RESUBMISSION_TO;
    char buf[240];
    char *temp = NULL;
    BOOL res;
    size_t maxlen = LYcolLimit - 5;

    if (!address) {
	return (NO);
    } else if (isLYNXIMGMAP(address)) {
	if (if_imgmap <= 0)
	    return (NO);
	else if (if_imgmap == 1)
	    return (YES);
	else
	    msg = CONFIRM_POST_LIST_RELOAD;
    } else if (isFILE_URL(address)) {
	if (if_file <= 0)
	    return (NO);
	else if (if_file == 1)
	    return (YES);
	else
	    msg = CONFIRM_POST_LIST_RELOAD;
    } else if (dump_output_immediately) {
	return (NO);
    }
    if (maxlen >= sizeof(buf))
	maxlen = sizeof(buf) - 1;
    if ((len1 = strlen(msg)) +
	strlen(address) <= maxlen) {
	sprintf(buf, msg, address);
	return HTConfirm(buf);
    }
    if (len1 + strlen(temp = HTParse(address, "",
				     PARSE_ACCESS + PARSE_HOST + PARSE_PATH
				     + PARSE_PUNCTUATION)) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return (res);
    }
    FREE(temp);
    if (title && (len1 + strlen(title) <= maxlen)) {
	sprintf(buf, msg, title);
	return HTConfirm(buf);
    }
    if (len1 + strlen(temp = HTParse(address, "",
				     PARSE_ACCESS + PARSE_HOST
				     + PARSE_PUNCTUATION)) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return (res);
    }
    FREE(temp);
    if ((temp = HTParse(address, "", PARSE_HOST)) && *temp &&
	len1 + strlen(temp) <= maxlen) {
	sprintf(buf, msg, temp);
	res = HTConfirm(buf);
	FREE(temp);
	return (res);
    }
    FREE(temp);
    return HTConfirm(CONFIRM_POST_RESUBMISSION);
}

/*	Prompt for answer and get text back.		HTPrompt()
 *	------------------------------------
 */
char *HTPrompt(const char *Msg, const char *deflt)
{
    char *rep = NULL;
    char Tmp[200];

    Tmp[0] = '\0';
    Tmp[sizeof(Tmp) - 1] = '\0';

    _statusline(Msg);
    if (deflt)
	strncpy(Tmp, deflt, sizeof(Tmp) - 1);

    if (!dump_output_immediately)
	LYgetstr(Tmp, VISIBLE, sizeof(Tmp), NORECALL);

    StrAllocCopy(rep, Tmp);

    return rep;
}

/*
 *	Prompt for password without echoing the reply.	HTPromptPassword()
 *	----------------------------------------------
 */
char *HTPromptPassword(const char *Msg)
{
    char *result = NULL;
    char pw[120];

    pw[0] = '\0';

    if (!dump_output_immediately) {
	_statusline(Msg ? Msg : PASSWORD_PROMPT);
	LYgetstr(pw, HIDDEN, sizeof(pw), NORECALL);	/* hidden */
	StrAllocCopy(result, pw);
    } else {
	printf("\n%s\n", PASSWORD_REQUIRED);
	StrAllocCopy(result, "");
    }
    return result;
}

/*	Prompt both username and password.	 HTPromptUsernameAndPassword()
 *	----------------------------------
 *
 *  On entry,
 *	Msg		is the prompting message.
 *	*username and
 *	*password	are char pointers which contain default
 *			or zero-length strings; they are changed
 *			to point to result strings.
 *	IsProxy 	should be TRUE if this is for
 *			proxy authentication.
 *
 *			If *username is not NULL, it is taken
 *			to point to a default value.
 *			Initial value of *password is
 *			completely discarded.
 *
 *  On exit,
 *	*username and *password point to newly allocated
 *	strings -- original strings pointed to by them
 *	are NOT freed.
 *
 */
void HTPromptUsernameAndPassword(const char *Msg,
				 char **username,
				 char **password,
				 BOOL IsProxy)
{
    if ((IsProxy == FALSE &&
	 authentication_info[0] && authentication_info[1]) ||
	(IsProxy == TRUE &&
	 proxyauth_info[0] && proxyauth_info[1])) {
	/*
	 * The -auth or -pauth parameter gave us both the username
	 * and password to use for the first realm or proxy server,
	 * respectively, so just use them without any prompting.  - FM
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
	 * We are not interactive and don't have both the
	 * username and password from the command line,
	 * but might have one or the other.  - FM
	 */
	if ((IsProxy == FALSE && authentication_info[0]) ||
	    (IsProxy == TRUE && proxyauth_info[0])) {
	    /*
	     * Use the command line username.  - FM
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
	     * Default to "WWWuser".  - FM
	     */
	    StrAllocCopy(*username, "WWWuser");
	}
	if ((IsProxy == FALSE && authentication_info[1]) ||
	    (IsProxy == TRUE && proxyauth_info[1])) {
	    /*
	     * Use the command line password.  - FM
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
	     * Default to a zero-length string.  - FM
	     */
	    StrAllocCopy(*password, "");
	}
	printf("\n%s\n", USERNAME_PASSWORD_REQUIRED);

    } else {
	/*
	 * We are interactive and don't have both the
	 * username and password from the command line,
	 * but might have one or the other.  - FM
	 */
	if ((IsProxy == FALSE && authentication_info[0]) ||
	    (IsProxy == TRUE && proxyauth_info[0])) {
	    /*
	     * Offer the command line username in the
	     * prompt for the first realm.  - FM
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
	 * Prompt for confirmation or entry of the username.  - FM
	 */
	if (Msg != NULL) {
	    *username = HTPrompt(Msg, *username);
	} else {
	    *username = HTPrompt(USERNAME_PROMPT, *username);
	}
	if ((IsProxy == FALSE && authentication_info[1]) ||
	    (IsProxy == TRUE && proxyauth_info[1])) {
	    /*
	     * Use the command line password for the first realm.  - FM
	     */
	    StrAllocCopy(*password, (IsProxy ?
				     proxyauth_info[1] : authentication_info[1]));
	    if (IsProxy) {
		FREE(proxyauth_info[1]);
	    } else {
		FREE(authentication_info[1]);
	    }
	} else if (non_empty(*username)) {
	    /*
	     * We have a non-zero length username,
	     * so prompt for the password.  - FM
	     */
	    *password = HTPromptPassword(PASSWORD_PROMPT);
	} else {
	    /*
	     * Return a zero-length password.  - FM
	     */
	    StrAllocCopy(*password, "");
	}
    }
}

/*	Confirm a cookie operation.			HTConfirmCookie()
 *	---------------------------
 *
 *  On entry,
 *	server			is the server sending the Set-Cookie.
 *	domain			is the domain of the cookie.
 *	path			is the path of the cookie.
 *	name			is the name of the cookie.
 *	value			is the value of the cookie.
 *
 *  On exit,
 *	Returns FALSE on cancel,
 *		TRUE if the cookie should be set.
 */
BOOL HTConfirmCookie(domain_entry * de, const char *server,
		     const char *name,
		     const char *value)
{
    int ch;
    const char *prompt = ADVANCED_COOKIE_CONFIRMATION;

    if (de == NULL)
	return FALSE;

    /* If the user has specified a list of domains to allow or deny
     * from the config file, then they'll already have de->bv set to
     * ACCEPT_ALWAYS or REJECT_ALWAYS so we can relax and let the
     * default cookie handling code cope with this fine.
     */

    /*
     * If the user has specified a constant action, don't prompt at all.
     */
    if (de->bv == ACCEPT_ALWAYS)
	return TRUE;
    if (de->bv == REJECT_ALWAYS)
	return FALSE;

    if (dump_output_immediately) {
	/*
	 * Non-interactive, can't respond.  Use the LYSetCookies value
	 * based on its compilation or configuration setting, or on the
	 * command line toggle.  - FM
	 */
	return LYSetCookies;
    }

    /*
     * Estimate how much of the cookie we can show.
     */
    if (!LYAcceptAllCookies) {
	int namelen, valuelen, space_free, percentage;
	char *message = 0;

	space_free = (LYcolLimit
		      - (LYstrCells(prompt)
			 - 10)	/* %s and %.*s and %.*s chars */
		      -strlen(server));
	if (space_free < 0)
	    space_free = 0;
	namelen = strlen(name);
	valuelen = strlen(value);
	if ((namelen + valuelen) > space_free) {
	    /*
	     * Argh...  there isn't enough space on our single line for
	     * the whole cookie.  Reduce them both by a percentage.
	     * This should be smarter.
	     */
	    percentage = (100 * space_free) / (namelen + valuelen);
	    namelen = (percentage * namelen) / 100;
	    valuelen = (percentage * valuelen) / 100;
	}
	HTSprintf(&message, prompt, server, namelen, name, valuelen, value);
	_statusline(message);
	FREE(message);
    }
    for (;;) {
	if (LYAcceptAllCookies) {
	    ch = 'A';
	} else {
	    ch = LYgetch_single();
#if defined(LOCALE) && defined(HAVE_GETTEXT)
	    {
#define L_PAREN '('
#define R_PAREN ')'
		/*
		 * Special-purpose workaround for gettext support (we should do
		 * this in a more general way) -TD
		 *
		 * NOTE TO TRANSLATORS:  If the prompt has been rendered into
		 * another language, and if yes/no are distinct, assume the
		 * translator can make an ordered list in parentheses with one
		 * capital letter for each as we assumed in HTConfirmDefault().
		 * The list has to be in the same order as in the original message,
		 * and the four capital letters chosen to not match those in the
		 * original unless they have the same position.
		 *
		 * Example:
		 * (Y/N/Always/neVer)              - English (original)
		 * (O/N/Toujours/Jamais)           - French
		 */
		char *p = gettext("Y/N/A/V");	/* placeholder for comment */
		char *s = "YNAV\007\003";	/* see ADVANCED_COOKIE_CONFIRMATION */

		if (strchr(s, ch) == 0
		    && isalpha(ch)
		    && (p = strrchr(prompt, L_PAREN)) != 0) {

		    CTRACE((tfp, "Looking for %c in %s\n", ch, p));
		    while (*p != R_PAREN && *p != 0 && isalpha(UCH(*s))) {
			if (isalpha(UCH(*p)) && (*p == TOUPPER(*p))) {
			    CTRACE((tfp, "...testing %c/%c\n", *p, *s));
			    if (*p == ch) {
				ch = *s;
				break;
			    }
			    ++s;
			}
			++p;
		    }
		}
	    }
#endif
	}
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    ch = 'N';
	}
#endif /* VMS */
	switch (ch) {
	case 'A':
	    /*
	     * Set to accept all cookies for this domain.
	     */
	    de->bv = ACCEPT_ALWAYS;
	    HTUserMsg2(ALWAYS_ALLOWING_COOKIES, de->domain);
	    return TRUE;

	case 'N':
	    /*
	     * Reject the cookie.
	     */
	  reject:
	    HTUserMsg(REJECTING_COOKIE);
	    return FALSE;

	case 'V':
	    /*
	     * Set to reject all cookies from this domain.
	     */
	    de->bv = REJECT_ALWAYS;
	    HTUserMsg2(NEVER_ALLOWING_COOKIES, de->domain);
	    return FALSE;

	case 'Y':
	    /*
	     * Accept the cookie.
	     */
	    HTInfoMsg(ALLOWING_COOKIE);
	    return TRUE;

	default:
	    if (LYCharIsINTERRUPT(ch))
		goto reject;
	    continue;
	}
    }
}

/*	Confirm redirection of POST.		HTConfirmPostRedirect()
 *	----------------------------
 *
 *  On entry,
 *	Redirecting_url 	    is the Location.
 *	server_status		    is the server status code.
 *
 *  On exit,
 *	Returns 0 on cancel,
 *	  1 for redirect of POST with content,
 *	303 for redirect as GET without content
 */
int HTConfirmPostRedirect(const char *Redirecting_url, int server_status)
{
    int result = -1;
    char *show_POST_url = NULL;
    char *StatusInfo = 0;
    char *url = 0;
    int on_screen = 0;		/* 0 - show menu

				 * 1 - show url
				 * 2 - menu is already on screen */

    if (server_status == 303 ||
	server_status == 302) {
	/*
	 * HTTP.c should not have called us for either of
	 * these because we're treating 302 as historical,
	 * so just return 303.  - FM
	 */
	return 303;
    }

    if (dump_output_immediately) {
	if (server_status == 301) {
	    /*
	     * Treat 301 as historical, i.e., like 303 (GET
	     * without content), when not interactive.  - FM
	     */
	    return 303;
	} else {
	    /*
	     * Treat anything else (e.g., 305, 306 or 307) as too
	     * dangerous to redirect without confirmation, and thus
	     * cancel when not interactive.  - FM
	     */
	    return 0;
	}
    }

    if (user_mode == NOVICE_MODE) {
	on_screen = 2;
	LYmove(LYlines - 2, 0);
	HTSprintf0(&StatusInfo, SERVER_ASKED_FOR_REDIRECTION, server_status);
	LYaddstr(StatusInfo);
	LYclrtoeol();
	LYmove(LYlines - 1, 0);
	HTSprintf0(&url, "URL: %.*s",
		   (LYcols < 250 ? LYcolLimit - 5 : 250), Redirecting_url);
	LYaddstr(url);
	LYclrtoeol();
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
	c = LYgetch_single();
	switch (c) {
	case 'P':
	    /*
	     * Proceed with 301 or 307 redirect of POST
	     * with same method and POST content.  - FM
	     */
	    FREE(show_POST_url);
	    result = 1;
	    break;

	case 7:
	case 'C':
	    /*
	     * Cancel request.
	     */
	    FREE(show_POST_url);
	    result = 0;
	    break;

	case 'U':
	    /*
	     * Show URL for intermediate or advanced mode.
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
		 * Treat as 303 (GET without content).
		 */
		FREE(show_POST_url);
		result = 303;
		break;
	    }
	    /* fall through to default */

	default:
	    /*
	     * Get another character.
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

#define okToSleep() (!crawl && !traversal && LYCursesON)

/*
 * Sleep for the given message class's time.
 */
void LYSleepAlert(void)
{
    if (okToSleep())
	LYSleep(AlertSecs);
}

void LYSleepDebug(void)
{
    if (okToSleep())
	LYSleep(DebugSecs);
}

void LYSleepInfo(void)
{
    if (okToSleep())
	LYSleep(InfoSecs);
}

void LYSleepMsg(void)
{
    if (okToSleep())
	LYSleep(MessageSecs);
}

#ifdef EXP_CMD_LOGGING
void LYSleepReplay(void)
{
    if (okToSleep())
	LYSleep(ReplaySecs);
}
#endif /* EXP_CMD_LOGGING */

/*
 * LYstrerror emulates the ANSI strerror() function.
 */
#ifndef LYStrerror
char *LYStrerror(int code)
{
    static char temp[80];

    sprintf(temp, "System errno is %d.\r\n", code);
    return temp;
}
#endif /* HAVE_STRERROR */
