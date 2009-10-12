/* Beginning of modification history */
/* Written 02-01-02 by Nick Ing-Simmons (nick@ing-simmons.net) */
/* Modified 02-03-27 by Paul Green (Paul.Green@stratus.com) to
     add socketpair() dummy. */
/* Modified 02-04-24 by Paul Green (Paul.Green@stratus.com) to
     have pow(0,0) return 1, avoiding c-1471. */
/* Modified 06-09-25 by Paul Green (Paul.Green@stratus.com) to
     add syslog entries. */
/* Modified 08-02-04 by Paul Green (Paul.Green@stratus.com) to
     open the syslog file in the working dir. */
/* End of modification history */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "vos/syslog.h"

/* VOS doesn't supply a truncate function, so we build one up
   from the available POSIX functions.  */

int
truncate(const char *path, off_t len)
{
 int fd = open(path,O_WRONLY);
 int code = -1;
 if (fd >= 0) {
   code = ftruncate(fd,len);
   close(fd); 
 }
 return code;
}

/* VOS doesn't implement AF_UNIX (AF_LOCAL) style sockets, and
   the perl emulation of them hangs on VOS (due to stcp-1257),
   so we supply this version that always fails.  */

int
socketpair (int family, int type, int protocol, int fd[2]) {
 fd[0] = 0;
 fd[1] = 0;
 errno = ENOSYS;
 return -1;
}

/* Supply a private version of the power function that returns 1
   for x**0.  This avoids c-1471.  Abigail's Japh tests depend
   on this fix.  We leave all the other cases to the VOS C
   runtime.  */

double s_crt_pow(double *x, double *y);

double pow(x,y)
double x, y;
{
     if (y == 0e0)                 /* c-1471 */
     {
          errno = EDOM;
          return (1e0);
     }

     return(s_crt_pow(&x,&y));
}

/* entries */

extern void s$log_system_message (
/*             char_varying (256)  *message_text, 
               char_varying (66)   *module_name, 
               short int           *error_code */ );

/* constants */

#define    ALL_PRIORITIES 255  /* 8 priorities, all enabled */
#define    BUFFER_LEN 256
#define    IDENT_LEN 64
#define    MSG_LEN 256
#define    PATH_LEN 257

/* static */

int  vos_syslog_facility = LOG_USER>>3;
int  vos_syslog_fd = -1;
int  vos_syslog_logopt = 0;
char vos_syslog_ident[IDENT_LEN] = "";
int  vos_syslog_ident_len = 0;
int  vos_syslog_mask = ALL_PRIORITIES;
char vos_syslog_path[PATH_LEN] = "syslog";

char vos_syslog_facility_name [17][10] = {
     "[KERN] ",    /* LOG_KERN */
     "[USER] ",    /* LOG_USER */
     "[MAIL] ",    /* LOG_MAIL */
     "[NEWS] ",    /* LOG_NEWS */
     "[UUCP] ",    /* LOG_UUCP */
     "[DAEMON] ",  /* LOG_DAEMON */
     "[AUTH] ",    /* LOG_AUTH */
     "[CRON] ",    /* LOG_CRON */
     "[LPR] ",     /* LOG_LPR */
     "[LOCAL0] ",  /* LOG_LOCAL0 */
     "[LOCAL1] ",  /* LOG_LOCAL1 */
     "[LOCAL2] ",  /* LOG_LOCAL2 */
     "[LOCAL3] ",  /* LOG_LOCAL3 */
     "[LOCAL4] ",  /* LOG_LOCAL4 */
     "[LOCAL5] ",  /* LOG_LOCAL5 */
     "[LOCAL6] ",  /* LOG_LOCAL6 */
     "[LOCAL7] "}; /* LOG_LOCAL7 */

/* syslog functions */

static void open_syslog (void)
{
     if (vos_syslog_fd >= 0)
          return;

     vos_syslog_fd = open (vos_syslog_path, O_RDWR | O_CREAT | O_APPEND, 0777);
     if (vos_syslog_fd < 0)
          fprintf (stderr, "Unable to open %s (errno=%d, os_errno=%d)\n",
               vos_syslog_path, errno, os_errno);
}

