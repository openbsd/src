
#include "iperlsys.h"

extern CPerlObj *pPerl;

#define CALLFUNC0RET(x)\
    int ret = x;\
    if (ret < 0)\
	err = errno;\
    return ret;

#define PROCESS_AND_RETURN  \
    if (errno)		    \
	err = errno;	    \
    return r

#define CALLFUNCRET(x)\
    int ret = x;\
    if (ret)\
	err = errno;\
    return ret;

#define CALLFUNCERR(x)\
    int ret = x;\
    if (errno)\
	err = errno;\
    return ret;

#define LCALLFUNCERR(x)\
    long ret = x;\
    if (errno)\
	err = errno;\
    return ret;

class CPerlDir : public IPerlDir
{
public:
    CPerlDir() {};
    virtual int Makedir(const char *dirname, int mode, int &err)
    {
	CALLFUNC0RET(win32_mkdir(dirname, mode));
    };
    virtual int Chdir(const char *dirname, int &err)
    {
	CALLFUNC0RET(win32_chdir(dirname));
    };
    virtual int Rmdir(const char *dirname, int &err)
    {
	CALLFUNC0RET(win32_rmdir(dirname));
    };
    virtual int Close(DIR *dirp, int &err)
    {
	return win32_closedir(dirp);
    };
    virtual DIR *Open(char *filename, int &err)
    {
	return win32_opendir(filename);
    };
    virtual struct direct *Read(DIR *dirp, int &err)
    {
	return win32_readdir(dirp);
    };
    virtual void Rewind(DIR *dirp, int &err)
    {
	win32_rewinddir(dirp);
    };
    virtual void Seek(DIR *dirp, long loc, int &err)
    {
	win32_seekdir(dirp, loc);
    };
    virtual long Tell(DIR *dirp, int &err)
    {
	return win32_telldir(dirp);
    };
};


extern char *		g_win32_get_privlib(char *pl);
extern char *		g_win32_get_sitelib(char *pl);

class CPerlEnv : public IPerlEnv
{
public:
    CPerlEnv() {};
    virtual char *Getenv(const char *varname, int &err)
    {
	return win32_getenv(varname);
    };
    virtual int Putenv(const char *envstring, int &err)
    {
	return win32_putenv(envstring);
    };
    virtual char* LibPath(char *pl)
    {
	return g_win32_get_privlib(pl);
    };
    virtual char* SiteLibPath(char *pl)
    {
	return g_win32_get_sitelib(pl);
    };
};

class CPerlSock : public IPerlSock
{
public:
    CPerlSock() {};
    virtual u_long Htonl(u_long hostlong)
    {
	return win32_htonl(hostlong);
    };
    virtual u_short Htons(u_short hostshort)
    {
	return win32_htons(hostshort);
    };
    virtual u_long Ntohl(u_long netlong)
    {
	return win32_ntohl(netlong);
    };
    virtual u_short Ntohs(u_short netshort)
    {
	return win32_ntohs(netshort);
    }

