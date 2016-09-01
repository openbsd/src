/*	$Id: main.c,v 1.5 2016/09/01 00:03:39 benno Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define AGREEMENT "https://letsencrypt.org" \
		  "/documents/LE-SA-v1.1.1-August-1-2016.pdf"
#define SSL_DIR "/etc/ssl/acme"
#define SSL_PRIV_DIR "/etc/ssl/acme/private"
#define ETC_DIR "/etc/acme"
#define WWW_DIR "/var/www/acme"
#define PRIVKEY_FILE "privkey.pem"

/*
 * This isn't RFC1035 compliant, but does the bare minimum in making
 * sure that we don't get bogus domain names on the command line, which
 * might otherwise screw up our directory structure.
 * Returns zero on failure, non-zero on success.
 */
static int
domain_valid(const char *cp)
{

	for ( ; '\0' != *cp; cp++)
		if (!('.' == *cp || '-' == *cp ||
		    '_' == *cp || isalnum((int)*cp)))
			return (0);
	return (1);
}

/*
 * Wrap around asprintf(3), which sometimes nullifies the input values,
 * sometimes not, but always returns <0 on error.
 * Returns NULL on failure or the pointer on success.
 */
static char *
doasprintf(const char *fmt, ...)
{
	int	 c;
	char	*cp;
	va_list	 ap;

	va_start(ap, fmt);
	c = vasprintf(&cp, fmt, ap);
	va_end(ap);
	return (c < 0 ? NULL : cp);
}

