/*	$OpenBSD: flashvar.h,v 1.5 2009/10/13 19:33:16 pirofti Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#ifndef _FLASHVAR_H_
#define _FLASHVAR_H_

#ifdef _KERNEL

/* Flash controller descriptor structure */
struct flash_ctl_tag {
	u_int8_t (*reg8_read)(void *, int);
	int	 (*regx_read_page)(void *, caddr_t, caddr_t);
	void	 (*reg8_write)(void *, int, u_int8_t);
	int	 (*regx_write_page)(void *, caddr_t, caddr_t);
	void	 (*default_disklabel)(void *, dev_t, struct disklabel *);
	int	 (*safe_strategy)(void *, struct buf *);
};

/*
 * Pseudo-registers for a fictitious flash controller
 *
 * Note that logical levels are assumed for CE and WP bits.
 * Signals corresponding to these bits are usually negated.
 */
#define FLASH_REG_DATA		0x00
#define FLASH_REG_COL		0x01
#define FLASH_REG_ROW		0x02
#define FLASH_REG_CMD		0x03
#define FLASH_REG_ALE		0x04
#define FLASH_REG_CLE		0x05
#define FLASH_REG_CE		0x06
#define FLASH_REG_WP		0x07
#define FLASH_REG_READY		0x0f

/* Flash device descriptor structure */
struct flashdev {
	u_int16_t	 id;
	const char	*longname;
	u_long		 pagesize;	/* bytes per page */
	u_long		 oobsize;	/* OOB bytes per page */
	u_long		 blkpages;	/* pages per erasable block */
	u_long		 capacity;	/* pages per device */
};

#define FLASH_DEVICE(v,d)		((FLASH_VENDOR_##v << 8) | (d))

/* Flash device vendors */
#define FLASH_VENDOR_SAMSUNG		0xec

/* Flash devices */
#define FLASH_DEVICE_SAMSUNG_K9F2808U0C FLASH_DEVICE(SAMSUNG, 0x73)
#define FLASH_DEVICE_SAMSUNG_K9F1G08U0A FLASH_DEVICE(SAMSUNG, 0xf1)

/* Maximum sizes for all devices */
#define FLASH_MAXPAGESIZE	2048
#define FLASH_MAXOOBSIZE	64

/*
 * Should-be private softc structure for the generic flash driver.
 */
struct flash_softc {
	struct device		 sc_dev;
	/* Disk device information */
	struct disk		 sc_dk;
	struct buf		 sc_q; 
	struct buf		*sc_bp;
	int			 sc_flags;
	/* Flash controller tag */
	struct flash_ctl_tag	*sc_tag;
	void			*sc_cookie;
	/* Flash device characteristics */
	const struct flashdev	*sc_flashdev;
	int			 sc_maxwaitready;
	int			 sc_maxwaitcomplete;
};

/* Values for sc_flags */
#define FDK_LOADED		 0x00000001
#define FDK_SAFE		 0x00000002

/*
 * Similar to vnd(4) devices there are two kinds of flash devices.
 * Both device kinds share the same disklabel.
 *
 * ``Safe'' devices have bit 11 set in the minor number and use the
 * out-of-band page data to implement wear-leveling and transparent
 * management of bad block information. Block erasing and rewriting
 * is also handled transparently; arbitrary pages can be modified.
 *
 * ``Unsafe'' devices provide raw access to the flash pages. Access
 * to OOB page data is possible via ioctl()s only with these devices.
 * Erasing the containing flash block may be necessary before a page
 * can be writting successfully, but the block erase command is only
 * provided as an ioctl().
 */
#define flashsafe(x)	(minor(x) & 0x800)
#define flashunit(x)	DISKUNIT(makedev(major(x), minor(x) & 0x7ff))
#define flashpart(x)	DISKPART(makedev(major(x), minor(x) & 0x7ff))
#define flashlabeldev(x) (MAKEDISKDEV(major(x), flashunit(x), RAW_PART)\
			 |flashsafe(x))

void	 flashattach(struct flash_softc *, struct flash_ctl_tag *, void *);
int	 flashdetach(struct device *, int);
int	 flashactivate(struct device *, int);

u_int8_t flash_reg8_read(struct flash_softc *, int);
void	 flash_reg8_read_page(struct flash_softc *, caddr_t, caddr_t);
void	 flash_reg8_write(struct flash_softc *, int, u_int8_t);
void	 flash_reg8_write_page(struct flash_softc *, caddr_t, caddr_t);
void	 flash_chip_enable(struct flash_softc *);
void	 flash_chip_disable(struct flash_softc *);
int	 flash_chip_reset(struct flash_softc *);
int	 flash_chip_identify(struct flash_softc *, u_int8_t *, u_int8_t *);
int	 flash_chip_erase_block(struct flash_softc *, long);
int	 flash_chip_read_block(struct flash_softc *, long, caddr_t);
int	 flash_chip_read_page(struct flash_softc *, long, caddr_t, caddr_t);
int	 flash_chip_read_oob(struct flash_softc *, long, caddr_t);
int	 flash_chip_write_block(struct flash_softc *, long, caddr_t, caddr_t);
int	 flash_chip_write_page(struct flash_softc *, long, caddr_t, caddr_t);
int	 flash_chip_verify_block(struct flash_softc *, long, caddr_t, caddr_t);
int	 flash_chip_verify_page(struct flash_softc *, long, caddr_t, caddr_t);

#endif /* _KERNEL */

/* XXX: define ioctl commands for OOB page data access and block erase. */

#endif /* _FLASHVAR_H_ */