    virtual SOCKET Accept(SOCKET s, struct sockaddr* addr, int* addrlen, int &err)
    {
	SOCKET r = win32_accept(s, addr, addrlen);
	PROCESS_AND_RETURN;
    };
    virtual int Bind(SOCKET s, const struct sockaddr* name, int namelen, int &err)
    {
	int r = win32_bind(s, name, namelen);
	PROCESS_AND_RETURN;
    };
    virtual int Connect(SOCKET s, const struct sockaddr* name, int namelen, int &err)
    {
	int r = win32_connect(s, name, namelen);
	PROCESS_AND_RETURN;
    };
    virtual void Endhostent(int &err)
    {
	win32_endhostent();
    };
    virtual void Endnetent(int &err)
    {
	win32_endnetent();
    };
    virtual void Endprotoent(int &err)
    {
	win32_endprotoent();
    };
    virtual void Endservent(int &err)
    {
	win32_endservent();
    };
    virtual struct hostent* Gethostbyaddr(const char* addr, int len, int type, int &err)
    {
	struct hostent *r = win32_gethostbyaddr(addr, len, type);
	PROCESS_AND_RETURN;
    };
    virtual struct hostent* Gethostbyname(const char* name, int &err)
    {
	struct hostent *r = win32_gethostbyname(name);
	PROCESS_AND_RETURN;
    };
    virtual struct hostent* Gethostent(int &err)
    {
        croak("gethostent not implemented!\n");
	return NULL;
    };
    virtual int Gethostname(char* name, int namelen, int &err)
    {
	int r = win32_gethostname(name, namelen);
	PROCESS_AND_RETURN;
    };
    virtual struct netent *Getnetbyaddr(long net, int type, int &err)
    {
	struct netent *r = win32_getnetbyaddr(net, type);
	PROCESS_AND_RETURN;
    };
    virtual struct netent *Getnetbyname(const char *name, int &err)
    {
	struct netent *r = win32_getnetbyname((char*)name);
	PROCESS_AND_RETURN;
    };
    virtual struct netent *Getnetent(int &err)
    {
	struct netent *r = win32_getnetent();
	PROCESS_AND_RETURN;
    };
    virtual int Getpeername(SOCKET s, struct sockaddr* name, int* namelen, int &err)
    {
	int r = win32_getpeername(s, name, namelen);
	PROCESS_AND_RETURN;
    };
    virtual struct protoent* Getprotobyname(const char* name, int &err)
    {
	struct protoent *r = win32_getprotobyname(name);
	PROCESS_AND_RETURN;
    };
    virtual struct protoent* Getprotobynumber(int number, int &err)
    {
	struct protoent *r = win32_getprotobynumber(number);
	PROCESS_AND_RETURN;
    };
    virtual struct protoent* Getprotoent(int &err)
    {
	struct protoent *r = win32_getprotoent();
	PROCESS_AND_RETURN;
    };
    virtual struct servent* Getservbyname(const char* name, const char* proto, int &err)
    {
	struct servent *r = win32_getservbyname(name, proto);
	PROCESS_AND_RETURN;
    };
    virtual struct servent* Getservbyport(int port, const char* proto, int &err)
    {
	struct servent *r = win32_getservbyport(port, proto);
	PROCESS_AND_RETURN;
    };
    virtual struct servent* Getservent(int &err)
    {
	struct servent *r = win32_getservent();
	PROCESS_AND_RETURN;
    };
    virtual int Getsockname(SOCKET s, struct sockaddr* name, int* namelen, int &err)
    {
	int r = win32_getsockname(s, name, namelen);
	PROCESS_AND_RETURN;
    };
    virtual int Getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen, int &err)
    {
	int r = win32_getsockopt(s, level, optname, optval, optlen);
	PROCESS_AND_RETURN;
    };
    virtual unsigned long InetAddr(const char* cp, int &err)
    {
	unsigned long r = win32_inet_addr(cp);
	PROCESS_AND_RETURN;
    };
    virtual char* InetNtoa(struct in_addr in, int &err)
    {
	char *r = win32_inet_ntoa(in);
	PROCESS_AND_RETURN;
    };
    virtual int Listen(SOCKET s, int backlog, int &err)
    {
	int r = win32_listen(s, backlog);
	PROCESS_AND_RETURN;
    };
    virtual int Recv(SOCKET s, char* buffer, int len, int flags, int &err)
    {
	int r = win32_recv(s, buffer, len, flags);
	PROCESS_AND_RETURN;
    };
    virtual int Recvfrom(SOCKET s, char* buffer, int len, int flags, struct sockaddr* from, int* fromlen, int &err)
    {
	int r = win32_recvfrom(s, buffer, len, flags, from, fromlen);
	PROCESS_AND_RETURN;
    };
    virtual int Select(int nfds, char* readfds, char* writefds, char* exceptfds, const struct timeval* timeout, int &err)
    {
	int r = win32_select(nfds, (Perl_fd_set*)readfds, (Perl_fd_set*)writefds, (Perl_fd_set*)exceptfds, timeout);
	PROCESS_AND_RETURN;
    };
    virtual int Send(SOCKET s, const char* buffer, int len, int flags, int &err)
    {
	int r = win32_send(s, buffer, len, flags);
	PROCESS_AND_RETURN;
    };
    virtual int Sendto(SOCKET s, const char* buffer, int len, int flags, const struct sockaddr* to, int tolen, int &err)
    {
	int r = win32_sendto(s, buffer, len, flags, to, tolen);
	PROCESS_AND_RETURN;
    };
    virtual void Sethostent(int stayopen, int &err)
    {
	win32_sethostent(stayopen);
    };
    virtual void Setnetent(int stayopen, int &err)
    {
	win32_setnetent(stayopen);
    };
    virtual void Setprotoent(int stayopen, int &err)
    {
	win32_setprotoent(stayopen);
    };
    virtual void Setservent(int stayopen, int &err)
    {
	win32_setservent(stayopen);
    };
    virtual int Setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen, int &err)
    {
	int r = win32_setsockopt(s, level, optname, optval, optlen);
	PROCESS_AND_RETURN;
    };
    virtual int Shutdown(SOCKET s, int how, int &err)
    {
	int r = win32_shutdown(s, how);
	PROCESS_AND_RETURN;
    };
    virtual SOCKET Socket(int af, int type, int protocol, int &err)
    {
	SOCKET r = win32_socket(af, type, protocol);
	PROCESS_AND_RETURN;
    };
    virtual int Socketpair(int domain, int type, int protocol, int* fds, int &err)
    {
        croak("socketpair not implemented!\n");
	return 0;
    };
    virtual int Closesocket(SOCKET s, int& err)
    {
	int r = win32_closesocket(s);
	PROCESS_AND_RETURN;
    };
    virtual int Ioctlsocket(SOCKET s, long cmd, u_long *argp, int& err)
    {
	int r = win32_ioctlsocket(s, cmd, argp);
	PROCESS_AND_RETURN;
    };
};

