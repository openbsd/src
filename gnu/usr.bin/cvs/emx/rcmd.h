/* rcmd.h --- interface to executing commands on remote hosts
   Karl Fogel <kfogel@cyclic.com> --- November 1995  */

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

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

/* Note that because we are using windows-NT/rcmd.c, this declaration
   must match windows-NT/rcmd.h (and rcmd.c).  It would be much
   more sensible to use a common header file.  But I haven't bothered to
   adjust the makefile accordingly, yet.  Probably the best long-term
   home for this would be in lib/rcmd.*, or perhaps src.  */
extern int rcmd (const char **AHOST,
		 unsigned short INPORT,
		 char *LOCUSER,
		 char *REMUSER,
		 char *CMD,
		 int *fd2p);
