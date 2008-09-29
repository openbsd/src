#define PERL_NO_GET_CONTEXT
#define WIN32_LEAN_AND_MEAN
#define WIN32IO_IS_STDIO
#include <tchar.h>
#ifdef __GNUC__
#define Win32_Winsock
#endif
#include <windows.h>
#include <cewin32.h>

#include <sys/stat.h>
#include "EXTERN.h"
#include "perl.h"

#ifdef PERLIO_LAYERS

#include "perliol.h"

#define NO_XSLOCKS
#include "XSUB.h"


/* Bottom-most level for Win32 case */

typedef struct
{
 struct _PerlIO base;       /* The generic part */
 HANDLE		h;          /* OS level handle */
 IV		refcnt;     /* REFCNT for the "fd" this represents */
 int		fd;         /* UNIX like file descriptor - index into fdtable */
} PerlIOWin32;

PerlIOWin32 *fdtable[256];
IV max_open_fd = -1;

IV
PerlIOWin32_popped(pTHX_ PerlIO *f)
{
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 if (--s->refcnt > 0)
  {
   *f = PerlIOBase(f)->next;
   return 1;
  }
 fdtable[s->fd] = NULL;
 return 0;
}

IV
PerlIOWin32_fileno(pTHX_ PerlIO *f)
{
 return PerlIOSelf(f,PerlIOWin32)->fd;
}

IV
PerlIOWin32_pushed(pTHX_ PerlIO *f, const char *mode, SV *arg, PerlIO_funcs *tab)
{
 IV code = PerlIOBase_pushed(aTHX_ f,mode,arg,tab);
 if (*PerlIONext(f))
  {
   PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
   s->fd     = PerlIO_fileno(PerlIONext(f));
  }
 PerlIOBase(f)->flags |= PERLIO_F_OPEN;
 return code;
}

PerlIO *
PerlIOWin32_open(pTHX_ PerlIO_funcs *self, PerlIO_list_t *layers, IV n, const char *mode, int fd, int imode, int perm, PerlIO *f, int narg, SV **args)
{
 const char *tmode = mode;
 HANDLE h = INVALID_HANDLE_VALUE;
 if (f)
  {
   /* Close if already open */
   if (PerlIOBase(f)->flags & PERLIO_F_OPEN)
    (*PerlIOBase(f)->tab->Close)(aTHX_ f);
  }
 if (narg > 0)
  {
   char *path = SvPV_nolen(*args);
   DWORD  access = 0;
   DWORD  share  = 0;
   DWORD  create = -1;
   DWORD  attr   = FILE_ATTRIBUTE_NORMAL;
   if (*mode == '#')
    {
     /* sysopen - imode is UNIX-like O_RDONLY etc.
        - do_open has converted that back to string form in mode as well
        - perm is UNIX like permissions
      */
     mode++;
    }
   else
    {
     /* Normal open - decode mode string */
    }
   switch(*mode)
    {
     case 'r':
      access  = GENERIC_READ;
      create  = OPEN_EXISTING;
      if (*++mode == '+')
       {
        access |= GENERIC_WRITE;
        create  = OPEN_ALWAYS;
        mode++;
       }
      break;

     case 'w':
      access  = GENERIC_WRITE;
      create  = TRUNCATE_EXISTING;
      if (*++mode == '+')
       {
        access |= GENERIC_READ;
        mode++;
       }
      break;

     case 'a':
      access = GENERIC_WRITE;
      create  = OPEN_ALWAYS;
      if (*++mode == '+')
       {
        access |= GENERIC_READ;
        mode++;
       }
      break;
    }
   if (*mode == 'b')
    {
     mode++;
    }
   else if (*mode == 't')
    {
     mode++;
    }
   if (*mode || create == -1)
    {
     //FIX-ME: SETERRNO(EINVAL,LIB$_INVARG);
     XCEMessageBoxA(NULL, "NEED TO IMPLEMENT a place in ../wince/win32io.c", "Perl(developer)", 0);
     return NULL;
    }
   if (!(access & GENERIC_WRITE))
    share = FILE_SHARE_READ;
   h = CreateFileW(path,access,share,NULL,create,attr,NULL);
   if (h == INVALID_HANDLE_VALUE)
    {
     if (create == TRUNCATE_EXISTING)
      h = CreateFileW(path,access,share,NULL,(create = OPEN_ALWAYS),attr,NULL);
    }
  }
 else
  {
   /* fd open */
   h = INVALID_HANDLE_VALUE;
   if (fd >= 0 && fd <= max_open_fd)
    {
     PerlIOWin32 *s = fdtable[fd];
     if (s)
      {
       s->refcnt++;
       if (!f)
        f = PerlIO_allocate(aTHX);
       *f = &s->base;
       return f;
      }
    }
   if (*mode == 'I')
    {
     mode++;
     switch(fd)
      {
       case 0:
        h = XCEGetStdHandle(STD_INPUT_HANDLE);
        break;
       case 1:
        h = XCEGetStdHandle(STD_OUTPUT_HANDLE);
        break;
       case 2:
        h = XCEGetStdHandle(STD_ERROR_HANDLE);
        break;
      }
    }
  }
 if (h != INVALID_HANDLE_VALUE)
  fd = win32_open_osfhandle((intptr_t) h, PerlIOUnix_oflags(tmode));
 if (fd >= 0)
  {
   PerlIOWin32 *s;
   if (!f)
    f = PerlIO_allocate(aTHX);
   s = PerlIOSelf(PerlIO_push(aTHX_ f,self,tmode,PerlIOArg),PerlIOWin32);
   s->h      = h;
   s->fd     = fd;
   s->refcnt = 1;
   if (fd >= 0)
    {
     fdtable[fd] = s;
     if (fd > max_open_fd)
      max_open_fd = fd;
    }
   return f;
  }
 if (f)
  {
   /* FIXME: pop layers ??? */
  }
 return NULL;
}

