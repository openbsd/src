#include <libc/stubs.h>
#include <io.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libc/file.h>
#include <process.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/fsext.h>
#include <crt0.h>
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* hold file pointer, command, mode, and the status of the command */
struct pipe_list {
  FILE *fp;
  int exit_status;
  struct pipe_list *next;
  char *command, mode;
};

/* static, global list pointer */
static struct pipe_list *pl = NULL;

FILE *
popen (const char *cm, const char *md) /* program name, pipe mode */
{
  struct pipe_list *l1;
  int fd;
  char *temp_name=NULL;

  /* make new node */
  if ((l1 = (struct pipe_list *) malloc (sizeof (*l1)))
     && (temp_name = malloc (L_tmpnam)) && tmpnam (temp_name))
  {
    l1->fp = NULL;
    l1->command = NULL;
    l1->next = pl;
    l1->exit_status = -1;
    l1->mode = md[0];

    /* if caller wants to read */
    if (md[0] == 'r' && (fd = dup (fileno (stdout))) >= 0)
    {
      if ((l1->fp = freopen (temp_name, "wb", stdout)))
      {
        l1->exit_status = system (cm);
        if (dup2 (fd, fileno (stdout)) >= 0)
          l1->fp = fopen (temp_name, md);
      }
      close (fd);
    }
    /* if caller wants to write */
    else if (md[0] == 'w' && (l1->command = malloc (1 + strlen (cm))))
    {
      strcpy (l1->command, cm);
      l1->fp = fopen (temp_name, md);
    }

    if (l1->fp)
    {
      l1->fp->_flag |= _IORMONCL; /* remove on close */
      l1->fp->_name_to_remove = temp_name;
      return (pl = l1)->fp;
    }
    free (l1->command);
  }
  free (temp_name);
  free (l1);
  return NULL;
}

int
pclose (FILE *pp)
{
  struct pipe_list *l1, **l2;   /* list pointers */
  int retval=-1;		/* function return value */

  for (l2 = &pl; *l2 && (*l2)->fp != pp; l2 = &((*l2)->next))
    ;
  if (!(l1 = *l2))
    return retval;
  *l2 = l1->next;

  /* if pipe was opened to write */
  if (l1->mode == 'w')
  {
    int fd;
    fflush (l1->fp);
    close (fileno (l1->fp)); 

    if ((fd = dup (fileno (stdin))) >= 0
       && (freopen (l1->fp->_name_to_remove, "rb", stdin)))
    {
      retval = system (l1->command);
      dup2 (fd, fileno (stdin));
    }
    close (fd);
    free (l1->command);
  }
  else
    /* if pipe was opened to read, return the exit status we saved */
    retval = l1->exit_status;

  fclose (l1->fp);              /* this removes the temp file */
  free (l1);
  return retval;                /* retval==0 ? OK : ERROR */
}

/**/

#define EXECF_SPAWN 0
#define EXECF_EXEC 1

static int
convretcode (pTHX_ int rc,char *prog,int fl)
{
    if (rc < 0 && ckWARN(WARN_EXEC))
        Perl_warner(aTHX_ WARN_EXEC,"Can't %s \"%s\": %s",
		    fl ? "exec" : "spawn",prog,Strerror (errno));
    if (rc >= 0)
        return rc << 8;
    return -1;
}

int
do_aspawn (pTHX_ SV *really,SV **mark,SV **sp)
{
    int  rc;
    char **a,*tmps,**argv; 
    STRLEN n_a;

    if (sp<=mark)
        return -1;
    a=argv=(char**) alloca ((sp-mark+3)*sizeof (char*));

    while (++mark <= sp)
        if (*mark)
            *a++ = SvPVx(*mark, n_a);
        else
            *a++ = "";
    *a = Nullch;

    if (argv[0][0] != '/' && argv[0][0] != '\\'
        && !(argv[0][0] && argv[0][1] == ':'
        && (argv[0][2] == '/' || argv[0][2] != '\\'))
     ) /* will swawnvp use PATH? */
         TAINT_ENV();	/* testing IFS here is overkill, probably */

    if (really && *(tmps = SvPV(really, n_a)))
        rc=spawnvp (P_WAIT,tmps,argv);
    else
        rc=spawnvp (P_WAIT,argv[0],argv);

    return convretcode (rc,argv[0],EXECF_SPAWN);
}

