/*	$Id: main.c,v 1.29 2017/01/21 15:53:15 jmc Exp $ */
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

#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "parse.h"

#define WWW_DIR "/var/www/acme"
#define CONF_FILE "/etc/acme-client.conf"

int
main(int argc, char *argv[])
{
	const char	 **alts = NULL;
	char		 *certdir = NULL, *certfile = NULL;
	char		 *chainfile = NULL, *fullchainfile = NULL;
	char		 *acctkey = NULL;
	char		 *chngdir = NULL, *auth = NULL, *agreement = NULL;
	char		 *conffile = CONF_FILE;
	int		  key_fds[2], acct_fds[2], chng_fds[2], cert_fds[2];
	int		  file_fds[2], dns_fds[2], rvk_fds[2];
	int		  force = 0;
	int		  c, rc, revocate = 0;
	int		  popts = 0;
	pid_t		  pids[COMP__MAX];
	extern int	  verbose;
	extern enum comp  proccomp;
	size_t		  i, altsz, ne;

	struct acme_conf	*conf = NULL;
	struct authority_c	*authority = NULL;
	struct domain_c		*domain = NULL;
	struct altname_c	*ac;

	while (-1 != (c = getopt(argc, argv, "FADrvnf:")))
		switch (c) {
		case 'f':
			if (NULL == (conffile = strdup(optarg)))
				err(EXIT_FAILURE, "strdup");
			break;
		case 'F':
			force = 1;
			break;
		case 'A':
			popts |= ACME_OPT_NEWACCT;
			break;
		case 'D':
			popts |= ACME_OPT_NEWDKEY;
			break;
		case 'r':
			revocate = 1;
			break;
		case 'v':
			verbose = verbose ? 2 : 1;
			popts |= ACME_OPT_VERBOSE;
			break;
		case 'n':
			popts |= ACME_OPT_CHECK;
			break;
		default:
			goto usage;
		}

	/* parse config file */
	if ((conf = parse_config(conffile, popts)) == NULL)
		exit(EXIT_FAILURE);

	argc -= optind;
	argv += optind;
	if (1 != argc)
		goto usage;

	if ((domain = domain_find(conf, argv[0])) == NULL)
		errx(EXIT_FAILURE, "domain %s not found\n", argv[0]);

	argc--;
	argv++;

	if (getuid() != 0)
		errx(EXIT_FAILURE, "must be run as root");

	if (domain->cert != NULL) {
		if ((certdir = dirname(domain->cert)) != NULL) {
			if ((certdir = strdup(certdir)) == NULL)
				err(EXIT_FAILURE, "strdup");
		} else
			err(EXIT_FAILURE, "dirname");
	} else {
		/* the parser enforces that at least cert or fullchain is set */
		if ((certdir = dirname(domain->fullchain)) != NULL) {
			if ((certdir = strdup(certdir)) == NULL)
				err(EXIT_FAILURE, "strdup");
		} else
			err(EXIT_FAILURE, "dirname");

	}

	if (domain->cert != NULL) {
		if ((certfile = basename(domain->cert)) != NULL) {
			if ((certfile = strdup(certfile)) == NULL)
				err(EXIT_FAILURE, "strdup");
		} else
			err(EXIT_FAILURE, "basename");
	}

	if(domain->chain != NULL) {
		if ((chainfile = strstr(domain->chain, certdir)) != NULL)
			chainfile = domain->chain + strlen(certdir);
		else
			chainfile = domain->chain;

		if ((chainfile = strdup(chainfile)) == NULL)
			err(EXIT_FAILURE, "strdup");
	}

	if(domain->fullchain != NULL) {
		if ((fullchainfile = strstr(domain->fullchain, certdir)) != NULL)
			fullchainfile = domain->fullchain + strlen(certdir);
		else
			fullchainfile = domain->fullchain;

		if ((fullchainfile = strdup(fullchainfile)) == NULL)
			err(EXIT_FAILURE, "strdup");
	}

	if ((auth = domain->auth) == NULL) {
		/* use the first authority from the config as default XXX */
		authority = authority_find0(conf);
		if (authority == NULL)
			errx(EXIT_FAILURE, "no authorities configured\n");
	} else {
		authority = authority_find(conf, auth);
		if (authority == NULL)
			errx(EXIT_FAILURE, "authority %s not found\n", auth);
	}

	agreement = authority->agreement;
	acctkey = authority->account;

	if (NULL == acctkey) {
		/* XXX replace with existance check in parse.y */
		err(EXIT_FAILURE, "no account key in config?");
	}
	if (domain->challengedir == NULL)
		chngdir = strdup(WWW_DIR);
	else
		chngdir = domain->challengedir;

	if (NULL == chngdir)
		err(EXIT_FAILURE, "strdup");

	/*
	 * Do some quick checks to see if our paths exist.
	 * This will be done in the children, but we might as well check
	 * now before the fork.
	 * XXX maybe use conf_check_file() from parse.y
	 */

	ne = 0;

	if (-1 == access(certdir, R_OK)) {
		warnx("%s: cert directory must exist", certdir);
		ne++;
	}

	if (!(popts & ACME_OPT_NEWDKEY) && -1 == access(domain->key, R_OK)) {
		warnx("%s: domain key file must exist", domain->key);
		ne++;
	} else if ((popts & ACME_OPT_NEWDKEY) && -1 != access(domain->key, R_OK)) {
		dodbg("%s: domain key exists (not creating)", domain->key);
		popts &= ~ACME_OPT_NEWDKEY;
	}

	if (-1 == access(chngdir, R_OK)) {
		warnx("%s: challenge directory must exist", chngdir);
		ne++;
	}

	if (!(popts & ACME_OPT_NEWACCT) && -1 == access(acctkey, R_OK)) {
		warnx("%s: account key file must exist", acctkey);
		ne++;
	} else if ((popts & ACME_OPT_NEWACCT) && -1 != access(acctkey, R_OK)) {
		dodbg("%s: account key exists (not creating)", acctkey);
		popts &= ~ACME_OPT_NEWACCT;
	}

	if (ne > 0)
		exit(EXIT_FAILURE);

	if (popts & ACME_OPT_CHECK)
		exit(EXIT_SUCCESS);

	/* Set the zeroth altname as our domain. */
	altsz = domain->altname_count + 1;
	alts = calloc(altsz, sizeof(char *));
	if (NULL == alts)
		err(EXIT_FAILURE, "calloc");
	alts[0] = domain->domain;
	i = 1;
	/* XXX get rid of alts[] later */
	TAILQ_FOREACH(ac, &domain->altname_list, entry)
		alts[i++] = ac->domain;

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
		    (popts & ACME_OPT_NEWACCT), revocate, authority,
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
		c = keyproc(key_fds[0], domain->key,
		    (const char **)alts, altsz, (popts & ACME_OPT_NEWDKEY));
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
		c = acctproc(acct_fds[0], acctkey, (popts & ACME_OPT_NEWACCT));
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
		c = chngproc(chng_fds[0], chngdir);
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
		c = fileproc(file_fds[1], certdir, certfile, chainfile,
		    fullchainfile);
		/*
		 * This is different from the other processes in that it
		 * can return 2 if the certificates were updated.
		 */
		exit(c > 1 ? 2 : (c ? EXIT_SUCCESS : EXIT_FAILURE));
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
		    certfile != NULL ? certfile : fullchainfile,
		    force, revocate,
		    (const char *const *)alts, altsz);
		free(alts);
		exit(c ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	close(rvk_fds[0]);

	/* Jail: sandbox, file-system, user. */

	if (pledge("stdio", NULL) == -1) {
		warn("pledge");
		exit(EXIT_FAILURE);
	}

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

	free(alts);
	return (COMP__MAX != rc ? EXIT_FAILURE :
	    (2 == c ? EXIT_SUCCESS : 2));
usage:
	fprintf(stderr,
	    "usage: acme-client [-ADFnrv] [-f configfile] domain\n");
	return (EXIT_FAILURE);
}
