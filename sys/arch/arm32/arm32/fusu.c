/* $NetBSD: fusu.c,v 1.2 1996/03/27 22:42:08 mark Exp $ */

/*
 * Copyright (C) 1993 Wolfgang Solfrank.
 * Copyright (C) 1993 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Emulate fubyte.
 */

int
fubyte(addr)
char *addr;
{
	unsigned char c;
	
	if (copyin(addr,&c,sizeof(c)))
		return -1;
	return c;
}

/*
 * Emulate fuibyte.
 * Note: This is the same as fubyte.
 *	 In case of separate I&D space this MUST be replaced.
 */

int
fuibyte(addr)
char *addr;
{
	unsigned char c;
	
	if (copyin(addr,&c,sizeof(c)))
		return -1;
	return c;
}

/*
 * Emulate fuiword
 * Note: This is the same as fuword.
 *	 In case of separate I&D space this MUST be replaced.
 */

int
fuiword(addr)
char *addr;
{
	unsigned long l;
	
	if (copyin(addr,&l,sizeof(l)))
		return -1;
	return l;
}

/*
 * Emulate fuswintr
 */

int
fuswintr(addr)
char *addr;
{
	unsigned short s;
	extern int nopagefault;
	int ret;
	
	nopagefault++;
	if (copyin(addr,&s,sizeof(s)))
		ret = -1;
	else
		ret = s;
	nopagefault--;
	return ret;
}

/*
 * Emulate fusword
 */

int
fusword(addr)
char *addr;
{
	unsigned short s;
	
	if (copyin(addr,&s,sizeof(s)))
		return -1;
	return s;
}

/*
 * Emulate fuword
 */

int
fuword(addr)
char *addr;
{
	unsigned long l;
	
	if (copyin(addr,&l,sizeof(l)))
		return -1;
	return l;
}

/*
 * Emulate subyte.
 */

int
subyte(addr,c)
char *addr;
unsigned char c;
{
	if (copyout(&c,addr,sizeof(c)))
		return -1;
	return 0;
}

/*
 * Emulate suibyte.
 * Note: This is the same as subyte.
 *	 In case of separate I&D space this MUST be replaced.
 */

int
suibyte(addr,c)
char *addr;
unsigned char c;
{
	if (copyout(&c,addr,sizeof(c)))
		return -1;
	return 0;
}

/*
 * Emulate suiword
 * Note: This is the same as suword.
 *	 In case of separate I&D space this MUST be replaced.
 */

int
suiword(addr,l)
char *addr;
unsigned long l;
{
	if (copyout(&l,addr,sizeof(l)))
		return -1;
	return 0;
}

/*
 * Emulate suswintr
 */

int
suswintr(addr,s)
char *addr;
unsigned short s;
{
	extern int nopagefault;
	int ret;
	
	nopagefault++;
	ret = copyout(&s,addr,sizeof(s)) ? -1 : 0;
	nopagefault--;
	return ret;
}

/*
 * Emulate susword
 */

int
susword(addr,s)
char *addr;
unsigned short s;
{
	if (copyout(&s,addr,sizeof(s)))
		return -1;
	return 0;
}

/*
 * Emulate suword
 */

int
suword(addr,l)
char *addr;
unsigned long l;
{
	if (copyout(&l,addr,sizeof(l)))
		return -1;
	return 0;
}
