/* rcmd.h --- interface to executing commands on remote hosts
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

/* Run the command CMD on the host *AHOST, and return a file descriptor for
   a bidirectional stream socket connected to the command's standard input
   and output.

   rcmd looks up *AHOST using gethostbyname, and sets *AHOST to the host's
   canonical name.  If *AHOST is not found, rcmd returns -1.

   rcmd connects to the remote host at TCP port INPORT.  This should
   probably be the "shell" service, port 514.

   LOCUSER is the name of the user on the local machine, and REMUSER is
   the name of the user on the remote machine; the remote machine uses this,
   along with the source address of the TCP connection, to authenticate
   the connection.

   CMD is the command to execute.  The remote host will tokenize it any way
   it damn well pleases.  Welcome to Unix.

   FD2P is a feature we don't support, but there's no point in making mindless
   deviations from the interface.  Callers should always pass this argument
   as zero.  */
extern int rcmd (const char **AHOST,
		 unsigned short INPORT,
		 char *LOCUSER,
		 char *REMUSER,
		 char *CMD,
		 int *fd2p);
