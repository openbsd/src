/*	$OpenBSD: setjmp.h,v 1.4 2016/08/07 02:02:57 guenther Exp $	*/
/*	$NetBSD: setjmp.h,v 1.2 2001/08/25 14:45:59 bjh21 Exp $	*/

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN	64		/* size, in longs, of a jmp_buf */

/*
 * Description of the setjmp buffer
 *
 * word  0	magic number	(dependant on creator)
 *       1 -  3	f4		fp register 4
 *	 4 -  6	f5		fp register 5
 *	 7 -  9 f6		fp register 6
 *	10 - 12	f7		fp register 7
 *	13	fpsr		fp status register
 *	14	r13		register 13 (sp) XOR cookie0
 *	15	r14		register 14 (lr) XOR cookie1
 *	16	r4		register 4
 *	17	r5		register 5
 *	18	r6		register 6
 *	19	r7		register 7
 *	20	r8		register 8
 *	21	r9		register 9
 *	22	r10		register 10 (sl)
 *	23	r11		register 11 (fp)
 *	24	unused		unused
 *	25	signal mask	(dependant on magic)
 *	26	(con't)
 *	27	(con't)
 *	28	(con't)
 *
 * The magic number number identifies the jmp_buf and
 * how the buffer was created as well as providing
 * a sanity check.
 *
 * A side note I should mention - please do not tamper
 * with the floating point fields. While they are
 * always saved and restored at the moment this cannot
 * be guaranteed especially if the compiler happens
 * to be generating soft-float code so no fp
 * registers will be used.
 *
 * Whilst this can be seen an encouraging people to
 * use the setjmp buffer in this way I think that it
 * is for the best then if changes occur compiles will
 * break rather than just having new builds falling over
 * mysteriously.
 */

#define _JB_MAGIC__SETJMP	0x4278f500
#define _JB_MAGIC_SETJMP	0x4278f501

/* Valid for all jmp_buf's */

#define _JB_MAGIC		 0
#define _JB_REG_F4		 1
#define _JB_REG_F5		 4
#define _JB_REG_F6		 7
#define _JB_REG_F7		10
#define _JB_REG_FPSR		13
#define _JB_REG_R4		16
#define _JB_REG_R5		17
#define _JB_REG_R6		18
#define _JB_REG_R7		19
#define _JB_REG_R8		20
#define _JB_REG_R9		21
#define _JB_REG_R10		22
#define _JB_REG_R11		23

/* Only valid with the _JB_MAGIC_SETJMP magic */

#define _JB_SIGMASK		25
