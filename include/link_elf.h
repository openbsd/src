/*	$OpenBSD: link_elf.h,v 1.1 2002/06/07 03:00:01 art Exp $	*/

/*
 * Public domain.
 */

#include <elf_abi.h>

#ifndef DT_PROCNUM
#define DT_PROCNUM 0
#endif

/*
 * struct link_map is a part of the protocol between the debugger and
 * ld.so.
 */
struct link_map {
	caddr_t		l_addr;		/* Base address of library */
	const char	*l_name;	/* Absolute path to library */
	void		*l_ld;		/* pointer to _DYNAMIC */
	struct link_map	*l_next;
	struct link_map	*l_prev;
};