class CPerlLIO : public IPerlLIO
{
public:
    CPerlLIO() {};
    virtual int Access(const char *path, int mode, int &err)
    {
	CALLFUNCRET(access(path, mode))
    };
    virtual int Chmod(const char *filename, int pmode, int &err)
    {
	CALLFUNCRET(chmod(filename, pmode))
    };
    virtual int Chown(const char *filename, uid_t owner, gid_t group, int &err)
    {
	CALLFUNCERR(chown(filename, owner, group))
    };
    virtual int Chsize(int handle, long size, int &err)
    {
	CALLFUNCRET(chsize(handle, size))
    };
    virtual int Close(int handle, int &err)
    {
	CALLFUNCRET(win32_close(handle))
    };
    virtual int Dup(int handle, int &err)
    {
	CALLFUNCERR(win32_dup(handle))
    };
    virtual int Dup2(int handle1, int handle2, int &err)
    {
	CALLFUNCERR(win32_dup2(handle1, handle2))
    };
    virtual int Flock(int fd, int oper, int &err)
    {
	CALLFUNCERR(win32_flock(fd, oper))
    };
    virtual int FileStat(int handle, struct stat *buffer, int &err)
    {
	CALLFUNCERR(fstat(handle, buffer))
    };
    virtual int IOCtl(int i, unsigned int u, char *data, int &err)
    {
	CALLFUNCERR(win32_ioctlsocket((SOCKET)i, (long)u, (u_long*)data))
    };
    virtual int Isatty(int fd, int &err)
    {
	return isatty(fd);
    };
    virtual long Lseek(int handle, long offset, int origin, int &err)
    {
	LCALLFUNCERR(win32_lseek(handle, offset, origin))
    };
    virtual int Lstat(const char *path, struct stat *buffer, int &err)
    {
	return NameStat(path, buffer, err);
    };
    virtual char *Mktemp(char *Template, int &err)
    {
	return mktemp(Template);
    };
    virtual int Open(const char *filename, int oflag, int &err)
    {
	CALLFUNCERR(win32_open(filename, oflag))
    };
    virtual int Open(const char *filename, int oflag, int pmode, int &err)
    {
	int ret;
	if(stricmp(filename, "/dev/null") == 0)
	    ret = open("NUL", oflag, pmode);
	else
	    ret = open(filename, oflag, pmode);

	if(errno)
	    err = errno;
	return ret;
    };
    virtual int Read(int handle, void *buffer, unsigned int count, int &err)
    {
	CALLFUNCERR(win32_read(handle, buffer, count))
    };
    virtual int Rename(const char *OldFileName, const char *newname, int &err)
    {
	CALLFUNCRET(win32_rename(OldFileName, newname))
    };
    virtual int Setmode(int handle, int mode, int &err)
    {
	CALLFUNCRET(win32_setmode(handle, mode))
    };
    virtual int NameStat(const char *path, struct stat *buffer, int &err)
    {
	return win32_stat(path, buffer);
    };
    virtual char *Tmpnam(char *string, int &err)
    {
	return tmpnam(string);
    };
    virtual int Umask(int pmode, int &err)
    {
	return umask(pmode);
    };
    virtual int Unlink(const char *filename, int &err)
    {
	chmod(filename, S_IREAD | S_IWRITE);
	CALLFUNCRET(unlink(filename))
    };
    virtual int Utime(char *filename, struct utimbuf *times, int &err)
    {
	CALLFUNCRET(win32_utime(filename, times))
    };
    virtual int Write(int handle, const void *buffer, unsigned int count, int &err)
    {
	CALLFUNCERR(win32_write(handle, buffer, count))
    };
};