SSize_t
PerlIOWin32_read(pTHX_ PerlIO *f, void *vbuf, Size_t count)
{
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 DWORD len;
 if (!(PerlIOBase(f)->flags & PERLIO_F_CANREAD))
  return 0;
 if (ReadFile(s->h,vbuf,count,&len,NULL))
  {
   return len;
  }
 else
  {
   if (GetLastError() != NO_ERROR)
    {
     PerlIOBase(f)->flags |= PERLIO_F_ERROR;
     return -1;
    }
   else
    {
     if (count != 0)
      PerlIOBase(f)->flags |= PERLIO_F_EOF;
     return 0;
    }
  }
}

SSize_t
PerlIOWin32_write(pTHX_ PerlIO *f, const void *vbuf, Size_t count)
{
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 DWORD len;
 if (WriteFile(s->h,vbuf,count,&len,NULL))
  {
   return len;
  }
 else
  {
   PerlIOBase(f)->flags |= PERLIO_F_ERROR;
   return -1;
  }
}

IV
PerlIOWin32_seek(pTHX_ PerlIO *f, Off_t offset, int whence)
{
 static const DWORD where[3] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 DWORD high = (sizeof(offset) > sizeof(DWORD)) ? (DWORD)(offset >> 32) : 0;
 DWORD low  = (DWORD) offset;
 DWORD res  = SetFilePointer(s->h,low,&high,where[whence]);
 if (res != 0xFFFFFFFF || GetLastError() != NO_ERROR)
  {
   return 0;
  }
 else
  {
   return -1;
  }
}

Off_t
PerlIOWin32_tell(pTHX_ PerlIO *f)
{
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 DWORD high = 0;
 DWORD res  = SetFilePointer(s->h,0,&high,FILE_CURRENT);
 if (res != 0xFFFFFFFF || GetLastError() != NO_ERROR)
  {
   return ((Off_t) high << 32) | res;
  }
 return (Off_t) -1;
}

IV
PerlIOWin32_close(pTHX_ PerlIO *f)
{
 PerlIOWin32 *s = PerlIOSelf(f,PerlIOWin32);
 if (s->refcnt == 1)
  {
   IV code = 0;
#if 0
   /* This does not do pipes etc. correctly */
   if (!CloseHandle(s->h))
    {
     s->h = INVALID_HANDLE_VALUE;
     return -1;
    }
#else
    PerlIOBase(f)->flags &= ~PERLIO_F_OPEN;
    return win32_close(s->fd);
#endif
  }
 return 0;
}

PerlIO *
PerlIOWin32_dup(pTHX_ PerlIO *f, PerlIO *o, CLONE_PARAMS *params, int flags)
{
 PerlIOWin32 *os = PerlIOSelf(f,PerlIOWin32);
 HANDLE proc = GetCurrentProcess();
 HANDLE new;
 if (DuplicateHandle(proc, os->h, proc, &new, 0, FALSE,  DUPLICATE_SAME_ACCESS))
  {
   char mode[8];
   int fd = win32_open_osfhandle((intptr_t) new, PerlIOUnix_oflags(PerlIO_modestr(o,mode)));
   if (fd >= 0)
    {
     f = PerlIOBase_dup(aTHX_ f, o, params, flags);
     if (f)
      {
       PerlIOWin32 *fs = PerlIOSelf(f,PerlIOWin32);
       fs->h  = new;
       fs->fd = fd;
       fs->refcnt = 1;
       fdtable[fd] = fs;
       if (fd > max_open_fd)
        max_open_fd = fd;
      }
     else
      {
       win32_close(fd);
      }
    }
   else
    {
     CloseHandle(new);
    }
  }
 return f;
}

PerlIO_funcs PerlIO_win32 = {
 sizeof(PerlIO_funcs),
 "win32",
 sizeof(PerlIOWin32),
 PERLIO_K_RAW,
 PerlIOWin32_pushed,
 PerlIOWin32_popped,
 PerlIOWin32_open,
 PerlIOBase_binmode,
 NULL,                 /* getarg */
 PerlIOWin32_fileno,
 PerlIOWin32_dup,
 PerlIOWin32_read,
 PerlIOBase_unread,
 PerlIOWin32_write,
 PerlIOWin32_seek,
 PerlIOWin32_tell,
 PerlIOWin32_close,
 PerlIOBase_noop_ok,   /* flush */
 PerlIOBase_noop_fail, /* fill */
 PerlIOBase_eof,
 PerlIOBase_error,
 PerlIOBase_clearerr,
 PerlIOBase_setlinebuf,
 NULL, /* get_base */
 NULL, /* get_bufsiz */
 NULL, /* get_ptr */
 NULL, /* get_cnt */
 NULL, /* set_ptrcnt */
};

#endif

