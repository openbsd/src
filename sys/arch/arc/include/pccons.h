/*	$OpenBSD: pccons.h,v 1.1.1.1 1996/06/24 09:07:17 pefo Exp $	*/
/*	$NetBSD: pccons.h,v 1.4 1996/02/02 18:06:06 mycroft Exp $	*/

/*
 * pccons.h -- pccons ioctl definitions
 */

#ifndef _PCCONS_H_
#define _PCCONS_H_

#include <sys/ioctl.h>

/* key types -- warning: pccons.c depends on most values */

#define	KB_SCROLL	0x0001	/* stop output */
#define	KB_NUM		0x0002	/* numeric shift  cursors vs. numeric */
#define	KB_CAPS		0x0004	/* caps shift -- swaps case of letter */
#define	KB_SHIFT	0x0008	/* keyboard shift */
#define	KB_CTL		0x0010	/* control shift  -- allows ctl function */
#define	KB_ASCII	0x0020	/* ascii code for this key */
#define	KB_ALTGR	0x0040	/* alternate graphics shift */
#define	KB_ALT		0x0080	/* alternate shift -- alternate chars */
#define	KB_FUNC		0x0100	/* function key */
#define	KB_KP		0x0200	/* Keypad keys */
#define	KB_NONE		0x0400	/* no function */

#define	KB_CODE_SIZE	4	/* Use a max of 4 for now... */
#define KB_NUM_KEYS	128	/* Number of scan codes */
typedef struct {
	u_short	type;
	char unshift[KB_CODE_SIZE];
	char shift[KB_CODE_SIZE];
	char ctl[KB_CODE_SIZE];
	char altgr[KB_CODE_SIZE];
	char shift_altgr[KB_CODE_SIZE];
} keymap_t;

#define CONSOLE_X_MODE_ON		_IO('t',121)
#define CONSOLE_X_MODE_OFF		_IO('t',122)
#define CONSOLE_X_BELL			_IOW('t',123,int[2])
#define CONSOLE_SET_TYPEMATIC_RATE	_IOW('t',124,u_char)
#define CONSOLE_GET_KEYMAP		_IOR('t',128,keymap_t[KB_NUM_KEYS])
#define CONSOLE_SET_KEYMAP		_IOW('t',129,keymap_t[KB_NUM_KEYS])

#endif /* _PCCONS_H_ */
