/*
 * This file contains constants related to the LED's on the cpu boards.
 * The LED's are two 7-segment displays.
 *
 * The segments of each digit are labeled as follows:
 *            a           a
 *           ---         ---
 *         f|   |b     f|   |b
 *           -g-         -g-
 *         e|   |c     e|   |c
 *           --- .       --- .
 *            d           d
 *
 * Each segment corresponds to a bit in the LED register:
 *
 *       left hand digit | right hand digit
 *       15                             0
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |.|g|f|e|d|c|b|a|.|g|f|e|d|c|b|a|
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *
 * To turn a segment ON, the bit must be cleared, and to
 * turn a segment OFF, the bit must be set.
 */

#define LED_SEG_A	0xfe
#define LED_SEG_B	0xfd
#define LED_SEG_C	0xfb
#define LED_SEG_D	0xf7
#define LED_SEG_E	0xef
#define LED_SEG_F	0xdf
#define LED_SEG_G	0xbf
#define LED_SEG_P	0x7f

/* codes mapped into LED register values: */
#define LED_0		0xc0		/* "0" */	
#define LED_1		0xf9		/* "1" */
#define LED_2		0xa4 		/* "2" */
#define LED_3		0xb0		/* "3" */
#define LED_4		0x99		/* "4" */	
#define LED_5		0x92		/* "5" */
#define LED_6		0x82		/* "6" */
#define LED_7		0xf8		/* "7" */
#define LED_8		0x80		/* "8" */
#define LED_9		0x98		/* "9" */
#define LED_A		0x88		/* "A" */
#define LED_b		0x83		/* "b" */
#define LED_C		0xc6		/* "C" */
#define LED_d		0xa1		/* "d" */
#define LED_E		0x86		/* "E" */
#define LED_F		0x8e		/* "F" */
#define LED_P		0x8c		/* "P" */
#define LED_L		0xc7		/* "L" */
#define LED_S		0x92		/* "S" */
#define LED_U		0xc1		/* "U" */
#define LED_t		(LED_SEG_F & LED_SEG_E & LED_SEG_G)	/* "t" */
#define LED_BLANK	0xff		/* " " */
#define LED_BAR		0xfe		/* "-" (upper) */
#define LED_DASH	0xbf		/* "-" (middle) */	
#define LED_RUB		0xf7		/* "_" (lower) */
#define LED_EQU		0xbe		/* "=" (upper) */
#define LED_EQL		0xb7		/* "=" (lower) */
#define LED_OU		0x9c		/* "o" (upper) */
#define LED_OL		0xa3		/* "o" (lower) */
#define LED_DP		0x7f		/* "." */
