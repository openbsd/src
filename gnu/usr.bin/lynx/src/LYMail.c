#include <HTUtils.h>
#include <HTParse.h>
#include <LYGlobalDefs.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYSignal.h>
#include <LYUtils.h>
#include <LYClean.h>
#include <LYStrings.h>
#include <GridText.h>
#include <LYMail.h>
#include <LYEdit.h>
#include <LYCharSets.h>  /* to get current charset for mail header */

#include <LYLeaks.h>

#define MAX_SUBJECT 70

BOOLEAN term_letter;	/* Global variable for async i/o. */

PRIVATE void terminate_letter ARGS1(int,sig GCC_UNUSED)
{
    term_letter = TRUE;
    /* Reassert the AST */
    signal(SIGINT, terminate_letter);
#if USE_VMS_MAILER || defined(PDCURSES)
    /*
     *	Refresh the screen to get rid of the "interrupt" message.
     */
    if (!dump_output_immediately) {
	lynx_force_repaint();
	LYrefresh();
    }
#endif /* VMS */
}

/* HTUnEscape with control-code nuking */
PRIVATE void SafeHTUnEscape ARGS1(
    char *,	string)
{
     int i;
     int flg = FALSE;

     HTUnEscape(string);
     for (i=0; string[i] != '\0'; i++)
     {
	/* FIXME: this is no longer explicitly 7-bit ASCII,
	   but are there portability problems? */
	if ((!LYIsASCII(string[i])) || !isprint(UCH(string[i])))
	{
	   string[i] = '?';
	   flg = TRUE;
	}
     }
     if (flg)
	HTAlert(MAILTO_SQUASH_CTL);
}

PRIVATE void remove_tildes ARGS1(char *,string)
{
   /*
    *  Change the first character to
    *  a space if it is a '~'.
    */
    if (*string == '~')
	*string = ' ';
}

PRIVATE void comma_append ARGS2(
    char **,	dst,
    char *,	src)
{
    if (*src) {
	while (*src == ',' || isspace(UCH(*src)))
	    src++;
	if (*src) {
	    if (isEmpty(*dst)) {
		StrAllocCopy(*dst, src);
	    } else {
		StrAllocCat(*dst, ",");
		StrAllocCat(*dst, src);
	    }
	}
    }
}

PRIVATE void extract_field ARGS3(
    char **,	dst,
    char *,	src,
    char *,	keyword)
{
    int len = strlen(keyword);
    char *cp, *cp1;

    cp = (src + 1);
    while (*cp != '\0') {
	if ((*(cp - 1) == '?' || *(cp - 1) == '&') &&
	    !strncasecomp(cp, keyword, len)) {
	    cp += len;
	    if ((cp1 = strchr(cp, '&')) != NULL) {
		*cp1 = '\0';
	    }
	    comma_append(dst, cp);
	    if (cp1) {
		*cp1 = '&';
		cp = cp1;
		cp1 = NULL;
	    } else {
		break;
	    }
	}
	cp++;
    }
    CTRACE((tfp, "extract_field(%s) = '%s'\n", keyword, NONNULL(*dst)));
}

/*
 * Seek and handle a subject=foo.  - FM
 */
PRIVATE void extract_subject ARGS2(
    char *,	dst,
    char *,	src)
{
    CONST char *keyword = "subject=";
    int len = strlen(keyword);
    char *cp, *cp1;

    cp = (src + 1);
    while (*cp != '\0') {
	if ((*(cp - 1) == '?' || *(cp - 1) == '&') &&
	    !strncasecomp(cp, keyword, len))
	    break;
	cp++;
    }
    if (*cp) {
	cp += len;
	if ((cp1 = strchr(cp, '&')) != NULL) {
	    *cp1 = '\0';
	}
	if (*cp) {
	    strncpy(dst, cp, MAX_SUBJECT);
	    dst[MAX_SUBJECT] = '\0';
	    SafeHTUnEscape(dst);
	}
	if (cp1) {
	    *cp1 = '&';
	    cp1 = NULL;
	}
    }
    CTRACE((tfp, "extract_subject(%s) = '%s'\n", keyword, NONNULL(dst)));
}

/*
 * Seek and handle body=foo fields.  - FM
 */
PRIVATE void extract_body ARGS2(
    char **,	dst,
    char *,	src)
{
    CONST char *keyword = "body=";
    int len = strlen(keyword);
    int i;
    char *cp, *cp0, *cp1, *temp = 0;

    cp = (src + 1);
    while (*cp != '\0') {
	if ((*(cp - 1) == '?' || *(cp - 1) == '&') &&
	    !strncasecomp(cp, keyword, len)) {
	    cp += len;
	    if ((cp1 = strchr(cp, '&')) != NULL) {
		*cp1 = '\0';
	    }
	    if (*cp) {
		/*
		 *  Break up the value into lines with
		 *  a maximum length of 78. - FM
		 */
		StrAllocCopy(temp, cp);
		HTUnEscape(temp);
		cp0 = temp;
		while((cp = strchr(cp0, '\n')) != NULL) {
		    *cp = '\0';
		    if (cp > cp0) {
			if (*(cp - 1) == '\r') {
			    *(cp - 1) = '\0';
			}
		    }
		    i = 0;
		    len = strlen(cp0);
		    while (len > 78) {
			HTSprintf(dst, "%.78s\n", &cp0[i]);
			i += 78;
			len = strlen(&cp0[i]);
		    }
		    HTSprintf(dst, "%s\n", &cp0[i]);
		    cp0 = (cp + 1);
		}
		i = 0;
		len = strlen(cp0);
		while (len > 78) {
		    HTSprintf(dst, "%.78s\n", &cp0[i]);
		    i += 78;
		    len = strlen(&cp0[i]);
		}
		if (len) {
		    HTSprintf(dst, "%s\n", &cp0[i]);
		}
		FREE(temp);
	    }
	    if (cp1) {
		*cp1 = '&';
		cp = cp1;
		cp1 = NULL;
	    } else {
		break;
	    }
	}
	cp++;
    }
    CTRACE((tfp, "extract_body(%s) = '%s'\n", keyword, NONNULL(*dst)));
}

/*
 * Convert any Explorer semi-colon Internet address separators to commas - FM
 */
PRIVATE BOOLEAN trim_comma ARGS1(
    char *,	address)
{
    if (address[(strlen(address) - 1)] == ',')
	address[(strlen(address) - 1)] = '\0';
    return (BOOL) (*address == '\0');
}

/*
 * Convert any Explorer semi-colon Internet address separators to commas - FM
 */
PRIVATE BOOLEAN convert_explorer ARGS1(
    char *,	address)
{
    char *cp = address;
    char *cp0;
    char *cp1;

    while ((cp1 = strchr(cp, '@')) != NULL) {
	cp1++;
	if ((cp0 = strchr(cp1, ';')) != NULL) {
	    *cp0 = ',';
	    cp1 = cp0 + 1;
	}
	cp = cp1;
    }
    return trim_comma(address);
}

