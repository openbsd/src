/*
**	$Id: parse.c,v 1.1.1.1 1995/10/18 08:43:18 deraadt Exp $
**
** parse.c                         This file contains the protocol parser
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 6 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>

#include <sys/types.h>
#include <netinet/in.h>

#ifndef HPUX7
#  include <arpa/inet.h>
#endif

#include <nlist.h>
#include <kvm.h>

#include <sys/types.h>
#include <sys/stat.h>

#if defined(MIPS) || defined(BSD43)
extern int errno;
#endif

#include "identd.h"
#include "error.h"

extern void *malloc();

/*
** This function will eat whitespace characters until
** either a non-whitespace character is read, or EOF
** occurs. This function is only used if the "-m" option
** is enabled.
*/
static int eat_whitespace()
{
  int c;

  
  while ((c = getchar()) != EOF &&
	 !(c == '\r' || c == '\n'))
    ;

  if (c != EOF)
    while ((c = getchar()) != EOF &&
	   (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
      ;

  if (c != EOF)
    ungetc(c, stdin);
  
  return (c != EOF);
}


#ifdef INCLUDE_EXTENSIONS
/*
** Validate an indirect request
*/
static int valid_fhost(faddr, password)
  struct in_addr *faddr;
  char *password;
{
  if (indirect_host == NULL)
    return 0;

  if (strcmp(indirect_host, "*") != 0)
  {
    if (isdigit(indirect_host[0]))
    {
      if (strcmp(inet_ntoa(*faddr), indirect_host))
      {
	syslog(LOG_NOTICE, "valid_fhost: access denied for: %s",
	       gethost(faddr));
	return 0;
      }
    }
    else
    {
      if (strcmp(gethost(faddr), indirect_host))
      {
	syslog(LOG_NOTICE, "valid_fhost: access denied for: %s",
	       gethost(faddr));
	return 0;
      }
    }
  }
      
  if (indirect_password == NULL)
    return 1;
  
  if (strcmp(password, indirect_password))
  {
    syslog(LOG_NOTICE, "valid_fhost: invalid password from: %s",
	   gethost(faddr));
    return 0;
  }

  return 1;
}
#endif

/*
** A small routine to check for the existance of the ".noident"
** file in a users home directory.
*/
static int check_noident(homedir)
  char *homedir;
{
  char *tmp_path;
  struct stat sbuf;
  int rcode;
  

  if (!homedir)
    return 0;
  
  tmp_path = (char *) malloc(strlen(homedir) + sizeof("/.noident") + 1);
  if (!tmp_path)
    return 0;

  strcpy(tmp_path, homedir);
  strcat(tmp_path, "/.noident");

  rcode = stat(tmp_path, &sbuf);
  free(tmp_path);

  return (rcode == 0);
}


int parse(fp, laddr, faddr)
  FILE *fp;
  struct in_addr *laddr, *faddr;
{
  int uid, try, rcode;
  struct passwd *pwp;
  char lhostaddr[16];
  char fhostaddr[16];
  char password[33];
#ifdef INCLUDE_EXTENSIONS  
  char arg[33];
  int c;
#endif
  struct in_addr laddr2;
  struct in_addr faddr2;
  
  
  if (debug_flag && syslog_flag)
    syslog(LOG_DEBUG, "In function parse()");
  
  /*
  ** Get the local/foreign port pair from the luser
  */
  do
  {
    if (debug_flag && syslog_flag)
      syslog(LOG_DEBUG, "  Before fscanf()");
    
    faddr2 = *faddr;
    laddr2 = *laddr;
    lport = fport = 0;
    lhostaddr[0] = fhostaddr[0] = password[0] = '\0';

    /* Read query from client */
    rcode = fscanf(fp, " %d , %d", &lport, &fport);

#ifdef INCLUDE_EXTENSIONS
    /*
    ** Do additional parsing in case of extended request
    */
    if (rcode == 0)
    {
      rcode = fscanf(fp, "%32[^ \t\n\r:]", arg);

      /* Skip leading space up to EOF, EOL or non-space char */
      while ((c = getc(fp)) == ' ' || c == '\t')
	;
      
      if (rcode <= 0)
      {
	printf("%d , %d : ERROR : %s\r\n",
	       lport, fport,
	       unknown_flag ? "UNKNOWN-ERROR" : "X-INVALID-REQUEST");
	continue;
      }

      /*
      ** Non-standard extended request, returns with Pidentd
      ** version information
      */
      if (strcmp(arg, "VERSION") == 0)
      {
	printf("%d , %d : ERROR : X-VERSION : %s\r\n", lport, fport,
	       version);
	continue;
      }

      /*
      ** Non-standard extended proxy request
      */
      else if (strcmp(arg, "PROXY") == 0 && c == ':')
      {
	/* We have a colon char, check for port numbers */
	rcode = fscanf(fp, " %d , %d : %15[0-9.] , %15[0-9.]",
		       &lport, &fport, fhostaddr, lhostaddr);

	if (!(rcode == 3 || rcode == 4))
	{
	  printf("%d , %d : ERROR : %s\r\n",
		 lport, fport,
		 unknown_flag ? "UNKNOWN-ERROR" : "X-INVALID-REQUEST");
	  continue;
	}

	if (rcode == 4)
	  (void) inet_aton(lhostaddr, &laddr2);
	
	(void) inet_aton(fhostaddr, &faddr2);

	proxy(&laddr2, &faddr2, lport, fport, NULL);
	continue;
      }
      
      /*
      ** Non-standard extended remote indirect request
      */
      else if (strcmp(arg, "REMOTE") == 0 && c == ':')
      {
	/* We have a colon char, check for port numbers */
	rcode = fscanf(fp, " %d , %d", &lport, &fport);
	
	/* Skip leading space up to EOF, EOL or non-space char */
	while ((c = getc(fp)) == ' ' || c == '\t')
	  ;

	if (rcode != 2 || c != ':')
	{
	  printf("%d , %d : ERROR : %s\r\n",
		 lport, fport,
		 unknown_flag ? "UNKNOWN-ERROR" : "X-INVALID-REQUEST");
	  continue;
	}
	    
	/* We have a colon char, check for addr and password */
	rcode = fscanf(fp, " %15[0-9.] , %32[^ \t\r\n]",
		       fhostaddr, password);
	if (rcode > 0)
	  rcode += 2;
	else
	{
	  printf("%d , %d : ERROR : %s\r\n",
		 lport, fport,
		 unknown_flag ? "UNKNOWN-ERROR" : "X-INVALID-REQUEST");
	  continue;
	}
	
	/*
	** Verify that the host originating the indirect request
	** is allowed to do that
	*/
	if (!valid_fhost(faddr, password))
	{
	  printf("%d , %d : ERROR : %s\r\n",
		 lport, fport,
		 unknown_flag ? "UNKNOWN-ERROR" : "X-ACCESS-DENIED");
	  continue;
	}
	
	(void) inet_aton(fhostaddr, &faddr2);
      }
      
      else
      {
	printf("%d , %d : ERROR : %s\r\n",
	       lport, fport,
	       unknown_flag ? "UNKNOWN-ERROR" : "X-INVALID-REQUEST");
	continue;
      }
    }
#endif /* EXTENSIONS */
    
    if (rcode < 2 || lport < 1 || lport > 65535 || fport < 1 || fport > 65535)
    {
      if (syslog_flag && rcode > 0)
	syslog(LOG_NOTICE, "scanf: invalid-port(s): %d , %d from %s",
	       lport, fport, gethost(faddr));
      
      printf("%d , %d : ERROR : %s\r\n",
	     lport, fport,
	     unknown_flag ? "UNKNOWN-ERROR" : "INVALID-PORT");
      continue;
    }

    if (syslog_flag && verbose_flag)
	syslog(LOG_NOTICE, "request for (%d,%d) from %s",
	       lport, fport, gethost(faddr));

    if (debug_flag && syslog_flag)
      syslog(LOG_DEBUG, "  After fscanf(), before k_getuid()");
    
    /*
    ** Next - get the specific TCP connection and return the
    ** uid - user number.
    **
    ** Try to fetch the information 5 times incase the
    ** kernel changed beneath us and we missed or took
    ** a fault.
    */
    for (try = 0;
	 (try < 5 &&
	   k_getuid(&faddr2, htons(fport), laddr, htons(lport), &uid) == -1);
	 try++)
      ;

    if (try >= 5)
    {
      if (syslog_flag)
	syslog(LOG_DEBUG, "Returned: %d , %d : NO-USER", lport, fport);
      
      printf("%d , %d : ERROR : %s\r\n",
	     lport, fport,
	     unknown_flag ? "UNKNOWN-ERROR" : "NO-USER");
      continue;
    }

    if (try > 0 && syslog_flag)
      syslog(LOG_NOTICE, "k_getuid retries: %d", try);
    
    if (debug_flag && syslog_flag)
      syslog(LOG_DEBUG, "  After k_getuid(), before getpwuid()");

    /*
    ** Then we should try to get the username. If that fails we
    ** return it as an OTHER identifier
    */
    pwp = getpwuid(uid);
    
    if (!pwp)
    {
      if (syslog_flag)
	syslog(LOG_WARNING, "getpwuid() could not map uid (%d) to name",
	       uid);

      printf("%d , %d : USERID : OTHER%s%s : %d\r\n",
	     lport, fport,
	     charset_name ? " , " : "",
	     charset_name ? charset_name : "",
	     uid);
      continue;
    }

    /*
    ** Hey! We finally made it!!!
    */
    if (syslog_flag)
      syslog(LOG_DEBUG, "Successful lookup: %d , %d : %s\n",
	     lport, fport, pwp->pw_name);

    if (noident_flag && check_noident(pwp->pw_dir))
    {
      if (syslog_flag && verbose_flag)
	syslog(LOG_NOTICE, "user %s requested HIDDEN-USER for host %s: %d, %d",
	       pwp->pw_name,
	       gethost(faddr),
	       lport, fport);
      
      printf("%d , %d : ERROR : HIDDEN-USER\r\n",
	   lport, fport);
      continue;
    }

    if (number_flag)
      printf("%d , %d : USERID : OTHER%s%s : %d\r\n",
	     lport, fport,
	     charset_name ? " , " : "",
	     charset_name ? charset_name : "",
	     uid);
    else
      printf("%d , %d : USERID : %s%s%s : %s\r\n",
	     lport, fport,
	     other_flag ? "OTHER" : "UNIX",
	     charset_name ? " , " : "",
	     charset_name ? charset_name : "",
	     pwp->pw_name);
    
  } while(fflush(stdout), fflush(stderr), multi_flag && eat_whitespace());

  return 0;
}
