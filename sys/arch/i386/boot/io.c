/*	$NetBSD: io.c,v 1.18 1995/12/23 17:21:26 perry Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/types.h>
#include <machine/pio.h>

void gateA20 __P((int on));
/*void printf __P((const char *format, int data));*/ /* not quite right XXX */
void putchar __P((int c));
int gets __P((char *buf));
int strcmp __P((const char *s1, const char *s2));
void bcopy __P((char *from, char *to, int len));
int awaitkey __P((int seconds));
void twiddle __P((void));

#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */

/*
 * Gate A20 for high memory
 */
void
gateA20(on)
	int on;
{
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
	while (inb(K_STATUS) & K_IBUF_FUL);

	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	while (inb(K_STATUS) & K_IBUF_FUL);

	if (on)
		outb(K_RDWR, 0xdf);
	else
		outb(K_RDWR, 0xcd);
	while (inb(K_STATUS) & K_IBUF_FUL);

	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);
#endif	IBM_L40
}

/* printf - only handles %d as decimal, %c as char, %s as string */
void
printf(format, data)
	const char *format;
	int data;
{
	int *dataptr = &data;
	char c;

	while (c = *format++) {
		if (c != '%') {
			putchar(c);
			continue;
		}
		c = *format++;
		if (c == 'd') {
			int num = *dataptr++, dig;
			char buf[10], *ptr = buf;
			if (num < 0) {
				num = -num;
				putchar('-');
			}
			do {
				dig = num % 10;
				*ptr++ = '0' + dig;
			} while (num /= 10);
			do
				putchar(*--ptr);
			while (ptr != buf);
		} else if (c == 'x') {
			unsigned int num = (unsigned int)*dataptr++, dig;
			char buf[8], *ptr = buf;
			do {
				dig = num & 0xf;
				*ptr++ = dig > 9 ?
					 'a' + dig - 10 :
					 '0' + dig;
			} while (num >>= 4);
			do
				putchar(*--ptr);
			while (ptr != buf);
		} else if (c == 'c') {
			putchar((char)*dataptr++);
		} else if (c == 's') {
			char *ptr = (char *)*dataptr++;
			while (c = *ptr++)
				putchar(c);
		}
	}
}

void
putchar(c)
	int c;
{
	if (c == '\n')
		putc('\r');
	putc(c);
}

int
gets(buf)
	char *buf;
{
	char *ptr = buf;

	for (;;) {
		register int c = getc();
		if (c == '\n' || c == '\r') {
			putchar('\n');
			*ptr = '\0';
			return 1;
		} else if (c == '\b' || c == '\177') {
			if (ptr > buf) {
				putchar('\b');
				putchar(' ');
				putchar('\b');
				ptr--;
			}
		} else {
			putchar(c);
			*ptr++ = c;
		}
	}

	/* shouldn't ever be reached; we have to return in the loop. */
}

int
strcmp(s1, s2)
	const char *s1, *s2;
{
	while (*s1 == *s2) {
		if (!*s1++)
			return 0;
		s2++;
	}
	return 1;
}

void
bcopy(from, to, len)
	char *from, *to;
	int len;
{
	if (from > to)
		while (--len >= 0)
			*to++ = *from++;
	else {
		to += len;
		from += len;
		while (--len >= 0)
			*--to = *--from;
	}
}

/* Number of milliseconds to sleep during each microsleep */
#define NAPTIME 50

/*
 * awaitkey takes a number of seconds to wait for a key to be
 * struck. If a key is struck during the period, it returns true, else
 * it returns false. It returns (nearly) as soon as the key is
 * hit. Note that it does something only slightly smarter than busy waiting.
 */
int
awaitkey(seconds)
	int seconds;
{
	int i;

	/*
	 * We sleep for brief periods (typically 50 milliseconds, set
	 * by NAPTIME), polling the input buffer at the end of
	 * each period.
	 */
	for (i = ((seconds*1000)/NAPTIME); i > 0; i--) {
		/* multiply by 1000 to get microseconds. */
		usleep(NAPTIME*1000);
		if (ischar())
			break;
	}

	/* If a character was hit, "i" will still be non-zero. */
	return (i != 0);
}

void
twiddle()
{
	static int pos;

	putchar("|/-\\"[pos++ & 3]);
	putchar('\b');
}