int
main(int argc, char *argv[])
{
	const char	 *domain, *agreement;
	char		 *certdir, *acctkey, *chngdir, *keyfile;
	int		  key_fds[2], acct_fds[2], chng_fds[2],
			  cert_fds[2], file_fds[2], dns_fds[2],
			  rvk_fds[2];
	pid_t		  pids[COMP__MAX];
	int		  c, rc, newacct, remote, revocate, force,
			  staging, multidir, newkey, backup;
	extern int	  verbose;
	extern enum comp  proccomp;
	size_t		  i, altsz, ne;
	const char	**alts;

	alts = NULL;
	newacct = remote = revocate = verbose = force =
		multidir = staging = newkey = backup = 0;
	certdir = keyfile = acctkey = chngdir = NULL;
	agreement = AGREEMENT;

	while (-1 != (c = getopt(argc, argv, "bFmnNrstva:f:c:C:k:")))
		switch (c) {
		case ('a'):
			agreement = optarg;
			break;
		case ('b'):
			backup = 1;
			break;
		case ('c'):
			free(certdir);
			if (NULL == (certdir = strdup(optarg)))
				err(EXIT_FAILURE, "strdup");
			break;
		case ('C'):
			free(chngdir);
			if (NULL == (chngdir = strdup(optarg)))
				err(EXIT_FAILURE, "strdup");
			break;
		case ('f'):
			free(acctkey);
			if (NULL == (acctkey = strdup(optarg)))
				err(EXIT_FAILURE, "strdup");
			break;
		case ('F'):
			force = 1;
			break;
		case ('k'):
			free(keyfile);
			if (NULL == (keyfile = strdup(optarg)))
				err(EXIT_FAILURE, "strdup");
			break;
		case ('m'):
			multidir = 1;
			break;
		case ('n'):
			newacct = 1;
			break;
		case ('N'):
			newkey = 1;
			break;
		case ('r'):
			revocate = 1;
			break;
		case ('s'):
			staging = 1;
			break;
		case ('t'):
			/*
			 / Undocumented feature.
			 * Don't use it.
			 */
			remote = 1;
			break;
		case ('v'):
			verbose = verbose ? 2 : 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;
	if (0 == argc)
		goto usage;

	/* Make sure that the domains are sane. */

	for (i = 0; i < (size_t)argc; i++) {
		if (domain_valid(argv[i]))
			continue;
		errx(EXIT_FAILURE, "%s: bad domain syntax", argv[i]);
	}

	domain = argv[0];
	argc--;
	argv++;

	if ( ! checkprivs())
		errx(EXIT_FAILURE, "must be run as root");

	/*
	 * Now we allocate our directories and file paths IFF we haven't
	 * specified them on the command-line.
	 * If we're in "multidir" (-m) mode, we use our initial domain
	 * name when specifying the prefixes.
	 * Otherwise, we put them all in a known location.
	 */

	if (NULL == certdir)
		certdir = multidir ?
			doasprintf(SSL_DIR "/%s", domain) :
			strdup(SSL_DIR);
	if (NULL == keyfile)
		keyfile = multidir ?
			doasprintf(SSL_PRIV_DIR "/%s/"
				PRIVKEY_FILE, domain) :
			strdup(SSL_PRIV_DIR "/" PRIVKEY_FILE);
	if (NULL == acctkey)
		acctkey = multidir ?
			doasprintf(ETC_DIR "/%s/"
				PRIVKEY_FILE, domain) :
			strdup(ETC_DIR "/" PRIVKEY_FILE);
	if (NULL == chngdir)
		chngdir = strdup(WWW_DIR);

	if (NULL == certdir || NULL == keyfile ||
	    NULL == acctkey || NULL == chngdir)
		err(EXIT_FAILURE, "strdup");

	/*
	 * Do some quick checks to see if our paths exist.
	 * This will be done in the children, but we might as well check
	 * now before the fork.
	 */

	ne = 0;

	if (-1 == access(certdir, R_OK)) {
		warnx("%s: -c directory must exist", certdir);
		ne++;
	}

	if ( ! newkey && -1 == access(keyfile, R_OK)) {
		warnx("%s: -k file must exist", keyfile);
		ne++;
	} else if (newkey && -1 != access(keyfile, R_OK)) {
		dodbg("%s: domain key exists "
			"(not creating)", keyfile);
		newkey = 0;
	}

	if (-1 == access(chngdir, R_OK)) {
		warnx("%s: -C directory must exist", chngdir);
		ne++;
	}

	if ( ! newacct && -1 == access(acctkey, R_OK)) {
		warnx("%s: -f file must exist", acctkey);
		ne++;
	} else if (newacct && -1 != access(acctkey, R_OK)) {
		dodbg("%s: account key exists "
			"(not creating)", acctkey);
		newacct = 0;
	}

	if (ne > 0)
		exit(EXIT_FAILURE);

	/* Set the zeroth altname as our domain. */

	altsz = argc + 1;
	alts = calloc(altsz, sizeof(char *));
	if (NULL == alts)
		err(EXIT_FAILURE, "calloc");
	alts[0] = domain;
	for (i = 0; i < (size_t)argc; i++)
		alts[i + 1] = argv[i];

	/*
	 * Open channels between our components.
	 */

	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, key_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, acct_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, chng_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, cert_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, file_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, dns_fds))
		err(EXIT_FAILURE, "socketpair");
	if (-1 == socketpair(AF_UNIX, SOCK_STREAM, 0, rvk_fds))
		err(EXIT_FAILURE, "socketpair");

	/* Start with the network-touching process. */

	if (-1 == (pids[COMP_NET] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_NET]) {
		proccomp = COMP_NET;
		close(key_fds[0]);
		close(acct_fds[0]);
		close(chng_fds[0]);
		close(cert_fds[0]);
		close(file_fds[0]);
		close(file_fds[1]);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		c = netproc(key_fds[1], acct_fds[1],
		    chng_fds[1], cert_fds[1],
		    dns_fds[1], rvk_fds[1],
		    newacct, revocate, staging,
		    (const char *const *)alts, altsz,
		    agreement);
		free(alts);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(key_fds[1]);
	close(acct_fds[1]);
	close(chng_fds[1]);
	close(cert_fds[1]);
	close(dns_fds[1]);
	close(rvk_fds[1]);

	/* Now the key-touching component. */

	if (-1 == (pids[COMP_KEY] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_KEY]) {
		proccomp = COMP_KEY;
		close(cert_fds[0]);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		close(acct_fds[0]);
		close(chng_fds[0]);
		close(file_fds[0]);
		close(file_fds[1]);
		c = keyproc(key_fds[0], keyfile,
			(const char **)alts, altsz, newkey);
		free(alts);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(key_fds[0]);

	/* The account-touching component. */

	if (-1 == (pids[COMP_ACCOUNT] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_ACCOUNT]) {
		proccomp = COMP_ACCOUNT;
		free(alts);
		close(cert_fds[0]);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		close(chng_fds[0]);
		close(file_fds[0]);
		close(file_fds[1]);
		c = acctproc(acct_fds[0], acctkey, newacct);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(acct_fds[0]);

	/* The challenge-accepting component. */

	if (-1 == (pids[COMP_CHALLENGE] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_CHALLENGE]) {
		proccomp = COMP_CHALLENGE;
		free(alts);
		close(cert_fds[0]);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		close(file_fds[0]);
		close(file_fds[1]);
		c = chngproc(chng_fds[0], chngdir, remote);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(chng_fds[0]);

	/* The certificate-handling component. */

	if (-1 == (pids[COMP_CERT] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_CERT]) {
		proccomp = COMP_CERT;
		free(alts);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		close(file_fds[1]);
		c = certproc(cert_fds[0], file_fds[0]);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(cert_fds[0]);
	close(file_fds[0]);

	/* The certificate-handling component. */

	if (-1 == (pids[COMP_FILE] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_FILE]) {
		proccomp = COMP_FILE;
		free(alts);
		close(dns_fds[0]);
		close(rvk_fds[0]);
		c = fileproc(file_fds[1], backup, certdir);
		/*
		 * This is different from the other processes in that it
		 * can return 2 if the certificates were updated.
		 */
		exit(c > 1 ? 2 :
		    (c ? EXIT_SUCCESS : EXIT_FAILURE));
	}

	close(file_fds[1]);

	/* The DNS lookup component. */

	if (-1 == (pids[COMP_DNS] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_DNS]) {
		proccomp = COMP_DNS;
		free(alts);
		close(rvk_fds[0]);
		c = dnsproc(dns_fds[0]);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(dns_fds[0]);

	/* The expiration component. */

	if (-1 == (pids[COMP_REVOKE] = fork()))
		err(EXIT_FAILURE, "fork");

	if (0 == pids[COMP_REVOKE]) {
		proccomp = COMP_REVOKE;
		c = revokeproc(rvk_fds[0], certdir,
			force, revocate,
			(const char *const *)alts, altsz);
		free(alts);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(rvk_fds[0]);

	/* Jail: sandbox, file-system, user. */

	if ( ! sandbox_before())
		exit(EXIT_FAILURE);
	else if ( ! dropfs(PATH_VAR_EMPTY))
		exit(EXIT_FAILURE);
	else if ( ! dropprivs())
		exit(EXIT_FAILURE);
	else if ( ! sandbox_after())
		exit(EXIT_FAILURE);

	/*
	 * Collect our subprocesses.
	 * Require that they both have exited cleanly.
	 */

	rc = checkexit(pids[COMP_KEY], COMP_KEY) +
	    checkexit(pids[COMP_CERT], COMP_CERT) +
	    checkexit(pids[COMP_NET], COMP_NET) +
	    checkexit_ext(&c, pids[COMP_FILE], COMP_FILE) +
	    checkexit(pids[COMP_ACCOUNT], COMP_ACCOUNT) +
	    checkexit(pids[COMP_CHALLENGE], COMP_CHALLENGE) +
	    checkexit(pids[COMP_DNS], COMP_DNS) +
	    checkexit(pids[COMP_REVOKE], COMP_REVOKE);

	free(certdir);
	free(keyfile);
	free(acctkey);
	free(chngdir);
	free(alts);
	return (COMP__MAX != rc ? EXIT_FAILURE :
	    (2 == c ? EXIT_SUCCESS : 2));
usage:
	fprintf(stderr, "usage: %s "
		"[-bFmnNrsv] "
		"[-a agreement] "
		"[-C challengedir] "
		"[-c certdir] "
		"[-f accountkey] "
		"[-k domainkey] "
		"domain [altnames...]\n",
		getprogname());
	free(certdir);
	free(keyfile);
	free(acctkey);
	free(chngdir);
	return (EXIT_FAILURE);
}
