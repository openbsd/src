/*	$OpenBSD: offs.h,v 1.1 2003/10/31 03:54:34 drahn Exp $	*/
/*	$NetBSD: hfs.h,v 1.1 2000/11/14 11:25:35 tsubai Exp $	*/

int offs_open(char *, struct open_file *);
int offs_close(struct open_file *);
int offs_read(struct open_file *, void *, size_t, size_t *);
int offs_write(struct open_file *, void *, size_t, size_t *);
off_t offs_seek(struct open_file *, off_t, int);
int offs_stat(struct open_file *, struct stat *);