/*
 * reply_by_mail() prompts line-by-line for header information, allowing
 * scrolling of the screen.
 */
PRIVATE int header_prompt ARGS3(
    char *,		label,
    char **,		result,
    unsigned,		limit)
{
    char buffer[LINESIZE];
    int ok;

    if (*result != 0) {
	LYaddstr(CTRL_U_TO_ERASE);
	LYstrncpy(buffer, *result, sizeof(buffer)-1);
    } else
	*buffer = 0;

    if (limit > sizeof(buffer))
	limit = sizeof(buffer);

    LYaddstr(gettext(label));
    LYaddstr(": ");
    ok = (LYgetstr(buffer, VISIBLE, limit, NORECALL) >= 0
	&& !term_letter);
    LYaddstr("\n");

    if (ok) {
	remove_tildes(buffer);
	StrAllocCopy(*result, buffer);
    }
    term_letter = FALSE;
    return ok;
}

PRIVATE void show_addresses ARGS1(
    char *,	addresses)
{
    char *cp = addresses;
    char *cp1;

    while ((cp1 = strchr(cp, ',')) != NULL) {
	*cp1 = '\0';
	while (*cp == ' ')
	    cp++;
	if (*cp) {
	    LYaddstr(cp);
	    LYaddstr(",\n  ");
	}
	*cp1 = ',';
	cp = (cp1 + 1);
    }
    if (*cp) {
	LYaddstr(cp);
    }
}

#if USE_BLAT_MAILER

/*
syntax:
Blat <filename> -t <recipient> [optional switches (see below)]

<filename>    : file with the message body
-t <recipient>: recipient list (comma separated)
-s <subj>     : subject line
-f <sender>   : overrides the default sender address (must be known to server)
-i <addr>     : a 'From:' address, not necessarily known to the SMTP server.
-c <recipient>: carbon copy recipient list (comma separated)
-b <recipient>: blind carbon copy recipient list (comma separated)
-h            : displays this help.
-mime         : MIME Quoted-Printable Content-Transfer-Encoding.
-q            : supresses *all* output.
-server <addr>: overrides the default SMTP server to be used.

*/

PRIVATE char *blat_cmd(
	char *mail_cmd,
	char *filename,
	char *address,
	char *subject,
	char *ccaddr,
	char *mail_addr)
{
    static char *b_cmd;

#ifdef USE_ALT_BLAT_MAILER

    HTSprintf0(&b_cmd, "%s %s -t \"%s\" -s \"%s\" %s%s%s%s",
		mail_cmd,
		filename,
		address,
		subject,
		system_mail_flags,
		ccaddr? " -c \"" : "",
		NonNull(ccaddr),
		ccaddr? "\"" : "");

#else /* !USE_ALT_BLAT_MAILER */

    static char bl_cmd_file[512];
    FILE *fp;
#ifdef __CYGWIN__
    char dosname[LY_MAXPATH];
#endif

    bl_cmd_file[0] = '\0';
    if ((fp = LYOpenTemp(bl_cmd_file, ".blt", "w")) == NULL) {
	HTAlert(FORM_MAILTO_FAILED);
	return NULL;
    }

#ifdef __CYGWIN__
    cygwin_conv_to_full_win32_path(filename, dosname);
    fprintf(fp, "%s\n", dosname);
#else
    fprintf(fp, "%s\n", filename);
#endif
    fprintf(fp, "-t\n%s\n", address);
    if (subject)
	fprintf(fp, "-s\n%s\n", subject);
    if (!isEmpty(mail_addr)) {
	fprintf(fp, "-f\n%s\n", mail_addr);
    }
    if (!isEmpty(ccaddr)) {
	fprintf(fp, "-c\n%s\n", ccaddr);
    }
    LYCloseOutput(fp);

#ifdef __CYGWIN__
    cygwin_conv_to_full_win32_path(bl_cmd_file, dosname);
    HTSprintf0(&b_cmd, "%s \"@%s\"", mail_cmd, dosname);
#else
    HTSprintf0(&b_cmd, "%s @%s", mail_cmd, bl_cmd_file);
#endif

#endif /* USE_ALT_BLAT_MAILER */

    return b_cmd;
}

#endif /* USE_BLAT_MAILER */

#if USE_VMS_MAILER
PUBLIC BOOLEAN LYMailPMDF(void)
{
    return (system_mail != 0)
	    ? !strncasecomp(system_mail, "PMDF SEND", 9)
	    : FALSE;
}

/*
 * Add all of the people in the address field to the command
 */
PRIVATE void vms_append_addrs (char **cmd, char *address, char *option)
{
    BOOLEAN first = TRUE;
    char *cp;
    char *address_ptr1;
    char *address_ptr2;

    address_ptr1 = address;
    do {
	if ((cp = strchr(address_ptr1, ',')) != NULL) {
	    address_ptr2 = (cp+1);
	    *cp = '\0';
	} else {
	    address_ptr2 = NULL;
	}

	/*
	 *  4 letters is arbitrarily the smallest possible mail
	 *  address, at least for lynx.  That way extra spaces
	 *  won't confuse the mailer and give a blank address.
	 */
	if (strlen(address_ptr1) > 3) {
	    if (!first) {
		StrAllocCat(*cmd, ",");
	    }
	    HTSprintf(cmd, mail_adrs, address_ptr1);
	    if (*option && LYMailPMDF())
		StrAllocCat(*cmd, option);
	    first = FALSE;
	}
	address_ptr1 = address_ptr2;
    } while (address_ptr1 != NULL);
}

PRIVATE void remove_quotes (char * string)
{
    while (*string != 0) {
	if (strchr("\"&|", *string) != 0)
	    *string = ' ';
	string++;
    }
}
#else
#if CAN_PIPE_TO_MAILER

/*
 * Open a pipe to the mailer
 */
PUBLIC FILE *LYPipeToMailer NOARGS
{
    char *buffer = NULL;
    FILE *fp = NULL;

    if (LYSystemMail()) {
	HTSprintf0(&buffer, "%s %s", system_mail, system_mail_flags);
	fp = popen(buffer, "w");
	CTRACE((tfp, "popen(%s) %s\n", buffer, fp != 0 ? "OK" : "FAIL"));
	FREE(buffer);
    }
    return fp;
}
#else	/* DOS, Win32, etc. */

