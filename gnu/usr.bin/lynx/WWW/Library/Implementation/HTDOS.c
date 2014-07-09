/*
 * $LynxId: HTDOS.c,v 1.40 2013/11/28 11:11:05 tom Exp $
 *							DOS specific routines
 */

#include <HTUtils.h>
#include <LYUtils.h>
#include <HTDOS.h>
#include <LYStrings.h>

#include <LYLeaks.h>

#ifdef _WINDOWS
#include <LYGlobalDefs.h>
#include <HTAlert.h>
#endif

/*
 * Make a copy of the source argument in the result, allowing some extra
 * space so we can append directly onto the result without reallocating.
 */
static char *copy_plus(char **result, const char *source)
{
    int length = (int) strlen(source);
    int extra = 10;
    int n;

    for (n = 0; n < length; ++n) {
	if (source[n] == ' ')
	    ++extra;
    }

    HTSprintf0(result, "%-*s", length + extra, source);
    (*result)[length] = 0;
    return (*result);
}

/* PUBLIC							HTDOS_wwwName()
 *		CONVERTS DOS Name into WWW Name
 * ON ENTRY:
 *	dosname		DOS file specification (NO NODE)
 *
 * ON EXIT:
 *	returns		WWW file specification
 *
 */
const char *HTDOS_wwwName(const char *dosname)
{
    static char *wwwname = NULL;
    char *cp_url = copy_plus(&wwwname, dosname);
    int wwwname_len;
    char ch;

    while ((ch = *dosname) != '\0') {
	switch (ch) {
	case '\\':
	    /* convert dos backslash to unix-style */
	    *cp_url++ = '/';
	    break;
	case ' ':
	    *cp_url++ = '%';
	    *cp_url++ = '2';
	    *cp_url++ = '0';
	    break;
	default:
	    *cp_url++ = ch;
	    break;
	}
	dosname++;
    }
    *cp_url = '\0';

    wwwname_len = (int) strlen(wwwname);
    if (wwwname_len > 1)
	cp_url--;		/* point last char */

    if (wwwname_len > 3 && *cp_url == '/') {
	cp_url++;
	*cp_url = '\0';
    }
    return (wwwname);
}

/*
 * Convert slashes from Unix to DOS
 */
char *HTDOS_slashes(char *path)
{
    char *s;

    for (s = path; *s != '\0'; ++s) {
	if (*s == '/') {
	    *s = '\\';
	}
    }
    return path;
}

/* PUBLIC							HTDOS_name()
 *		CONVERTS WWW name into a DOS name
 * ON ENTRY:
 *	wwwname		WWW file name
 *
 * ON EXIT:
 *	returns		DOS file specification
 */
char *HTDOS_name(const char *wwwname)
{
    static char *result = NULL;
    int joe;

#if defined(SH_EX)		/* 2000/03/07 (Tue) 18:32:42 */
    if (unsafe_filename(wwwname)) {
	HTUserMsg2("unsafe filename : %s", wwwname);
	copy_plus(&result, "BAD_LOCAL_FILE_NAME");
    } else {
	copy_plus(&result, wwwname);
    }
#else
    copy_plus(&result, wwwname);
#endif
#ifdef __DJGPP__
    if (result[0] == '/'
	&& result[1] == 'd'
	&& result[2] == 'e'
	&& result[3] == 'v'
	&& result[4] == '/'
	&& isalpha(result[5])) {
	return (result);
    }
#endif /* __DJGPP__ */

    (void) HTDOS_slashes(result);

    /* pesky leading slash, rudiment from file://localhost/  */
    /* the rest of path may be with or without drive letter  */
    if ((result[1] != '\\') && (result[0] == '\\')) {
	for (joe = 0; (result[joe] = result[joe + 1]) != 0; joe++) ;
    }
    /* convert '|' after the drive letter to ':' */
    if (isalpha(UCH(result[0])) && result[1] == '|') {
	result[1] = ':';
    }
#ifdef _WINDOWS			/* 1998/04/02 (Thu) 08:59:48 */
    if (LYLastPathSep(result) != NULL
	&& !LYIsDosDrive(result)) {
	char temp_buff[LY_MAXPATH];

	sprintf(temp_buff, "%.3s\\%.*s", windows_drive,
		(int) (sizeof(temp_buff) - 5), result);
	StrAllocCopy(result, temp_buff);
    }
#endif
    /*
     * If we have only a device, add a trailing slash.  Otherwise it just
     * refers to the current directory on the given device.
     */
    if (LYLastPathSep(result) == NULL
	&& LYIsDosDrive(result))
	LYAddPathSep0(result);

    CTRACE((tfp, "HTDOS_name changed `%s' to `%s'\n", wwwname, result));
    return (result);
}

#ifdef WIN_EX
char *HTDOS_short_name(const char *path)
{
    static char sbuf[LY_MAXPATH];
    char *ret;
    DWORD r;

    if (StrChr(path, '/'))
	path = HTDOS_name(path);
    r = GetShortPathName(path, sbuf, sizeof sbuf);
    if (r >= sizeof(sbuf) || r == 0) {
	ret = LYStrNCpy(sbuf, path, sizeof(sbuf));
    } else {
	ret = sbuf;
    }
    return ret;
}
#endif

#if defined(DJGPP)
/*
 * Poll tcp/ip lib and yield to DPMI-host while nothing in
 * keyboard buffer (head = tail) (simpler than kbhit).
 * This is required to be able to finish off dead sockets,
 * answer pings etc.
 */
#include <pc.h>
#include <dpmi.h>
#include <libc/farptrgs.h>
#include <go32.h>

void djgpp_idle_loop(void)
{
    while (_farpeekw(_dos_ds, 0x41a) == _farpeekw(_dos_ds, 0x41c)) {
	tcp_tick(NULL);
	__dpmi_yield();
#if defined(USE_SLANG)
	if (SLang_input_pending(1))
	    break;
#endif
    }
}

/* PUBLIC       getxkey()
 *              Replaces libc's getxkey() with polling of tcp/ip
 *              library (WatTcp or Watt-32). *
 * ON EXIT:
 *      returns extended keypress.
 */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */

int getxkey(void)
{
#if defined(DJGPP_KEYHANDLER)
    __dpmi_regs r;

    djgpp_idle_loop();

    r.h.ah = 0x10;
    __dpmi_int(0x16, &r);

    if (r.h.al == 0x00)
	return 0x0100 | r.h.ah;
    if (r.h.al == 0xe0)
	return 0x0200 | r.h.ah;
    return r.h.al;

#elif defined(USE_SLANG)
    djgpp_idle_loop();
    return SLkp_getkey();
#else
    /* PDcurses uses myGetChar() in LYString.c */
#endif
}
#endif /* DJGPP */
