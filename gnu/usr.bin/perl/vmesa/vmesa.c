/************************************************************/
/*                                                          */
/* Module ID  - vmesa.c                                     */
/*                                                          */
/* Function   - Provide operating system dependent process- */
/*              ing for perl under VM/ESA.                  */
/*                                                          */
/* Parameters - See individual entry points.                */
/*                                                          */
/* Called By  - N/A - see individual entry points.          */
/*                                                          */
/* Calling To - N/A - see individual entry points.          */
/*                                                          */
/* Notes      - (1) ....................................... */
/*                                                          */
/*              (2) ....................................... */
/*                                                          */
/* Name       - Neale Ferguson.                             */
/*                                                          */
/* Date       - August, 1998.                               */
/*                                                          */
/*                                                          */
/* Associated    - (1) Refer To ........................... */
/* Documentation                                            */
/*                 (2) Refer To ........................... */
/*                                                          */
/************************************************************/
/************************************************************/
/*                                                          */
/*                MODULE MAINTENANCE HISTORY                */
/*                --------------------------                */
/*                                                          */
static char REQ_REL_WHO [13] =
/*--------------       -------------------------------------*/
    "9999_99 NAF "; /* Original module                      */
/*                                                          */
/*============ End of Module Maintenance History ===========*/

/************************************************************/
/*                                                          */
/*                       DEFINES                            */
/*                       -------                            */
/*                                                          */
/************************************************************/

#define FAIL  65280

/*=============== END OF DEFINES ===========================*/

/************************************************************/
/*                                                          */
/*                INCLUDE STATEMENTS                        */
/*                ------------------                        */
/*                                                          */
/************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dll.h>
#include "EXTERN.h"
#include "perl.h"
#pragma map(truncate, "@@TRUNC")

/*================== End of Include Statements =============*/

/************************************************************/
/*                                                          */
/*               Global Variables                           */
/*               ----------------                           */
/*                                                          */
/************************************************************/

static int Perl_stdin_fd  = STDIN_FILENO,
           Perl_stdout_fd = STDOUT_FILENO;

static long dl_retcode = 0;

/*================== End of Global Variables ===============*/

/************************************************************/
/*                                                          */
/*               FUNCTION PROTOTYPES                        */
/*               -------------------                        */
/*                                                          */
/************************************************************/

int    do_aspawn(SV *, SV **, SV **);
int    do_spawn(char *, int);
static int spawnit(char *);
static pid_t spawn_cmd(char *, int, int);
struct perl_thread * getTHR(void);

/*================== End of Prototypes =====================*/

/************************************************************/
/*                                                          */
/*                     D O _ A S P A W N                    */
/*                     -----------------                    */
/*                                                          */
/************************************************************/

