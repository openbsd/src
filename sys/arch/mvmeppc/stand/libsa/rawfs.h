/*	$OpenBSD: rawfs.h,v 1.2 2002/03/14 01:26:41 millert Exp $	*/

/*
 * Raw file system - for stream devices like tapes.
 * No random access, only sequential read allowed.
 */

int	rawfs_open(char *path, struct open_file *f);
int	rawfs_close(struct open_file *f);
int	rawfs_read(struct open_file *f, void *buf,
		u_int size, u_int *resid);
int	rawfs_write(struct open_file *f, void *buf,
		u_int size, u_int *resid);
off_t	rawfs_seek(struct open_file *f, off_t offset, int where);
int	rawfs_stat(struct open_file *f, struct stat *sb);
