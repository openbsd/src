#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <process.h>

static int
not_here(s)
char *s;
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static unsigned long
constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    if (name[0] == 'P' && name[1] == '_') {
	if (strEQ(name, "P_BACKGROUND"))
#ifdef P_BACKGROUND
	    return P_BACKGROUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DEBUG"))
#ifdef P_DEBUG
	    return P_DEBUG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DEFAULT"))
#ifdef P_DEFAULT
	    return P_DEFAULT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_DETACH"))
#ifdef P_DETACH
	    return P_DETACH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_FOREGROUND"))
#ifdef P_FOREGROUND
	    return P_FOREGROUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_FULLSCREEN"))
#ifdef P_FULLSCREEN
	    return P_FULLSCREEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_MAXIMIZE"))
#ifdef P_MAXIMIZE
	    return P_MAXIMIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_MINIMIZE"))
#ifdef P_MINIMIZE
	    return P_MINIMIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOCLOSE"))
#ifdef P_NOCLOSE
	    return P_NOCLOSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOSESSION"))
#ifdef P_NOSESSION
	    return P_NOSESSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_NOWAIT"))
#ifdef P_NOWAIT
	    return P_NOWAIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_OVERLAY"))
#ifdef P_OVERLAY
	    return P_OVERLAY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_PM"))
#ifdef P_PM
	    return P_PM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_QUOTE"))
#ifdef P_QUOTE
	    return P_QUOTE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_SESSION"))
#ifdef P_SESSION
	    return P_SESSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_TILDE"))
#ifdef P_TILDE
	    return P_TILDE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_UNRELATED"))
#ifdef P_UNRELATED
	    return P_UNRELATED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_WAIT"))
#ifdef P_WAIT
	    return P_WAIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "P_WINDOWED"))
#ifdef P_WINDOWED
	    return P_WINDOWED;
#else
	    goto not_there;
#endif
    }

    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}


MODULE = OS2::Process		PACKAGE = OS2::Process


unsigned long
constant(name,arg)
	char *		name
	int		arg

