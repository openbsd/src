/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$Id: yplog.c,v 1.1 1995/11/01 16:56:20 deraadt Exp $";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

FILE	*yplogfile;

void
yplog_date(line)
	char *line;
{
	char datestr[20];
	time_t t;

	if (yplogfile != NULL) {
	  (void)time(&t);
	  (void)strftime(datestr,20,"%b %d %T",localtime(&t));
	  fprintf(yplogfile,"%s %s\n",datestr,line);
	  fflush(yplogfile);
	}
}

void
yplog_line(line)
	char *line;
{
	if (yplogfile != NULL) {
	  fprintf(yplogfile,"                %s\n",line);
	  fflush(yplogfile);
	}
}

void
yplog_str(line)
	char *line;
{
	if (yplogfile != NULL) {
	  fprintf(yplogfile,"                %s",line);
	  fflush(yplogfile);
	}
}

void
yplog_cat(line)
	char *line;
{
	if (yplogfile != NULL) {
	  fprintf(yplogfile,"%s",line);
	  fflush(yplogfile);
	}
}

void
yplog_call(transp)
	SVCXPRT *transp;
{
	struct sockaddr_in *caller;

	if (yplogfile != NULL) {
	  caller = svc_getcaller(transp);
	  fprintf(yplogfile,"                  caller: %s %d\n",
		  inet_ntoa(caller->sin_addr),
		  ntohs(caller->sin_port));
	  fflush(yplogfile);
	}
}

void
yplog_init(progname)
	char *progname;
{
	char file_path[255];
	struct stat finfo;

	sprintf(file_path,"/var/yp/%s.log",progname);
	if ((stat(file_path, &finfo) == 0) &&
	    ((finfo.st_mode & S_IFMT) == S_IFREG)) {
	  yplogfile = fopen(file_path,"a");
	  sprintf(file_path,"%s[%d] : started",progname,getpid());
	  yplog_date(file_path);
	}
}

void
yplog_exit()
{
	if (yplogfile != NULL) {
	  yplog_date("controlled shutdown");
	  fclose(yplogfile);
	}
}

