/*	$OpenBSD: config.h,v 1.1.1.1 1998/09/14 21:53:31 art Exp $	*/
/* include/config.h.  Generated automatically by configure.  */
/* include/config.h.in.  Generated automatically from configure.in by autoheader.  */

#ifndef BROKEN_MMAP
/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1
#endif

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/*  Define this if we can include both sys/dir.h and dirent.h */
#define USE_SYS_DIR_H 1

/*  Define this if RETSIGTYPE == void  */
/* #undef VOID_RETSIGTYPE */

/*  Define this if struct winsize is declared in sys/termios.h */
#define HAVE_STRUCT_WINSIZE 1

/*  Define this if struct winsize have ws_xpixel */
#define HAVE_WS_XPIXEL 1

/*  Define this if struct winsize have ws_ypixel */
#define HAVE_WS_YPIXEL 1

/* Define this if `struct sockaddr' includes sa_len */
#define SOCKADDR_HAS_SA_LEN 1

/* define if the system is missing a prototype for strtok_r() */
#define NEED_STRTOK_R_PROTO 1

/* define if you have h_errno */
#define HAVE_H_ERRNO 1

/* define if you have h_errlist but not hstrerror */
#define HAVE_H_ERRLIST 1

/* define if you have h_nerr but not hstrerror */
#define HAVE_H_NERR 1

/* define if your system has a declaration for h_errlist */
/* #undef HAVE_H_ERRLIST_DECLARATION */

/* define if your system has a declaration for h_nerr */
/* #undef HAVE_H_NERR_DECLARATION */

/* define if your system has a declaration for h_errno */
#define HAVE_H_ERRNO_DECLARATION 1

/* define if your system has optreset */
#define HAVE_OPTRESET 1

/* define if your system has a declaration for optreset */
#define HAVE_OPTRESET_DECLARATION 1

/* define if your compiler has __FUNCTION__ */
#define HAVE___FUNCTION__ 1

/* define if your compiler has __attribute__ */
#define HAVE___ATTRIBUTE__ 1

/* Check if select need a prototype */
/* #undef NEED_SELECT_PROTO */

/* Define if you have the FOUR_ARGUMENT_VFS_BUSY function.  */
#define HAVE_FOUR_ARGUMENT_VFS_BUSY 1

/* Define if you have the THREE_ARGUMENT_VFS_BUSY function.  */
/* #undef HAVE_THREE_ARGUMENT_VFS_BUSY */

/* Define if you have the THREE_ARGUMENT_VGET function.  */
#define HAVE_THREE_ARGUMENT_VGET 1

/* Define if you have the THREE_ARGUMENT_VOP_LOCK function.  */
#define HAVE_THREE_ARGUMENT_VOP_LOCK 1

/* Define if you have the TWO_ARGUMENT_VFS_GETNEWFSID function.  */
/* #undef HAVE_TWO_ARGUMENT_VFS_GETNEWFSID */

/* Define if you have the TWO_ARGUMENT_VGET function.  */
/* #undef HAVE_TWO_ARGUMENT_VGET */

/* Define if you have the TWO_ARGUMENT_VOP_LOCK function.  */
/* #undef HAVE_TWO_ARGUMENT_VOP_LOCK */

/* Define if you have the asnprintf function.  */
/* #undef HAVE_ASNPRINTF */

/* Define if you have the asprintf function.  */
#define HAVE_ASPRINTF 1

/* Define if you have the chown function.  */
#define HAVE_CHOWN 1

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Define if you have the dbm_firstkey function.  */
#define HAVE_DBM_FIRSTKEY 1

/* Define if you have the dn_expand function.  */
#define HAVE_DN_EXPAND 1

/* Define if you have the el_init function.  */
#define HAVE_EL_INIT 1

/* Define if you have the err function.  */
#define HAVE_ERR 1

/* Define if you have the errx function.  */
#define HAVE_ERRX 1

/* Define if you have the fchown function.  */
#define HAVE_FCHOWN 1

/* Define if you have the fcntl function.  */
#define HAVE_FCNTL 1

/* Define if you have the flock function.  */
#define HAVE_FLOCK 1

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the gethostbyname function.  */
#define HAVE_GETHOSTBYNAME 1

/* Define if you have the getopt function.  */
#define HAVE_GETOPT 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getrlimit function.  */
#define HAVE_GETRLIMIT 1

/* Define if you have the getspnam function.  */
/* #undef HAVE_GETSPNAM */

