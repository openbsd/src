/* db.c

   Persistent database management routines for DHCPD... */

/*
 * Copyright (c) 1995, 1996 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"

FILE *db_file;

static int counting = 0;
static int count = 0;
TIME write_time;

/* Write the specified lease to the current lease database file. */

int write_lease (lease)
	struct lease *lease;
{
	struct tm *t;
	char tbuf [64];
	int errors = 0;
	int i;

	if (counting)
		++count;
	errno = 0;
	fprintf (db_file, "lease %s {\n", piaddr (lease -> ip_addr));
	if (errno) {
		++errors;
	}

	t = gmtime (&lease -> starts);
	snprintf (tbuf, sizeof tbuf, "%d %d/%02d/%02d %02d:%02d:%02d;",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	errno = 0;
	fprintf (db_file, "\tstarts %s\n", tbuf);
	if (errno) {
		++errors;
	}

	t = gmtime (&lease -> ends);
	snprintf (tbuf, sizeof tbuf,"%d %d/%02d/%02d %02d:%02d:%02d;",
		 t -> tm_wday, t -> tm_year + 1900,
		 t -> tm_mon + 1, t -> tm_mday,
		 t -> tm_hour, t -> tm_min, t -> tm_sec);
	errno = 0;
	fprintf (db_file, "\tends %s", tbuf);
	if (errno) {
		++errors;
	}

	if (lease -> hardware_addr.hlen) {
		errno = 0;
		fprintf (db_file, "\n\thardware %s %s;",
			 hardware_types [lease -> hardware_addr.htype],
			 print_hw_addr (lease -> hardware_addr.htype,
					lease -> hardware_addr.hlen,
					lease -> hardware_addr.haddr));
		if (errno) {
			++errors;
		}
	}
	if (lease -> uid_len) {
		int i;
		errno = 0;
		fprintf (db_file, "\n\tuid %2.2x", lease -> uid [0]);
		if (errno) {
			++errors;
		}
		for (i = 1; i < lease -> uid_len; i++) {
			errno = 0;
			fprintf (db_file, ":%2.2x", lease -> uid [i]);
			if (errno) {
				++errors;
			}
		}
		putc (';', db_file);
	}
	if (lease -> flags & BOOTP_LEASE) {
		errno = 0;
		fprintf (db_file, "\n\tdynamic-bootp;");
		if (errno) {
			++errors;
		}
	}
	if (lease -> flags & ABANDONED_LEASE) {
		errno = 0;
		fprintf (db_file, "\n\tabandoned;");
		if (errno) {
			++errors;
		}
	}
	if (lease -> client_hostname) {
		for (i = 0; lease -> client_hostname [i]; i++)
			if (lease -> client_hostname [i] < 33 ||
			    lease -> client_hostname [i] > 126)
				goto bad_client_hostname;
		errno = 0;
		fprintf (db_file, "\n\tclient-hostname \"%s\";",
			 lease -> client_hostname);
		if (errno) {
			++errors;
		}
	}
       bad_client_hostname:
	if (lease -> hostname) {
		for (i = 0; lease -> hostname [i]; i++)
			if (lease -> hostname [i] < 33 ||
			    lease -> hostname [i] > 126)
				goto bad_hostname;
		errno = 0;
		errno = 0;
		fprintf (db_file, "\n\thostname \"%s\";",
			 lease -> hostname);
		if (errno) {
			++errors;
		}
	}
       bad_hostname:
	errno = 0;
	fputs ("\n}\n", db_file);
	if (errno) {
		++errors;
	}
	if (errors)
		note ("write_lease: unable to write lease %s",
		      piaddr (lease -> ip_addr));
	return !errors;
}

/* Commit any leases that have been written out... */

int commit_leases ()
{
	/* Commit any outstanding writes to the lease database file.
	   We need to do this even if we're rewriting the file below,
	   just in case the rewrite fails. */
	if (fflush (db_file) == EOF) {
		note ("commit_leases: unable to commit: %m");
		return 0;
	}
	if (fsync (fileno (db_file)) == -1) {
		note ("commit_leases: unable to commit: %m");
		return 0;
	}

	/* If we've written more than a thousand leases or if
	   we haven't rewritten the lease database in over an
	   hour, rewrite it now. */
	if (count > 1000 || (count && cur_time - write_time > 3600)) {
		count = 0;
		write_time = cur_time;
		new_lease_file ();
	}
	return 1;
}

void db_startup ()
{
	/* Read in the existing lease file... */
	read_leases ();

	GET_TIME (&write_time);
	new_lease_file ();
}

void new_lease_file ()
{
	char newfname [MAXPATHLEN];
	char backfname [MAXPATHLEN];
	TIME t;
	int db_fd;

	/* If we already have an open database, close it. */
	if (db_file) {
		fclose (db_file);
	}

	/* Make a temporary lease file... */
	GET_TIME (&t);
	snprintf (newfname, sizeof newfname,"%s.%d", path_dhcpd_db, (int)t);
	db_fd = open (newfname, O_WRONLY | O_TRUNC | O_CREAT, 0664);
	if (db_fd == -1) {
		error ("Can't create new lease file: %m");
	}
	if ((db_file = fdopen (db_fd, "w")) == NULL) {
		error ("Can't fdopen new lease file!");
	}

	/* Write an introduction so people don't complain about time
	   being off. */
	fprintf (db_file, "# All times in this file are in UTC (GMT), not %s",
		 "your local timezone.\n");
	fprintf (db_file, "# The format of this file is documented in the %s",
		 "dhcpd.leases(5) manual page.\n\n");

	/* Write out all the leases that we know of... */
	counting = 0;
	write_leases ();

	/* Get the old database out of the way... */
	snprintf (backfname, sizeof backfname, "%s~", path_dhcpd_db);
	if (unlink (backfname) == -1 && errno != ENOENT)
		error ("Can't remove old lease database backup %s: %m",
		       backfname);
	if (link (path_dhcpd_db, backfname) == -1)
		error ("Can't backup lease database %s to %s: %m",
		       path_dhcpd_db, backfname);
	
	/* Move in the new file... */
	if (rename (newfname, path_dhcpd_db) == -1)
		error ("Can't install new lease database %s to %s: %m",
		       newfname, path_dhcpd_db);

	counting = 1;
}
