/*	$OpenBSD: libsa.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <lib/libsa/stand.h>

#define	EXEC_ELF
#define	EXEC_SOM

#define	DEFAULT_KERNEL_ADDRESS	0

extern dev_t bootdev;

void pdc_init(void);
struct pz_device;
struct pz_device *pdc_findev(int, int);

int iodcstrategy(void *, int, daddr_t, size_t, void *, size_t *);

int ctopen(struct open_file *, ...);
int ctclose(struct open_file *);

int dkopen(struct open_file *, ...);
int dkclose(struct open_file *);

int lfopen(struct open_file *, ...);
int lfstrategy(void *, int, daddr_t, size_t, void *, size_t *);
int lfclose(struct open_file *);

void ite_probe(struct consdev *);
void ite_init(struct consdev *);
int ite_getc(dev_t);
void ite_putc(dev_t, int);
void ite_pollc(dev_t, int);

void machdep(void);
void devboot(dev_t, char *);
void fcacheall(void);
void run_loadfile(u_long *marks, int howto);

int lif_open(char *path, struct open_file *f);
int lif_close(struct open_file *f);
int lif_read(struct open_file *f, void *buf, size_t size, size_t *resid);
int lif_write(struct open_file *f, void *buf, size_t size, size_t *resid);
off_t lif_seek(struct open_file *f, off_t offset, int where);
int lif_stat(struct open_file *f, struct stat *sb);
int lif_readdir(struct open_file *f, char *name);

union x_header;
struct x_param;
int som_probe(int, union x_header *);
int som_load(int, struct x_param *);
int som_ldsym(int, struct x_param *);

extern int debug;

#define	MACHINE_CMD	cmd_machine	/* we have hppa specific commands */