int
do_aspawn(SV* really, SV **mark, SV **sp)
{
 char   **a,
        *tmps;
 struct inheritance inherit;
 pid_t  pid;
 int    status,
        fd,
        nFd,
        fdMap[3];
 SV     *sv,
        **p_sv;
 STRLEN	n_a;

    status = FAIL;
    if (sp > mark)
    {
       New(401,PL_Argv, sp - mark + 1, char*);
       a = PL_Argv;
       while (++mark <= sp)
       {
           if (*mark)
              *a++ = SvPVx(*mark, n_a);
           else
              *a++ = "";
       }
       inherit.flags        = SPAWN_SETGROUP;
       inherit.pgroup       = SPAWN_NEWPGROUP;
       fdMap[STDIN_FILENO]  = Perl_stdin_fd;
       fdMap[STDOUT_FILENO] = Perl_stdout_fd;
       fdMap[STDERR_FILENO] = STDERR_FILENO;
       nFd                  = 3;
       *a = Nullch;
       /*-----------------------------------------------------*/
       /* Will execvp() use PATH?                             */
       /*-----------------------------------------------------*/
       if (*PL_Argv[0] != '/')
           TAINT_ENV();
       if (really && *(tmps = SvPV(really, n_a)))
           pid = spawnp(tmps, nFd, fdMap, &inherit,
                        (const char **) PL_Argv,
                        (const char **) environ);
       else
           pid = spawnp(PL_Argv[0], nFd, fdMap, &inherit,
                        (const char **) PL_Argv,
                        (const char **) environ);
       if (pid < 0)
       {
          status = FAIL;
          if (ckWARN(WARN_EXEC))
             warner(WARN_EXEC,"Can't exec \"%s\": %s",
                    PL_Argv[0],
                    Strerror(errno));
       }
       else
       {
          /*------------------------------------------------*/
          /* If the file descriptors have been remapped then*/
          /* we've been called following a my_popen request */
          /* therefore we don't want to wait for spawnned   */
          /* program to complete. We need to set the fdpid  */
          /* value to the value of the spawnned process' pid*/
          /*------------------------------------------------*/
          fd = 0;
          if (Perl_stdin_fd != STDIN_FILENO)
             fd = Perl_stdin_fd;
          else
             if (Perl_stdout_fd != STDOUT_FILENO)
                fd = Perl_stdout_fd;
          if (fd != 0)
          {
             /*---------------------------------------------*/
             /* Get the fd of the other end of the pipe,    */
             /* use this to reference the fdpid which will  */
             /* be used by my_pclose                        */
             /*---------------------------------------------*/
             close(fd);
             MUTEX_LOCK(&PL_fdpid_mutex);
             p_sv  = av_fetch(PL_fdpid,fd,TRUE);
             fd    = (int) SvIVX(*p_sv);
             SvREFCNT_dec(*p_sv);
             *p_sv = &PL_sv_undef;
             sv    = *av_fetch(PL_fdpid,fd,TRUE);
             MUTEX_UNLOCK(&PL_fdpid_mutex);
             (void) SvUPGRADE(sv, SVt_IV);
             SvIVX(sv) = pid;
             status    = 0;
          }
          else
             wait4pid(pid, &status, 0);
       }
       do_execfree();
    }
    return (status);
}

/*===================== End of do_aspawn ===================*/

/************************************************************/
/*                                                          */
/*                     D O _ S P A W N                      */
/*                     ---------------                      */
/*                                                          */
/************************************************************/

int
do_spawn(char *cmd, int execf)
{
 char   **a,
        *s,
        flags[10];
 int    status,
        nFd,
        fdMap[3];
 struct inheritance inherit;
 pid_t  pid;

    while (*cmd && isSPACE(*cmd))
       cmd++;

    /*------------------------------------------------------*/
    /* See if there are shell metacharacters in it          */
    /*------------------------------------------------------*/

    if (*cmd == '.' && isSPACE(cmd[1]))
       return (spawnit(cmd));
    else
    {
       if (strnEQ(cmd,"exec",4) && isSPACE(cmd[4]))
          return (spawnit(cmd));
       else
       {
          /*------------------------------------------------*/
          /* Catch VAR=val gizmo                            */
          /*------------------------------------------------*/
          for (s = cmd; *s && isALPHA(*s); s++);
          if (*s != '=')
          {
             for (s = cmd; *s; s++)
             {
                if (*s != ' ' &&
                    !isALPHA(*s) &&
                    strchr("$&*(){}[]'\";\\|?<>~`\n",*s))
                {
                   if (*s == '\n' && !s[1])
                   {
                      *s = '\0';
                      break;
                   }
                   return(spawnit(cmd));
                }
             }
          }
       }
    }

    New(402,PL_Argv, (s - cmd) / 2 + 2, char*);
    PL_Cmd = savepvn(cmd, s-cmd);
    a = PL_Argv;
    for (s = PL_Cmd; *s;)
    {
       while (*s && isSPACE(*s)) s++;
       if (*s)
           *(a++) = s;
       while (*s && !isSPACE(*s)) s++;
       if (*s)
           *s++ = '\0';
    }
    *a                   = Nullch;
    fdMap[STDIN_FILENO]  = Perl_stdin_fd;
    fdMap[STDOUT_FILENO] = Perl_stdout_fd;
    fdMap[STDERR_FILENO] = STDERR_FILENO;
    nFd                  = 3;
    inherit.flags        = 0;
    if (PL_Argv[0])
    {
       pid = spawnp(PL_Argv[0], nFd, fdMap, &inherit,
                    (const char **) PL_Argv,
                    (const char **) environ);
       if (pid < 0)
       {
          status = FAIL;
          if (ckWARN(WARN_EXEC))
             warner(WARN_EXEC,"Can't exec \"%s\": %s",
                    PL_Argv[0],
                    Strerror(errno));
       }
       else
          wait4pid(pid, &status, 0);
    }
    do_execfree();
    return (status);
}