#define EXTRA "\x00\x00\x00\x00\x00\x00"

int
do_spawn2 (pTHX_ char *cmd,int execf)
{
    char **a,*s,*shell,*metachars;
    int  rc,unixysh;

    if ((shell=getenv("SHELL"))==NULL && (shell=getenv("COMSPEC"))==NULL)
    	shell="c:\\command.com" EXTRA;

    unixysh=_is_unixy_shell (shell);
    metachars=unixysh ? "$&*(){}[]'\";\\?>|<~`\n" EXTRA : "*?[|<>\"\\" EXTRA;

    while (*cmd && isSPACE(*cmd))
	cmd++;

    if (strnEQ (cmd,"/bin/sh",7) && isSPACE (cmd[7]))
        cmd+=5;

    /* save an extra exec if possible */
    /* see if there are shell metacharacters in it */
    if (strstr (cmd,"..."))
        goto doshell;
    if (unixysh)
    {
        if (*cmd=='.' && isSPACE (cmd[1]))
            goto doshell;
        if (strnEQ (cmd,"exec",4) && isSPACE (cmd[4]))
            goto doshell;
        for (s=cmd; *s && isALPHA (*s); s++) ;	/* catch VAR=val gizmo */
            if (*s=='=')
                goto doshell;
    }
    for (s=cmd; *s; s++)
	if (strchr (metachars,*s))
	{
	    if (*s=='\n' && s[1]=='\0')
	    {
		*s='\0';
		break;
	    }
doshell:
	    if (execf==EXECF_EXEC)
                return convretcode (execl (shell,shell,unixysh ? "-c" : "/c",cmd,NULL),cmd,execf);
            return convretcode (system (cmd),cmd,execf);
	}

    New (1303,PL_Argv,(s-cmd)/2+2,char*);
    PL_Cmd=savepvn (cmd,s-cmd);
    a=PL_Argv;
    for (s=PL_Cmd; *s;) {
	while (*s && isSPACE (*s)) s++;
	if (*s)
	    *(a++)=s;
	while (*s && !isSPACE (*s)) s++;
	if (*s)
	    *s++='\0';
    }
    *a=Nullch;
    if (!PL_Argv[0])
        return -1;

    if (execf==EXECF_EXEC)
        rc=execvp (PL_Argv[0],PL_Argv);
    else
        rc=spawnvp (P_WAIT,PL_Argv[0],PL_Argv);
    return convretcode (rc,PL_Argv[0],execf);
}

int
do_spawn (pTHX_ char *cmd)
{
    return do_spawn2 (aTHX_ cmd,EXECF_SPAWN);
}

bool
Perl_do_exec (pTHX_ char *cmd)
{
    do_spawn2 (aTHX_ cmd,EXECF_EXEC);
    return FALSE;
}

/**/

struct globinfo
{
    int    fd;
    char   *matches;
    size_t size;
    fpos_t pos;
};

#define MAXOPENGLOBS 10

static struct globinfo myglobs[MAXOPENGLOBS];

static struct globinfo *
searchfd (int fd)
{
    int ic;
    for (ic=0; ic<MAXOPENGLOBS; ic++)
        if (myglobs[ic].fd==fd)
            return myglobs+ic;
    return NULL;
}