class CPerlMem : public IPerlMem
{
public:
    CPerlMem() {};
    virtual void* Malloc(size_t size)
    {
	return win32_malloc(size);
    };
    virtual void* Realloc(void* ptr, size_t size)
    {
	return win32_realloc(ptr, size);
    };
    virtual void Free(void* ptr)
    {
	win32_free(ptr);
    };
};

#define EXECF_EXEC 1
#define EXECF_SPAWN 2

extern char *		g_getlogin(void);
extern int		do_spawn2(char *cmd, int exectype);
extern int		g_do_aspawn(void *vreally, void **vmark, void **vsp);

class CPerlProc : public IPerlProc
{
public:
    CPerlProc() {};
    virtual void Abort(void)
    {
	win32_abort();
    };
    virtual char * Crypt(const char* clear, const char* salt)
    {
	return win32_crypt(clear, salt);
    };
    virtual void Exit(int status)
    {
	exit(status);
    };
    virtual void _Exit(int status)
    {
	_exit(status);
    };
    virtual int Execl(const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3)
    {
	return execl(cmdname, arg0, arg1, arg2, arg3);
    };
    virtual int Execv(const char *cmdname, const char *const *argv)
    {
	return win32_execvp(cmdname, argv);
    };
    virtual int Execvp(const char *cmdname, const char *const *argv)
    {
	return win32_execvp(cmdname, argv);
    };
    virtual uid_t Getuid(void)
    {
	return getuid();
    };
    virtual uid_t Geteuid(void)
    {
	return geteuid();
    };
    virtual gid_t Getgid(void)
    {
	return getgid();
    };
    virtual gid_t Getegid(void)
    {
	return getegid();
    };
    virtual char *Getlogin(void)
    {
	return g_getlogin();
    };
    virtual int Kill(int pid, int sig)
    {
	return win32_kill(pid, sig);
    };
    virtual int Killpg(int pid, int sig)
    {
	croak("killpg not implemented!\n");
	return 0;
    };
    virtual int PauseProc(void)
    {
	return win32_sleep((32767L << 16) + 32767);
    };
    virtual PerlIO* Popen(const char *command, const char *mode)
    {
	win32_fflush(stdout);
	win32_fflush(stderr);
	return (PerlIO*)win32_popen(command, mode);
    };
    virtual int Pclose(PerlIO *stream)
    {
	return win32_pclose((FILE*)stream);
    };
    virtual int Pipe(int *phandles)
    {
	return win32_pipe(phandles, 512, O_BINARY);
    };
    virtual int Setuid(uid_t u)
    {
	return setuid(u);
    };
    virtual int Setgid(gid_t g)
    {
	return setgid(g);
    };
    virtual int Sleep(unsigned int s)
    {
	return win32_sleep(s);
    };
    virtual int Times(struct tms *timebuf)
    {
	return win32_times(timebuf);
    };
    virtual int Wait(int *status)
    {
	return win32_wait(status);
    };
    virtual int Waitpid(int pid, int *status, int flags)
    {
	return win32_waitpid(pid, status, flags);
    };
    virtual Sighandler_t Signal(int sig, Sighandler_t subcode)
    {
	return 0;
    };
    virtual void GetSysMsg(char*& sMsg, DWORD& dwLen, DWORD dwErr)
    {
	dwLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
			  |FORMAT_MESSAGE_IGNORE_INSERTS
			  |FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			   dwErr, 0, (char *)&sMsg, 1, NULL);
	if (0 < dwLen) {
	    while (0 < dwLen  &&  isspace(sMsg[--dwLen]))
		;
	    if ('.' != sMsg[dwLen])
		dwLen++;
	    sMsg[dwLen]= '\0';
	}
	if (0 == dwLen) {
	    sMsg = (char*)LocalAlloc(0, 64/**sizeof(TCHAR)*/);
	    dwLen = sprintf(sMsg,
			"Unknown error #0x%lX (lookup 0x%lX)",
			dwErr, GetLastError());
	}
    };
    virtual void FreeBuf(char* sMsg)
    {
	LocalFree(sMsg);
    };
    virtual BOOL DoCmd(char *cmd)
    {
	do_spawn2(cmd, EXECF_EXEC);
        return FALSE;
    };
    virtual int Spawn(char* cmds)
    {
	return do_spawn2(cmds, EXECF_SPAWN);
    };
    virtual int Spawnvp(int mode, const char *cmdname, const char *const *argv)
    {
	return win32_spawnvp(mode, cmdname, argv);
    };
    virtual int ASpawn(void *vreally, void **vmark, void **vsp)
    {
	return g_do_aspawn(vreally, vmark, vsp);
    };
};