/* Define if you have the getspuid function.  */
/* #undef HAVE_GETSPUID */

/* Define if you have the getusershell function.  */
#define HAVE_GETUSERSHELL 1

/* Define if you have the getvfsbyname function.  */
/* #undef HAVE_GETVFSBYNAME */

/* Define if you have the hstrerror function.  */
#define HAVE_HSTRERROR 1

/* Define if you have the inet_aton function.  */
#define HAVE_INET_ATON 1

/* Define if you have the initgroups function.  */
#define HAVE_INITGROUPS 1

/* Define if you have the kvm_nlist function.  */
#define HAVE_KVM_NLIST 1

/* Define if you have the kvm_open function.  */
#define HAVE_KVM_OPEN 1

/* Define if you have the lstat function.  */
#define HAVE_LSTAT 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the rcmd function.  */
#define HAVE_RCMD 1

/* Define if you have the readline function.  */
#define HAVE_READLINE 1

/* Define if you have the readv function.  */
#define HAVE_READV 1

/* Define if you have the recvmsg function.  */
#define HAVE_RECVMSG 1

/* Define if you have the res_search function.  */
#define HAVE_RES_SEARCH 1

/* Define if you have the sendmsg function.  */
#define HAVE_SENDMSG 1

/* Define if you have the setegid function.  */
#define HAVE_SETEGID 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setregid function.  */
#define HAVE_SETREGID 1

/* Define if you have the setresuid function.  */
/* #undef HAVE_SETRESUID */

/* Define if you have the setreuid function.  */
#define HAVE_SETREUID 1

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the setsockopt function.  */
#define HAVE_SETSOCKOPT 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the strlwr function.  */
/* #undef HAVE_STRLWR */

/* Define if you have the strnlen function.  */
/* #undef HAVE_STRNLEN */

/* Define if you have the strsep function.  */
#define HAVE_STRSEP 1

/* Define if you have the strtok_r function.  */
/* #undef HAVE_STRTOK_R */

/* Define if you have the strupr function.  */
/* #undef HAVE_STRUPR */

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the sysctl function.  */
#define HAVE_SYSCTL 1

/* Define if you have the syslog function.  */
#define HAVE_SYSLOG 1

/* Define if you have the tgetent function.  */
#define HAVE_TGETENT 1

/* Define if you have the unsetenv function.  */
#define HAVE_UNSETENV 1

/* Define if you have the vasnprintf function.  */
/* #undef HAVE_VASNPRINTF */

/* Define if you have the vasprintf function.  */
#define HAVE_VASPRINTF 1

/* Define if you have the verr function.  */
#define HAVE_VERR 1

/* Define if you have the verrx function.  */
#define HAVE_VERRX 1

/* Define if you have the vfsisloadable function.  */
/* #undef HAVE_VFSISLOADABLE */

/* Define if you have the vfsload function.  */
/* #undef HAVE_VFSLOAD */

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the vsyslog function.  */
#define HAVE_VSYSLOG 1

/* Define if you have the vwarn function.  */
#define HAVE_VWARN 1

/* Define if you have the vwarnx function.  */
#define HAVE_VWARNX 1

/* Define if you have the warn function.  */
#define HAVE_WARN 1

/* Define if you have the warnx function.  */
#define HAVE_WARNX 1

/* Define if you have the writev function.  */
#define HAVE_WRITEV 1

/* Define if you have the <arpa/inet.h> header file.  */
#define HAVE_ARPA_INET_H 1

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <crypt.h> header file.  */
/* #undef HAVE_CRYPT_H */

/* Define if you have the <dbm.h> header file.  */
/* #undef HAVE_DBM_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <grp.h> header file.  */
#define HAVE_GRP_H 1

/* Define if you have the <inttypes.h> header file.  */
#define HAVE_INTTYPES_H 1

/* Define if you have the <ktypes.h> header file.  */
/* #undef HAVE_KTYPES_H */

/* Define if you have the <kvm.h> header file.  */
#define HAVE_KVM_H 1

/* Define if you have the <libelf/nlist.h> header file.  */
/* #undef HAVE_LIBELF_NLIST_H */

/* Define if you have the <linux/types.h> header file.  */
/* #undef HAVE_LINUX_TYPES_H */

/* Define if you have the <miscfs/genfs/genfs.h> header file.  */
/* #undef HAVE_MISCFS_GENFS_GENFS_H */

/* Define if you have the <ndbm.h> header file.  */
#define HAVE_NDBM_H 1