PUBLIC int LYSendMailFile ARGS5(
    char *,	the_address,
    char *,	the_filename,
    char *,	the_subject GCC_UNUSED,
    char *,	the_ccaddr GCC_UNUSED,
    char *,	message)
{
    char *cmd = NULL;
#ifdef __DJGPP__
    char *shell;
#endif /* __DJGPP__ */
    int code;

    if (!LYSystemMail())
	return 0;

#if USE_BLAT_MAILER
    if (mail_is_blat)
	StrAllocCopy(cmd,
		blat_cmd(
		    system_mail,
		    the_filename,
		    the_address,
		    the_subject,
		    the_ccaddr,
		    personal_mail_address
		)
	);
    else
#endif
#ifdef __DJGPP__
	if ((shell = LYGetEnv("SHELL")) != NULL) {
	    if (strstr(shell, "sh") != NULL) {
		HTSprintf0(&cmd, "%s -c %s -t \"%s\" -F %s",
			   shell,
			   system_mail,
			   the_address,
			   the_filename);
	    } else {
		HTSprintf0(&cmd, "%s /c %s -t \"%s\" -F %s",
			   shell,
			   system_mail,
			   the_address,
			   the_filename);
	    }
	} else {
	    HTSprintf0(&cmd, "%s -t \"%s\" -F %s",
		       system_mail,
		       the_address,
		       the_filename);
	}
#else
	HTSprintf0(&cmd, "%s -t \"%s\" -F %s",
		   system_mail,
		   the_address,
		   the_filename);
#endif /* __DJGPP__ */

    stop_curses();
    SetOutputMode(O_TEXT);
    printf("%s\n\n$ %s\n\n%s",
	   *message ? message : gettext("Sending"),
	   cmd, PLEASE_WAIT);
    code = LYSystem(cmd);
    LYSleepMsg();
    start_curses();
    SetOutputMode( O_BINARY );

    FREE(cmd);

    return code;
}
#endif /* CAN_PIPE_TO_FILE */
#endif /* USE_VMS_MAILER */

