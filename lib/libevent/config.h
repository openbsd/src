/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if kqueue works correctly with pipes */
#define HAVE_WORKING_KQUEUE 1

/* Define if timeradd is defined in <sys/time.h> */
#define HAVE_TIMERADD 1
#ifndef HAVE_TIMERADD
#define timeradd(tvp, uvp, vvp)      \
 do {        \
  (vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;  \
  (vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
  if ((vvp)->tv_usec >= 1000000) {   \
   (vvp)->tv_sec++;    \
   (vvp)->tv_usec -= 1000000;   \
  }       \
 } while (0)
#define timersub(tvp, uvp, vvp)      \
 do {        \
  (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;  \
  (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec; \
  if ((vvp)->tv_usec < 0) {    \
   (vvp)->tv_sec--;    \
   (vvp)->tv_usec += 1000000;   \
  }       \
 } while (0)
#endif /* !HAVE_TIMERADD */

/* Define if TAILQ_FOREACH is defined in <sys/queue.h> */
#define HAVE_TAILQFOREACH 1
#ifndef HAVE_TAILQFOREACH
#define TAILQ_FIRST(head)  ((head)->tqh_first)
#define TAILQ_END(head)   NULL
#define TAILQ_NEXT(elm, field)  ((elm)->field.tqe_next)
#define TAILQ_FOREACH(var, head, field)     \
 for((var) = TAILQ_FIRST(head);     \
     (var) != TAILQ_END(head);     \
     (var) = TAILQ_NEXT(var, field))
#define TAILQ_INSERT_BEFORE(listelm, elm, field) do {   \
 (elm)->field.tqe_prev = (listelm)->field.tqe_prev;  \
 (elm)->field.tqe_next = (listelm);    \
 *(listelm)->field.tqe_prev = (elm);    \
 (listelm)->field.tqe_prev = &(elm)->field.tqe_next;  \
} while (0)
#endif /* TAILQ_FOREACH */

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the kqueue function.  */
#define HAVE_KQUEUE 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the warnx function.  */
#define HAVE_WARNX 1

/* Define if you have the <sys/event.h> header file.  */
#define HAVE_SYS_EVENT_H 1

/* Define if you have the <sys/queue.h> header file.  */
#define HAVE_SYS_QUEUE_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Name of package */
#define PACKAGE "libevent"

/* Version number of package */
#define VERSION "0.5"

