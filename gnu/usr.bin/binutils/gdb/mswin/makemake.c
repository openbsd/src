
/* makefile creator for MSVC++ development environment.
   Contributed by Cygnus Support.

   To compile:
	i386-go32-gcc makemake.c -o makemake.exe 
   To run:
	On a win32 system, 
		copy c:\msvc\bin\cl.exe c:\msvc\bin\clsav.exe
		copy c:\msvc\bin\link.exe c:\msvc\bin\linksav.exe
		copy makemake.exe c:\msvc\bin\cl.exe
		copy makemake.exe c:\msvc\bin\link.exe
	run msvc as you normally would to create your application.
	when msvc has completed, you should have the file "makefile"
	in the current directory.  
	This makefile can be built outside msvc by typing "nmake".

    To build wingdb:
	This program was originally created to enable remote builds of wingdb.  
	The wingdb source directories are:
		src/gdb
		src/gdb/mswin
		src/gdb/mswin/prebuilt
		src/gdb/mswin/prebuilt/<target_specifics>
		src/bfd
		src/readline
		src/libiberty
		src/opcodes
		src/...
	The src/gdb/mswin contains 3 makefiles gui.mak, 
	serdll32.mak and serdll16.mak.
	serdll16.mak requires msvc version 1.5.  
	The other makefiles use msvc version 2.0.

	To create msvc-free makefiles from the above, do the following:
	build makemake.exe
	set up g: to point to devo directory
	g:
	cd g:\gdb\mswin
	copy cl.exe to clsav.exe 
	copy link.exe to linksav.exe
	copy makemake.exe to cl.exe and link.exe
	run msvc serdll32.mak
	select build-all
	copy makefile serdll32_cmd.mak
	run msvc gui.mak
	select build-all
	copy makefile gui_cmd.mak
	copy clsav.exe to cl.exe 
	copy linksav.exe to link.exe
	run nmake -f serdll32_cmd.mak
	run nmake -f gui_cmd.mak

	There is also a file makemake.bat which was created to help make
	the above steps easier.
	=====copied from makemake.bat===========
	rem This file runs makemake program to create a makefile
	rem which can be run remotely at the command line
	rem from the msvc++ IDE.

	rem g: must be set up to point to the devo (g:\gdb\mswin) directory
	rem c: must be set up to point to the c:\ directory
	if not exist g:\gdb goto nog
	if not exist c:\gs goto noc

	MSVC=c:\msvc20

	copy %MSVC%\cl.exe %MSVC%\clsav.exe
	copy %MSVC%\link.exe %MSVC%\linksav.exe

	copy makemake.exe %MSVC%\cl.exe
	copy makemake.exe %MSVC%\link.exe

	%MSVC%\vcvars32.bat
	msvc gui.mak

	copy %MSVC%\clsav.exe %MSVC%\cl.exe
	copy %MSVC%\linksav.exe %MSVC%\link.exe

*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))
void split_path(char *src, char *drive, char *dir, char *fname, char *ext);
void dos2posix(char *dpath);
void dbgprintfile(FILE *fout, char *filename);
FILE* get_make(void);
void add_cl(char *filename);
void add_link(char *filename);
void add_to_make(char *fmt,...);
void add_args_to_make(char* args,char* argfile);
void touch(char* file);
/* #define DEBUG */
/* #define NO_TOUCH */
#define _MAX_CMDLEN 128
#ifndef _MAX_PATH
  /* #define POSIX */
  #define _MAX_PATH 256
  #define _MAX_DRIVE 256
  #define _MAX_DIR 256
  #define _MAX_FNAME 256
  #define _MAX_EXT 10
  #define COPY "cp"
  #define DEL "rm -f"
  #define SEP '/'
#else
  #define COPY "copy"
  #define DEL "del"
  #define SEP '\\'
#endif
#ifdef DEBUG
    #define dbgprintf(x) printf x
#else
    #define dbgprintf(x) 
#endif


