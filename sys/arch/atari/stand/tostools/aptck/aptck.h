/*	$NetBSD: aptck.h,v 1.1.1.1 1996/01/07 21:54:15 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
 * All rights reserved.
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APTCK_H
#define APTCK_H

#define MINOR(bus, target, lun)	(lun)
#define MAJOR(bus, target, lun)	(((bus) << 3) + (target))

#define LUN(major, minor)	(minor)
#define TARGET(major, minor)	((major) & 0x0007)
#define BUS(major, minor)	(((major) >> 3) & 0x1FFF)
#define BIOSDEV(major, minor)	(((minor) == 0) ? ((major) + 2) : 0)

typedef signed char	int8_t;
typedef unsigned char	u_int8_t;
typedef signed short	int16_t;
typedef unsigned short	u_int16_t;
typedef signed long	int32_t;
typedef unsigned long	u_int32_t;

typedef enum {
	ACSI = 0,
	SCSI = 1,
	IDE  = 2
} bus_t;

typedef struct {
	char		id[4];
	u_int		start;
	u_int		end;
	u_int		rsec;
	u_int		rent;
} part_t;

typedef struct {
	u_int		major;		/* XHDI major number		*/
	u_int		minor;		/* XHDI minor number		*/
	char *		sname;		/* short name (s00)		*/
	char *		fname;		/* full name (scsi target 0 lun 0)*/
	char *		product;	/* product name			*/
	u_long		bsize;		/* block size in bytes		*/
	u_long		msize;		/* medium size in blocks	*/
	u_int		bblock;		/* NetBSD boot block		*/
	u_int		lblofs;		/* label offset in boot block	*/
	u_int		hdsize;		/* medium size from root sector	*/
	u_int		bslst;		/* start of bad sector list	*/
	u_int		bslend;		/* end of bad sector list	*/
	u_int		nroots;		/* # of auxilary root sectors	*/
	u_int		*roots;		/* list of auxilary roots	*/
	u_int		nparts;		/* number of regular partitions	*/
	part_t 		*parts;		/* list of partition descriptors */
} disk_t;


/*
 *	biosrw.s
 */
EXTERN int	bios_read   PROTO((void *, u_int, u_int, u_int));
EXTERN int	bios_write  PROTO((void *, u_int, u_int, u_int));
EXTERN void	bios_critic PROTO((void));

/*
 *	diskio.c
 */
EXTERN disk_t *	disk_open  PROTO((char *));
EXTERN void	disk_close PROTO((disk_t *));
EXTERN void *	disk_read  PROTO((disk_t *, u_int, u_int));
EXTERN int	disk_write PROTO((disk_t *, u_int, u_int, void *));

/*
 *	disklabel.c
 */
EXTERN int	readdisklabel PROTO((disk_t *));

#endif	/* APTCK_H */
