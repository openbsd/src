/* Define if the target supports PTRACE_PEEKUSR for register access.  */
#undef HAVE_LINUX_USRREGS

/* Define if the target supports PTRACE_GETREGS for register access.  */
#undef HAVE_LINUX_REGSETS

/* Define if the target supports PTRACE_GETFPXREGS for extended
   register access.  */
#undef HAVE_PTRACE_GETFPXREGS

/* Define if <sys/procfs.h> has prgregset_t. */
#undef HAVE_PRGREGSET_T

/* Define if <sys/procfs.h> has prfpregset_t. */
#undef HAVE_PRFPREGSET_T

/* Define if <sys/procfs.h> has lwpid_t. */
#undef HAVE_LWPID_T

/* Define if <sys/procfs.h> has psaddr_t. */
#undef HAVE_PSADDR_T

/* Define if the prfpregset_t type is broken. */
#undef PRFPREGSET_T_BROKEN
