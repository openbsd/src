/*	$OpenBSD: ikeca.c,v 1.3 2010/06/07 14:15:27 jsg Exp $	*/
/*	$vantronix: ikeca.c,v 1.13 2010/06/03 15:52:52 reyk Exp $	*/

/*
 * Copyright (c) 2010 Jonathan Gray <jsg@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <fcntl.h>
#include <fts.h>
#include <dirent.h>

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "parser.h"

#define SSL_CNF		"/etc/ssl/openssl.cnf"
#define X509_CNF	"/etc/ssl/x509v3.cnf"
#define IKECA_CNF	"/etc/ssl/ikeca.cnf"
#define KEYBASE		"/etc/iked"

#define PATH_OPENSSL	"/usr/sbin/openssl"
#define PATH_ZIP	"/usr/local/bin/zip"
#define PATH_TAR	"/bin/tar"

const char *cafiles[] = {
	"ca.crt",
	"ca.pfx",
	"private/ca.key",
	"private/ca.pfx"
};

struct ca {
	char		 sslpath[PATH_MAX];
	char		 passfile[PATH_MAX];
	char		 sslcnf[PATH_MAX];
	char		 extcnf[PATH_MAX];
	char		*caname;
};


struct ca	*ca_setup(char *, int);
int		 ca_create(struct ca *);
int		 ca_delete(struct ca *);
int		 ca_key(char *, char *, char *);
int		 ca_delkey(struct ca *, char *);
int		 ca_sign(struct ca *, char *, int);
int		 ca_request(char *, char *, char *);
int		 ca_certificate(struct ca *, char *, int);
int		 ca_cert_install(struct ca *, char *);
int		 ca_newpass(char *);
int		 ca_export(struct ca *, char *);
int		 ca_install(struct ca *);
int		 ca_show_certs(struct ca *);
int		 fcopy(char *, char *, mode_t);
int		 rm_dir(char *);

int
ca_delete(struct ca *ca)
{
	return (rm_dir(ca->sslpath));
}

int
ca_key(char *sslpath, char *caname, char *keyname)
{
	char			 cmd[PATH_MAX * 2];
	char			 path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.key", sslpath, keyname);

	snprintf(cmd, sizeof(cmd),
	    "%s genrsa -out %s 2048",
	    PATH_OPENSSL, path);
	system(cmd);
	chmod(path, 0600);

	return (0);
}

int
ca_delkey(struct ca *ca, char *keyname)
{
	char		file[PATH_MAX];

	snprintf(file, sizeof(file), "%s/%s.crt", ca->sslpath, keyname);
	unlink(file);

	snprintf(file, sizeof(file), "%s/private/%s.key", ca->sslpath, keyname);
	unlink(file);

	snprintf(file, sizeof(file), "%s/private/%s.csr", ca->sslpath, keyname);
	unlink(file);

	snprintf(file, sizeof(file), "%s/private/%s.pfx", ca->sslpath, keyname);
	unlink(file);

	return (0);
}

int
ca_request(char *sslpath, char *sslcnf, char *keyname)
{
	char		cmd[PATH_MAX * 2];
	char		path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.csr", sslpath, keyname);
	snprintf(cmd, sizeof(cmd), "env CERT_CN=%s %s req -new"
	    " -key %s/private/%s.key -out %s -config %s",
	    keyname, PATH_OPENSSL, sslpath, keyname, path, sslcnf);
	system(cmd);
	chmod(path, 0600);

	return (0);
}

int
ca_sign(struct ca *ca, char *keyname, int type)
{
	char		cmd[PATH_MAX * 2];
	char		hostname[MAXHOSTNAMELEN];
	char		name[128];

	strlcpy(name, keyname, sizeof(name));

	if (type == HOST_IPADDR) {
		snprintf(cmd, sizeof(cmd), "env CERTIP=%s %s x509 -req"
		    " -days 365 -in %s/private/%s.csr"
		    " -CA %s/ca.crt -CAkey %s/private/ca.key -CAcreateserial"
		    " -extfile %s -extensions x509v3_IPAddr -out %s/%s.crt"
		    " -passin file:%s", name, PATH_OPENSSL, ca->sslpath,
		    keyname, ca->sslpath, ca->sslpath, ca->extcnf, ca->sslpath,
		    keyname, ca->passfile);
	} else if (type == HOST_FQDN) {
		if (!strcmp(keyname, "local")) {
			if (gethostname(hostname, sizeof(hostname)))
				err(1, "gethostname");
			strlcpy(name, hostname, sizeof(name));
		}
		snprintf(cmd, sizeof(cmd), "env CERTFQDN=%s %s x509 -req"
		    " -days 365 -in %s/private/%s.csr"
		    " -CA %s/ca.crt -CAkey %s/private/ca.key -CAcreateserial"
		    " -extfile %s -extensions x509v3_FQDN -out %s/%s.crt"
		    " -passin file:%s", name, PATH_OPENSSL, ca->sslpath,
		    keyname, ca->sslpath, ca->sslpath, ca->extcnf, ca->sslpath,
		    keyname, ca->passfile);
	} else 
	    err(1, "unknown host type %d", type);

	system(cmd);

	return (0);
}

int
ca_certificate(struct ca *ca, char *keyname, int type)
{
	ca_key(ca->sslpath, ca->caname, keyname);
	ca_request(ca->sslpath, ca->sslcnf, keyname);
	ca_sign(ca, keyname, type);
	
	return (0);
}

int
ca_cert_install(struct ca *ca, char *keyname)
{
	struct stat	st;
	char		cmd[PATH_MAX * 2];
	char		src[PATH_MAX];
	char		dst[PATH_MAX];

	snprintf(src, sizeof(src), "%s/private/%s.key", ca->sslpath, keyname);
	if (stat(src, &st) == -1) {
		if (errno == ENOENT)
			printf("key for '%s' does not exist\n", ca->caname);
		else
			warn("could not access key");
		return (1);
	}

	snprintf(dst, sizeof(dst), "%s/private/local.key", KEYBASE);
	fcopy(src, dst, 0600);

	snprintf(cmd, sizeof(cmd), "%s rsa -out %s/local.pub"
	    " -in %s/private/local.key -pubout", PATH_OPENSSL, KEYBASE,
	    KEYBASE);
	system(cmd);

	snprintf(src, sizeof(src), "%s/%s.crt", ca->sslpath, keyname);
	snprintf(dst, sizeof(dst), "%s/certs/%s.crt", KEYBASE, keyname);
	fcopy(src, dst, 0644);

	return (0);
}

int
ca_newpass(char *passfile)
{
	FILE	*f;
	char	*pass;
	char	 prev[_PASSWORD_LEN + 1];

	pass = getpass("CA passphrase:");
	if (pass == NULL || *pass == '\0')
		err(1, "password not set");

	strlcpy(prev, pass, sizeof(prev));
	pass = getpass("Retype CA passphrase:");
	if (pass == NULL || strcmp(prev, pass) != 0)
		errx(1, "passphrase does not match!");

	if ((f = fopen(passfile, "wb")) == NULL)
		err(1, "could not open passfile %s", passfile);
	chmod(passfile, 0600);

	fprintf(f, "%s\n%s\n", pass, pass);

	fclose(f);

	return (0);
}

int
ca_create(struct ca *ca)
{
	char			 cmd[PATH_MAX * 2];
	char			 path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/ca.key", ca->sslpath);
	snprintf(cmd, sizeof(cmd), "%s genrsa -aes256 -out"
	    " %s -passout file:%s 2048", PATH_OPENSSL,
	    path, ca->passfile);
	system(cmd);
	chmod(path, 0600);

	snprintf(path, sizeof(path), "%s/private/ca.csr", ca->sslpath);
	snprintf(cmd, sizeof(cmd), "env CERT_CN='VPN CA' %s req -new"
	    " -key %s/private/ca.key"
	    " -config %s -out %s -passin file:%s",
	    PATH_OPENSSL, ca->sslpath, ca->sslcnf, path, ca->passfile);
	system(cmd);
	chmod(path, 0600);

	snprintf(cmd, sizeof(cmd), "%s x509 -req -days 365"
	    " -in %s/private/ca.csr -signkey %s/private/ca.key"
	    " -extfile %s -extensions x509v3_CA -out %s/ca.crt -passin file:%s",
	    PATH_OPENSSL, ca->sslpath, ca->sslpath, ca->extcnf, ca->sslpath,
	    ca->passfile);
	system(cmd);

	return (0);
}

int
ca_install(struct ca *ca)
{
	struct stat	st;
	char		src[PATH_MAX];
	char		dst[PATH_MAX];
	
	snprintf(src, sizeof(src), "%s/ca.crt", ca->sslpath);
	if (stat(src, &st) == -1) {
		printf("CA '%s' does not exist\n", ca->caname);
		return (1);
	}

	snprintf(dst, sizeof(dst), "%s/ca/ca.crt", KEYBASE);
	if (fcopy(src, dst, 0644) == 0)
		printf("certificate for CA '%s' installed into %s\n", ca->caname,
		    dst);

	return (0);
}

int
ca_show_certs(struct ca *ca)
{
	DIR		*dir;
	struct dirent	*de;
	char		 cmd[PATH_MAX * 2];
	char		 path[PATH_MAX];
	char		*p;

	if ((dir = opendir(ca->sslpath)) == NULL)
		err(1, "could not open directory %s", ca->sslpath);

	while ((de = readdir(dir)) != NULL) {
		if (de->d_namlen > 4) {
			p = de->d_name + de->d_namlen - 4;
			if (strcmp(".crt", p) != 0)
				continue;
			snprintf(path, sizeof(path), "%s/%s", ca->sslpath,
			    de->d_name);
			snprintf(cmd, sizeof(cmd), "%s x509 -subject"
			    " -fingerprint -dates -noout -in %s",
			    PATH_OPENSSL, path);
			system(cmd);
			printf("\n");
		}
	}

	closedir(dir);

	return (0);
}

int
fcopy(char *src, char *dst, mode_t mode)
{
	int		ifd, ofd;
	u_int8_t	buf[BUFSIZ];
	ssize_t		r;

	if ((ifd = open(src, O_RDONLY)) == -1)
		err(1, "open %s", src);

	if ((ofd = open(dst, O_WRONLY|O_CREAT, mode)) == -1) {
		close(ifd);
		err(1, "open %s", dst);
	}

	while ((r = read(ifd, buf, sizeof(buf))) > 0) {
		write(ofd, buf, r);
	}

	close(ofd);
	close(ifd);

	return (r == -1);
}

int
rm_dir(char *path)
{
	FTS		*fts;
	FTSENT		*p;
	static char	*fpath[] = { NULL, NULL };

	fpath[0] = path;
	if ((fts = fts_open(fpath, FTS_PHYSICAL, NULL)) == NULL) {
		warn("fts_open %s", path);
		return (1);
	}

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DP:
		case FTS_DNR:
			if (rmdir(p->fts_accpath) == -1)
				warn("rmdir %s", p->fts_accpath);
			break;
		case FTS_F:
			if (unlink(p->fts_accpath) == -1)
				warn("unlink %s", p->fts_accpath);
			break;
		case FTS_D:
		case FTS_DOT:
		default:
			continue;
		}
	}
	fts_close(fts);

	return (0);
}

int
ca_export(struct ca *ca, char *keyname)
{
	struct stat	 st;
	char		*pass;
	char		 prev[_PASSWORD_LEN + 1];
	char		 cmd[PATH_MAX * 2];
	char		 oname[PATH_MAX];
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		*p;
	char		 tpl[] = "/tmp/ikectl.XXXXXXXXXX";
	const char	*exdirs[] = { "/ca", "/certs", "/private", "/export" };
	u_int		 i;

	/* colons are not valid characters in windows filenames... */
	strlcpy(oname, keyname, sizeof(oname));
	while ((p = strchr(oname, ':')) != NULL)
		*p = '_';

	pass = getpass("Export passphrase:");
	if (pass == NULL || *pass == '\0')
		err(1, "password not set");

	strlcpy(prev, pass, sizeof(prev));
	pass = getpass("Retype export passphrase:");
	if (pass == NULL || strcmp(prev, pass) != 0)
		errx(1, "passphrase does not match!");

	snprintf(cmd, sizeof(cmd), "env EXPASS=%s %s pkcs12 -export"
	    " -name %s -CAfile %s/ca.crt -inkey %s/private/%s.key"
	    " -in %s/%s.crt -out %s/private/%s.pfx -passout env:EXPASS"
	    " -passin file:%s", pass, PATH_OPENSSL, keyname, ca->sslpath,
	    ca->sslpath, keyname, ca->sslpath, keyname, ca->sslpath,
	    oname, ca->passfile);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "env EXPASS=%s %s pkcs12 -export"
	    " -caname '%s' -name '%s' -cacerts -nokeys"
	    " -in %s/ca.crt -out %s/ca.pfx -passout env:EXPASS -passin file:%s",
	    pass, PATH_OPENSSL, ca->caname, ca->caname, ca->sslpath,
	    ca->sslpath, ca->passfile);
	system(cmd);

	if ((p = mkdtemp(tpl)) == NULL)
		err(1, "could not create temp dir");

	for (i = 0; i < nitems(exdirs); i++) {
		strlcpy(dst, p, sizeof(dst));
		strlcat(dst, exdirs[i], sizeof(dst));
		if (mkdir(dst, 0700) != 0)
			err(1, "failed to create dir %s", dst);
	}

	snprintf(src, sizeof(src), "%s/private/%s.pfx", ca->sslpath, oname);
	snprintf(dst, sizeof(dst), "%s/export/%s.pfx", p, oname);
	fcopy(src, dst, 0644);

	snprintf(src, sizeof(src), "%s/ca.pfx", ca->sslpath);
	snprintf(dst, sizeof(dst), "%s/export/ca.pfx", p);
	fcopy(src, dst, 0644);

	snprintf(src, sizeof(src), "%s/ca.crt", ca->sslpath);
	snprintf(dst, sizeof(dst), "%s/ca/ca.crt", p);
	fcopy(src, dst, 0644);

	snprintf(src, sizeof(src), "%s/private/%s.key", ca->sslpath, keyname);
	snprintf(dst, sizeof(dst), "%s/private/%s.key", p, keyname);
	fcopy(src, dst, 0600);
	snprintf(dst, sizeof(dst), "%s/private/local.key", p);
	fcopy(src, dst, 0600);

	snprintf(src, sizeof(src), "%s/%s.crt", ca->sslpath, keyname);
	snprintf(dst, sizeof(dst), "%s/certs/%s.crt", p, keyname);
	fcopy(src, dst, 0644);

	snprintf(cmd, sizeof(cmd), "%s rsa -out %s/local.pub"
	    " -in %s/private/%s.key -pubout", PATH_OPENSSL, p, ca->sslpath,
	    keyname);
	system(cmd);

	if (stat(PATH_TAR, &st) == 0) {
		snprintf(cmd, sizeof(cmd), "%s -zcf %s.tgz -C %s .", PATH_TAR,
		    oname, p);
		system(cmd);
		snprintf(src, sizeof(src), "%s.tgz", oname);
		if (realpath(src, dst) != NULL)
			printf("exported files in %s\n", dst);
	}

	if (stat(PATH_ZIP, &st) == 0) {
		snprintf(dst, sizeof(dst), "%s/export", p);
		if (getcwd(src, sizeof(src)) == NULL)
			err(1, "could not get cwd");

		if (chdir(dst) == -1)
			err(1, "could not change %s", dst);

		snprintf(dst, sizeof(dst), "%s/%s.zip", src, oname);
		snprintf(cmd, sizeof(cmd), "zip -qr %s .", dst);
		system(cmd);
		printf("exported files in %s\n", dst);

		if (chdir(src) == -1)
			err(1, "could not change %s", dst);
	}

	rm_dir(p);

	return (0);
}

