/*	$OpenBSD: ksh_dir.h,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/* Wrapper around the ugly dir includes/ifdefs */

#if defined(HAVE_DIRENT_H)
# include <dirent.h>
# define NLENGTH(dirent)	(strlen(dirent->d_name))
#else
# define dirent direct
# define NLENGTH(dirent)	(dirent->d_namlen)
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYSDIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */

#ifdef OPENDIR_DOES_NONDIR
extern DIR *ksh_opendir ARGS((const char *d));
#else /* OPENDIR_DOES_NONDIR */
# define ksh_opendir(d)	opendir(d)
#endif /* OPENDIR_DOES_NONDIR */