/*===================== End of do_spawn ====================*/

/************************************************************/
/*                                                          */
/* Name      - spawnit.                                     */
/*                                                          */
/* Function  - Spawn command and return status.             */
/*                                                          */
/* On Entry  - cmd - command to be spawned.                 */
/*                                                          */
/* On Exit   - status returned.                             */
/*                                                          */
/************************************************************/

int
spawnit(char *cmd)
{
 pid_t  pid;
 int    status;

    pid = spawn_cmd(cmd, STDIN_FILENO, STDOUT_FILENO);
    if (pid < 0)
       status = FAIL;
    else
       wait4pid(pid, &status, 0);

    return (status);
}

/*===================== End of spawnit =====================*/

/************************************************************/
/*                                                          */
/* Name      - spawn_cmd.                                   */
/*                                                          */
/* Function  - Spawn command and return pid.                */
/*                                                          */
/* On Entry  - cmd - command to be spawned.                 */
/*                                                          */
/* On Exit   - pid returned.                                */
/*                                                          */
/************************************************************/

pid_t
spawn_cmd(char *cmd, int inFd, int outFd)
{
 struct inheritance inherit;
 pid_t  pid;
 const  char *argV[4] = {"/bin/sh","-c",NULL,NULL};
 int    nFd,
        fdMap[3];

    argV[2]              = cmd;
    fdMap[STDIN_FILENO]  = inFd;
    fdMap[STDOUT_FILENO] = outFd;
    fdMap[STDERR_FILENO] = STDERR_FILENO;
    nFd                  = 3;
    inherit.flags        = SPAWN_SETGROUP;
    inherit.pgroup       = SPAWN_NEWPGROUP;
    pid = spawn(argV[0], nFd, fdMap, &inherit,
                argV, (const char **) environ);
    return (pid);
}

/*===================== End of spawnit =====================*/

/************************************************************/
/*                                                          */
/* Name      - my_popen.                                    */
/*                                                          */
/* Function  - Use popen to execute a command return a      */
/*             file descriptor.                             */
/*                                                          */
/* On Entry  - cmd - command to be executed.                */
/*                                                          */
/* On Exit   - FILE * returned.                             */
/*                                                          */
/************************************************************/

#include <ctest.h>
PerlIO *
my_popen(char *cmd, char *mode)
{
 FILE *fd;
 int  pFd[2],
      this,
      that,
      pid;
 SV   *sv;

   if (PerlProc_pipe(pFd) >= 0)
   {
      this = (*mode == 'w');
      that = !this;
      /*-------------------------------------------------*/
      /* If this is a read mode pipe                     */
      /* - map the write end of the pipe to STDOUT       */
      /* - return the *FILE for the read end of the pipe */
      /*-------------------------------------------------*/
      if (!this)
         Perl_stdout_fd = pFd[that];
      /*-------------------------------------------------*/
      /* Else                                            */
      /* - map the read end of the pipe to STDIN         */
      /* - return the *FILE for the write end of the pipe*/
      /*-------------------------------------------------*/
      else
         Perl_stdin_fd = pFd[that];
      if (strNE(cmd,"-"))
      {
         PERL_FLUSHALL_FOR_CHILD;
         pid = spawn_cmd(cmd, Perl_stdin_fd, Perl_stdout_fd);
         if (pid >= 0)
         {
            MUTEX_LOCK(&PL_fdpid_mutex);
            sv = *av_fetch(PL_fdpid,pFd[this],TRUE);
            MUTEX_UNLOCK(&PL_fdpid_mutex);
            (void) SvUPGRADE(sv, SVt_IV);
            SvIVX(sv) = pid;
            fd = PerlIO_fdopen(pFd[this], mode);
            close(pFd[that]);
         }
         else
            fd = Nullfp;
      }
      else
      {
         MUTEX_LOCK(&PL_fdpid_mutex);
         sv = *av_fetch(PL_fdpid,pFd[that],TRUE);
         MUTEX_UNLOCK(&PL_fdpid_mutex);
         (void) SvUPGRADE(sv, SVt_IV);
         SvIVX(sv) = pFd[this];
         fd = PerlIO_fdopen(pFd[this], mode);
      }
   }
   else
      fd = Nullfp;
   return (fd);
}