class CPerlStdIO : public IPerlStdIO
{
public:
    CPerlStdIO() {};
    virtual PerlIO* Stdin(void)
    {
	return (PerlIO*)win32_stdin();
    };
    virtual PerlIO* Stdout(void)
    {
	return (PerlIO*)win32_stdout();
    };
    virtual PerlIO* Stderr(void)
    {
	return (PerlIO*)win32_stderr();
    };
    virtual PerlIO* Open(const char *path, const char *mode, int &err)
    {
	PerlIO*pf = (PerlIO*)win32_fopen(path, mode);
	if(errno)
	    err = errno;
	return pf;
    };
    virtual int Close(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_fclose(((FILE*)pf)))
    };
    virtual int Eof(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_feof((FILE*)pf))
    };
    virtual int Error(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_ferror((FILE*)pf))
    };
    virtual void Clearerr(PerlIO* pf, int &err)
    {
	win32_clearerr((FILE*)pf);
    };
    virtual int Getc(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_getc((FILE*)pf))
    };
    virtual char* GetBase(PerlIO* pf, int &err)
    {
#ifdef FILE_base
	FILE *f = (FILE*)pf;
	return FILE_base(f);
#else
	return Nullch;
#endif
    };
    virtual int GetBufsiz(PerlIO* pf, int &err)
    {
#ifdef FILE_bufsiz
	FILE *f = (FILE*)pf;
	return FILE_bufsiz(f);
#else
	return (-1);
#endif
    };
    virtual int GetCnt(PerlIO* pf, int &err)
    {
#ifdef USE_STDIO_PTR
	FILE *f = (FILE*)pf;
	return FILE_cnt(f);
#else
	return (-1);
#endif
    };
    virtual char* GetPtr(PerlIO* pf, int &err)
    {
#ifdef USE_STDIO_PTR
	FILE *f = (FILE*)pf;
	return FILE_ptr(f);
#else
	return Nullch;
#endif
    };
    virtual char* Gets(PerlIO* pf, char* s, int n, int& err)
    {
	char* ret = win32_fgets(s, n, (FILE*)pf);
	if(errno)
	    err = errno;
	return ret;
    };
    virtual int Putc(PerlIO* pf, int c, int &err)
    {
	CALLFUNCERR(win32_fputc(c, (FILE*)pf))
    };
    virtual int Puts(PerlIO* pf, const char *s, int &err)
    {
	CALLFUNCERR(win32_fputs(s, (FILE*)pf))
    };
    virtual int Flush(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_fflush((FILE*)pf))
    };
    virtual int Ungetc(PerlIO* pf,int c, int &err)
    {
	CALLFUNCERR(win32_ungetc(c, (FILE*)pf))
    };
    virtual int Fileno(PerlIO* pf, int &err)
    {
	CALLFUNCERR(win32_fileno((FILE*)pf))
    };
    virtual PerlIO* Fdopen(int fd, const char *mode, int &err)
    {
	PerlIO* pf = (PerlIO*)win32_fdopen(fd, mode);
	if(errno)
	    err = errno;
	return pf;
    };
    virtual PerlIO* Reopen(const char*path, const char*mode, PerlIO* pf, int &err)
    {
	PerlIO* newPf = (PerlIO*)win32_freopen(path, mode, (FILE*)pf);
	if(errno)
	    err = errno;
	return newPf;
    };
    virtual SSize_t Read(PerlIO* pf, void *buffer, Size_t size, int &err)
    {
	SSize_t i = win32_fread(buffer, 1, size, (FILE*)pf);
	if(errno)
	    err = errno;
	return i;
    };
    virtual SSize_t Write(PerlIO* pf, const void *buffer, Size_t size, int &err)
    {
	SSize_t i = win32_fwrite(buffer, 1, size, (FILE*)pf);
	if(errno)
	    err = errno;
	return i;
    };
    virtual void SetBuf(PerlIO* pf, char* buffer, int &err)
    {
	win32_setbuf((FILE*)pf, buffer);
    };
    virtual int SetVBuf(PerlIO* pf, char* buffer, int type, Size_t size, int &err)
    {
	int i = win32_setvbuf((FILE*)pf, buffer, type, size);
	if(errno)
	    err = errno;
	return i;
    };
    virtual void SetCnt(PerlIO* pf, int n, int &err)
    {
#ifdef STDIO_CNT_LVALUE
	FILE *f = (FILE*)pf;
	FILE_cnt(f) = n;
#endif
    };
    virtual void SetPtrCnt(PerlIO* pf, char * ptr, int n, int& err)
    {
#ifdef STDIO_PTR_LVALUE
	FILE *f = (FILE*)pf;
	FILE_ptr(f) = ptr;
	FILE_cnt(f) = n;
#endif
    };
    virtual void Setlinebuf(PerlIO* pf, int &err)
    {
	win32_setvbuf((FILE*)pf, NULL, _IOLBF, 0);
    };
    virtual int Printf(PerlIO* pf, int &err, const char *format,...)
    {
	va_list(arglist);
	va_start(arglist, format);
	int i = win32_vfprintf((FILE*)pf, format, arglist);
	if(errno)
	    err = errno;
	return i;
    };
    virtual int Vprintf(PerlIO* pf, int &err, const char *format, va_list arglist)
    {
	int i = win32_vfprintf((FILE*)pf, format, arglist);
	if(errno)
	    err = errno;
	return i;
    };
    virtual long Tell(PerlIO* pf, int &err)
    {
	long l = win32_ftell((FILE*)pf);
	if(errno)
	    err = errno;
	return l;
    };
    virtual int Seek(PerlIO* pf, off_t offset, int origin, int &err)
    {
	int i = win32_fseek((FILE*)pf, offset, origin);
	if(errno)
	    err = errno;
	return i;
    };
    virtual void Rewind(PerlIO* pf, int &err)
    {
	win32_rewind((FILE*)pf);
    };
    virtual PerlIO* Tmpfile(int &err)
    {
	PerlIO* pf = (PerlIO*)win32_tmpfile();
	if(errno)
	    err = errno;
	return pf;
    };
    virtual int Getpos(PerlIO* pf, Fpos_t *p, int &err)
    {
	int i = win32_fgetpos((FILE*)pf, p);
	if(errno)
	    err = errno;
	return i;
    };
    virtual int Setpos(PerlIO* pf, const Fpos_t *p, int &err)
    {
	int i = win32_fsetpos((FILE*)pf, p);
	if(errno)
	    err = errno;
	return i;
    };
    virtual void Init(int &err)
    {
    };
    virtual void InitOSExtras(void* p)
    {
	Perl_init_os_extras();
    };
    virtual int OpenOSfhandle(long osfhandle, int flags)
    {
	return win32_open_osfhandle(osfhandle, flags);
    }
    virtual int GetOSfhandle(int filenum)
    {
	return win32_get_osfhandle(filenum);
    }
};