struct ca *
ca_setup(char *caname, int create)
{
	struct stat	 st;
	struct ca	*ca;
	char		 path[PATH_MAX];
	u_int32_t	 rnd[256];

	if (stat(PATH_OPENSSL, &st) == -1)
		err(1, "openssl binary not available");

	if ((ca = calloc(1, sizeof(struct ca))) == NULL)
		err(1, "calloc");

	ca->caname = strdup(caname);
	strlcpy(ca->sslpath, "/etc/ssl/", sizeof(ca->sslpath));
	strlcat(ca->sslpath, caname, sizeof(ca->sslpath));

	strlcpy(ca->passfile, ca->sslpath, sizeof(ca->passfile));
	strlcat(ca->passfile, "/ikeca.passwd", sizeof(ca->passfile));

	if (stat(IKECA_CNF, &st) == 0) {
		strlcpy(ca->extcnf, IKECA_CNF, sizeof(ca->extcnf));
		strlcpy(ca->sslcnf, IKECA_CNF, sizeof(ca->sslcnf));
	} else {
		strlcpy(ca->extcnf, X509_CNF, sizeof(ca->extcnf));
		strlcpy(ca->sslcnf, SSL_CNF, sizeof(ca->sslcnf));
	}

	if (create == 0 && stat(ca->sslpath, &st) == -1) {
		free(ca->caname);
		free(ca);
		errx(1, "CA '%s' does not exist", caname);
	}

	strlcpy(path, ca->sslpath, sizeof(path));
	if (mkdir(path, 0777) == -1 && errno != EEXIST)
		err(1, "failed to create dir %s", path);
	strlcat(path, "/private", sizeof(path));
	if (mkdir(path, 0700) == -1 && errno != EEXIST)
		err(1, "failed to create dir %s", path);

	if (stat(ca->passfile, &st) == -1 && errno == ENOENT)
		ca_newpass(ca->passfile);

	arc4random_buf(rnd, sizeof(rnd));
	RAND_seed(rnd, sizeof(rnd));

	return (ca);
}