/*===================== End of my_popen ====================*/

/************************************************************/
/*                                                          */
/* Name      - my_pclose.                                   */
/*                                                          */
/* Function  - Use pclose to terminate a piped command      */
/*             file stream.                                 */
/*                                                          */
/* On Entry  - fd  - FILE pointer.                          */
/*                                                          */
/* On Exit   - Status returned.                             */
/*                                                          */
/************************************************************/

long
my_pclose(FILE *fp)
{
 int  pid,
      saveErrno,
      status;
 long rc,
      wRc;
 SV   **sv;
 FILE *other;

   MUTEX_LOCK(&PL_fdpid_mutex);
   sv        = av_fetch(PL_fdpid,PerlIO_fileno(fp),TRUE);
   MUTEX_UNLOCK(&PL_fdpid_mutex);
   pid       = (int) SvIVX(*sv);
   SvREFCNT_dec(*sv);
   *sv       = &PL_sv_undef;
   rc        = PerlIO_close(fp);
   saveErrno = errno;
   do
   {
      wRc = waitpid(pid, &status, 0);
   } while ((wRc == -1) && (errno == EINTR));
   Perl_stdin_fd  = STDIN_FILENO;
   Perl_stdout_fd = STDOUT_FILENO;
   errno          = saveErrno;
   if (rc != 0)
      SETERRNO(errno, garbage);
   return (rc);

}

/************************************************************/
/*                                                          */
/* Name      - dlopen.                                      */
/*                                                          */
/* Function  - Load a DLL.                                  */
/*                                                          */
/* On Exit   -                                              */
/*                                                          */
/************************************************************/

void *
dlopen(const char *path)
{
 dllhandle *handle;

fprintf(stderr,"Loading %s\n",path);
   handle     = dllload(path);
   dl_retcode = errno;
fprintf(stderr,"Handle %08X %s\n",handle,strerror(errno));
   return ((void *) handle);
}

/*===================== End of dlopen ======================*/

/************************************************************/
/*                                                          */
/* Name      - dlsym.                                       */
/*                                                          */
/* Function  - Locate a DLL symbol.                         */
/*                                                          */
/* On Exit   -                                              */
/*                                                          */
/************************************************************/

void *
dlsym(void *handle, const char *symbol)
{
 void *symLoc;

fprintf(stderr,"Finding %s\n",symbol);
   symLoc  = dllqueryvar((dllhandle *) handle, (char *) symbol);
   if (symLoc == NULL)
      symLoc = (void *) dllqueryfn((dllhandle *) handle,
                                   (char *) symbol);
   dl_retcode = errno;
   return(symLoc);
}

/*===================== End of dlsym =======================*/

/************************************************************/
/*                                                          */
/* Name      - dlerror.                                     */
/*                                                          */
/* Function  - Return the last errno pertaining to a DLL    */
/*             operation.                                   */
/*                                                          */
/* On Exit   -                                              */
/*                                                          */
/************************************************************/

void *
dlerror(void)
{
 char * dlEmsg;

 dlEmsg     = strerror(dl_retcode);
 dl_retcode = 0;
 return(dlEmsg);
}

/*===================== End of dlerror =====================*/

/************************************************************/
/*                                                          */
/* Name      - TRUNCATE.                                    */
/*                                                          */
/* Function  - Truncate a file identified by 'path' to      */
/*             a given length.                              */
/*                                                          */
/* On Entry  - path - Path of file to be truncated.         */
/*             length - length of truncated file.           */
/*                                                          */
/* On Exit   - retC - return code.                          */
/*                                                          */
/************************************************************/

int
truncate(const unsigned char *path, off_t length)
{
 int fd,
     retC;

   fd = open((const char *) path, O_RDWR);
   if (fd > 0)
   {
      retC = ftruncate(fd, length);
      close(fd);
   }
   else
      retC = fd;
   return(retC);
}

/*===================== End of trunc =======================*/
