#ifndef WWW_WAIT_H
#define WWW_WAIT_H 1

#include <HTUtils.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WEXITSTATUS(status)	(status.w_retcode)
# else
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
# endif
#endif

#ifndef WTERMSIG
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WTERMSIG(status)	(status.w_termsig)
# else
#  define	WTERMSIG(status)	((status) & 0x7f)
# endif
#endif

#endif /* WWW_WAIT_H */
