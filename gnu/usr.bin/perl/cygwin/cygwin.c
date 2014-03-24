/*
 * Cygwin extras
 */

#include "EXTERN.h"
#include "perl.h"
#undef USE_DYNAMIC_LOADING
#include "XSUB.h"

#include <unistd.h>
#include <process.h>
#include <sys/cygwin.h>
#include <cygwin/version.h>
#include <mntent.h>
#include <alloca.h>
#include <dlfcn.h>
#if (CYGWIN_VERSION_API_MINOR >= 181)
#include <wchar.h>
#endif

/*
 * pp_system() implemented via spawn()
 * - more efficient and useful when embedding Perl in non-Cygwin apps
 * - code mostly borrowed from djgpp.c
 */
static int
do_spawnvp (const char *path, const char * const *argv)
{
    dTHX;
    Sigsave_t ihand,qhand;
    int childpid, result, status;

    rsignal_save(SIGINT, (Sighandler_t) SIG_IGN, &ihand);
    rsignal_save(SIGQUIT, (Sighandler_t) SIG_IGN, &qhand);
    childpid = spawnvp(_P_NOWAIT,path,argv);
    if (childpid < 0) {
	status = -1;
	if(ckWARN(WARN_EXEC))
	    Perl_warner(aTHX_ packWARN(WARN_EXEC),"Can't spawn \"%s\": %s",
		    path,Strerror (errno));
    } else {
	do {
	    result = wait4pid(childpid, &status, 0);
	} while (result == -1 && errno == EINTR);
	if(result < 0)
	    status = -1;
    }
    (void)rsignal_restore(SIGINT, &ihand);
    (void)rsignal_restore(SIGQUIT, &qhand);
    return status;
}

int
do_aspawn (SV *really, void **mark, void **sp)
{
    dTHX;
    int  rc;
    char const **a;
    char *tmps,**argv;
    STRLEN n_a;

    if (sp<=mark)
        return -1;
    argv=(char**) alloca ((sp-mark+3)*sizeof (char*));
    a=(char const **)argv;

    while (++mark <= sp)
        if (*mark)
            *a++ = SvPVx((SV *)*mark, n_a);
        else
            *a++ = "";
    *a = (char*)NULL;

    if (argv[0][0] != '/' && argv[0][0] != '\\'
        && !(argv[0][0] && argv[0][1] == ':'
        && (argv[0][2] == '/' || argv[0][2] != '\\'))
     ) /* will swawnvp use PATH? */
         TAINT_ENV();	/* testing IFS here is overkill, probably */

    if (really && *(tmps = SvPV(really, n_a)))
        rc=do_spawnvp (tmps,(const char * const *)argv);
    else
        rc=do_spawnvp (argv[0],(const char *const *)argv);

    return rc;
}

int
do_spawn (char *cmd)
{
    dTHX;
    char const **a;
    char *s;
    char const *metachars = "$&*(){}[]'\";\\?>|<~`\n";
    const char *command[4];

    while (*cmd && isSPACE(*cmd))
	cmd++;

    if (strnEQ (cmd,"/bin/sh",7) && isSPACE (cmd[7]))
        cmd+=5;

    /* save an extra exec if possible */
    /* see if there are shell metacharacters in it */
    if (strstr (cmd,"..."))
	goto doshell;
    if (*cmd=='.' && isSPACE (cmd[1]))
	goto doshell;
    if (strnEQ (cmd,"exec",4) && isSPACE (cmd[4]))
	goto doshell;
    for (s=cmd; *s && isALPHA (*s); s++) ;	/* catch VAR=val gizmo */
	if (*s=='=')
	    goto doshell;

    for (s=cmd; *s; s++)
	if (strchr (metachars,*s))
	{
	    if (*s=='\n' && s[1]=='\0')
	    {
		*s='\0';
		break;
	    }
	doshell:
	    command[0] = "sh";
	    command[1] = "-c";
	    command[2] = cmd;
	    command[3] = NULL;

	    return do_spawnvp("sh",command);
	}

    Newx (PL_Argv, (s-cmd)/2+2, const char*);
    PL_Cmd=savepvn (cmd,s-cmd);
    a=PL_Argv;
    for (s=PL_Cmd; *s;) {
	while (*s && isSPACE (*s)) s++;
	if (*s)
	    *(a++)=s;
	while (*s && !isSPACE (*s)) s++;
	if (*s)
	    *s++='\0';
    }
    *a = (char*)NULL;
    if (!PL_Argv[0])
        return -1;

    return do_spawnvp(PL_Argv[0],(const char * const *)PL_Argv);
}

