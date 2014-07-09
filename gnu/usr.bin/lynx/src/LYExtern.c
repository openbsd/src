/*
 * $LynxId: LYExtern.c,v 1.54 2013/11/29 00:21:20 tom Exp $
 *
 External application support.
 This feature allows lynx to pass a given URL to an external program.
 It was written for three reasons.
 1) To overcome the deficiency	of Lynx_386 not supporting ftp and news.
    External programs can be used instead by passing the URL.

 2) To allow for background transfers in multitasking systems.
    I use wget for http and ftp transfers via the external command.

 3) To allow for new URLs to be used through lynx.
    URLs can be made up such as mymail: to spawn desired applications
    via the external command.

 See lynx.cfg for other info.
*/

#include <LYUtils.h>

#ifdef USE_EXTERNALS

#include <HTAlert.h>
#include <LYGlobalDefs.h>
#include <LYExtern.h>
#include <LYLeaks.h>
#include <LYCurses.h>
#include <LYReadCFG.h>
#include <LYStrings.h>

#ifdef WIN_EX
/* ASCII char -> HEX digit */
#define ASC2HEXD(x) ((UCH(x) >= '0' && UCH(x) <= '9') ?               \
		     (UCH(x) - '0') : (toupper(UCH(x)) - 'A' + 10))

/* Decodes the forms %xy in a URL to the character the hexadecimal
   code of which is xy. xy are hexadecimal digits from
   [0123456789ABCDEF] (case-insensitive). If x or y are not hex-digits
   or '%' is near '\0', the whole sequence is inserted literally. */

static char *decode_string(char *s)
{
    char *save_s;
    char *p = s;

    save_s = s;
    for (; *s; s++, p++) {
	if (*s != '%')
	    *p = *s;
	else {
	    /* Do nothing if at the end of the string. Or if the chars
	       are not hex-digits. */
	    if (!*(s + 1) || !*(s + 2)
		|| !(isxdigit(UCH(*(s + 1))) && isxdigit(UCH(*(s + 2))))) {
		*p = *s;
		continue;
	    }
	    *p = (char) ((ASC2HEXD(*(s + 1)) << 4) + ASC2HEXD(*(s + 2)));
	    s += 2;
	}
    }
    *p = '\0';
    return save_s;
}
#endif /* WIN_EX */

#ifdef WIN_EX
/*
 *  Delete dangerous characters as local path.
 *  We delete '<>|' and also '%"'.
 *  '%' should be deleted because it's difficut to escape for all cases.
 *  So we can't treat paths which include '%'.
 *  '"' should be deleted because it's a obstacle to quote whole path.
 */
static void delete_danger_characters(char *src)
{
    char *dst;

    for (dst = src; *src != '\0'; src++) {
	if (StrChr("<>|%\"", *src) == NULL) {
	    *dst = *src;
	    dst++;
	}
    }
    *dst = '\0';
}

static char *escapeParameter(CONST char *parameter)
{
    size_t i;
    size_t last = strlen(parameter);
    size_t n = 0;
    size_t encoded = 0;
    size_t escaped = 0;
    char *result;
    char *needs_encoded = "<>|";
    char *needs_escaped = "%";
    char *needs_escaped_NT = "%&^";

    for (i = 0; i < last; ++i) {
	if (StrChr(needs_encoded, parameter[i]) != NULL) {
	    ++encoded;
	}
	if (system_is_NT) {
	    if (StrChr(needs_escaped_NT, parameter[i]) != NULL) {
		++escaped;
	    }
	} else if (StrChr(needs_escaped, parameter[i]) != NULL) {
	    ++escaped;
	}
    }

    result = (char *) malloc(last + encoded * 2 + escaped + 1);
    if (result == NULL)
	outofmem(__FILE__, "escapeParameter");

    n = 0;
    for (i = 0; i < last; i++) {
	if (StrChr(needs_encoded, parameter[i]) != NULL) {
	    sprintf(result + n, "%%%02X", (unsigned char) parameter[i]);
	    n += 3;
	    continue;
	}
	if (system_is_NT) {
	    if (StrChr(needs_escaped_NT, parameter[i]) != NULL) {
		result[n++] = '^';
		result[n++] = parameter[i];
		continue;
	    }
	} else if (StrChr(needs_escaped, parameter[i]) != NULL) {
	    result[n++] = '%';	/* parameter[i] is '%' */
	    result[n++] = parameter[i];
	    continue;
	}
	result[n++] = parameter[i];
    }
    result[n] = '\0';

    return result;
}
#endif /* WIN_EX */

static void format(char **result,
		   char *fmt,
		   char *parm)
{
    *result = NULL;
    HTAddParam(result, fmt, 1, parm);
    HTEndParam(result, fmt, 1);
}