/* Define if you have the <netdb.h> header file.  */
#define HAVE_NETDB_H 1

/* Define if you have the <netinet/in.h> header file.  */
#define HAVE_NETINET_IN_H 1

/* Define if you have the <netinet/in6.h> header file.  */
/* #undef HAVE_NETINET_IN6_H */

/* Define if you have the <netinet/in6_machtypes.h> header file.  */
/* #undef HAVE_NETINET_IN6_MACHTYPES_H */

/* Define if you have the <netinet6/in6.h> header file.  */
/* #undef HAVE_NETINET6_IN6_H */

/* Define if you have the <nlist.h> header file.  */
#define HAVE_NLIST_H 1

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <pwd.h> header file.  */
#define HAVE_PWD_H 1

/* Define if you have the <resolv.h> header file.  */
#define HAVE_RESOLV_H 1

/* Define if you have the <rpcsvc/dbm.h> header file.  */
/* #undef HAVE_RPCSVC_DBM_H */

/* Define if you have the <shadow.h> header file.  */
/* #undef HAVE_SHADOW_H */

/* Define if you have the <sys/bitypes.h> header file.  */
/* #undef HAVE_SYS_BITYPES_H */

/* Define if you have the <sys/cdefs.h> header file.  */
#define HAVE_SYS_CDEFS_H 1

/* Define if you have the <sys/dirent.h> header file.  */
#define HAVE_SYS_DIRENT_H 1

/* Define if you have the <sys/filedesc.h> header file.  */
#define HAVE_SYS_FILEDESC_H 1

/* Define if you have the <sys/ioccom.h> header file.  */
#define HAVE_SYS_IOCCOM_H 1

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/lkm.h> header file.  */
#define HAVE_SYS_LKM_H 1

/* Define if you have the <sys/mman.h> header file.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/mount.h> header file.  */
#define HAVE_SYS_MOUNT_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/poll.h> header file.  */
#define HAVE_SYS_POLL_H 1

/* Define if you have the <sys/proc.h> header file.  */
#define HAVE_SYS_PROC_H 1

/* Define if you have the <sys/queue.h> header file.  */
#define HAVE_SYS_QUEUE_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/sysconfig.h> header file.  */
/* #undef HAVE_SYS_SYSCONFIG_H */

/* Define if you have the <sys/sysctl.h> header file.  */
#define HAVE_SYS_SYSCTL_H 1

/* Define if you have the <sys/sysent.h> header file.  */
/* #undef HAVE_SYS_SYSENT_H */

/* Define if you have the <sys/sysproto.h> header file.  */
/* #undef HAVE_SYS_SYSPROTO_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/tty.h> header file.  */
#define HAVE_SYS_TTY_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/uio.h> header file.  */
#define HAVE_SYS_UIO_H 1

/* Define if you have the <sys/vfs.h> header file.  */
/* #undef HAVE_SYS_VFS_H */

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the curses library (-lcurses).  */
/* #undef HAVE_LIBCURSES */

/* Define if you have the db library (-ldb).  */
/* #undef HAVE_LIBDB */

/* Define if you have the edit library (-ledit).  */
#define HAVE_LIBEDIT 1

/* Define if you have the gdbm library (-lgdbm).  */
/* #undef HAVE_LIBGDBM */

/* Define if you have the kvm library (-lkvm).  */
#define HAVE_LIBKVM 1

/* Define if you have the ndbm library (-lndbm).  */
/* #undef HAVE_LIBNDBM */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the readline library (-lreadline).  */
/* #undef HAVE_LIBREADLINE */

/* Define if you have the resolv library (-lresolv).  */
/* #undef HAVE_LIBRESOLV */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the syslog library (-lsyslog).  */
/* #undef HAVE_LIBSYSLOG */

/* Define if you have the termcap library (-ltermcap).  */
#define HAVE_LIBTERMCAP 1

#define HAVE_INT8_T 1
#define HAVE_INT16_T 1
#define HAVE_INT32_T 1
#define HAVE_INT64_T 1
#define HAVE_U_INT8_T 1
#define HAVE_U_INT16_T 1
#define HAVE_U_INT32_T 1
#define HAVE_U_INT64_T 1
/* #undef HAVE_BOOL */
#define HAVE_SSIZE_T 1
#define HAVE_REGISTER_T 1
/* #undef HAVE_INT32 */
/* #undef HAVE_U_INT32 */

#define EFF_NTOHL ntohl

/* RCSID */
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

#define VERSION "0.9"
#define PACKAGE "arla"