#if (CYGWIN_VERSION_API_MINOR >= 181)
char*
wide_to_utf8(const wchar_t *wbuf)
{
    char *buf;
    int wlen = 0;
    char *oldlocale = setlocale(LC_CTYPE, NULL);
    setlocale(LC_CTYPE, "utf-8");

    /* uvuni_to_utf8(buf, chr) or Encoding::_bytes_to_utf8(sv, "UCS-2BE"); */
    wlen = wcsrtombs(NULL, (const wchar_t **)&wbuf, wlen, NULL);
    buf = (char *) safemalloc(wlen+1);
    wcsrtombs(buf, (const wchar_t **)&wbuf, wlen, NULL);

    if (oldlocale) setlocale(LC_CTYPE, oldlocale);
    else setlocale(LC_CTYPE, "C");
    return buf;
}

wchar_t*
utf8_to_wide(const char *buf)
{
    wchar_t *wbuf;
    mbstate_t mbs;
    char *oldlocale = setlocale(LC_CTYPE, NULL);
    int wlen = sizeof(wchar_t)*strlen(buf);

    setlocale(LC_CTYPE, "utf-8");
    wbuf = (wchar_t *) safemalloc(wlen);
    /* utf8_to_uvuni_buf(pathname, pathname + wlen, wpath) or Encoding::_utf8_to_bytes(sv, "UCS-2BE"); */
    wlen = mbsrtowcs(wbuf, (const char**)&buf, wlen, &mbs);

    if (oldlocale) setlocale(LC_CTYPE, oldlocale);
    else setlocale(LC_CTYPE, "C");
    return wbuf;
}
#endif /* cygwin 1.7 */

