/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <nlist.h>
#include <kvm.h>

#include "identd.h"
#include "error.h"

/*
 * A small routine to check for the existance of the ".noident"
 * file in a users home directory.
 */
static int 
check_noident(homedir)
	char   *homedir;
{
	char   path[MAXPATHLEN];
	struct stat st;

	if (!homedir)
		return 0;
	if (snprintf(path, sizeof path, "%s/.noindent", homedir) >= sizeof path)
		return 0;
	if (stat(path, &st) == 0)
		return 1;
	return 0;
}

int 
parse(fp, laddr, faddr)
	FILE   *fp;
	struct in_addr *laddr, *faddr;
{
	char	lhostaddr[16], fhostaddr[16], password[33];
	struct	in_addr laddr2, faddr2;
	struct	passwd *pw;
	int	try, rcode;
	uid_t	uid;

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "In function parse()");

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  Before fscanf()");
	faddr2 = *faddr;
	laddr2 = *laddr;
	lport = fport = 0;
	lhostaddr[0] = fhostaddr[0] = password[0] = '\0';

	/* Read query from client */
	rcode = fscanf(fp, " %d , %d", &lport, &fport);

	if (rcode < 2 || lport < 1 || lport > 65535 ||
	    fport < 1 || fport > 65535) {
		if (syslog_flag && rcode > 0)
			syslog(LOG_NOTICE,
			    "scanf: invalid-port(s): %d , %d from %s",
			    lport, fport, gethost(faddr));
		printf("%d , %d : ERROR : %s\r\n", lport, fport,
		    unknown_flag ? "UNKNOWN-ERROR" : "INVALID-PORT");
		return 0;
	}
	if (syslog_flag && verbose_flag)
		syslog(LOG_NOTICE, "request for (%d,%d) from %s",
		    lport, fport, gethost(faddr));

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After fscanf(), before k_getuid()");

	/*
	 * Next - get the specific TCP connection and return the
	 * uid - user number.
	 *
	 * Try to fetch the information 5 times incase the
	 * kernel changed beneath us and we missed or took
	 * a fault.
	 */
	for (try = 0; try < 5; try++)
		if (k_getuid(&faddr2, htons(fport), laddr,
		    htons(lport), &uid) != -1)
			break;

	if (try >= 5) {
		if (syslog_flag)
			syslog(LOG_DEBUG, "Returned: %d , %d : NO-USER",	
			    lport, fport);
		printf("%d , %d : ERROR : %s\r\n", lport, fport,
		    unknown_flag ? "UNKNOWN-ERROR" : "NO-USER");
		return 0;
	}
	if (try > 0 && syslog_flag)
		syslog(LOG_NOTICE, "k_getuid retries: %d", try);

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After k_getuid(), before getpwuid()");

	pw = getpwuid(uid);
	if (!pw) {
		if (syslog_flag)
			syslog(LOG_WARNING,
			    "getpwuid() could not map uid (%d) to name",
			    uid);
		printf("%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		return 0;
	}

	if (syslog_flag)
		syslog(LOG_DEBUG, "Successful lookup: %d , %d : %s\n",
		    lport, fport, pw->pw_name);

	if (noident_flag && check_noident(pw->pw_dir)) {
		if (syslog_flag && verbose_flag)
			syslog(LOG_NOTICE,
			    "user %s requested HIDDEN-USER for host %s: %d, %d",
			    pw->pw_name, gethost(faddr), lport, fport);
		printf("%d , %d : ERROR : HIDDEN-USER\r\n", lport, fport);
		return 0;
	}

	if (number_flag) {
		printf("%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		return 0;
	}
	printf("%d , %d : USERID : %s%s%s :%s\r\n",
	    lport, fport, other_flag ? "OTHER" : "UNIX",
	    charset_name ? " , " : "",
	    charset_name ? charset_name : "", pw->pw_name);
	return 0;
}