/*
**  mailform() sends form content to the mailto address(es). - FM
*/
PUBLIC void mailform ARGS4(
    CONST char *,	mailto_address,
    CONST char *,	mailto_subject,
    CONST char *,	mailto_content,
    CONST char *,	mailto_type)
{
    FILE *fd;
    char *address = NULL;
    char *ccaddr = NULL;
    char *keywords = NULL;
    char *cp = NULL;
    char self[MAX_SUBJECT + 10];
    char subject[MAX_SUBJECT + 10];
    char *searchpart = NULL;
    char buf[512];
    int ch, len, i;
#if USE_VMS_MAILER
    static char *cmd;
    char *command = NULL;
    BOOLEAN isPMDF = LYMailPMDF();
    char hdrfile[LY_MAXPATH];
#endif
#if !CAN_PIPE_TO_MAILER
    char my_tmpfile[LY_MAXPATH];
#endif

    CTRACE((tfp, "mailto_address: \"%s\"\n", NONNULL(mailto_address)));
    CTRACE((tfp, "mailto_subject: \"%s\"\n", NONNULL(mailto_subject)));
    CTRACE((tfp, "mailto_content: \"%s\"\n", NONNULL(mailto_content)));
    CTRACE((tfp, "mailto_type:    \"%s\"\n", NONNULL(mailto_type)));

    if (!LYSystemMail())
	return;

    if (!mailto_address || !mailto_content) {
	HTAlert(BAD_FORM_MAILTO);
	return;
    }
    subject[0] = '\0';
    self[0] = '\0';

    if ((cp = (char *)strchr(mailto_address,'\n')) != NULL)
	*cp = '\0';
    StrAllocCopy(address, mailto_address);

    /*
     *	Check for a ?searchpart. - FM
     */
    if ((cp = strchr(address, '?')) != NULL) {
	StrAllocCopy(searchpart, cp);
	*cp = '\0';
	cp = (searchpart + 1);
	if (*cp != '\0') {
	    /*
	     *	Seek and handle a subject=foo. - FM
	     */
	    extract_subject(subject, searchpart);

	    /*
	     *	Seek and handle to=address(es) fields.
	     *	Appends to address. - FM
	     */
	    extract_field(&address, searchpart, "to=");

	    /*
	     *	Seek and handle cc=address(es) fields.	Excludes
	     *	Bcc=address(es) as unsafe.  We may append our own
	     *	cc (below) as a list for the actual mailing. - FM
	     */
	    extract_field(&ccaddr, searchpart, "cc=");

	    /*
	     *	Seek and handle keywords=term(s) fields. - FM
	     */
	    extract_field(&keywords, searchpart, "keywords=");

	    if (keywords != NULL) {
		if (*keywords != '\0') {
		    SafeHTUnEscape(keywords);
		} else {
		    FREE(keywords);
		}
	    }

	    FREE(searchpart);
	}
    }

    if (convert_explorer(address)) {
	HTAlert(BAD_FORM_MAILTO);
	goto cleanup;
    }
    if (ccaddr != NULL) {
	if (convert_explorer(ccaddr)) {
	    FREE(ccaddr);
	}
    }

    /*
     *	Unescape the address and ccaddr fields. - FM
     */
    SafeHTUnEscape(address);
    if (ccaddr != NULL) {
	SafeHTUnEscape(ccaddr);
    }

    /*
     *	Allow user to edit the default Subject - FM
     */
    if (isEmpty(subject)) {
	if (!isEmpty(mailto_subject)) {
	    LYstrncpy(subject, mailto_subject, MAX_SUBJECT);
	} else {
	    sprintf(subject, "mailto:%.63s", address);
	}
    }
    _statusline(SUBJECT_PROMPT);
    if ((ch = LYgetstr(subject, VISIBLE, MAX_SUBJECT, NORECALL)) < 0) {
	/*
	 * User cancelled via ^G. - FM
	 */
	HTInfoMsg(FORM_MAILTO_CANCELLED);
	goto cleanup;
    }

    /*
     *	Allow user to specify a self copy via a CC:
     *	entry, if permitted. - FM
     */
    if (!LYNoCc) {
	sprintf(self, "%.*s", MAX_SUBJECT,
		isEmpty(personal_mail_address) ? "" : personal_mail_address);
	_statusline("Cc: ");
	if ((ch = LYgetstr(self, VISIBLE, MAX_SUBJECT, NORECALL)) < 0) {
	    /*
	     * User cancelled via ^G. - FM
	     */
	    HTInfoMsg(FORM_MAILTO_CANCELLED);
	    goto cleanup;
	}
	remove_tildes(self);
	if (ccaddr == NULL) {
	    StrAllocCopy(ccaddr, self);
	} else {
	    StrAllocCat(ccaddr, ",");
	    StrAllocCat(ccaddr, self);
	}
    }

#if CAN_PIPE_TO_MAILER
    if ((fd = LYPipeToMailer()) == 0) {
	HTAlert(FORM_MAILTO_FAILED);
	goto cleanup;
    }

    if (!isEmpty(mailto_type)) {
	fprintf(fd, "Mime-Version: 1.0\n");
	fprintf(fd, "Content-Type: %s\n", mailto_type);
    }
    fprintf(fd, "To: %s\n", address);
    if (!isEmpty(personal_mail_address))
	fprintf(fd, "From: %s\n", personal_mail_address);
    if (!isEmpty(ccaddr))
	fprintf(fd, "Cc: %s\n", ccaddr);
    fprintf(fd, "Subject: %s\n\n", subject);
    if (!isEmpty(keywords))
	fprintf(fd, "Keywords: %s\n", keywords);
    _statusline(SENDING_FORM_CONTENT);
#else	/* e.g., VMS, DOS */
    if ((fd = LYOpenTemp(my_tmpfile, ".txt", "w")) == NULL) {
	HTAlert(FORM_MAILTO_FAILED);
	goto cleanup;
    }
#if USE_VMS_MAILER
    if (isPMDF) {
	FILE *hfd;
	if ((hfd = LYOpenTemp(hdrfile, ".txt", "w")) == NULL) {
	    HTAlert(FORM_MAILTO_FAILED);
	    LYCloseTempFP(fd);
	    goto cleanup;
	}
	if (!isEmpty(mailto_type)) {
	    fprintf(hfd, "Mime-Version: 1.0\n");
	    fprintf(hfd, "Content-Type: %s\n", mailto_type);
	    if (!isEmpty(personal_mail_address))
		fprintf(hfd, "From: %s\n", personal_mail_address);
	}
	/*
	 *  For PMDF, put any keywords and the subject
	 *  in the header file and close it. - FM
	 */
	if (!isEmpty(keywords)) {
	    fprintf(hfd, "Keywords: %s\n", keywords);
	}
	fprintf(hfd, "Subject: %s\n\n", subject);
	LYCloseTempFP(hfd);
    } else if (mailto_type &&
	       !strncasecomp(mailto_type, "multipart/form-data", 19)) {
	/*
	 *  Ugh!  There's no good way to include headers while
	 *  we're still using "generic" VMS MAIL, so we'll put
	 *  this in the body of the message. - FM
	 */
	fprintf(fd, "X-Content-Type: %s\n\n", mailto_type);
    }
#else	/* !VMS (DOS) */
#if USE_BLAT_MAILER
    if (mail_is_blat) {
	if (strlen(subject) > MAX_SUBJECT)
	    subject[MAX_SUBJECT] = '\0';
    } else
#endif
    {
	if (!isEmpty(mailto_type)) {
	    fprintf(fd, "Mime-Version: 1.0\n");
	    fprintf(fd, "Content-Type: %s\n", mailto_type);
	}
	fprintf(fd,"To: %s\n", address);
	if (!isEmpty(personal_mail_address))
	    fprintf(fd,"From: %s\n", personal_mail_address);
	fprintf(fd,"Subject: %.70s\n\n", subject);
    }
#endif /* VMS */
#endif /* CAN_PIPE_TO_MAILER */

    /*
     *	Break up the content into lines with a maximum length of 78.
     *	If the ENCTYPE was text/plain, we have physical newlines and
     *	should take them into account.	Otherwise, the actual newline
     *	characters in the content are hex escaped. - FM
     */
    while((cp = strchr(mailto_content, '\n')) != NULL) {
	*cp = '\0';
	i = 0;
	len = strlen(mailto_content);
	while (len > 78) {
	    strncpy(buf, &mailto_content[i], 78);
	    buf[78] = '\0';
	    fprintf(fd, "%s\n", buf);
	    i += 78;
	    len = strlen(&mailto_content[i]);
	}
	fprintf(fd, "%s\n", &mailto_content[i]);
	mailto_content = (cp+1);
    }
    i = 0;
    len = strlen(mailto_content);
    while (len > 78) {
	strncpy(buf, &mailto_content[i], 78);
	buf[78] = '\0';
	fprintf(fd, "%s\n", buf);
	i += 78;
	len = strlen(&mailto_content[i]);
    }
    if (len)
	fprintf(fd, "%s\n", &mailto_content[i]);

#if CAN_PIPE_TO_MAILER
    pclose(fd);
    LYSleepMsg();
#else
    LYCloseTempFP(fd);
#if USE_VMS_MAILER
    /*
     *	Set the mail command. - FM
     */
    if (isPMDF) {
	/*
	 *  Now set up the command. - FM
	 */
	HTSprintf0(&cmd,
		"%s %s %s,%s ",
		system_mail,
		system_mail_flags,
		hdrfile,
		my_tmpfile);
    } else {
	/*
	 *  For "generic" VMS MAIL, include the subject in the
	 *  command, and ignore any keywords to minimize risk
	 *  of them making the line too long or having problem
	 *  characters. - FM
	 */
	HTSprintf0(&cmd,
		"%s %s%s/subject=\"%s\" %s ",
		system_mail,
		system_mail_flags,
		(strncasecomp(system_mail, "MAIL", 4) ? "" : "/noself"),
		subject,
		my_tmpfile);
    }
    StrAllocCopy(command, cmd);

    vms_append_addrs(&command, address, "");
    if (!isEmpty(ccaddr)) {
	vms_append_addrs(&command, ccaddr, "/CC");
    }

    stop_curses();
    printf("%s\n\n$ %s\n\n%s", SENDING_FORM_CONTENT, command, PLEASE_WAIT);
    LYSystem(command);	/* Mail (VMS) */
    FREE(command);
    LYSleepAlert();
    start_curses();
    LYRemoveTemp(my_tmpfile);
    if (isPMDF)
	LYRemoveTemp(hdrfile);
#else /* DOS */
    LYSendMailFile (
	address,
	my_tmpfile,
	subject,
	ccaddr,
	SENDING_FORM_CONTENT);
    LYRemoveTemp(my_tmpfile);
#endif /* USE_VMS_MAILER */
#endif /* CAN_PIPE_TO_MAILER */

cleanup:
    FREE(address);
    FREE(ccaddr);
    FREE(keywords);
    return;
}