/* see also Cwd.pm */
XS(Cygwin_cwd)
{
    dXSARGS;
    char *cwd;

    /* See http://rt.perl.org/rt3/Ticket/Display.html?id=38628 
       There is Cwd->cwd() usage in the wild, and previous versions didn't die.
     */
    if(items > 1)
	Perl_croak(aTHX_ "Usage: Cwd::cwd()");
    if((cwd = getcwd(NULL, -1))) {
	ST(0) = sv_2mortal(newSVpv(cwd, 0));
	free(cwd);
#ifndef INCOMPLETE_TAINTS
	SvTAINTED_on(ST(0));
#endif
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

XS(XS_Cygwin_pid_to_winpid)
{
    dXSARGS;
    dXSTARG;
    pid_t pid, RETVAL;

    if (items != 1)
        Perl_croak(aTHX_ "Usage: Cygwin::pid_to_winpid(pid)");

    pid = (pid_t)SvIV(ST(0));

    if ((RETVAL = cygwin_internal(CW_CYGWIN_PID_TO_WINPID, pid)) > 0) {
	XSprePUSH; PUSHi((IV)RETVAL);
        XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

XS(XS_Cygwin_winpid_to_pid)
{
    dXSARGS;
    dXSTARG;
    pid_t pid, RETVAL;

    if (items != 1)
        Perl_croak(aTHX_ "Usage: Cygwin::winpid_to_pid(pid)");

    pid = (pid_t)SvIV(ST(0));

#if (CYGWIN_VERSION_API_MINOR >= 181)
    RETVAL = cygwin_winpid_to_pid(pid);
#else
    RETVAL = cygwin32_winpid_to_pid(pid);
#endif
    if (RETVAL > 0) {
        XSprePUSH; PUSHi((IV)RETVAL);
        XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

XS(XS_Cygwin_win_to_posix_path)

{
    dXSARGS;
    int absolute_flag = 0;
    STRLEN len;
    int err = 0;
    char *src_path;
    char *posix_path;
    int isutf8 = 0;

    if (items < 1 || items > 2)
        Perl_croak(aTHX_ "Usage: Cygwin::win_to_posix_path(pathname, [absolute])");

    src_path = SvPV(ST(0), len);
    if (items == 2)
	absolute_flag = SvTRUE(ST(1));

    if (!len)
	Perl_croak(aTHX_ "can't convert empty path");
    isutf8 = SvUTF8(ST(0));

#if (CYGWIN_VERSION_API_MINOR >= 181)
    /* Check utf8 flag and use wide api then.
       Size calculation: On overflow let cygwin_conv_path calculate the final size.
     */
    if (isutf8) {
	int what = absolute_flag ? CCP_WIN_W_TO_POSIX : CCP_WIN_W_TO_POSIX | CCP_RELATIVE;
	int wlen = sizeof(wchar_t)*(len + 260 + 1001);
	wchar_t *wpath = (wchar_t *) safemalloc(sizeof(wchar_t)*len);
	wchar_t *wbuf = (wchar_t *) safemalloc(wlen);
	if (!IN_BYTES) {
	    mbstate_t mbs;
            char *oldlocale = setlocale(LC_CTYPE, NULL);
            setlocale(LC_CTYPE, "utf-8");
	    /* utf8_to_uvuni_buf(src_path, src_path + wlen, wpath) or Encoding::_utf8_to_bytes(sv, "UCS-2BE"); */
	    wlen = mbsrtowcs(wpath, (const char**)&src_path, wlen, &mbs);
	    if (wlen > 0)
		err = cygwin_conv_path(what, wpath, wbuf, wlen);
            if (oldlocale) setlocale(LC_CTYPE, oldlocale);
            else setlocale(LC_CTYPE, "C");
	} else { /* use bytes; assume already ucs-2 encoded bytestream */
	    err = cygwin_conv_path(what, src_path, wbuf, wlen);
	}
	if (err == ENOSPC) { /* our space assumption was wrong, not enough space */
	    int newlen = cygwin_conv_path(what, wpath, wbuf, 0);
	    wbuf = (wchar_t *) realloc(&wbuf, newlen);
	    err = cygwin_conv_path(what, wpath, wbuf, newlen);
	    wlen = newlen;
	}
	/* utf16_to_utf8(*p, *d, bytlen, *newlen) */
	posix_path = (char *) safemalloc(wlen*3);
	Perl_utf16_to_utf8(aTHX_ (U8*)&wpath, (U8*)posix_path, (I32)wlen*2, (I32*)&len);
	/*
	wlen = wcsrtombs(NULL, (const wchar_t **)&wbuf, wlen, NULL);
	posix_path = (char *) safemalloc(wlen+1);
	wcsrtombs(posix_path, (const wchar_t **)&wbuf, wlen, NULL);
	*/
    } else {
	int what = absolute_flag ? CCP_WIN_A_TO_POSIX : CCP_WIN_A_TO_POSIX | CCP_RELATIVE;
	posix_path = (char *) safemalloc (len + 260 + 1001);
	err = cygwin_conv_path(what, src_path, posix_path, len + 260 + 1001);
	if (err == ENOSPC) { /* our space assumption was wrong, not enough space */
	    int newlen = cygwin_conv_path(what, src_path, posix_path, 0);
	    posix_path = (char *) realloc(&posix_path, newlen);
	    err = cygwin_conv_path(what, src_path, posix_path, newlen);
	}
    }
#else
    posix_path = (char *) safemalloc (len + 260 + 1001);
    if (absolute_flag)
	err = cygwin_conv_to_full_posix_path(src_path, posix_path);
    else
	err = cygwin_conv_to_posix_path(src_path, posix_path);
#endif
    if (!err) {
	EXTEND(SP, 1);
	ST(0) = sv_2mortal(newSVpv(posix_path, 0));
	if (isutf8) { /* src was utf-8, so result should also */
	    /* TODO: convert ANSI (local windows encoding) to utf-8 on cygwin-1.5 */
	    SvUTF8_on(ST(0));
	}
	safefree(posix_path);
        XSRETURN(1);
    } else {
	safefree(posix_path);
	XSRETURN_UNDEF;
    }
}

XS(XS_Cygwin_posix_to_win_path)
{
    dXSARGS;
    int absolute_flag = 0;
    STRLEN len;
    int err = 0;
    char *src_path, *win_path;
    int isutf8 = 0;

    if (items < 1 || items > 2)
        Perl_croak(aTHX_ "Usage: Cygwin::posix_to_win_path(pathname, [absolute])");

    src_path = SvPVx(ST(0), len);
    if (items == 2)
	absolute_flag = SvTRUE(ST(1));

    if (!len)
	Perl_croak(aTHX_ "can't convert empty path");
    isutf8 = SvUTF8(ST(0));
#if (CYGWIN_VERSION_API_MINOR >= 181)
    /* Check utf8 flag and use wide api then.
       Size calculation: On overflow let cygwin_conv_path calculate the final size.
     */
    if (isutf8) {
	int what = absolute_flag ? CCP_POSIX_TO_WIN_W : CCP_POSIX_TO_WIN_W | CCP_RELATIVE;
	int wlen = sizeof(wchar_t)*(len + 260 + 1001);
	wchar_t *wpath = (wchar_t *) safemalloc(sizeof(wchar_t)*len);
	wchar_t *wbuf = (wchar_t *) safemalloc(wlen);
	char *oldlocale = setlocale(LC_CTYPE, NULL);
	setlocale(LC_CTYPE, "utf-8");
	if (!IN_BYTES) {
	    mbstate_t mbs;
	    /* utf8_to_uvuni_buf(src_path, src_path + wlen, wpath) or Encoding::_utf8_to_bytes(sv, "UCS-2BE"); */
	    wlen = mbsrtowcs(wpath, (const char**)&src_path, wlen, &mbs);
	    if (wlen > 0)
		err = cygwin_conv_path(what, wpath, wbuf, wlen);
	} else { /* use bytes; assume already ucs-2 encoded bytestream */
	    err = cygwin_conv_path(what, src_path, wbuf, wlen);
	}
	if (err == ENOSPC) { /* our space assumption was wrong, not enough space */
	    int newlen = cygwin_conv_path(what, wpath, wbuf, 0);
	    wbuf = (wchar_t *) realloc(&wbuf, newlen);
	    err = cygwin_conv_path(what, wpath, wbuf, newlen);
	    wlen = newlen;
	}
	/* also see utf8.c: Perl_utf16_to_utf8() or Encoding::_bytes_to_utf8(sv, "UCS-2BE"); */
	wlen = wcsrtombs(NULL, (const wchar_t **)&wbuf, wlen, NULL);
	win_path = (char *) safemalloc(wlen+1);
	wcsrtombs(win_path, (const wchar_t **)&wbuf, wlen, NULL);
	if (oldlocale) setlocale(LC_CTYPE, oldlocale);
	else setlocale(LC_CTYPE, "C");
    } else {
	int what = absolute_flag ? CCP_POSIX_TO_WIN_A : CCP_POSIX_TO_WIN_A | CCP_RELATIVE;
	win_path = (char *) safemalloc(len + 260 + 1001);
	err = cygwin_conv_path(what, src_path, win_path, len + 260 + 1001);
	if (err == ENOSPC) { /* our space assumption was wrong, not enough space */
	    int newlen = cygwin_conv_path(what, src_path, win_path, 0);
	    win_path = (char *) realloc(&win_path, newlen);
	    err = cygwin_conv_path(what, src_path, win_path, newlen);
	}
    }
#else
    if (isutf8)
	Perl_warn(aTHX_ "can't convert utf8 path");
    win_path = (char *) safemalloc(len + 260 + 1001);
    if (absolute_flag)
	err = cygwin_conv_to_full_win32_path(src_path, win_path);
    else
	err = cygwin_conv_to_win32_path(src_path, win_path);
#endif
    if (!err) {
	EXTEND(SP, 1);
	ST(0) = sv_2mortal(newSVpv(win_path, 0));
	if (isutf8) {
	    SvUTF8_on(ST(0));
	}
	safefree(win_path);
	XSRETURN(1);
    } else {
	safefree(win_path);
	XSRETURN_UNDEF;
    }
}

XS(XS_Cygwin_mount_table)
{
    dXSARGS;
    struct mntent *mnt;

    if (items != 0)
        Perl_croak(aTHX_ "Usage: Cygwin::mount_table");
    /* => array of [mnt_dir mnt_fsname mnt_type mnt_opts] */

    setmntent (0, 0);
    while ((mnt = getmntent (0))) {
	AV* av = newAV();
	av_push(av, newSVpvn(mnt->mnt_dir, strlen(mnt->mnt_dir)));
	av_push(av, newSVpvn(mnt->mnt_fsname, strlen(mnt->mnt_fsname)));
	av_push(av, newSVpvn(mnt->mnt_type, strlen(mnt->mnt_type)));
	av_push(av, newSVpvn(mnt->mnt_opts, strlen(mnt->mnt_opts)));
	XPUSHs(sv_2mortal(newRV_noinc((SV*)av)));
    }
    endmntent (0);
    PUTBACK;
}

XS(XS_Cygwin_mount_flags)
{
    dXSARGS;
    char *pathname;
    char flags[PATH_MAX];
    flags[0] = '\0';

    if (items != 1)
        Perl_croak(aTHX_ "Usage: Cygwin::mount_flags( mnt_dir | '/cygdrive' )");

    pathname = SvPV_nolen(ST(0));

    if (!strcmp(pathname, "/cygdrive")) {
	char user[PATH_MAX];
	char system[PATH_MAX];
	char user_flags[PATH_MAX];
	char system_flags[PATH_MAX];

	cygwin_internal (CW_GET_CYGDRIVE_INFO, user, system,
			 user_flags, system_flags);

        if (strlen(user) > 0) {
            sprintf(flags, "%s,cygdrive,%s", user_flags, user);
        } else {
            sprintf(flags, "%s,cygdrive,%s", system_flags, system);
        }

	ST(0) = sv_2mortal(newSVpv(flags, 0));
	XSRETURN(1);

    } else {
	struct mntent *mnt;
	int found = 0;
	setmntent (0, 0);
	while ((mnt = getmntent (0))) {
	    if (!strcmp(pathname, mnt->mnt_dir)) {
		strcpy(flags, mnt->mnt_type);
		if (strlen(mnt->mnt_opts) > 0) {
		    strcat(flags, ",");
		    strcat(flags, mnt->mnt_opts);
		}
		found++;
		break;
	    }
	}
	endmntent (0);

	/* Check if arg is the current volume moint point if not default,
	 * and then use CW_GET_CYGDRIVE_INFO also.
	 */
	if (!found) {
	    char user[PATH_MAX];
	    char system[PATH_MAX];
	    char user_flags[PATH_MAX];
	    char system_flags[PATH_MAX];

	    cygwin_internal (CW_GET_CYGDRIVE_INFO, user, system,
			     user_flags, system_flags);

	    if (strlen(user) > 0) {
		if (strcmp(user,pathname)) {
		    sprintf(flags, "%s,cygdrive,%s", user_flags, user);
		    found++;
		}
	    } else {
		if (strcmp(user,pathname)) {
		    sprintf(flags, "%s,cygdrive,%s", system_flags, system);
		    found++;
		}
	    }
	}
	if (found) {
	    ST(0) = sv_2mortal(newSVpv(flags, 0));
	    XSRETURN(1);
	} else {
	    XSRETURN_UNDEF;
	}
    }
}

XS(XS_Cygwin_is_binmount)
{
    dXSARGS;
    char *pathname;

    if (items != 1)
        Perl_croak(aTHX_ "Usage: Cygwin::is_binmount(pathname)");

    pathname = SvPV_nolen(ST(0));

    ST(0) = boolSV(cygwin_internal(CW_GET_BINMODE, pathname));
    XSRETURN(1);
}

XS(XS_Cygwin_sync_winenv){ cygwin_internal(CW_SYNC_WINENV); }

void
init_os_extras(void)
{
    dTHX;
    char const *file = __FILE__;
    void *handle;

    newXS("Cwd::cwd", Cygwin_cwd, file);
    newXSproto("Cygwin::winpid_to_pid", XS_Cygwin_winpid_to_pid, file, "$");
    newXSproto("Cygwin::pid_to_winpid", XS_Cygwin_pid_to_winpid, file, "$");
    newXSproto("Cygwin::win_to_posix_path", XS_Cygwin_win_to_posix_path, file, "$;$");
    newXSproto("Cygwin::posix_to_win_path", XS_Cygwin_posix_to_win_path, file, "$;$");
    newXSproto("Cygwin::mount_table", XS_Cygwin_mount_table, file, "");
    newXSproto("Cygwin::mount_flags", XS_Cygwin_mount_flags, file, "$");
    newXSproto("Cygwin::is_binmount", XS_Cygwin_is_binmount, file, "$");
    newXS("Cygwin::sync_winenv", XS_Cygwin_sync_winenv, file);

    /* Initialize Win32CORE if it has been statically linked. */
    handle = dlopen(NULL, RTLD_LAZY);
    if (handle) {
        void (*pfn_init)(pTHX);
        pfn_init = (void (*)(pTHX))dlsym(handle, "init_Win32CORE");
        if (pfn_init)
            pfn_init(aTHX);
        dlclose(handle);
    }
}
