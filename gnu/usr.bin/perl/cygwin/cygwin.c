/*
 * Cygwin extras
 */

#include "EXTERN.h"
#include "perl.h"
#undef USE_DYNAMIC_LOADING
#include "XSUB.h"

#include <unistd.h>
#include <process.h>

/*
 * pp_system() implemented via spawn()
 * - more efficient and useful when embedding Perl in non-Cygwin apps
 * - code mostly borrowed from djgpp.c
 */
static int
do_spawnvp (const char *path, const char * const *argv)
{
    dTHXo;
    Sigsave_t ihand,qhand;
    int childpid, result, status;

    rsignal_save(SIGINT, SIG_IGN, &ihand);
    rsignal_save(SIGQUIT, SIG_IGN, &qhand);
    childpid = spawnvp(_P_NOWAIT,path,argv);
    if (childpid < 0) {
	status = -1;
	if(ckWARN(WARN_EXEC))
	    Perl_warner(aTHX_ WARN_EXEC,"Can't spawn \"%s\": %s",
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
    dTHXo;
    int  rc;
    char **a,*tmps,**argv; 
    STRLEN n_a; 

    if (sp<=mark)
        return -1;
    a=argv=(char**) alloca ((sp-mark+3)*sizeof (char*));

    while (++mark <= sp)
        if (*mark)
            *a++ = SvPVx(*mark, n_a);
        else
            *a++ = "";
    *a = Nullch;

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
    dTHXo;
    char **a,*s,*metachars = "$&*(){}[]'\";\\?>|<~`\n";
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

    New (1303,PL_Argv,(s-cmd)/2+2,char*);
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
    *a=Nullch;
    if (!PL_Argv[0])
        return -1;

    return do_spawnvp(PL_Argv[0],(const char * const *)PL_Argv);
}

/* see also Cwd.pm */
static
XS(Cygwin_cwd)
{
    dXSARGS;
    char *cwd;

    if(items != 0)
	Perl_croak(aTHX_ "Usage: Cwd::cwd()");
    if((cwd = getcwd(NULL, -1))) {
	ST(0) = sv_2mortal(newSVpv(cwd, 0));
	safesysfree(cwd);
	XSRETURN(1);
    }
    XSRETURN_UNDEF;
}

void
init_os_extras(void)
{
    char *file = __FILE__;
    dTHX;

    newXS("Cwd::cwd", Cygwin_cwd, file);
}
