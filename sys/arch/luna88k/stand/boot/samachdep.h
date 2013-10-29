/*	$OpenBSD: samachdep.h,v 1.2 2013/10/29 18:51:37 miod Exp $	*/
/*	$NetBSD: samachdep.h,v 1.10 2013/03/05 15:34:53 tsutsui Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)samachdep.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <lib/libsa/stand.h>

#define	NSCSI		2
#define NSD		8

#define MHZ_25		25
#define MHZ_33		33

#define MAXDEVNAME	16

struct consdev;
typedef struct label_t {
	long val[19];
} label_t;

/* autoconf.c */
void configure(void);
void find_devs(void);

/* awaitkey.c */
char awaitkey(const char *, int, int);

/* bcd.c */
unsigned int bcdtobin(unsigned int);

/* bmc.c */
void bmccnprobe(struct consdev *);
void bmccninit(struct consdev *);
int  bmccngetc(dev_t);
void bmccnputc(dev_t, int);

/* bmd.c */
void bmdinit(void);
int bmdputc(int);
void bmdadjust(short, short);
void bmdclear(void);

/* boot.c */
extern int howto;
int boot(int, char **);
int bootunix(char *);

extern void (*cpu_boot)(uint32_t, uint32_t);
extern uint32_t cpu_bootarg1;
extern uint32_t cpu_bootarg2;
#define	BOOT_MAGIC	0xf1abde3f

/* cons.c */
void cninit(void);
int cngetc(void);
void cnputc(int);

/* devopen.c */
extern	u_int opendev;
int atoi(char *);

/* fault.c */
int badaddr(void *, int);

/* font.c */
extern const u_short bmdfont[][20];

/* getline.c */
int getline(char *, char *);

/* if_le.c */
int leinit(void *);

/* init_main.c */
extern int cpuspeed;
extern int nplane;
extern int machtype;
extern char default_file[];
extern char fuse_rom_data[];

/* kbd.c */
int kbd_decode(u_char);

/* lance.c */
void *lance_attach(int, void *, void *, uint8_t *);
void *lance_cookie(int);
uint8_t *lance_eaddr(void *);
int lance_init(void *);
int lance_get(void *, void *, size_t);
int lance_put(void *, void *, size_t);
int lance_end(void *);

/* locore.S */
extern u_int bootdev;
extern uint16_t dipswitch;
extern volatile uint32_t tick;
int setjmp(label_t *);
void delay(int);

/* prf.c */
int tgetchar(void);

/* parse.c */
int exit_program(int, char **);
int parse(int, char **);
int getargs(char *, char **, int);

/* sc.c */
struct scsi_fmt_cdb;
int scsi_immed_command(int, int, int, struct scsi_fmt_cdb *, u_char *,
    unsigned int);
int scsi_request_sense(int, int, int, u_char *, unsigned int);
int scsi_test_unit_rdy(int, int, int);

/* sd.c */
int sdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int sdopen(struct open_file *, ...);
int sdclose(struct open_file *);

/* sio.c */
void _siointr(void);
void siocnprobe(struct consdev *);
void siocninit(struct consdev *);
int  siocngetc(dev_t);
void siocnputc(dev_t, int);
void sioinit(void);

/* ufs_disklabel.c */
char *readdisklabel(int, int, struct disklabel *);

#define DELAY(n)	delay(n)

extern	struct fs_ops file_system_disk[];
extern	int nfsys_disk;
extern	struct fs_ops file_system_nfs[];