int main(int argc, char *argv[], char *env[])
{
    int i;
    FILE *fmak;
    char buf[512];
    char respfile[512];
    time_t t;

    for (i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        dbgprintf(("argv[%d] = '%s'\n", i, arg));
        if (arg[0] == '@')
        {
            char* cmd;
#if 0
            /* grab a unique respfile here... */
                t = time(NULL); /* = tempnam(s); */
            sprintf(respfile,"cl_%d.arg",t);
            sprintf(buf,"%s %s %s",COPY,&arg[1],respfile);
            system(buf);
#endif
            cmd = strrchr(argv[0],SEP); 
            if (cmd==0) cmd=argv[0];
            else cmd++;
            if (strcmp(cmd,"cl")==0
                || strcmp(cmd,"cl.exe")==0) /* if compiling, ... */
                add_cl(&arg[1]);
            else if (strcmp(cmd,"link")==0
                || strcmp(cmd,"link.exe")==0) /* else, if linking, ... */
                add_link(&arg[1]);
            else 
                fprintf(stderr,"Unknown command: %s %s\n",cmd,arg);
            dbgprintfile(stdout, &arg[1]);
        }
        else
            fprintf(stderr,"Unknown command: %s\n",arg);
    }

    for (i = 0; env[i]; i++)
    dbgprintf(("env[%d] = '%s'\n", i, env[i]));

    return 0;
}

static char stoken[256];
/*    #define getstr() ((fscanf(fin, "%s", stoken) == 1) ? stoken : 0)
    #define getopt(token) (token[2] ? token+2 : getstr())
*/
char* getstr(FILE* fin) { 
    if (fscanf(fin, "%s", stoken) == 1) 
    {
        char *p;
        if (p=strchr(stoken,'\r'),p)
            *p=0;
        if (p=strchr(stoken,'\n'),p)
            *p=0;
        return stoken; 
    }
    return 0; 
}
/* assumes /D is part of token */
char* getopt(FILE* fin,char* token) {
    return (token[2] ? token+2 : getstr(fin));
}

/* open response file and do what cl would do */
void add_cl(char *respfile)
{
    int c;
    char* token;
    char cfile[_MAX_PATH];
    char objfile[_MAX_PATH];
    char objdir[_MAX_PATH]="";
    char drive[_MAX_PATH];
    char srcdir[_MAX_PATH];
    char fname[_MAX_PATH];
    char ext[_MAX_PATH];
    FILE *fin;
    char define[1024]="", inc[1024]="", libpath[1024]="", misc[1024]="";

    fin = fopen(respfile, "rb");
    if (fin == NULL)
    {
    fprintf(stderr,"file %s does not exist\n", respfile);
    return;
    }

    while (token=getstr(fin),token)
    {
    dbgprintf(("Scanning token '%s'\n", token));
    if (sscanf(token, "/Fo\"%[^\"]\"", objdir) == 1
        || sscanf(token, "/Fo%s", objdir) == 1)
    {
        dbgprintf(("objdir is '%s'\n", objdir));
    }
    else if (token[0] == '/' || token[0] == '-')
    {
        switch (token[1]) 
        {
            case 'D': 
                sprintf(define,"%s /D%s",define,getopt(fin,token));
                break;
            case 'I': 
                sprintf(inc,"%s /I%s",inc,getopt(fin,token));
                break;
            case 'L': 
                sprintf(libpath,"%s /L%s",libpath,getopt(fin,token));
                break;
            default:
                sprintf(misc,"%s %s",misc,token);
                break;
        }
    }
    /* yuk!  create fake obj for source file */
    /* we expect to get "path\source.c" at the beginning of the line */
    else if (sscanf(token, "\"%[^\"]\"", cfile) == 1)
    {
        /* eg, given "g:\\gdb\\devo\\gdb\\fexptab.c",
            drive=g:\\, srcdir=\\gdb\\devo\\gdb\\f,
            fname=fexptab., ext=.c */
        split_path(cfile, drive, srcdir, fname, ext);
        if (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0)
        {
            char objfile[1024]="";
            /* this is a compile, so add compile rule */
            add_to_make("\n%s%s.obj: %s%s%s\n",
                objdir,fname,srcdir,fname,ext);
            if (strlen(define)+strlen(inc)+strlen(libpath)+strlen(misc)
                > _MAX_CMDLEN-40)
            {
                char args[2048];
                sprintf(args,"%s %s %s %s\n", 
                    define,inc,libpath,misc);
                add_args_to_make(args,"_args");
                add_to_make("\t$(CC) @_args $(CFLAGS) %s%s%s\n",
                        srcdir,fname,ext);
            }
            else
                add_to_make("\t$(CC) %s %s %s %s $(CFLAGS) %s%s%s\n", 
                    define,inc,libpath,misc, srcdir,fname,ext);
            sprintf(objfile,"%s%s.%s",objdir,fname,"obj");
            touch(objfile);
            add_to_make("");
        }
    }
    else
    {
        /* dbgprintf(("option '%s' ignored\n", token)); */
        add_to_make("%s",token);
    }
    }
    fclose(fin);
}