/*
**  mailmsg() sends a message to the owner of the file, if one is defined,
**  telling of errors (i.e., link not available).
*/
PUBLIC void mailmsg ARGS4(
	int,		cur,
	char *,		owner_address,
	char *,		filename,
	char *,		linkname)
{
    FILE *fd, *fp;
    char *address = NULL;
    char *searchpart = NULL;
    char *cmd = NULL, *cp;
#ifdef ALERTMAIL
    BOOLEAN skip_parsing = FALSE;
#endif
#if !CAN_PIPE_TO_MAILER
    char *ccaddr;
    char subject[128];
    char my_tmpfile[LY_MAXPATH];
#endif
#if USE_VMS_MAILER
    BOOLEAN isPMDF = LYMailPMDF();
    char hdrfile[LY_MAXPATH];
    char *command = NULL;

    CTRACE((tfp, "mailmsg(%d, \"%s\", \"%s\", \"%s\")\n", cur,
	NONNULL(owner_address),
	NONNULL(filename),
	NONNULL(linkname)));

#endif /* VMS */

    if (!LYSystemMail())
	return;

#ifdef ALERTMAIL
    if (owner_address == NULL) {
	owner_address = ALERTMAIL;
	skip_parsing = TRUE;
    }
#endif

    if (isEmpty(owner_address))
	return;
    if ((cp = (char *)strchr(owner_address,'\n')) != NULL) {
#ifdef ALERTMAIL
	if (skip_parsing)
	    return;		/* invalidly defined - ignore - kw */
#else
	*cp = '\0';
#endif
    }
    if (!strncasecomp(owner_address, "lynx-dev@", 9)) {
	/*
	 *  Silently refuse sending bad link messages to lynx-dev.
	 */
	return;
    }
    StrAllocCopy(address, owner_address);

#ifdef ALERTMAIL
    /*
     *  If we are using a fixed address given by ALERTMAIL, it is
     *  supposed to already be in usable form, without URL-isms like
     *  ?-searchpart and URL-escaping.  So skip some code. - kw
     */
    if (!skip_parsing)
#endif
    {
	/*
	 *	Check for a ?searchpart. - FM
	 */
	if ((cp = strchr(address, '?')) != NULL) {
	    StrAllocCopy(searchpart, cp);
	    *cp = '\0';
	    cp = (searchpart + 1);
	    if (*cp != '\0') {
		/*
		 * Seek and handle to=address(es) fields.
		 * Appends to address.  We ignore any other
		 * headers in the ?searchpart.  - FM
		 */
		extract_field(&address, searchpart, "to=");
	    }
	}

	convert_explorer(address);

	/*
	 *  Unescape the address field. - FM
	 */
	SafeHTUnEscape(address);
    }

    if (trim_comma(address)) {
	FREE(address);
	CTRACE((tfp, "mailmsg: No address in '%s'.\n", owner_address));
	return;
    }

#if CAN_PIPE_TO_MAILER
    if ((fd = LYPipeToMailer()) == 0) {
	FREE(address);
	CTRACE((tfp, "mailmsg: '%s' failed.\n", cmd));
	return;
    }

    fprintf(fd, "To: %s\n", address);
    fprintf(fd, "Subject: Lynx Error in %s\n", filename);
    if (!isEmpty(personal_mail_address)) {
	fprintf(fd, "Cc: %s\n", personal_mail_address);
    }
    fprintf(fd, "X-URL: %s\n", filename);
    fprintf(fd, "X-Mailer: %s, Version %s\n\n", LYNX_NAME, LYNX_VERSION);
#else
    if ((fd = LYOpenTemp(my_tmpfile, ".txt", "w")) == NULL) {
	CTRACE((tfp, "mailmsg: Could not fopen '%s'.\n", my_tmpfile));
	FREE(address);
	return;
    }
    sprintf(subject, "Lynx Error in %.56s", filename);
    ccaddr = personal_mail_address;
#if USE_VMS_MAILER
    if (isPMDF) {
	FILE *hfd;
	if ((hfd = LYOpenTemp(hdrfile, ".txt", "w")) == NULL) {
	    CTRACE((tfp, "mailmsg: Could not fopen '%s'.\n", hdrfile));
	    FREE(address);
	    return;
	}

	if (!isEmpty(personal_mail_address)) {
	    fprintf(fd, "Cc: %s\n", personal_mail_address);
	}
	fprintf(fd, "X-URL: %s\n", filename);
	fprintf(fd, "X-Mailer: %s, Version %s\n\n", LYNX_NAME, LYNX_VERSION);
	/*
	 *  For PMDF, put the subject in the
	 *  header file and close it. - FM
	 */
	fprintf(hfd, "Subject: Lynx Error in %.56s\n\n", filename);
	LYCloseTempFP(hfd);
    }
#endif /* USE_VMS_MAILER */
#endif /* CAN_PIPE_TO_MAILER */

    fprintf(fd, gettext("The link   %s :?: %s \n"),
		links[cur].lname, links[cur].target);
    fprintf(fd, gettext("called \"%s\"\n"), LYGetHiliteStr(cur, 0));
    fprintf(fd, gettext("in the file \"%s\" called \"%s\"\n"), filename, linkname);
    fprintf(fd, "%s\n\n", gettext("was requested but was not available."));
    fprintf(fd, "%s\n\n", gettext("Thought you might want to know."));

    fprintf(fd, "%s\n", gettext("This message was automatically generated by"));
    fprintf(fd, "%s %s", LYNX_NAME, LYNX_VERSION);
    if ((LynxSigFile != NULL) &&
	(fp = fopen(LynxSigFile, TXT_R)) != NULL) {
	fputs("-- \n", fd);
	while (LYSafeGets(&cmd, fp) != NULL)
	    fputs(cmd, fd);
	LYCloseInput(fp);
    }
#if CAN_PIPE_TO_MAILER
    pclose(fd);
#else
    LYCloseTempFP(fd);
#if USE_VMS_MAILER
    if (isPMDF) {
	/*
	 *  Now set up the command. - FM
	 */
	HTSprintf0(&command,
		"%s %s %s,%s ",
		system_mail,
		system_mail_flags,
		hdrfile,
		my_tmpfile);
    } else {
	/*
	 *  For "generic" VMS MAIL, include the
	 *  subject in the command. - FM
	 */
	HTSprintf0(&command,
		"%s %s/self/subject=\"Lynx Error in %.56s\" %s ",
		system_mail,
		system_mail_flags,
		filename,
		my_tmpfile);
    }
    vms_append_addrs(&command, address, "");

    LYSystem(command);	/* VMS */
    FREE(command);
    FREE(cmd);
    LYRemoveTemp(my_tmpfile);
    if (isPMDF) {
	LYRemoveTemp(hdrfile);
    }
#else /* DOS */
    LYSendMailFile (
	address,
	my_tmpfile,
	subject,
	ccaddr,
	"");
    LYRemoveTemp(my_tmpfile);
#endif /* USE_VMS_MAILER */
#endif /* CAN_PIPE_TO_MAILER */

    if (traversal) {
	FILE *ofp;

	if ((ofp = LYAppendToTxtFile(TRAVERSE_ERRORS)) == NULL) {
	    if ((ofp = LYNewTxtFile(TRAVERSE_ERRORS)) == NULL) {
		perror(NOOPEN_TRAV_ERR_FILE);
		exit_immediately(EXIT_FAILURE);
	    }
	}

	fprintf(ofp, "%s\t%s \tin %s\n",
		     links[cur].lname, links[cur].target, filename);
	LYCloseOutput(ofp);
    }

    FREE(address);
    return;
}

