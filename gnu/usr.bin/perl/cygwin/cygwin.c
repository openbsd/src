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
#include <mntent.h>
#include <alloca.h>
#include <dlfcn.h>

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
    char **a,*tmps,**argv; 
    STRLEN n_a; 

    if (sp<=mark)
        return -1;
    a=argv=(char**) alloca ((sp-mark+3)*sizeof (char*));

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
    char *s,*metachars = "$&*(){}[]'\";\\?>|<~`\n";
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

    Newx (PL_Argv,(s-cmd)/2+2,const char*);
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

    if ((RETVAL = cygwin32_winpid_to_pid(pid)) > 0) {
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
    int err;
    char *pathname, *buf;

    if (items < 1 || items > 2)
        Perl_croak(aTHX_ "Usage: Cygwin::win_to_posix_path(pathname, [absolute])");

    pathname = SvPV(ST(0), len);
    if (items == 2)
	absolute_flag = SvTRUE(ST(1));

    if (!len)
	Perl_croak(aTHX_ "can't convert empty path");
    buf = (char *) safemalloc (len + 260 + 1001);

    if (absolute_flag)
	err = cygwin_conv_to_full_posix_path(pathname, buf);
    else
	err = cygwin_conv_to_posix_path(pathname, buf);
    if (!err) {
	ST(0) = sv_2mortal(newSVpv(buf, 0));
	safefree(buf);
       XSRETURN(1);
    } else {
	safefree(buf);
	XSRETURN_UNDEF;
    }
}

XS(XS_Cygwin_posix_to_win_path)
{
    dXSARGS;
    int absolute_flag = 0;
    STRLEN len;
    int err;
    char *pathname, *buf;

    if (items < 1 || items > 2)
        Perl_croak(aTHX_ "Usage: Cygwin::posix_to_win_path(pathname, [absolute])");

    pathname = SvPV(ST(0), len);
    if (items == 2)
	absolute_flag = SvTRUE(ST(1));

    if (!len)
	Perl_croak(aTHX_ "can't convert empty path");
    buf = (char *) safemalloc(len + 260 + 1001);

    if (absolute_flag)
	err = cygwin_conv_to_full_win32_path(pathname, buf);
    else
	err = cygwin_conv_to_win32_path(pathname, buf);
    if (!err) {
	ST(0) = sv_2mortal(newSVpv(buf, 0));
	safefree(buf);
       XSRETURN(1);
    } else {
	safefree(buf);
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
    char flags[260];

    if (items != 1)
        Perl_croak(aTHX_ "Usage: Cygwin::mount_flags(mnt_dir|'/cygwin')");

    pathname = SvPV_nolen(ST(0));

    /* TODO: Check for cygdrive registry setting,
     *       and then use CW_GET_CYGDRIVE_INFO
     */
    if (!strcmp(pathname, "/cygdrive")) {
	char user[260];
	char system[260];
	char user_flags[260];
	char system_flags[260];

	cygwin_internal (CW_GET_CYGDRIVE_INFO, user, system, user_flags,
			 system_flags);

        if (strlen(user) > 0) {
            sprintf(flags, "%s,cygdrive,%s", user_flags, user);
        } else {
            sprintf(flags, "%s,cygdrive,%s", system_flags, system);
        }

	ST(0) = sv_2mortal(newSVpv(flags, 0));
	XSRETURN(1);

    } else {
	struct mntent *mnt;
	setmntent (0, 0);
	while ((mnt = getmntent (0))) {
	    if (!strcmp(pathname, mnt->mnt_dir)) {
		strcpy(flags, mnt->mnt_type);
		if (strlen(mnt->mnt_opts) > 0) {
		    strcat(flags, ",");
		    strcat(flags, mnt->mnt_opts);
		}
		break;
	    }
	}
	endmntent (0);
	ST(0) = sv_2mortal(newSVpv(flags, 0));
	XSRETURN(1);
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

void
init_os_extras(void)
{
    dTHX;
    char *file = __FILE__;
    void *handle;

    newXS("Cwd::cwd", Cygwin_cwd, file);
    newXSproto("Cygwin::winpid_to_pid", XS_Cygwin_winpid_to_pid, file, "$");
    newXSproto("Cygwin::pid_to_winpid", XS_Cygwin_pid_to_winpid, file, "$");
    newXSproto("Cygwin::win_to_posix_path", XS_Cygwin_win_to_posix_path, file, "$;$");
    newXSproto("Cygwin::posix_to_win_path", XS_Cygwin_posix_to_win_path, file, "$;$");
    newXSproto("Cygwin::mount_table", XS_Cygwin_mount_table, file, "");
    newXSproto("Cygwin::mount_flags", XS_Cygwin_mount_flags, file, "$");
    newXSproto("Cygwin::is_binmount", XS_Cygwin_is_binmount, file, "$");

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