/* open response file and do what link would do */
void add_link(char *respfile)
{
    char lfile[_MAX_PATH];
    char *token;
    char misc[1024]="";
    char objs[6000]="";
    FILE* fin = fopen(respfile, "rb");
    if (fin == NULL)
    {
    fprintf(stderr,"file %s does not exist\n", respfile);
    return;
    }
#ifdef DEBUG
    else
    {
        char tmpy[1024];
        printf("%s %s link.arg\n",COPY,respfile);
        sprintf(tmpy,"%s %s link.arg\n",COPY,respfile);
        system(tmpy);
    }
#endif
    while (token=getstr(fin),token)
    {
    dbgprintf(("Scanning token '%s'\n", token));
    /* expects link name to be in quotes */
    if (sscanf(token, "/OUT:\"%[^\"]\"", lfile) == 1)
    {
        dbgprintf(("lfile='%s'\n", lfile));
    }
    else if (token[0] == '/' || token[0] == '-')
    {
        switch (token[1]) 
        {
            default:
                sprintf(misc,"%s %s",misc,token);
                break;
        }
    }
    else
    {
        if (token[0])
            sprintf(objs,"%s\\\n\t%s", objs, token);
    }
    }
    /* now write the link rule */
    if (lfile)
    {
        add_to_make("\n\n\nproject:%s\n",lfile);
        add_to_make("OBJS=%s\n",objs);
        /* this is a link, so add link rule */
        add_to_make("\n%s: $(OBJS)\n", lfile);
        if (strlen(misc) > _MAX_CMDLEN-40 || strlen(objs) > _MAX_CMDLEN-40)
        {
            add_args_to_make(misc,"_args");
            add_args_to_make(objs,"_objs");
            add_to_make("\t$(LD) $(LFLAGS) @_args @_objs /OUT:%s $(LIBS)\n", lfile);
        }
        else
            add_to_make("\t$(LD) $(LFLAGS) %s /OUT:%s $(OBJS) $(LIBS)\n", 
                misc, lfile);
        add_to_make("");
        /* yuk!  create fake exe for source file */
        touch(lfile);
    }
    fclose(fin);
}

/* add token to makefile */
void add_to_make(char *fmt,...)
{
    va_list v;
    FILE* fmak = get_make();
    /* add token to makefile */
    va_start(v, fmt);

    if (fmak) 
      {
        vfprintf(fmak,fmt,v);
        fclose(fmak);
      }
    va_end(v);
}

void add_args_to_make(char* args,char* argfile)
{
                char *pargs=args;
                int j;
                add_to_make("\t%s %s\n",DEL,argfile);
                for (j=0,pargs=args; pargs&&*pargs; pargs+=j)
                {
                  char *p;
                  for (p=pargs; p&&p-pargs<55; p=strchr(++p,' '), p=p?p:p=strchr(p,'\n'))
                      j=p-pargs;	/* last blank */
                  if (!p) 
                      j=strlen(pargs)-1;
                  pargs[j++]=0;
                  add_to_make("\techo %s>> %s\n", pargs,argfile);
                }
}

FILE* get_make(void)
{
    /* if makefile already there, we'll append.  else, we create */
    FILE* fmak;
    fmak = fopen("makefile", "r");
    if (fmak == NULL)
    {
        fmak = fopen("makefile", "w+");
        if (fmak == NULL)
        {
        fprintf(stderr,"can't open file %s for output\n", "makefile");
        return 0;
        }
        fprintf(fmak,"#makefile -\n");
        fprintf(fmak,"#    change the following as appropriate:\n");
        fprintf(fmak,"#    srcdir, objdir, msvcdir, makefile\n");
        /* fprintf(fmak,"SRC=%s\n", "h:/gdb/devo"); */
        /* fprintf(fmak,"OBJ=%s\n", "h:/gdb/devel/win32/x/lose/objs"); */
        fprintf(fmak,"MSVC=%s\n\n","c:\\msvc22");
        fprintf(fmak,"INCLUDE=/I$(MSVC)\\include /I$(MSVC)\\mfc\\include\n");
        fprintf(fmak,"CFLAGS=\n");
        fprintf(fmak,"LFLAGS=\n");
        fprintf(fmak,"LIB=$(MSVC)\\lib;$(MSVC)\\mfc\\lib\n");
        fprintf(fmak,"CC=cl $(INCLUDE) /nologo -DALMOST_STDC\n");
        fprintf(fmak,"LD=link /nologo\n");
        fprintf(fmak,"\nall: project\n");
        fprintf(fmak,"\n");
    }
    else 
    {
        fclose(fmak);
        fmak = fopen("makefile", "a+");
    }
    return fmak;
}