/*
**  reply_by_mail() invokes sendmail on Unix or mail on VMS to send
**  a comment from the users to the owner
*/
PUBLIC void reply_by_mail ARGS4(
	char *,		mail_address,
	char *,		filename,
	CONST char *,	title,
	CONST char *,	refid)
{
#ifndef NO_ANONYMOUS_EMAIL
    static char *personal_name = NULL;
#endif
    char user_input[LINESIZE];
    FILE *fd, *fp;
    char *label = NULL;
    char *from_address = NULL;
    char *cc_address = NULL;
    char *to_address = NULL;
    char *the_subject = NULL;
    char *ccaddr = NULL;
    char *keywords = NULL;
    char *searchpart = NULL;
    char *body = NULL;
    char *cp = NULL, *cp1 = NULL;
    int i;
    int c = 0;	/* user input */
    char my_tmpfile[LY_MAXPATH];
    char default_subject[MAX_SUBJECT + 10];
#if USE_VMS_MAILER
    char *command = NULL;
    BOOLEAN isPMDF = LYMailPMDF();
    char hdrfile[LY_MAXPATH];
    FILE *hfd = 0;
#else
#if !CAN_PIPE_TO_MAILER
    char tmpfile2[LY_MAXPATH];
#endif
    char buf[4096];	/* 512 */
    char *header = NULL;
    int n;
#endif /* USE_VMS_MAILER */

    CTRACE((tfp, "reply_by_mail(\"%s\", \"%s\", \"%s\", \"%s\")\n",
	NONNULL(mail_address),
	NONNULL(filename),
	NONNULL(title),
	NONNULL(refid)));

    term_letter = FALSE;

    if (!LYSystemMail())
	return;

    if (isEmpty(mail_address)) {
	HTAlert(NO_ADDRESS_IN_MAILTO_URL);
	return;
    }
    StrAllocCopy(to_address, mail_address);

    if ((fd = LYOpenTemp(my_tmpfile, ".txt", "w")) == NULL) {
	HTAlert(MAILTO_URL_TEMPOPEN_FAILED);
	return;
    }
#if USE_VMS_MAILER
    if (isPMDF) {
	if ((hfd = LYOpenTemp(hdrfile, ".txt", "w")) == NULL) {
	    HTAlert(MAILTO_URL_TEMPOPEN_FAILED);
	    return;
	}
    }
#endif /* VMS */
    default_subject[0] = '\0';

    /*
     *	Check for a ?searchpart. - FM
     */
    if ((cp = strchr(to_address, '?')) != NULL) {
	StrAllocCopy(searchpart, cp);
	*cp = '\0';
	cp = (searchpart + 1);
	if (*cp != '\0') {
	    /*
	     *	Seek and handle a subject=foo. - FM
	     */
	    extract_subject(default_subject, searchpart);

	    /*
	     *	Seek and handle to=address(es) fields.
	     *	Appends to address. - FM
	     */
	    extract_field(&to_address, searchpart, "to=");

	    /*
	     *	Seek and handle cc=address(es) fields.	Excludes
	     *	Bcc=address(es) as unsafe.  We may append our own
	     *	cc (below) as a list for the actual mailing. - FM
	     */
	    extract_field(&ccaddr, searchpart, "cc=");

	    /*
	     *	Seek and handle keywords=term(s) fields. - FM
	     */
	    extract_field(&keywords, searchpart, "keywords=");

	    if (keywords != NULL) {
		if (*keywords != '\0') {
		    SafeHTUnEscape(keywords);
		} else {
		    FREE(keywords);
		}
	    }

	    /*
	     *	Seek and handle body=foo fields. - FM
	     */
	    extract_body(&body, searchpart);

	    FREE(searchpart);
	}
    }

    if (convert_explorer(to_address)) {
	HTAlert(NO_ADDRESS_IN_MAILTO_URL);
	goto cancelled;
    }
    if (ccaddr != NULL) {
	if (convert_explorer(ccaddr)) {
	    FREE(ccaddr);
	}
    }

    /*
     *	Unescape the address and ccaddr fields. - FM
     */
    SafeHTUnEscape(to_address);
    if (ccaddr != NULL) {
	SafeHTUnEscape(ccaddr);
    }

    /*
     *	Set the default subject. - FM
     */
    if (isEmpty(default_subject) && !isEmpty(title)) {
	strncpy(default_subject, title, MAX_SUBJECT);
	default_subject[MAX_SUBJECT] = '\0';
    }

    /*
     *	Use ^G to cancel mailing of comment
     *	and don't let SIGINTs exit lynx.
     */
    signal(SIGINT, terminate_letter);

#if USE_VMS_MAILER
    if (isPMDF || !body) {
	/*
	 *  Put the X-URL and X-Mailer lines in the hdrfile
	 *  for PMDF or my_tmpfile for VMS MAIL. - FM
	 */
	fprintf((isPMDF ? hfd : fd),
		"X-URL: %s%s\n",
		isEmpty(filename) ? STR_MAILTO_URL : filename,
		isEmpty(filename) ? to_address : "");
	fprintf((isPMDF ? hfd : fd),
		"X-Mailer: %s, Version %s\n", LYNX_NAME, LYNX_VERSION);
#ifdef NO_ANONYMOUS_EMAIL
	if (!isPMDF) {
	    fprintf(fd, "\n");
	}
#endif /* NO_ANONYMOUS_EMAIL */
    }
#else /* Unix/DOS/Windows */
    /*
     *	Put the To: line in the header.
     */
#ifndef DOSPATH
    asprintf(&header, "To: %s\n", to_address);
    if (!header) {
	fprintf(stderr, "Out of memory, you lose!\n");
	exit(1);
    }
#endif

    /*
     *	Put the Mime-Version, Content-Type and
     *	Content-Transfer-Encoding in the header.
     *	This assumes that the same character set is used
     *	for composing the mail which is currently selected
     *	as display character set...
     *	Don't send a charset if we have a CJK character set
     *	selected, since it may not be appropriate for mail...
     *	Also don't use an unofficial "x-" charset.
     *	Also if the charset would be "us-ascii" (7-bit replacements
     *	selected, don't send any MIME headers. - kw
     */
    if (strncasecomp(LYCharSet_UC[current_char_set].MIMEname,
		     "us-ascii", 8) != 0) {
	StrAllocCat(header, "Mime-Version: 1.0\n");
	if (!LYHaveCJKCharacterSet &&
	    strncasecomp(LYCharSet_UC[current_char_set].MIMEname, "x-", 2)
	    != 0) {
	    HTSprintf(&header, "Content-Type: text/plain; charset=%s\n",
		    LYCharSet_UC[current_char_set].MIMEname);
	}
	StrAllocCat(header, "Content-Transfer-Encoding: 8bit\n");
    }
    /*
     *	Put the X-URL and X-Mailer lines in the header.
     */
    if (!isEmpty(filename)) {
	HTSprintf(&header, "X-URL: %s\n", filename);
    } else {
	HTSprintf(&header, "X-URL: mailto:%s\n", to_address);
    }
    HTSprintf(&header, "X-Mailer: %s, Version %s\n", LYNX_NAME, LYNX_VERSION);

    if (!isEmpty(refid)) {
	HTSprintf(&header, "In-Reply-To: <%s>\n", refid);
    }
#endif /* VMS */

    /*
     *	Clear the screen and inform the user.
     */
    LYclear();
    LYmove(2,0);
    scrollok(LYwin, TRUE);	/* Enable scrolling. */
    if (body)
	LYaddstr(SENDING_MESSAGE_WITH_BODY_TO);
    else
	LYaddstr(SENDING_COMMENT_TO);
    show_addresses(to_address);
    if (
#if USE_VMS_MAILER
	(isPMDF == TRUE) &&
#endif /* VMS */
	(cp = ccaddr) != NULL)
    {
	if (strchr(cp, ',') != NULL) {
	    LYaddstr(WITH_COPIES_TO);
	} else {
	    LYaddstr(WITH_COPY_TO);
	}
	show_addresses(ccaddr);
    }
    LYaddstr(CTRL_G_TO_CANCEL_SEND);

#if USE_VMS_MAILER
    if (isPMDF || !body) {
#endif /* USE_VMS_MAILER */
#ifndef NO_ANONYMOUS_EMAIL
    /*
     *	Get the user's personal name.
     */
    LYaddstr(ENTER_NAME_OR_BLANK);
#if USE_VMS_MAILER
    if (isPMDF) {
	label = "Personal_name: ";
    } else {
	label = "X-Personal_name: ";
    }
#else
    label = "X-Personal_Name: ";
#endif /* USE_VMS_MAILER */
    if (!header_prompt(label, &personal_name, LINESIZE)) {
	goto cancelled;
    }
    if (*personal_name) {
#if USE_VMS_MAILER
	fprintf((isPMDF ? hfd : fd), "%s: %s\n", label, personal_name);
#else
	HTSprintf(&header, "%s: %s\n", label, personal_name);
#endif /* VMS */
    }

    /*
     *	Get the user's return address.
     */
    LYaddstr(ENTER_MAIL_ADDRESS_OR_OTHER);
    LYaddstr(MEANS_TO_CONTACT_FOR_RESPONSE);
#if USE_VMS_MAILER
    if (isPMDF) {
	label = "From";
    } else {
	label = "X-From";
    }
#else
    label = "From";
#endif /* VMS */
    /* Add the personal mail address if there is one. */
    if (personal_mail_address)
	StrAllocCopy(from_address, personal_mail_address);
    if (!header_prompt(label, &from_address, LINESIZE)) {
	goto cancelled;
    }
#if USE_VMS_MAILER
    if (*from_address) {
	fprintf(isPMDF ? hfd : fd, "%s: %s\n", label, from_address);
    }
    if (!isPMDF) {
	fprintf(fd, "\n");
    }
#else
    HTSprintf(&header, "%s: %s\n", label, from_address);
#endif /* USE_VMS_MAILER */
#endif /* !NO_ANONYMOUS_EMAIL */
#if USE_VMS_MAILER
    }
#endif /* USE_VMS_MAILER */

    /*
     *	Get the subject line.
     */
    LYaddstr(ENTER_SUBJECT_LINE);
    label = "Subject";
    if (*default_subject) {
	StrAllocCopy(the_subject, default_subject);
    } else if (!isEmpty(filename)) {
	HTSprintf(&the_subject, "%s", filename);
    } else {
	HTSprintf(&the_subject, "mailto:%s", to_address);
    }
    if (!header_prompt(label, &the_subject, MAX_SUBJECT)) {
	goto cancelled;
    }

    /*
     *	Offer a CC line, if permitted. - FM
     */
    if (!LYNoCc) {
	LYaddstr(ENTER_ADDRESS_FOR_CC);
	LYaddstr(BLANK_FOR_NO_COPY);
	if (personal_mail_address)
	    StrAllocCopy(cc_address, personal_mail_address);
	if (!header_prompt("Cc", &cc_address, LINESIZE)) {
	    goto cancelled;
	}
	comma_append(&ccaddr, cc_address);
    }

#if !USE_VMS_MAILER
    HTSprintf(&header, "%s: %s\n", label, the_subject);
#if !CAN_PIPE_TO_MAILER
    if (*to_address) {
	HTSprintf(&header, "To: %s\n", to_address);
    }
#endif

    /*
    **	Add the Cc: header. - FM
    */
    if (!isEmpty(ccaddr)) {
	HTSprintf(&header, "Cc: %s\n", ccaddr);
    }

    /*
    **	Add the Keywords: header. - FM
    */
    if (!isEmpty(keywords)) {
	HTSprintf(&header, "Keywords: %s\n", keywords);
    }

    /*
     *	Terminate the header.
     */
    StrAllocCat(header, "\n");
    CTRACE((tfp,"**header==\n%s",header));
#endif /* !VMS */

    if (!no_editor && !isEmpty(editor)) {

	if (body) {
	    cp1 = body;
	    while((cp = strchr(cp1, '\n')) != NULL) {
		*cp++ = '\0';
		fprintf(fd, "%s\n", cp1);
		cp1 = cp;
	    }
	} else if (strcmp(HTLoadedDocumentURL(), "")) {
	    /*
	     *	Ask if the user wants to include the original message.
	     */
	    BOOLEAN is_preparsed = (BOOL) (LYPreparsedSource &&
				    HTisDocumentSource());
	    if (HTConfirm(is_preparsed
		? INC_PREPARSED_MSG_PROMPT
		: INC_ORIG_MSG_PROMPT) == YES) {
		print_wwwfile_to_fd(fd, (BOOL) !is_preparsed);
	    }
	}
	LYCloseTempFP(fd);	/* Close the tmpfile. */
	scrollok(LYwin,FALSE);	/* Stop scrolling.    */

	if (term_letter || LYCharIsINTERRUPT(c))
	    goto cleanup;

	/*
	 *  Spawn the users editor on the mail file
	 */
	edit_temporary_file(my_tmpfile, "", SPAWNING_EDITOR_FOR_MAIL);

    } else if (body) {
	/*
	 *  Let user review the body. - FM
	 */
	LYclear();
	LYmove(0,0);
	LYaddstr(REVIEW_MESSAGE_BODY);
	LYrefresh();
	cp1 = body;
	i = (LYlines - 5);
	while((cp = strchr(cp1, '\n')) != NULL) {
	    if (i <= 0) {
		LYaddstr(RETURN_TO_CONTINUE);
		LYrefresh();
		c = LYgetch();
		LYaddstr("\n");
		if (term_letter || LYCharIsINTERRUPT(c)) {
		    goto cancelled;
		}
		i = (LYlines - 2);
	    }
	    *cp++ = '\0';
	    fprintf(fd, "%s\n", cp1);
	    LYaddstr(cp1);
	    LYaddstr("\n");
	    cp1 = cp;
	    i--;
	}
	while (i >= 0) {
	    LYaddstr("\n");
	    i--;
	}
	LYrefresh();
	LYCloseTempFP(fd);	/* Close the tmpfile.	  */
	scrollok(LYwin,FALSE);	/* Stop scrolling.	  */

    } else {
	/*
	 *  Use the internal line editor for the message.
	 */
	LYaddstr(ENTER_MESSAGE_BELOW);
	LYaddstr(ENTER_PERIOD_WHEN_DONE_A);
	LYaddstr(ENTER_PERIOD_WHEN_DONE_B);
	LYaddstr("\n\n");
	LYrefresh();
	*user_input = '\0';
	if (LYgetstr(user_input, VISIBLE, sizeof(user_input), NORECALL) < 0 ||
	    term_letter || STREQ(user_input, ".")) {
	    goto cancelled;
	}

	while (!STREQ(user_input, ".") && !term_letter) {
	    LYaddstr("\n");
	    remove_tildes(user_input);
	    fprintf(fd, "%s\n", user_input);
	    *user_input = '\0';
	    if (LYgetstr(user_input, VISIBLE,
			 sizeof(user_input), NORECALL) < 0) {
		goto cancelled;
	    }
	}

	fprintf(fd, "\n");	/* Terminate the message. */
	LYCloseTempFP(fd);	/* Close the tmpfile.	  */
	scrollok(LYwin,FALSE);	/* Stop scrolling.	  */
    }

#if !USE_VMS_MAILER
    /*
     *	Ignore CTRL-C on this last question.
     */
    signal(SIGINT, SIG_IGN);
#endif /* !VMS */
    LYStatusLine = (LYlines - 1);
    c = HTConfirm (body ? SEND_MESSAGE_PROMPT : SEND_COMMENT_PROMPT);
    LYStatusLine = -1;
    if (c != YES) {
	LYclear();  /* clear the screen */
	goto cleanup;
    }
    if ((body == NULL && LynxSigFile != NULL) &&
	(fp = fopen(LynxSigFile, TXT_R)) != NULL) {
	LYStatusLine = (LYlines - 1);
	if (term_letter) {
	    _user_message(APPEND_SIG_FILE, LynxSigFile);
	    c = 0;
	} else {
	    char *msg = NULL;
	    HTSprintf0(&msg, APPEND_SIG_FILE, LynxSigFile);
	    c = HTConfirm(msg);
	    FREE(msg);
	}
	LYStatusLine = -1;
	if (c == YES) {
	    if ((fd = fopen(my_tmpfile, TXT_A)) != NULL) {
		char *buffer = NULL;
		fputs("-- \n", fd);
		while (LYSafeGets(&buffer, fp) != NULL) {
		    fputs(buffer, fd);
		}
		LYCloseOutput(fd);
		FREE(buffer);
	    }
	}
	LYCloseInput(fp);
    }
    LYclear();  /* Clear the screen. */

    /*
     *	Send the message.
     */
#if USE_VMS_MAILER
    /*
     *	Set the mail command. - FM
     */
    if (isPMDF) {
	/*
	 *  For PMDF, put any keywords and the subject
	 *  in the header file and close it. - FM
	 */
	if (!isEmpty(keywords)) {
	    fprintf(hfd, "Keywords: %s\n", keywords);
	}
	fprintf(hfd, "Subject: %s\n\n", the_subject);
	LYCloseTempFP(hfd);
	/*
	 *  Now set up the command. - FM
	 */
	HTSprintf0(&command, "%s %s %s,%s ",
		system_mail,
		system_mail_flags,
		hdrfile,
		my_tmpfile);
    } else {
	/*
	 *  For "generic" VMS MAIL, include the subject in the
	 *  command, and ignore any keywords to minimize risk
	 *  of them making the line too long or having problem
	 *  characters. - FM
	 */
	HTSprintf0(&command, "%s %s%s/subject=\"%s\" %s ",
		system_mail,
		system_mail_flags,
		(strncasecomp(system_mail, "MAIL", 4) ? "" : "/noself"),
		the_subject,
		my_tmpfile);
    }

    vms_append_addrs(&command, to_address, "");
    if (!isEmpty(ccaddr)) {
	vms_append_addrs(&command, ccaddr, "/CC");
    }

    stop_curses();
    printf("%s\n\n$ %s\n\n%s", SENDING_COMMENT, command, PLEASE_WAIT);
    LYSystem(command);	/* SENDING COMMENT (VMS) */
    FREE(command);
    LYSleepAlert();
    start_curses();
#else /* Unix/DOS/Windows */
    /*
     *	Send the tmpfile into sendmail.
     */
    _statusline(SENDING_YOUR_MSG);
#if CAN_PIPE_TO_MAILER
    signal(SIGINT, SIG_IGN);
    if ((fp = LYPipeToMailer()) == 0) {
	HTInfoMsg(CANCELLED);
    }
#else
    if ((fp = LYOpenTemp(tmpfile2, ".txt", "w")) == NULL) {
	HTAlert(MAILTO_URL_TEMPOPEN_FAILED);
    }
#endif /* CAN_PIPE_TO_MAILER */
    if (fp != 0) {
	fd = fopen(my_tmpfile, TXT_R);
	if (fd == NULL) {
	    HTInfoMsg(CANCELLED);
#if CAN_PIPE_TO_MAILER
	    pclose(fp);
#else
	    LYCloseTempFP(fp);
#endif /* CAN_PIPE_TO_MAILER */
	} else {
#if USE_BLAT_MAILER
	    if (!mail_is_blat)
		fputs(header, fp);
#else
	    fputs(header, fp);
#endif
	    while ((n = fread(buf, 1, sizeof(buf), fd)) != 0) {
		fwrite(buf, 1, n, fp);
	    }
#if CAN_PIPE_TO_MAILER
	    pclose(fp);
#else
	    LYCloseTempFP(fp);	/* Close the tmpfile. */
	    LYSendMailFile (
		to_address,
		tmpfile2,
		the_subject,
		ccaddr,
		SENDING_COMMENT);
	    LYRemoveTemp(tmpfile2);	/* Delete the tmpfile. */
#endif /* CAN_PIPE_TO_MAILER */
	    LYCloseInput(fd); /* Close the tmpfile. */
	}
    }
#endif /* USE_VMS_MAILER */
    goto cleanup;

    /*
     *	Come here to cleanup and exit.
     */
cancelled:
    HTInfoMsg(CANCELLED);
    LYCloseTempFP(fd);		/* Close the tmpfile.	*/
    scrollok(LYwin,FALSE);	/* Stop scrolling.	*/
cleanup:
    signal(SIGINT, cleanup_sig);
    term_letter = FALSE;

#if USE_VMS_MAILER
    while (LYRemoveTemp(my_tmpfile) == 0)
	;		 /* Delete the tmpfile(s). */
    if (isPMDF) {
	LYRemoveTemp(hdrfile); /* Delete the hdrfile. */
    }
#else
    FREE(header);
    LYRemoveTemp(my_tmpfile);  /* Delete the tmpfile. */
#endif /* VMS */

    FREE(from_address);
    FREE(the_subject);
    FREE(cc_address);
    FREE(to_address);
    FREE(ccaddr);
    FREE(keywords);
    FREE(body);
    return;
}

/*
 * Check that we have configured values for system mailer.
 */
PUBLIC BOOLEAN LYSystemMail NOARGS
{
    if (system_mail == 0 || !strcmp(system_mail, "unknown")) {
	HTAlert(gettext("No system mailer configured"));
	return FALSE;
    }
    return TRUE;
}
