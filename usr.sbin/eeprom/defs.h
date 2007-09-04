/*	$OpenBSD: defs.h,v 1.5 2007/09/04 23:28:26 fgsch Exp $	*/
/*	$NetBSD: defs.h,v 1.2 1996/02/28 01:13:20 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#undef BUFSIZE
#define BUFSIZE		1024

#define IO_READ		0
#define IO_WRITE	1

#define MAXIMUM(a, b)	((a) > (b) ? (a) : (b))

/*
 * Misc. location declarations.
 */
#define EE_SIZE			0x500
#define EE_WC_LOC		0x04
#define EE_CKSUM_LOC		0x0c
#define EE_HWUPDATE_LOC		0x10
#define EE_BANNER_ENABLE_LOC	0x20

/*
 * Keyword table entry.  Contains a pointer to the keyword, the 
 * offset into the prom where the value lives, and a pointer to
 * the function that handles that value.
 */
struct	keytabent {
	char	*kt_keyword;		/* keyword for this entry */
	u_int	kt_offset;		/* offset into prom of value */
	void	(*kt_handler)(struct keytabent *, char *);
					/* handler function for this entry */
};

/*
 * String-value table entry.  Maps a string to a numeric value and
 * vice-versa.
 */
struct	strvaltabent {
	char	*sv_str;		/* the string ... */
	u_char	sv_val;			/* ... and the value */
};

#ifdef __sparc__
/*
 * This is an entry in a table which describes a set of `exceptions'.
 * In other words, these are Openprom fields that we either can't
 * `just print' or don't know how to deal with.
 */
struct	extabent {
	char	*ex_keyword;		/* keyword for this entry */
	void	(*ex_handler)(struct extabent *, struct opiocdesc *, char *);
					/* handler function for this entry */
};
#endif /* __sparc__ */

/* Sun 3/4 EEPROM handlers. */
void	ee_hwupdate(struct keytabent *, char *);
void	ee_num8(struct keytabent *, char *);
void	ee_num16(struct keytabent *, char *);
void	ee_screensize(struct keytabent *, char *);
void	ee_truefalse(struct keytabent *, char *);
void	ee_bootdev(struct keytabent *, char *);
void	ee_kbdtype(struct keytabent *, char *);
void	ee_constype(struct keytabent *, char *);
void	ee_diagpath(struct keytabent *, char *);
void	ee_banner(struct keytabent *, char *);
void	ee_notsupp(struct keytabent *, char *);

/* Sun 3/4 EEPROM checksum routines. */
u_char	ee_checksum(u_char *, size_t);
void	ee_updatechecksums(void);
void	ee_verifychecksums(void);

#ifdef __sparc__
/* Sparc Openprom handlers. */
char	*op_handler(char *, char *);
void	 op_dump(void);
void	 op_tree(void);
#endif /* __sparc__ */
