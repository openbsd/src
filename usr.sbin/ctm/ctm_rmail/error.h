/* $OpenBSD: error.h,v 1.2 1999/07/13 23:02:06 deraadt Exp $ */

extern	void	err_set_log(char *log_file);
extern	void	err_prog_name(char *name);
extern	void	err(char *fmt, ...);
