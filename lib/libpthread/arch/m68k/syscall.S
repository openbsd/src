/*	$OpenBSD: syscall.S,v 1.1 1997/04/13 17:13:09 briggs Exp $	*/

#ifndef lint
    .text
    .asciz "$Id: syscall.S,v 1.1 1997/04/13 17:13:09 briggs Exp $";
#endif
 
#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
#define SYSCALL(x)      .even; ENTRY(machdep_sys_ ## x); \
			movl \#SYS_ ## x,d0; trap \#0; jcs err; rts
#else /* !__STDC__ */
#define SYSCALL(x)      .even; ENTRY(machdep_sys_/**/x); \
			movl #SYS_/**/x,d0; trap #0; jcs err; rts
#endif /* !__STDC__ */

/*
 * Initial asm stuff for all functions.
 */
	.text
	.even


/* ==========================================================================
 * error code for all syscalls. The error value is returned as the negative
 * of the errno value.
 */

err:
	negl		d0
	rts

/* ==========================================================================
 * machdep_sys_write()
 */
SYSCALL(write)

/* ==========================================================================
 * machdep_sys_read()
 */
SYSCALL(read)

/* ==========================================================================
 * machdep_sys_open()
 */
SYSCALL(open)

/* ==========================================================================
 * machdep_sys_close()
 */
SYSCALL(close)

/* ==========================================================================
 * machdep_sys_fcntl()
 */
SYSCALL(fcntl)

/* ==========================================================================
 * machdep_sys_lseek()
 */
SYSCALL(lseek)

/* ========================================================================== 
 * Nonstandard calls used to make the system work
 *
 * ==========================================================================
 * machdep_sys_select()
 */
SYSCALL(select)

/* ========================================================================== 
 * Berkeley socket stuff
 * 
 * ==========================================================================
 * machdep_sys_socket()
 */
SYSCALL(socket)

/* ==========================================================================
 * machdep_sys_bind()
 */
SYSCALL(bind)

/* ==========================================================================
 * machdep_sys_connect()
 */
SYSCALL(connect)

/* ==========================================================================
 * machdep_sys_accept()
 */
SYSCALL(accept)

/* ==========================================================================
 * machdep_sys_listen()
 */
SYSCALL(listen)

/* ==========================================================================
 * machdep_sys_getsockopt()
 */
SYSCALL(getsockopt)

/* ==========================================================================
 * machdep_sys_readv()
 */
SYSCALL(readv)

/* ==========================================================================
 * machdep_sys_writev()
 */
SYSCALL(writev)

/* ==========================================================================
 * machdep_sys_getpeername()
 */
SYSCALL(getpeername)

/* ==========================================================================
 * machdep_sys_getsockname()
 */
SYSCALL(getsockname)

