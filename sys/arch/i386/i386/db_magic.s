/*	$OpenBSD: db_magic.s,v 1.1 1996/05/04 14:33:00 mickey Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1995 Carnegie Mellon University
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

#include <machine/asm.h>
#define S_ARG0         4(%esp)
#define S_ARG1         8(%esp)
#define S_ARG2        12(%esp)
#define S_ARG3        16(%esp)
#define B_ARG0         8(%ebp)
#define B_ARG1        12(%ebp)
#define B_ARG2        16(%ebp)
#define B_ARG3        20(%ebp)
/*
 * void outb(unsigned char *io_port,
 *         unsigned char byte)
 *
 * Output a byte to an IO port.
 */
ENTRY(outb)
      movl    S_ARG0,%edx             /* IO port address */
      movl    S_ARG1,%eax             /* data to output */
      outb    %al,%dx                 /* send it out */
      ret
/*
 * unsigned char inb(unsigned char *io_port)
 *
 * Input a byte from an IO port.
 */
ENTRY(inb)
      movl    S_ARG0,%edx             /* IO port address */
      xor     %eax,%eax               /* clear high bits of register */
      inb     %dx,%al                 /* get the byte */
      ret
/*
 * void outw(unsigned short *io_port,
 *         unsigned short word)
 *
 * Output a word to an IO port.
 */
ENTRY(outw)
      movl    S_ARG0,%edx             /* IO port address */
      movl    S_ARG1,%eax             /* data to output */
      outw    %ax,%dx                 /* send it out */
      ret
/*
 * unsigned short inw(unsigned short *io_port)
 *
 * Input a word from an IO port.
 */
ENTRY(inw)
      movl    S_ARG0,%edx             /* IO port address */
      xor     %eax,%eax               /* clear high bits of register */
      inw     %dx,%ax                 /* get the word */
      ret
/*
 * void outl(unsigned int *io_port,
 *         unsigned int byte)
 *
 * Output an int to an IO port.
 */
ENTRY(outl)
      movl    S_ARG0,%edx             /* IO port address */
      movl    S_ARG1,%eax             /* data to output */
      outl    %eax,%dx                /* send it out */
      ret
/*
 * unsigned int inl(unsigned int *io_port)
 *
 * Input an int from an IO port.
 */
ENTRY(inl)
      movl    S_ARG0,%edx             /* IO port address */
      inl     %dx,%eax                /* get the int */
      ret
ENTRY(dr6)
      movl    %db6, %eax
      ret
/*    dr<i>(address, type, len, persistence)
 *    type:
 *       00   execution (use len 00)
 *       01   data write
 *       11   data read/write
 *    len:
 *       00   one byte
 *       01   two bytes
 *       11   four bytes
 */
ENTRY(dr0)
      movl    S_ARG0, %eax
      movl    %eax,_dr_addr
      movl    %eax, %db0
      movl    $0, %ecx
      jmp     0f
ENTRY(dr1)
      movl    S_ARG0, %eax
      movl    %eax,_dr_addr+1*4
      movl    %eax, %db1
      movl    $2, %ecx
      jmp     0f
ENTRY(dr2)
      movl    S_ARG0, %eax
      movl    %eax,_dr_addr+2*4
      movl    %eax, %db2
      movl    $4, %ecx
      jmp     0f
ENTRY(dr3)
      movl    S_ARG0, %eax
      movl    %eax,_dr_addr+3*4
      movl    %eax, %db3
      movl    $6, %ecx
0:
      pushl   %ebp
      movl    %esp, %ebp
      movl    %db7, %edx
      movl    %edx,_dr_addr+4*4
      andl    dr_msk(,%ecx,2),%edx    /* clear out new entry */
      movl    %edx,_dr_addr+5*4
      movzbl  B_ARG3, %eax
      andb    $3, %al
      shll    %cl, %eax
      orl     %eax, %edx
      movzbl  B_ARG1, %eax
      andb    $3, %al
      addb    $0x10, %ecx
      shll    %cl, %eax
      orl     %eax, %edx
      movzbl  B_ARG2, %eax
      andb    $3, %al
      addb    $0x2, %ecx
      shll    %cl, %eax
      orl     %eax, %edx
      movl    %edx, %db7
      movl    %edx,_dr_addr+7*4
      movl    %edx, %eax
      leave
      ret
      .data
dr_msk:
      .long   ~0x000f0003
      .long   ~0x00f0000c
      .long   ~0x0f000030
      .long   ~0xf00000c0
ENTRY(dr_addr)
      .long   0,0,0,0
      .long   0,0,0,0
      .text

