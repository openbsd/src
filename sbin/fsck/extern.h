/*	$NetBSD: extern.h,v 1.4 1995/04/12 21:24:07 mycroft Exp $	*/

/*
 * Copyright (c) 1994 James A. Jegers
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

void adjust __P((struct inodesc *, short));
int allocblk __P((long));
int allocdir __P((ino_t, ino_t, int));
void blkerror __P((ino_t, char *, daddr_t));
int bread __P((int, char *, daddr_t, long));
void bufinit();
void bwrite __P((int, char *, daddr_t, long));
void cacheino __P((struct dinode *, ino_t));
int changeino __P((ino_t, char *, ino_t));
int checkfstab __P((int, int, int (*)(), int (*)() ));
int chkrange __P((daddr_t, int));
void ckfini __P((int));
int ckinode __P((struct dinode *, struct inodesc *));
void clri __P((struct inodesc *, char *, int));
int dircheck __P((struct inodesc *, struct direct *));
void direrror __P((ino_t, char *));
int dirscan __P((struct inodesc *));
int dofix __P((struct inodesc *, char *));
void fileerror __P((ino_t, ino_t, char *));
int findino __P((struct inodesc *));
int findname __P((struct inodesc *));
void flush __P((int, struct bufarea *));
void freeblk __P((daddr_t, long));
void freeino __P((ino_t));
void freeinodebuf();
int ftypeok __P((struct dinode *));
void getpathname __P((char *, ino_t, ino_t));
void inocleanup();
void inodirty();
int linkup __P((ino_t, ino_t));
int makeentry __P((ino_t, ino_t, char *));
void pass1();
void pass1b();
void pass2();
void pass3();
void pass4();
int pass4check();
void pass5();
void pinode __P((ino_t));
void propagate();
int reply __P((char *));
void resetinodebuf();
int setup __P((char *));

