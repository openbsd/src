/*	$NetBSD: common.h,v 1.2 1995/07/25 22:20:26 gwr Exp $	*/

/* RE_SID: @(%)/usr/dosnfs/shades_SCCS/unix/pcnfsd/v2/src/SCCS/s.common.h 1.3 91/12/17 14:32:05 SMI */
/*
**=====================================================================
** Copyright (c) 1986,1987,1988,1989,1990,1991 by Sun Microsystems, Inc.
**
**         D I S C L A I M E R   S E C T I O N ,   E T C .
**
** pcnfsd is copyrighted software, but is freely licensed. This
** means that you are free to redistribute it, modify it, ship it
** in binary with your system, whatever, provided:
**
** - you leave the Sun copyright notice in the source code
** - you make clear what changes you have introduced and do
**   not represent them as being supported by Sun.
** - you do not charge money for the source code (unlikely, given
**   its free availability)
**
** If you make changes to this software, we ask that you do so in
** a way which allows you to build either the "standard" version or
** your custom version from a single source file. Test it, lint
** it (it won't lint 100%, very little does, and there are bugs in
** some versions of lint :-), and send it back to Sun via email
** so that we can roll it into the source base and redistribute
** it. We'll try to make sure your contributions are acknowledged
** in the source, but after all these years it's getting hard to
** remember who did what.
**
** The main contributors have been (in no special order):
**
** Glen Eustace <G.Eustace@massey.ac.nz>
**    user name caching for b-i-g password files
** Paul Emerson <paul@sdgsun.uucp>
**    cleaning up Interactive 386/ix handling, fixing the lp
**    interface, and generally tidying up the sources
** Keith Ericson <keithe@sail.labs.tek.com>
**    more 386/ix fixes
** Jeff Stearns <jeff@tc.fluke.com>
**    setuid/setgid for lpr
** Peter Van Campen <petervc@sci.kun.nl>
**    fixing setuid/gid stuff, syslog
** Ted Nolan <ted@usasoc.soc.mil>
**    /usr/adm/wtmp, other security suggestions
**
** Thanks to everyone who has contributed.
**
**    Geoff Arnold, PC-NFS architect <geoff@East.Sun.COM>
**=====================================================================
*/
/*
**=====================================================================
**             C U S T O M I Z A T I O N   S E C T I O N              *
**                                                                    *
** You should not uncomment these #defines in this version of pcnfsd  *
** Instead you should edit the makefile CDEFS variable.               *
**                                                                    *
**=====================================================================
*/

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to enable the use of a 
** shadow password file
**---------------------------------------------------------------------
**/

/* #define SHADOW_SUPPORT */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to enable the logging 
** of authentication requests to /usr/adm/wtmp
**---------------------------------------------------------------------
**/

/* #define WTMP */

/*
**------------------------------------------------------------------------
** Define (via Makefile) the following symbol conform to Interactive
** System's 2.0
**------------------------------------------------------------------------
*/

/* #define ISC_2_0 */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to use a cache of recently-used
** user names. This has certain uses in university and other settings
** where (1) the pasword file is very large, and (2) a group of users
** frequently logs in together using the same account (for example,
** a class userid).
**---------------------------------------------------------------------
*/

/* #define USER_CACHE */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to build a System V version
**---------------------------------------------------------------------
*/

/* #define SYSV */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to build a version that uses
** System V style "lp" instead of BSD-style "lpr" to print
**---------------------------------------------------------------------
*/

/* #define USE_LP */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to build a typical
** "local feature": in this case recognizing the special printer
** names "rotated" and "2column" and using the Adobe "enscript"
** command to format the output appropriately.
**---------------------------------------------------------------------
*/

/* #define HACK_FOR_ROTATED_TRANSCRIPT */

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to build a version that
** will use the setusershell()/getusershell()/endusershell() calls
** to determine if a password entry contains a legal shell (and therefore
** identifies a user who may log in). The default is to check that
** the last two characters of the shell field are "sh", which should
** cope with "sh", "csh", "ksh", "bash".... See the routine get_pasword()
** in pcnfsd_misc.c for more details.
**
** Note: For some reason that I haven't yet figured out, getusershell()
** only seems to work when RPC_SVC_FG is defined (for debugging). It doesn't
** seem to matter whether /etc/shells exists or not. Tracing
** things doesn't throw any light on this....  Geoff Dec.17 '91
*/

/*
**---------------------------------------------------------------------
** Define (via Makefile) the following symbol to build a version that
** will consult the NIS (formerly Yellow Pages) "auto.home" map to
** locate the user's home directory (returned by the V2 authentication
** procedure).
**---------------------------------------------------------------------
*/

/* #define USE_YP */


/* #define USE_GETUSERSHELL */


/*
**---------------------------------------------------------------------
** The following should force the right things for Interactive 2.0
**---------------------------------------------------------------------
*/
#ifdef ISC_2_0
#define SYSV
#define USE_LP
#define SHADOW_SUPPORT
#endif

/*
**---------------------------------------------------------------------
** Other #define's 
**---------------------------------------------------------------------
*/

#define assert(ex) {\
	if (!(ex)) { \
		char asstmp[256]; \
		(void)snprintf(asstmp, sizeof asstmp, \
		    "rpc.pcnfsd: Assertion failed: line %d of %s: \"%s\"\n", \
		    __LINE__, __FILE__, "ex"); \
		(void)msg_out(asstmp); \
		sleep (10); \
		exit(1); \
	} \
}