/* Check for posix signals */
#define HAVE_POSIX_SIGNALS 1

#define HAVE_READLINE 1

/* prefix for /dev/fd */
/* #undef DEV_FD_PREFIX */

/* does the system have /dev/fd? */
/* #undef HAVE_DEV_FD */

/* we always have stds.h */
#define HAVE_STDS_H

/* We have krb_principal from kth-krb ? */
#define HAVE_KRB_PRINCIPAL 1

/* If we have _res */
/* #undef HAVE__RES */

/* Define if you have kerberos */
#define KERBEROS 1

/* Define if your kernel has a vop_nolock */
/* #undef HAVE_KERNEL_VOP_NOLOCK */

/* Define if your kernel has a vop_nounlock */
/* #undef HAVE_KERNEL_VOP_NOUNLOCK */

/* Define if your kernel has a vop_noislocked */
/* #undef HAVE_KERNEL_VOP_NOISLOCKED */

/* Define if your kernel has a vop_revoke */
/* #undef HAVE_KERNEL_VOP_REVOKE */

/* Define if your kernel has a genfs_nolock */
/* #undef HAVE_KERNEL_GENFS_NOLOCK */

/* Define if your kernel has a genfs_nounlock */
/* #undef HAVE_KERNEL_GENFS_NOUNLOCK */

/* Define if your kernel has a genfs_noislocked */
/* #undef HAVE_KERNEL_GENFS_NOISLOCKED */

/* Define if your kernel has a genfs_revoke */
/* #undef HAVE_KERNEL_GENFS_REVOKE */

/* Define if your kernel has a vfs_opv_init_default */
#define HAVE_KERNEL_VFS_OPV_INIT_DEFAULT 1

/* Define if your kernel has a vfs_attach */
/* #undef HAVE_KERNEL_VFS_ATTACH */

/* Define if your kernel has a vfs_getnewfsid */
#define HAVE_KERNEL_VFS_GETNEWFSID 1

/* Define if your kernel has a vfs_opv_init_explicit */
#define HAVE_KERNEL_VFS_OPV_INIT_EXPLICIT 1

/* Define if your struct dirent has a field d_type */
#define HAVE_STRUCT_DIRENT_D_TYPE 1

/* Define if your struct vfsconf has a field vfc_refcount */
#define HAVE_STRUCT_VFSCONF_VFC_REFCOUNT 1

/* Define if your struct uio has a field uio_procp */
/* #undef HAVE_STRUCT_UIO_UIO_PROCP */

/* Define if your struct vfsops has a field vfs_opv_descs */
/* #undef HAVE_STRUCT_VFSOPS_VFS_OPV_DESCS */

/* Define if your struct vfsops has a field vfs_name */
/* #undef HAVE_STRUCT_VFSOPS_VFS_NAME */

/* Define if you want to use mmap:ed time */
/* #undef USE_MMAPTIME */

/* Define if your hstrerror needs const like SunOS 5.6 */ 
#define NEED_HSTRERROR_CONST 1

/* Define if your hstrerror need proto */
/* #undef NEED_HSTRERROR_PROTO */

/* Define if you have gnu libc */
/* #undef HAVE_GLIBC */

/* Define this if `struct sockaddr' includes sa_len */
#define SOCKADDR_HAS_SA_LEN 1

/* Define this if htnol is broken, but can be fixed with define magic */
/* #undef HAVE_REPAIRABLE_HTONL */

/* Define this if struct ViceIoctl is defined by linux/fs.h */
/* #undef HAVE_STRUCT_VICEIOCTL_IN */

/* Linux kernel types */
/* #undef HAVE_LINUX_KERNEL_INT8_T */
/* #undef HAVE_LINUX_KERNEL_INT16_T */
/* #undef HAVE_LINUX_KERNEL_INT32_T */
/* #undef HAVE_LINUX_KERNEL_INT64_T */
/* #undef HAVE_LINUX_KERNEL_U_INT8_T */
/* #undef HAVE_LINUX_KERNEL_U_INT16_T */
/* #undef HAVE_LINUX_KERNEL_U_INT32_T */
/* #undef HAVE_LINUX_KERNEL_U_INT64_T */

/* Define this if you have a type vop_t */
/* #undef HAVE_VOP_T */

/* Define this is you have a vfssw */
/* #undef HAVE_VFSSW */

/* Define this if struct proc have p_sigmask */
#define HAVE_STRUCT_PROC_P_SIGMASK 1