class CPerlHost
{
public:
    CPerlHost() { pPerl = NULL; };
    inline BOOL PerlCreate(void)
    {
	try
	{
	    pPerl = perl_alloc(&perlMem, &perlEnv, &perlStdIO, &perlLIO,
			       &perlDir, &perlSock, &perlProc);
	    if(pPerl != NULL)
	    {
		try
		{
		    pPerl->perl_construct();
		}
		catch(...)
		{
		    win32_fprintf(stderr, "%s\n",
				  "Error: Unable to construct data structures");
		    pPerl->perl_free();
		    pPerl = NULL;
		}
	    }
	}
	catch(...)
	{
	    win32_fprintf(stderr, "%s\n", "Error: Unable to allocate memory");
	    pPerl = NULL;
	}
	return (pPerl != NULL);
    };
    inline int PerlParse(void (*xs_init)(CPerlObj*), int argc, char** argv, char** env)
    {
	int retVal;
	try
	{
	    retVal = pPerl->perl_parse(xs_init, argc, argv, env);
	}
	catch(int x)
	{
	    // this is where exit() should arrive
	    retVal = x;
	}
	catch(...)
	{
	    win32_fprintf(stderr, "Error: Parse exception\n");
	    retVal = -1;
	}
	*win32_errno() = 0;
	return retVal;
    };
    inline int PerlRun(void)
    {
	int retVal;
	try
	{
	    retVal = pPerl->perl_run();
	}
	catch(int x)
	{
	    // this is where exit() should arrive
	    retVal = x;
	}
	catch(...)
	{
	    win32_fprintf(stderr, "Error: Runtime exception\n");
	    retVal = -1;
	}
	return retVal;
    };
    inline void PerlDestroy(void)
    {
	try
	{
	    pPerl->perl_destruct();
	    pPerl->perl_free();
	}
	catch(...)
	{
	}
    };

protected:
    CPerlDir	perlDir;
    CPerlEnv	perlEnv;
    CPerlLIO	perlLIO;
    CPerlMem	perlMem;
    CPerlProc	perlProc;
    CPerlSock	perlSock;
    CPerlStdIO	perlStdIO;
};