static int
glob_handler (__FSEXT_Fnumber n,int *rv,va_list args)
{
    unsigned ic;
    struct globinfo *gi;
    switch (n)
    {
        case __FSEXT_open:
        {
            char   *p1,*pattern,*name=va_arg (args,char*);
            STRLEN len;
            glob_t pglob;

            if (strnNE (name,"/dev/dosglob/",13))
                break;
            if ((gi=searchfd (-1)) == NULL)
                break;

            gi->pos=0;
            pattern=alloca (strlen (name+=13)+1);
            strcpy (pattern,name);
            if (!_USE_LFN)
                strlwr (pattern);
            ic=pglob.gl_pathc=0;
            pglob.gl_pathv=NULL;
            while (pattern)
            {
                if ((p1=strchr (pattern,' '))!=NULL)
                    *p1=0;
                glob (pattern,ic,0,&pglob);
                ic=GLOB_APPEND;
                if ((pattern=p1)!=NULL)
                    pattern++;
            }
            for (ic=len=0; ic<pglob.gl_pathc; ic++)
                len+=1+strlen (pglob.gl_pathv[ic]);
            if (len)
            {
                if ((gi->matches=p1=(char*) malloc (gi->size=len))==NULL)
                    break;
                for (ic=0; ic<pglob.gl_pathc; ic++)
                {
                    strcpy (p1,pglob.gl_pathv[ic]);
                    p1+=strlen (p1)+1;
                }
            }
            else
            {
                if ((gi->matches=strdup (name))==NULL)
                    break;
                gi->size=strlen (name)+1;
            }
            globfree (&pglob);
            gi->fd=*rv=__FSEXT_alloc_fd (glob_handler);
            return 1;
        }
        case __FSEXT_read:
        {
            int      fd=va_arg (args,int);
            char   *buf=va_arg (args,char*);
            size_t  siz=va_arg (args,size_t);

            if ((gi=searchfd (fd))==NULL)
                break;

            if (siz+gi->pos > gi->size)
                siz = gi->size - gi->pos;
            memcpy (buf,gi->pos+gi->matches,siz);
            gi->pos += siz;
            *rv=siz;
            return 1;
        }
        case __FSEXT_close:
        {
            int fd=va_arg (args,int);

            if ((gi=searchfd (fd))==NULL)
                break;
            free (gi->matches);
            gi->fd=-1;
            break;
        }
        default:
            break;
    }
    return 0;
}

static
XS(dos_GetCwd)
{
    dXSARGS;

    if (items)
        Perl_croak (aTHX_ "Usage: Dos::GetCwd()");
    {
        char tmp[PATH_MAX+2];
        ST(0)=sv_newmortal ();
        if (getcwd (tmp,PATH_MAX+1)!=NULL)
            sv_setpv ((SV*)ST(0),tmp);
    }
    XSRETURN (1);
}

static
XS(dos_UseLFN)
{
    dXSARGS;
    XSRETURN_IV (_USE_LFN);
}

void
Perl_init_os_extras(pTHX)
{
    char *file = __FILE__;

    dXSUB_SYS;
    
    newXS ("Dos::GetCwd",dos_GetCwd,file);
    newXS ("Dos::UseLFN",dos_UseLFN,file);

    /* install my File System Extension for globbing */
    __FSEXT_add_open_handler (glob_handler);
    memset (myglobs,-1,sizeof (myglobs));
}

static char *perlprefix;

#define PERL5 "/perl5"

char *djgpp_pathexp (const char *p)
{
    static char expp[PATH_MAX];
    strcpy (expp,perlprefix);
    switch (p[0])
    {
        case 'B':
            strcat (expp,"/bin");
            break;
        case 'S':
            strcat (expp,"/lib" PERL5 "/site");
            break;
        default:
            strcat (expp,"/lib" PERL5);
            break;
    }
    return expp;
}

void
Perl_DJGPP_init (int *argcp,char ***argvp)
{
    char *p;

    perlprefix=strdup (**argvp);
    strlwr (perlprefix);
    if ((p=strrchr (perlprefix,'/'))!=NULL)
    {
        *p=0;
        if (strEQ (p-4,"/bin"))
            p[-4]=0;
    }
    else
        strcpy (perlprefix,"..");
}

int
djgpp_fflush (FILE *fp)
{
    int res;

    if ((res = fflush(fp)) == 0 && fp) {
	Stat_t s;
	if (Fstat(fileno(fp), &s) == 0 && !S_ISSOCK(s.st_mode))
	    res = fsync(fileno(fp));
    }
/*
 * If the flush succeeded but set end-of-file, we need to clear
 * the error because our caller may check ferror().  BTW, this
 * probably means we just flushed an empty file.
 */
    if (res == 0 && fp && ferror(fp) == EOF) clearerr(fp);

    return res;
}