/*
 * Format the given command into a buffer, returning the resulting string.
 *
 * It is too dangerous to leave any URL that may come along unquoted.  They
 * often contain '&', ';', and '?' chars, and who knows what else may occur.
 * Prevent spoofing of the shell.  Dunno how this needs to be modified for VMS
 * or DOS.  - kw
 */
static char *format_command(char *command,
			    char *param)
{
    char *cmdbuf = NULL;

#if defined(WIN_EX)
    char pram_string[LY_MAXPATH];
    char *escaped = NULL;

    if (strncasecomp("file://localhost/", param, 17) == 0) {
	/* decode local path parameter for programs to be
	   able to interpret - TH */
	LYStrNCpy(pram_string, param, sizeof(pram_string) - 1);
	decode_string(pram_string);
	param = pram_string;
    } else {
	/* encode or escape URL parameter - TH */
	escaped = escapeParameter(param);
	param = escaped;
    }

    if (isMAILTO_URL(param)) {
	format(&cmdbuf, command, param + 7);
    } else if (strncasecomp("telnet://", param, 9) == 0) {
	char host[sizeof(pram_string)];
	int last_pos;

	LYStrNCpy(host, param + 9, sizeof(host));
	last_pos = strlen(host) - 1;
	if (last_pos > 1 && host[last_pos] == '/')
	    host[last_pos] = '\0';

	format(&cmdbuf, command, host);
    } else if (strncasecomp("file://localhost/", param, 17) == 0) {
	char e_buff[LY_MAXPATH], *p;

	p = param + 17;
	delete_danger_characters(p);
	*e_buff = 0;
	if (StrChr(p, ':') == NULL) {
	    sprintf(e_buff, "%.3s/", windows_drive);
	}
	strncat(e_buff, p, sizeof(e_buff) - strlen(e_buff) - 1);
	p = strrchr(e_buff, '.');
	if (p) {
	    trimPoundSelector(p);
	}

	/* Less ==> short filename with backslashes,
	 * less ==> long filename with forward slashes, may be quoted
	 */
	if (ISUPPER(command[0])) {
	    char *short_name = HTDOS_short_name(e_buff);

	    p = quote_pathname(short_name);
	    format(&cmdbuf, command, p);
	    FREE(p);
	} else {
	    p = quote_pathname(e_buff);
	    format(&cmdbuf, command, p);
	    FREE(p);
	}
    } else {
	format(&cmdbuf, command, param);
    }
    FREE(escaped);
#else
    format(&cmdbuf, command, param);
#endif
    return cmdbuf;
}

/*
 * Find the EXTERNAL command which matches the given name 'param'.  If there is
 * more than one possibility, make a popup menu of the matching commands and
 * allow the user to select one.  Return the selected command.
 */
static char *lookup_external(char *param,
			     int only_overriders)
{
    int pass, num_disabled, num_matched, num_choices, cur_choice;
    size_t length = 0;
    char *cmdbuf = NULL;
    char **actions = 0;
    char **choices = 0;
    lynx_list_item_type *ptr = 0;

    for (pass = 0; pass < 2; pass++) {
	num_disabled = 0;
	num_matched = 0;
	num_choices = 0;
	for (ptr = externals; ptr != 0; ptr = ptr->next) {

	    if (match_item_by_name(ptr, param, only_overriders)) {
		++num_matched;
		CTRACE((tfp, "EXTERNAL: '%s' <==> '%s'\n", ptr->name, param));
		if (no_externals && !ptr->always_enabled && !only_overriders) {
		    ++num_disabled;
		} else {
		    if (pass == 0) {
			length++;
		    } else if (pass != 0) {
			cmdbuf = format_command(ptr->command, param);
			if (length > 1) {
			    actions[num_choices] = cmdbuf;
			    choices[num_choices] =
				format_command(ptr->menu_name, param);
			}
		    }
		    num_choices++;
		}
	    }
	}
	if (length > 1) {
	    if (pass == 0) {
		actions = typecallocn(char *, length + 1);
		choices = typecallocn(char *, length + 1);

		if (actions == 0 || choices == 0)
		    outofmem(__FILE__, "lookup_external");
	    } else {
		actions[num_choices] = 0;
		choices[num_choices] = 0;
	    }
	}
    }

    if (num_disabled != 0
	&& num_disabled == num_matched) {
	HTUserMsg(EXTERNALS_DISABLED);
    } else if (num_choices > 1) {
	int old_y, old_x;

	LYGetYX(old_y, old_x);
	cur_choice = LYhandlePopupList(-1,
				       0,
				       old_x,
				       (STRING2PTR) choices,
				       -1,
				       -1,
				       FALSE,
				       TRUE);
	wmove(LYwin, old_y, old_x);
	CTRACE((tfp, "selected choice %d of %d\n", cur_choice, num_choices));
	if (cur_choice < 0) {
	    HTInfoMsg(CANCELLED);
	    cmdbuf = 0;
	}
	for (pass = 0; choices[pass] != 0; pass++) {
	    if (pass == cur_choice) {
		cmdbuf = actions[pass];
	    } else {
		FREE(actions[pass]);
	    }
	    FREE(choices[pass]);
	}
    }

    if (actions) {
	for (pass = 0; actions[pass] != 0; ++pass) {
	    if (actions[pass] != cmdbuf)
		FREE(actions[pass]);
	}
	FREE(actions);
    }

    if (choices) {
	for (pass = 0; choices[pass] != 0; ++pass) {
	    FREE(choices[pass]);
	}
	FREE(choices);
    }

    return cmdbuf;
}