void closelog (void)
{
     if (vos_syslog_fd >= 0)
          close (vos_syslog_fd);

     vos_syslog_facility = LOG_USER>>3;
     vos_syslog_fd = -1;
     vos_syslog_logopt = 0;
     vos_syslog_ident[0] = '\0';
     vos_syslog_ident_len = 0;
     vos_syslog_mask = ALL_PRIORITIES;
     return;
}

void openlog (const char *ident, int logopt, int facility)
{
int  n;

     if (ident != NULL)
     {
          strncpy (vos_syslog_ident, ident, sizeof (vos_syslog_ident));
          n = IDENT_LEN -
               strnlen (vos_syslog_ident, sizeof (vos_syslog_ident));
          strncat (vos_syslog_ident, ": ", n);
          vos_syslog_ident_len = strnlen (vos_syslog_ident,
               sizeof (vos_syslog_ident));
     }

     vos_syslog_logopt = logopt;
     vos_syslog_facility = facility>>3;

     if ((logopt & LOG_NDELAY) == LOG_NDELAY)
          open_syslog ();

     return;
}

int setlogmask (int maskpri)
{
int  old_mask;

     old_mask = vos_syslog_mask;

     if (maskpri > 0)
          vos_syslog_mask = maskpri;

     return old_mask;
}

void syslog (int priority, const char *format, ...)
{
va_list             ap;
int                 bare_facility;
int                 bare_priority;
int                 buffer_n;
char                buffer[BUFFER_LEN];
short int           code;
char_varying(MSG_LEN) message;
char_varying(66)    module_name;
int                 n;
int                 pid_n;
char                pid_string[32];
int                 r;
int                 user_n;
char                user_string[256];

     /* Calculate priority and facility value.  */

     bare_priority = priority & 3;
     bare_facility = priority >> 3;

     /* If the priority is not set in the mask, do not log the
        message.  */

     if ((vos_syslog_mask & LOG_MASK(bare_priority)) == 0)
          return;

     /* Output facility name.  */

     if (bare_facility == 0)
          bare_facility = vos_syslog_facility;

     strcpy (buffer, vos_syslog_facility_name[bare_facility]);

     /* Output priority value. */

     /* TBD */

     /* Output identity string. */

     buffer_n = BUFFER_LEN - strlen (buffer);
     strncat (buffer, vos_syslog_ident, buffer_n);

     /* Output process ID.  */

     if ((vos_syslog_logopt & LOG_PID) == LOG_PID)
     {
          pid_n = snprintf (pid_string, sizeof (pid_string),
               "PID=0x%x ", getpid ());
          if (pid_n)
          {
               buffer_n = BUFFER_LEN - strlen (buffer);
               strncat (buffer, pid_string, buffer_n);
          }
     }

     /* Output formatted message.  */

     va_start (ap, format);
     user_n = vsnprintf (user_string, sizeof (user_string), format, ap);
     va_end (ap);

     /* Ensure string ends in a newline.  */

     if (user_n > 0)
     {
          if (user_n >= sizeof (user_string))
               user_n = sizeof (user_string) - 1;

          /* arrays are zero-origin.... */

          if (user_string [user_n-1] != '\n')
          {
               user_string [user_n-1] = '\n';
               user_string [user_n++] = '\0';
          }
     }        
     else
     {
          user_string [0] = '\n';
          user_string [1] = '\0';
          user_n = 1;
     }

     buffer_n = BUFFER_LEN - strnlen (buffer, sizeof (buffer));
     strncat (buffer, user_string, buffer_n);

     /* If the log is not open, try to open it now.  */

     if (vos_syslog_fd < 0)
          open_syslog ();

     /* Try to write the message to the syslog file.  */

     if (vos_syslog_fd < 0)
          r = -1;
     else
     {
          buffer_n = strnlen (buffer, sizeof (buffer));
          r = write (vos_syslog_fd, buffer, buffer_n);
     }

     /* If we were unable to write to the log and if LOG_CONS is
        set, send it to the console.  */

     if (r < 0)
          if ((vos_syslog_logopt & LOG_CONS) == LOG_CONS)
          {
               strcpy_vstr_nstr (&message, "syslog: ");
               n = MSG_LEN - sizeof ("syslog: ");
               strncat_vstr_nstr (&message, buffer, n);
               strcpy_vstr_nstr (&module_name, "");
               s$log_system_message (&message, &module_name, &code);
          }

     return;
}