/* openning and printing response file */
void dbgprintfile(FILE *fout, char *filename)
{
#ifdef DEBUG
    int c;
    FILE *fin = fopen(filename, "r");

    if (fin == NULL)
    {
    fprintf(fout, "file %s does not exist\n", filename);
    return;
    }

    fprintf(fout, "File %s:\n", filename);
    while ((c = getc(fin)) != EOF)
    putc(c, fout);
    fprintf(fout, "End of file %s\n", filename);
    fclose(fin);
#endif
}

void touch(char* tfile) 
{
#ifndef NO_TOUCH
    FILE *ofile;
    /* create empty object to fool compiler */
#ifdef POSIX
    dos2posix(tfile);
#endif
    ofile = fopen(tfile, "a");
    if (ofile == NULL)
        fprintf(stderr,"Unable to touch %s\n", tfile);
    else
        fclose(ofile);
    dbgprintf(("tfile=%s\n", tfile));
#endif
}


void _strncpy(char *dest, char *src, int n) 
{
      if (n!=0 && dest != NULL && src != NULL) 
      {
        strncpy (dest, src, n);
        dest[n] = 0;
      }
}
/* change dos file type to posix eg: g:\\gnu\\devo -> //g/gnu/devo/ */
void dos2posix(char *dpath) 
{
  int i, j;
  char upath[_MAX_PATH]="";
  char* p=upath;
  if (p == NULL) return;

  /* copy drive */
  i = 0;
  while (dpath[i] != 0 && dpath[i] != ':')
    ++i;
  if (i > 0 && dpath[i] == ':')
    {
        /* what about g:gndevo ? what's the posix of that? */
        p[0]='/';
        p[1]='/';
        _strncpy (p+2, dpath, i+1);
        p[i+1]='/';
        p[i+2]=0;
        p += i+2;
      dpath += i+1;
    }
  else 
    *p = 0;
  
  /* copy dir */
  for (j = 0; dpath[j] != 0; ++j)
  {
    if (dpath[j] == '/' || dpath[j] == '\\')
    {
      i = j + 1;
      p[j]=SEP;
    }
    else
      p[j]=dpath[j];
  }
  /* p[i]=0;    leave filename and ext */
  strcpy(dpath,upath);
}

/* eg, given "g:\\gdb\\devo\\gdb\\fexptab.c",
    drive=g:\\, srcdir=\\gdb\\devo\\gdb\\f,
    fname=fexptab., ext=.c */
void split_path(char *src, char *drive, char *dir, char *fname, char *ext)
{
  int i, j;

  /* copy drive */
  i = 0;
  while (i < _MAX_DRIVE-2 && src[i] != 0 && src[i] != ':')
    ++i;
  if (i > 0 && src[i] == ':')
    {
      if (drive != NULL)
        _strncpy (drive, src, i+1);
      src += i+1;
    }
  else if (drive != NULL)
    *drive = 0;
  
  /* copy dir */
  i = 0;
  for (j = 0; src[j] != 0; ++j)
  {
    /* note, if MAC, want : too! */
    if (src[j] == '/' || src[j] == '\\')
    {
      i = j + 1;
    }
  }
  if (dir != NULL) 
    _strncpy (dir, src, min (_MAX_DIR, i));
  src += i;
  
  /* copy fname */
  i = 0;
  for (j = 0; src[j] != 0; ++j)
    if (src[j] == '.')
      i = j;
  if (i == 0)
    i = j;
  if (fname != NULL)
    _strncpy (fname, src, min (_MAX_FNAME, i));
  src += i;
  
  /* copy ext */
  if (ext != NULL)
    _strncpy (ext, src, _MAX_EXT);
#ifdef DEBUG
    printf("src=%s, drive=%s, dir=%s, fname=%s, ext=%s\n", 
        src, drive, dir, fname, ext);
#endif
}
