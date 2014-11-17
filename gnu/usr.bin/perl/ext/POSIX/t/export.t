#!./perl -w

use strict;
use Test::More;
use Config;

plan(skip_all => "POSIX is unavailable")
    unless $Config{extensions} =~ /\bPOSIX\b/;

require POSIX;
POSIX->import();

# @POSIX::EXPORT and @POSIX::EXPORT_OK are generated. The intent of this test is
# to catch *unintended* changes to them introduced by bugs in refactoring.

my %expect = (
    EXPORT => [qw(%SIGRT ARG_MAX B0 B110 B1200 B134 B150 B1800 B19200 B200
		  B2400 B300 B38400 B4800 B50 B600 B75 B9600 BRKINT BUFSIZ
		  CHAR_BIT CHAR_MAX CHAR_MIN CHILD_MAX CLK_TCK CLOCAL
		  CLOCKS_PER_SEC CREAD CS5 CS6 CS7 CS8 CSIZE CSTOPB DBL_DIG
		  DBL_EPSILON DBL_MANT_DIG DBL_MAX DBL_MAX_10_EXP DBL_MAX_EXP
		  DBL_MIN DBL_MIN_10_EXP DBL_MIN_EXP E2BIG EACCES EADDRINUSE
		  EADDRNOTAVAIL EAFNOSUPPORT EAGAIN EALREADY EBADF EBADMSG EBUSY
		  ECANCELED ECHILD ECHO ECHOE ECHOK ECHONL ECONNABORTED
		  ECONNREFUSED ECONNRESET EDEADLK EDESTADDRREQ EDOM EDQUOT
		  EEXIST EFAULT EFBIG EHOSTDOWN EHOSTUNREACH EIDRM EILSEQ
		  EINPROGRESS EINTR EINVAL EIO EISCONN EISDIR ELOOP EMFILE
		  EMLINK EMSGSIZE ENAMETOOLONG ENETDOWN ENETRESET ENETUNREACH
		  ENFILE ENOBUFS ENODATA ENODEV ENOENT ENOEXEC ENOLCK ENOLINK
		  ENOMEM ENOMSG ENOPROTOOPT ENOSPC ENOSR ENOSTR ENOSYS ENOTBLK
		  ENOTCONN ENOTDIR ENOTEMPTY ENOTRECOVERABLE ENOTSOCK ENOTSUP
		  ENOTTY ENXIO EOF EOPNOTSUPP EOTHER EOVERFLOW EOWNERDEAD EPERM
		  EPFNOSUPPORT EPIPE EPROCLIM EPROTO EPROTONOSUPPORT EPROTOTYPE
		  ERANGE EREMOTE ERESTART EROFS ESHUTDOWN ESOCKTNOSUPPORT ESPIPE
		  ESRCH ESTALE ETIME ETIMEDOUT ETOOMANYREFS ETXTBSY EUSERS
		  EWOULDBLOCK EXDEV
		  EXIT_FAILURE EXIT_SUCCESS FD_CLOEXEC FILENAME_MAX
		  FLT_DIG FLT_EPSILON FLT_MANT_DIG FLT_MAX FLT_MAX_10_EXP
		  FLT_MAX_EXP FLT_MIN FLT_MIN_10_EXP FLT_MIN_EXP FLT_RADIX
		  FLT_ROUNDS F_DUPFD F_GETFD F_GETFL F_GETLK F_OK F_RDLCK
		  F_SETFD F_SETFL F_SETLK F_SETLKW F_UNLCK F_WRLCK HUGE_VAL
		  HUPCL ICANON ICRNL IEXTEN IGNBRK IGNCR IGNPAR INLCR INPCK
		  INT_MAX INT_MIN ISIG ISTRIP IXOFF IXON LC_ALL LC_COLLATE
		  LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME LDBL_DIG
		  LDBL_EPSILON LDBL_MANT_DIG LDBL_MAX LDBL_MAX_10_EXP
		  LDBL_MAX_EXP LDBL_MIN LDBL_MIN_10_EXP LDBL_MIN_EXP LINK_MAX
		  LONG_MAX LONG_MIN L_ctermid L_cuserid L_tmpname MAX_CANON
		  MAX_INPUT MB_CUR_MAX MB_LEN_MAX NAME_MAX NCCS NDEBUG
		  NGROUPS_MAX NOFLSH NULL OPEN_MAX OPOST O_ACCMODE O_APPEND
		  O_CREAT O_EXCL O_NOCTTY O_NONBLOCK O_RDONLY O_RDWR O_TRUNC
		  O_WRONLY PARENB PARMRK PARODD PATH_MAX PIPE_BUF RAND_MAX R_OK
		  SA_NOCLDSTOP SA_NOCLDWAIT SA_NODEFER SA_ONSTACK SA_RESETHAND
		  SA_RESTART SA_SIGINFO SCHAR_MAX SCHAR_MIN SEEK_CUR SEEK_END
		  SEEK_SET SHRT_MAX SHRT_MIN SIGABRT SIGALRM SIGBUS SIGCHLD
		  SIGCONT SIGFPE SIGHUP SIGILL SIGINT SIGKILL SIGPIPE SIGPOLL
		  SIGPROF SIGQUIT SIGRTMAX SIGRTMIN SIGSEGV SIGSTOP SIGSYS
		  SIGTERM SIGTRAP SIGTSTP SIGTTIN SIGTTOU SIGURG SIGUSR1
		  SIGUSR2 SIGVTALRM SIGXCPU SIGXFSZ SIG_BLOCK SIG_DFL SIG_ERR
		  SIG_IGN SIG_SETMASK SIG_UNBLOCK SSIZE_MAX STDERR_FILENO
		  STDIN_FILENO STDOUT_FILENO STREAM_MAX S_IRGRP S_IROTH S_IRUSR
		  S_IRWXG S_IRWXO S_IRWXU S_ISBLK S_ISCHR S_ISDIR S_ISFIFO
		  S_ISGID S_ISREG S_ISUID S_IWGRP S_IWOTH S_IWUSR S_IXGRP
		  S_IXOTH S_IXUSR TCIFLUSH TCIOFF TCIOFLUSH TCION TCOFLUSH
		  TCOOFF TCOON TCSADRAIN TCSAFLUSH TCSANOW TMP_MAX TOSTOP
		  TZNAME_MAX UCHAR_MAX UINT_MAX ULONG_MAX USHRT_MAX VEOF VEOL
		  VERASE VINTR VKILL VMIN VQUIT VSTART VSTOP VSUSP VTIME
		  WEXITSTATUS WIFEXITED WIFSIGNALED WIFSTOPPED WNOHANG WSTOPSIG
		  WTERMSIG WUNTRACED W_OK X_OK _PC_CHOWN_RESTRICTED
		  _PC_LINK_MAX _PC_MAX_CANON _PC_MAX_INPUT _PC_NAME_MAX
		  _PC_NO_TRUNC _PC_PATH_MAX _PC_PIPE_BUF _PC_VDISABLE
		  _POSIX_ARG_MAX _POSIX_CHILD_MAX _POSIX_CHOWN_RESTRICTED
		  _POSIX_JOB_CONTROL _POSIX_LINK_MAX _POSIX_MAX_CANON
		  _POSIX_MAX_INPUT _POSIX_NAME_MAX _POSIX_NGROUPS_MAX
		  _POSIX_NO_TRUNC _POSIX_OPEN_MAX _POSIX_PATH_MAX
		  _POSIX_PIPE_BUF _POSIX_SAVED_IDS _POSIX_SSIZE_MAX
		  _POSIX_STREAM_MAX _POSIX_TZNAME_MAX _POSIX_VDISABLE
		  _POSIX_VERSION _SC_ARG_MAX _SC_CHILD_MAX _SC_CLK_TCK
		  _SC_JOB_CONTROL _SC_NGROUPS_MAX _SC_OPEN_MAX _SC_PAGESIZE
		  _SC_SAVED_IDS _SC_STREAM_MAX _SC_TZNAME_MAX _SC_VERSION _exit
		  abort access acos asctime asin assert atan atexit atof atoi
		  atol bsearch calloc ceil cfgetispeed cfgetospeed cfsetispeed
		  cfsetospeed clearerr clock cosh creat ctermid ctime cuserid
		  difftime div dup dup2 errno execl execle execlp execv execve
		  execvp fabs fclose fdopen feof ferror fflush fgetc fgetpos
		  fgets floor fmod fopen fpathconf fprintf fputc fputs fread
		  free freopen frexp fscanf fseek fsetpos fstat fsync ftell
		  fwrite getchar getcwd getegid getenv geteuid getgid getgroups
		  getpid gets getuid isalnum isalpha isatty iscntrl isdigit
		  isgraph islower isprint ispunct isspace isupper isxdigit labs
		  ldexp ldiv localeconv log10 longjmp lseek malloc mblen
		  mbstowcs mbtowc memchr memcmp memcpy memmove memset mkfifo
		  mktime modf offsetof pathconf pause perror pow putc putchar
		  puts qsort raise realloc remove rewind scanf setbuf setgid
		  setjmp setlocale setpgid setsid setuid setvbuf sigaction
		  siglongjmp signal sigpending sigprocmask sigsetjmp sigsuspend
		  sinh sscanf stderr stdin stdout strcat strchr strcmp strcoll
		  strcpy strcspn strerror strftime strlen strncat strncmp
		  strncpy strpbrk strrchr strspn strstr strtod strtok strtol
		  strtoul strxfrm sysconf tan tanh tcdrain tcflow tcflush
		  tcgetattr tcgetpgrp tcsendbreak tcsetattr tcsetpgrp tmpfile
		  tmpnam tolower toupper ttyname tzname tzset uname ungetc
		  vfprintf vprintf vsprintf wcstombs wctomb)],
    EXPORT_OK => [qw(abs alarm atan2 chdir chmod chown close closedir cos exit
		     exp fcntl fileno fork getc getgrgid getgrnam getlogin
		     getpgrp getppid getpwnam getpwuid gmtime kill lchown link
		     localtime log mkdir nice open opendir pipe printf rand
		     read readdir rename rewinddir rmdir sin sleep sprintf sqrt
		     srand stat system time times umask unlink utime wait
		     waitpid write)],
);

plan (tests => 2 * keys %expect);

while (my ($var, $expect) = each %expect) {
    my $have = *{$POSIX::{$var}}{ARRAY};
    cmp_ok(@$have, '==', @$expect,
	   "Correct number of entries for \@POSIX::$var");
    is_deeply([sort @$have], $expect, "Correct entries for \@POSIX::$var");
}
