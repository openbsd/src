/*	$OpenBSD: ldasm.S,v 1.2 2001/05/29 08:52:11 art Exp $ */

/*
 * Copyright (c) 2001 Niklas Hallqvist
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
 *	Niklas Hallqvist.
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

#include <machine/asm.h>
#include <machine/pal.h>
#include <sys/syscall.h>

#define AUX_entry 9

	.extern _GLOBAL_OFFSET_TABLE_

/* Not really a leaf... but we are special.  */
LEAF_NOPROFILE(_dl_start, 0)
	.set	noreorder
	lda	sp, (-8 - ((AUX_entry + 1) * 8))(sp)
	br	pv, L1
L1:
	LDGP(pv)
	ldiq	t2, L1
	subq	pv, t2, t2
	lda	t0, _GLOBAL_OFFSET_TABLE_
	addq	t0, t2, t0
	lda	t1, _DYNAMIC
	addq	t1, t2, t1	
L2:	cmpult	t0, t1, t3	
	beq	t3, L3
	ldq	t3, 0(t0)
	addq	t3, t2, t3
	stq	t3, 0(t0)
	lda	t0, 8(t0)
	br	L2
L3:
	mov	a0, s0
	mov	t2, a1		/* relocation displacement */
	mov	a1, s1
	ldq	a2, 0(a0)	/* argc */
	lda	a3, 8(a0)	/* argv */
	mov	a3, s3
	lda	t3, 1(a2)
	sll	t3, 3, t3
	addq	a3, t3, a4	/* envp */
	mov	a4, s4
	mov	t1, a5		/* dynamic */
	mov	a5, s5
	lda	s2, 8(sp)
	stq	s2, 0(sp)	/* dl_link */
	CALL(_dl_boot_bind)
	mov	s3, a0
	mov	s4, a1
	mov	s1, a2
	mov	s5, a3
	mov	s2, a4
	CALL(_dl_boot)
	mov	s0, a0
	mov	v0, pv
	jsr	ra, (pv)
END(_dl_start)

/*
 * In reality these are not leaves, but they are stubs which does not need
 * further register saving.
 */
	
LEAF_NOPROFILE(_dl_exit, 1)
	ldiq	v0, SYS_exit
	call_pal PAL_OSF1_callsys
	RET
END(_dl_exit)

LEAF_NOPROFILE(_dl_open, 2)
	ldiq	v0, SYS_open
	call_pal PAL_OSF1_callsys
	beq	a3, _dl_open_no_error
	subq	zero, v0, v0
_dl_open_no_error:
	RET
END(_dl_open)

LEAF_NOPROFILE(_dl_close, 1)
	ldiq	v0, SYS_close
	call_pal PAL_OSF1_callsys
	RET
END(_dl_close)

LEAF_NOPROFILE(_dl_write, 3)
	ldiq	v0, SYS_write
	call_pal PAL_OSF1_callsys
	RET
END(_dl_write)

LEAF_NOPROFILE(_dl_read, 3)
	ldiq	v0, SYS_read
	call_pal PAL_OSF1_callsys
	RET
END(_dl_exit)

LEAF_NOPROFILE(_dl_mmap, 6)
	lda	sp, -8(sp)
	stq	a5, 0(sp)
	ldiq	v0, SYS_mmap
	call_pal PAL_OSF1_callsys
	lda	sp, 8(sp)
	RET
END(_dl_mmap)

LEAF_NOPROFILE(_dl_munmap, 2)
	ldiq	v0, SYS_munmap
	call_pal PAL_OSF1_callsys
	RET
END(_dl_munmap)

LEAF_NOPROFILE(_dl_mprotect, 3)
	ldiq	v0, SYS_mprotect
	call_pal PAL_OSF1_callsys
	RET
END(_dl_mprotect)

LEAF_NOPROFILE(_dl_getuid, 0)
	ldiq	v0, SYS_getuid
	call_pal PAL_OSF1_callsys
	RET
END(_dl_getuid)

LEAF_NOPROFILE(_dl_geteuid, 0)
	ldiq	v0, SYS_geteuid
	call_pal PAL_OSF1_callsys
	RET
END(_dl_geteuid)

LEAF_NOPROFILE(_dl_getgid, 0)
	ldiq	v0, SYS_getgid
	call_pal PAL_OSF1_callsys
	RET
END(_dl_getgid)

LEAF_NOPROFILE(_dl_getegid, 0)
	ldiq	v0, SYS_getegid
	call_pal PAL_OSF1_callsys
	RET
END(_dl_getegid)

#ifdef USE_CACHE
LEAF_NOPROFILE(_dl_stat, 2)
	ldiq	v0, SYS_stat
	call_pal PAL_OSF1_callsys
	RET
END(_dl_stat)
#endif
