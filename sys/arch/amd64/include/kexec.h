/*	$OpenBSD: kexec.h,v 1.1 2025/11/12 11:34:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2019-2020 Visa Hankala
 * Copyright (c) 2025 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_KEXEC_H_
#define _MACHINE_KEXEC_H_

#include <sys/ioccom.h>

#define KEXEC_TRAMPOLINE	(35 * NBPG)	/* @ 140 Kb */
#define KEXEC_TRAMP_DATA	(36 * NBPG)
#define PA_KERN			(16*1024*1024)	/* @ 16 Mb */

#ifndef _LOCORE

#define KEXEC_MAX_ARGS	8	/* maximum number of boot arguments */

struct kexec_args {
	char		*kimg;		/* kernel image buffer */
	size_t		klen;		/* size of kernel image */
	int		boothowto;
	u_char		bootduid[8];
};

#define KIOC_KEXEC		_IOW('K', 1, struct kexec_args)
#define KIOC_GETBOOTDUID	_IOR('K', 2, u_char[8])

extern unsigned int	kexec_size;
extern void		kexec_tramp(vaddr_t, vaddr_t, paddr_t, size_t, vaddr_t);

#endif	/* !_LOCORE */

#endif /* _MACHINE_KEXEC_H_ */
