
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	netware.h
 * DESCRIPTION	:	Include for NetWare stuff.
 *                  This is based on the win32.h file of Win32 port.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */



#ifndef  _INC_NW_PERL5
#define  _INC_NW_PERL5

#include <dirent.h>
#include "stdio.h"

// to get the internal debugger break for functions that are not yet handled
#include "deb.h"

#ifndef EXT
#include "EXTERN.h"
#endif

//structure that will be used by times routine.
struct tms {
	long	tms_utime;
	long	tms_stime;
	long	tms_cutime;
	long	tms_cstime;
};

#define PERL_GET_CONTEXT_DEFINED
#define ENV_IS_CASELESS

#undef   init_os_extras
#define  init_os_extras Perl_init_os_extras

#define HAVE_INTERP_INTERN
struct interp_intern {
    void *	internal_host;
    long	perlshell_items;	// For system() ;  Ananth, 3 Sept 2001

    char *	perlshell_tokens;	// For system() ; From Win32 of Perl 5.8 on 24 June 2002
    char **	perlshell_vec;	// For system() ; From Win32 of Perl 5.8 on 24 June 2002
};

/*
 * handle socket stuff, assuming socket is always available
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

//This is clashing with a definition in perly.h, hence
//undefine, may have to redefine if need be - CHKSGP
#undef WORD

#ifndef SOCKET
typedef u_int           SOCKET;
#endif

#define nw_internal_host		(PL_sys_intern.internal_host)
#define nw_perlshell_items	(PL_sys_intern.perlshell_items)		// For system() ;  Ananth, 3 Sept 2001

#define nw_perlshell_tokens	(PL_sys_intern.perlshell_tokens)	// For system() ; From Win32 of Perl 5.8 on 24 June 2002
#define nw_perlshell_vec	(PL_sys_intern.perlshell_vec)	// For system() ; From Win32 of Perl 5.8 on 24 June 2002

EXTERN_C void	Perl_nw5_init(int *argcp, char ***argvp);

#define PTHREAD_ATFORK(prepare,parent,child)	NOOP

/*
 * This provides a layer of functions and macros to ensure extensions will
 * get to use the same RTL functions as the core.
 */
#include "nw5iop.h"

// Below is called in Run.c file when a perl script executes/runs.
#ifdef MPK_ON
	#define PERL_ASYNC_CHECK() kYieldThread();
#else
	#define PERL_ASYNC_CHECK() ThreadSwitch();
#endif


#endif /* _INC_NW_PERL5 */

