/*	$OpenBSD: ataio.h,v 1.1 1999/07/18 21:25:20 csapuntz Exp $	*/
/*	$NetBSD: ataio.h,v 1.2 1998/11/23 22:58:23 kenh Exp $	*/

#ifndef _SYS_ATAIO_H_
#define _SYS_ATAIO_H_

#include <sys/types.h>
#include <sys/ioctl.h>

typedef struct	atareq {
	u_long	flags;		/* info about the request status and type */
	u_char	command;	/* command code */
	u_char	features;	/* feature modifier bits for command */
	u_char	sec_count;	/* sector count */
	u_char	sec_num;	/* sector number */
	u_char	head;		/* head number */
	u_short	cylinder;	/* cylinder/lba address */

	caddr_t	databuf;	/* Pointer to I/O data buffer */
	u_long	datalen;	/* length of data buffer */
	int	timeout;	/* Command timeout */
	u_char	retsts;		/* the return status for the command */
	u_char	error;		/* error bits */
} atareq_t;

/* bit defintions for flags */
#define ATACMD_READ		0x00000001
#define ATACMD_WRITE		0x00000002
#define ATACMD_READREG		0x00000004

/* definitions for the return status (retsts) */
#define ATACMD_OK	0x00
#define ATACMD_TIMEOUT	0x01
#define ATACMD_ERROR	0x02
#define ATACMD_DF	0x03

#define ATAIOCCOMMAND	_IOWR('Q', 8, atareq_t)

#endif /* _SYS_ATAIO_H_ */
