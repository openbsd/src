/*	$OpenBSD: lk201reg.h,v 1.1 2001/05/16 22:15:17 hugh Exp $	*/
/* $NetBSD: lk201reg.h,v 1.1 1998/09/17 20:01:57 drochner Exp $ */

/* 
 * command keycodes for Digital LK200/LK400 series keyboards.
 */

/*
 * special keycodes
 */
#define LK_POWER_UP	0x01
#define	LK_KEY_R_SHIFT	0xab
#define LK_KEY_SHIFT	0xae
#define LK_KEY_LOCK	0xb0
#define LK_KEY_CONTROL	0xaf
#define	LK_KEY_R_ALT	0xb2
#define LK_KEY_UP	0xb3
#define LK_KEY_REPEAT	0xb4
#define LK_KEY_HOLD	0x56	/* F1 */
#define LK_KDOWN_ERROR	0x3d		/* key down on powerup error	*/
#define LK_POWER_ERROR	0x3e		/* keyboard failure on pwrup tst*/
#define LK_OUTPUT_ERROR 0xb5		/* keystrokes lost during inhbt */
#define LK_INPUT_ERROR	0xb6		/* garbage command to keyboard	*/
#define LK_LOWEST	0x56		/* lowest significant keycode	*/

/*
 * keyboard commands
 */
#define LK_UPDOWN	0x86		/* bits for setting lk201 modes */
#define LK_AUTODOWN	0x82
#define LK_DOWN		0x80
#define LK_DEFAULTS	0xd3		/* reset mode settings          */
#define LK_AR_ENABLE	0xe3		/* global auto repeat enable	*/
#define LK_CL_ENABLE	0x1b		/* keyclick enable		*/
#define LK_CL_DISABLE	0x99		/* keyclick disable		*/
#define LK_CCL_ENABLE	0xbb		/* enable keyclick for CTRL	*/
#define LK_CCL_DISABLE	0xb9		/* disable keyclick for CTRL	*/
#define LK_KBD_ENABLE	0x8b		/* keyboard enable		*/
#define LK_BELL_ENABLE	0x23		/* enable the bell		*/
#define LK_BELL_DISABLE	0xa1		/* disable the bell		*/
#define LK_LED_ENABLE	0x13		/* light led			*/
#define LK_LED_DISABLE	0x11		/* turn off led			*/
#define LK_RING_BELL	0xa7		/* ring keyboard bell		*/
#define LK_LED_1	0x81		/* led bits			*/
#define LK_LED_2	0x82
#define LK_LED_3	0x84
#define LK_LED_4	0x88
#define LK_LED_WAIT	0x81
#define LK_LED_COMP	0x82
#define LK_LED_LOCK	0x84
#define LK_LED_HOLD	0x88
#define LK_LED_ALL	0x8f
#define LK_HELP		0x7c		/* help key			*/
#define LK_DO		0x7d		/* do key			*/
#define LK_DIV6_START	0xad		/* start of div 6		*/
#define LK_DIV5_END	0xb2		/* end of div 5			*/
#define LK_ENABLE_401	0xe9		/* turn on LK401 mode		*/
#define LK_MODE_CHANGE	0xba		/* mode change ack		*/

/* max volume is 0, lowest is 0x7 */
#define	LK_PARAM_VOLUME(v)		(0x80|((v)&0x7))

/* mode command details */
#define	LK_CMD_MODE(m,div)		((m)|((div)<<3))
