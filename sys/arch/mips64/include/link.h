/*	$OpenBSD: link.h,v 1.2 2004/08/10 20:28:13 deraadt Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MIPS_LINK_H_
#define _MIPS_LINK_H_

#include <elf_abi.h>
#include <machine/elf_abi.h>

/*
 *	Debug rendezvous struct. Pointer to this is set up in the
 *	target code pointed by the DT_MIPS_RLD_MAP tag. If it is
 *	defined.
 */

struct r_debug {
	int	r_version;	/* Protocol version.	*/
	struct link_map *r_map;	/* Head of list of loaded objects.  */

	Elf32_Addr r_brk;
	enum {
		RT_CONSISTENT,	/* Mapping change is complete.  */
		RT_ADD,		/* Adding a new object.  */
		RT_DELETE,	/* Removing an object mapping.  */
	} r_state;

	Elf32_Addr r_ldbase;	/* Base address the linker is loaded at.  */
};

/*
 * Shared object map data used by the debugger.
 */

struct link_map {
    Elf32_Addr	l_addr;		/* Base address shared object is loaded at.  */
    Elf32_Addr	l_offs;		/* Offset from link address */
    char	*l_name;	/* Absolute file name object was found in.  */
    Elf32_Dyn	*l_ld;		/* Dynamic section of the shared object.  */
    struct link_map *l_next, *l_prev; /* Chain of loaded objects.  */
};

#endif /* !_MIPS_LINK_H_ */