BOOL run_external(char *param,
		  int only_overriders)
{
#ifdef WIN_EX
    int status;
#endif
    int redraw_flag = TRUE;
    char *cmdbuf = NULL;
    BOOL found = FALSE;
    int confirmed = TRUE;

    if (externals == NULL)
	return 0;

#ifdef WIN_EX			/* 1998/01/26 (Mon) 09:16:13 */
    if (param == NULL) {
	HTInfoMsg(gettext("External command is null"));
	return 0;
    }
#endif

    cmdbuf = lookup_external(param, only_overriders);
    if (non_empty(cmdbuf)) {
#ifdef WIN_EX			/* 1997/10/17 (Fri) 14:07:50 */
	int len;
	char buff[LY_MAXPATH];

	CTRACE((tfp, "Lynx EXTERNAL: '%s'\n", cmdbuf));
#ifdef WIN_GUI			/* 1997/11/06 (Thu) 14:17:15 */
	confirmed = MessageBox(GetForegroundWindow(), cmdbuf,
			       "Lynx (EXTERNAL COMMAND EXEC)",
			       MB_ICONQUESTION | MB_SETFOREGROUND | MB_OKCANCEL)
	    != IDCANCEL;
#else
	confirmed = HTConfirm(LYElideString(cmdbuf, 40)) != NO;
#endif
	if (confirmed) {
	    len = strlen(cmdbuf);
	    if (len > 255) {
		sprintf(buff, "Lynx: command line too long (%d > 255)", len);
#ifdef WIN_GUI			/* 1997/11/06 (Thu) 14:17:02 */
		MessageBox(GetForegroundWindow(), buff,
			   "Lynx (EXTERNAL COMMAND EXEC)",
			   MB_ICONEXCLAMATION | MB_SETFOREGROUND | MB_OK);
		SetConsoleTitle("Lynx for Win32");
#else
		HTConfirm(LYElideString(buff, 40));
#endif
		confirmed = FALSE;
	    } else {
		SetConsoleTitle(cmdbuf);
	    }
	}

	if (strncasecomp(cmdbuf, "start ", 6) == 0)
	    redraw_flag = FALSE;
	else
	    redraw_flag = TRUE;
#else
	HTUserMsg(cmdbuf);
#endif
	found = TRUE;
	if (confirmed) {
	    if (redraw_flag) {
		stop_curses();
		fflush(stdout);
	    }

	    /* command running. */
#ifdef WIN_EX			/* 1997/10/17 (Fri) 14:07:50 */
#if defined(__CYGWIN__) || defined(__MINGW32__)
	    status = system(cmdbuf);
#else
	    status = xsystem(cmdbuf);
#endif
	    if (status != 0) {
		sprintf(buff,
			"EXEC code = %04x (%2d, %2d)\r\n"
			"'%s'",
			status, (status / 256), (status & 0xff),
			cmdbuf);
#ifdef SH_EX			/* WIN_GUI for ERROR only */
		MessageBox(GetForegroundWindow(), buff,
			   "Lynx (EXTERNAL COMMAND EXEC)",
			   MB_ICONSTOP | MB_SETFOREGROUND | MB_OK);
#else
		HTConfirm(LYElideString(buff, 40));
#endif /* 1 */
	    }
#else /* Not WIN_EX */
	    LYSystem(cmdbuf);
#endif /* WIN_EX */

#if defined(WIN_EX)
	    SetConsoleTitle("Lynx for Win32");
#endif
	    if (redraw_flag) {
		fflush(stdout);
		start_curses();
	    }
	}
    }

    FREE(cmdbuf);
    return found;
}
#endif /* USE_EXTERNALS */
