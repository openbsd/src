/*	$OpenBSD: ikeca.c,v 1.32 2015/08/15 04:47:28 semarie Exp $	*/

/*
 * Copyright (c) 2010 Jonathan Gray <jsg@openbsd.org>
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
#include <limits.h>

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "types.h"
#include "parser.h"

#ifndef PREFIX
#define PREFIX		""
#endif
#ifndef SSLDIR
#define SSLDIR		PREFIX "/etc/ssl"
#endif
#define SSL_CNF		SSLDIR "/openssl.cnf"
#define X509_CNF	SSLDIR "/x509v3.cnf"
#define IKECA_CNF	SSLDIR "/ikeca.cnf"
#define KEYBASE		PREFIX "/etc/iked"
#ifndef EXPDIR
#define EXPDIR		PREFIX "/usr/share/iked"
#endif

#ifndef PATH_OPENSSL
#define PATH_OPENSSL	"/usr/bin/openssl"
#endif
#ifndef PATH_ZIP
#define PATH_ZIP	"/usr/local/bin/zip"
#endif
#ifndef PATH_TAR
#define PATH_TAR	"/bin/tar"
#endif

struct ca {
	char		 sslpath[PATH_MAX];
	char		 passfile[PATH_MAX];
	char		 sslcnf[PATH_MAX];
	char		 extcnf[PATH_MAX];
	char		 batch[PATH_MAX];
	char		*caname;
};

struct {
	char	*dir;
	mode_t	 mode;
} hier[] = {
	{ "",		0755 },
	{ "/ca",	0755 },
	{ "/certs",	0755 },
	{ "/crls",	0755 },
	{ "/export",	0755 },
	{ "/private",	0700 }
};

int		 ca_sign(struct ca *, char *, int, char *);
int		 ca_request(struct ca *, char *);
int		 ca_newpass(char *, char *);
char *		 ca_readpass(char *, size_t *);
int		 fcopy(char *, char *, mode_t);
int		 rm_dir(char *);
int		 ca_hier(char *);

int
ca_delete(struct ca *ca)
{
	return (rm_dir(ca->sslpath));
}

int
ca_key_create(struct ca *ca, char *keyname)
{
	struct stat		 st;
	char			 cmd[PATH_MAX * 2];
	char			 path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.key", ca->sslpath, keyname);

	/* don't recreate key if one is already present */
	if (stat(path, &st) == 0) {
		return (0);
	}

	snprintf(cmd, sizeof(cmd),
	    "%s genrsa -out %s 2048",
	    PATH_OPENSSL, path);
	system(cmd);
	chmod(path, 0600);

	return (0);
}

int
ca_key_import(struct ca *ca, char *keyname, char *import)
{
	struct stat		 st;
	char			 dst[PATH_MAX];

	if (stat(import, &st) != 0) {
		warn("could not access keyfile %s", import);
		return (1);
	}

	snprintf(dst, sizeof(dst), "%s/private/%s.key", ca->sslpath, keyname);
	fcopy(import, dst, 0600);

	return (0);
}

int
ca_key_delete(struct ca *ca, char *keyname)
{
	char			 path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.key", ca->sslpath, keyname);
	unlink(path);

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
ca_request(struct ca *ca, char *keyname)
{
	char		cmd[PATH_MAX * 2];
	char		path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.csr", ca->sslpath, keyname);
	snprintf(cmd, sizeof(cmd), "env CERT_CN=%s %s req %s-new"
	    " -key %s/private/%s.key -out %s -config %s",
	    keyname, PATH_OPENSSL, ca->batch, ca->sslpath, keyname,
	    path, ca->sslcnf);

	system(cmd);
	chmod(path, 0600);

	return (0);
}

int
ca_sign(struct ca *ca, char *keyname, int type, char *envargs)
{
	char		cmd[PATH_MAX * 2];
	char		hostname[HOST_NAME_MAX+1];
	char		name[128];

	strlcpy(name, keyname, sizeof(name));

	if (envargs == NULL)
		envargs = "";

	if (type == HOST_IPADDR) {
		snprintf(cmd, sizeof(cmd), "env CERTIP=%s%s %s x509 -req"
		    " -days 365 -in %s/private/%s.csr"
		    " -CA %s/ca.crt -CAkey %s/private/ca.key -CAcreateserial"
		    " -extfile %s -extensions x509v3_IPAddr -out %s/%s.crt"
		    " -passin file:%s", name, envargs, PATH_OPENSSL,
		    ca->sslpath, keyname, ca->sslpath, ca->sslpath,
		    ca->extcnf, ca->sslpath, keyname, ca->passfile);
	} else if (type == HOST_FQDN) {
		if (!strcmp(keyname, "local")) {
			if (gethostname(hostname, sizeof(hostname)))
				err(1, "gethostname");
			strlcpy(name, hostname, sizeof(name));
		}
		snprintf(cmd, sizeof(cmd), "env CERTFQDN=%s%s %s x509 -req"
		    " -days 365 -in %s/private/%s.csr"
		    " -CA %s/ca.crt -CAkey %s/private/ca.key -CAcreateserial"
		    " -extfile %s -extensions x509v3_FQDN -out %s/%s.crt"
		    " -passin file:%s", name, envargs, PATH_OPENSSL,
		    ca->sslpath, keyname, ca->sslpath, ca->sslpath,
		    ca->extcnf, ca->sslpath, keyname, ca->passfile);
	} else
		errx(1, "unknown host type %d", type);

	system(cmd);

	return (0);
}

int
ca_certificate(struct ca *ca, char *keyname, int type, int action)
{
	char	*envargs = "";

	switch (action) {
	case CA_SERVER:
		envargs = " EXTCERTUSAGE=serverAuth NSCERTTYPE=server"
		    " CERTUSAGE=digitalSignature,keyEncipherment";
		break;
	case CA_CLIENT:
		envargs = " EXTCERTUSAGE=clientAuth NSCERTTYPE=client"
		    " CERTUSAGE=digitalSignature,keyAgreement";
		break;
	default:
		break;
	}

	ca_key_create(ca, keyname);
	ca_request(ca, keyname);
	ca_sign(ca, keyname, type, envargs);

	return (0);
}

int
ca_key_install(struct ca *ca, char *keyname, char *dir)
{
	struct stat	 st;
	char		 cmd[PATH_MAX * 2];
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		*p = NULL;

	snprintf(src, sizeof(src), "%s/private/%s.key", ca->sslpath, keyname);
	if (stat(src, &st) == -1) {
		if (errno == ENOENT)
			printf("key for '%s' does not exist\n", ca->caname);
		else
			warn("could not access key");
		return (1);
	}

	if (dir == NULL)
		p = dir = strdup(KEYBASE);

	ca_hier(dir);

	snprintf(dst, sizeof(dst), "%s/private/local.key", dir);
	fcopy(src, dst, 0600);

	snprintf(cmd, sizeof(cmd), "%s rsa -out %s/local.pub"
	    " -in %s/private/local.key -pubout", PATH_OPENSSL, dir, dir);
	system(cmd);

	free(p);

	return (0);
}

int
ca_cert_install(struct ca *ca, char *keyname, char *dir)
{
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	int		 r;
	char		*p = NULL;

	if (dir == NULL)
		p = dir = strdup(KEYBASE);

	ca_hier(dir);

	if ((r = ca_key_install(ca, keyname, dir)) != 0) {
		free(dir);
		return (r);
	}

	snprintf(src, sizeof(src), "%s/%s.crt", ca->sslpath, keyname);
	snprintf(dst, sizeof(dst), "%s/certs/%s.crt", dir, keyname);
	fcopy(src, dst, 0644);

	free(p);

	return (0);
}

int
ca_newpass(char *passfile, char *password)
{
	FILE	*f;
	char	*pass;
	char	 prev[_PASSWORD_LEN + 1];

	if (password != NULL) {
		pass = password;
		goto done;
	}

	pass = getpass("CA passphrase:");
	if (pass == NULL || *pass == '\0')
		err(1, "password not set");

	strlcpy(prev, pass, sizeof(prev));
	pass = getpass("Retype CA passphrase:");
	if (pass == NULL || strcmp(prev, pass) != 0)
		errx(1, "passphrase does not match!");

 done:
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
	snprintf(cmd, sizeof(cmd), "env CERT_CN='VPN CA' %s req %s-new"
	    " -key %s/private/ca.key"
	    " -config %s -out %s -passin file:%s", PATH_OPENSSL,
	    ca->batch, ca->sslpath, ca->sslcnf, path, ca->passfile);
	system(cmd);
	chmod(path, 0600);

	snprintf(cmd, sizeof(cmd), "%s x509 -req -days 365"
	    " -in %s/private/ca.csr -signkey %s/private/ca.key"
	    " -extfile %s -extensions x509v3_CA -out %s/ca.crt -passin file:%s",
	    PATH_OPENSSL, ca->sslpath, ca->sslpath, ca->extcnf, ca->sslpath,
	    ca->passfile);
	system(cmd);

	/* Create the CRL revocation list */
	ca_revoke(ca, NULL);

	return (0);
}

int
ca_install(struct ca *ca, char *dir)
{
	struct stat	 st;
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		*p = NULL;

	snprintf(src, sizeof(src), "%s/ca.crt", ca->sslpath);
	if (stat(src, &st) == -1) {
		printf("CA '%s' does not exist\n", ca->caname);
		return (1);
	}

	if (dir == NULL)
		p = dir = strdup(KEYBASE);

	ca_hier(dir);

	snprintf(dst, sizeof(dst), "%s/ca/ca.crt", dir);
	if (fcopy(src, dst, 0644) == 0)
		printf("certificate for CA '%s' installed into %s\n",
		    ca->caname, dst);

	snprintf(src, sizeof(src), "%s/ca.crl", ca->sslpath);
	if (stat(src, &st) == 0) {
		snprintf(dst, sizeof(dst), "%s/crls/ca.crl", dir);
		if (fcopy(src, dst, 0644) == 0)
			printf("CRL for CA '%s' installed to %s\n",
			    ca->caname, dst);
	}

	free(p);

	return (0);
}

int
ca_show_certs(struct ca *ca, char *name)
{
	DIR		*dir;
	struct dirent	*de;
	char		 cmd[PATH_MAX * 2];
	char		 path[PATH_MAX];
	char		*p;
	struct stat	 st;

	if (name != NULL) {
		snprintf(path, sizeof(path), "%s/%s.crt",
		    ca->sslpath, name);
		if (stat(path, &st) != 0)
			err(1, "could not open file %s.crt", name);
		snprintf(cmd, sizeof(cmd), "%s x509 -text"
		    " -in %s", PATH_OPENSSL, path);
		system(cmd);
		printf("\n");
		return (0);
	}

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

	if ((ofd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1) {
		int saved_errno = errno;
		close(ifd);
		errc(1, saved_errno, "open %s", dst);
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
ca_hier(char *path)
{
	struct stat	 st;
	char		 dst[PATH_MAX];
	u_int		 i;

	for (i = 0; i < nitems(hier); i++) {
		strlcpy(dst, path, sizeof(dst));
		strlcat(dst, hier[i].dir, sizeof(dst));
		if (stat(dst, &st) != 0 && errno == ENOENT &&
		    mkdir(dst, hier[i].mode) != 0)
			err(1, "failed to create dir %s", dst);
	}

	return (0);
}

int
ca_export(struct ca *ca, char *keyname, char *myname, char *password)
{
	DIR		*dexp;
	struct dirent	*de;
	struct stat	 st;
	char		*pass;
	char		 prev[_PASSWORD_LEN + 1];
	char		 cmd[PATH_MAX * 2];
	char		 oname[PATH_MAX];
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		*p;
	char		 tpl[] = "/tmp/ikectl.XXXXXXXXXX";
	u_int		 i;
	int		 fd;

	if (keyname != NULL) {
		if (strlcpy(oname, keyname, sizeof(oname)) >= sizeof(oname))
			errx(1, "name too long");
	} else {
		strlcpy(oname, "ca", sizeof(oname));
	}

	/* colons are not valid characters in windows filenames... */
	while ((p = strchr(oname, ':')) != NULL)
		*p = '_';

	if (password != NULL)
		pass = password;
	else {
		pass = getpass("Export passphrase:");
		if (pass == NULL || *pass == '\0')
			err(1, "password not set");

		strlcpy(prev, pass, sizeof(prev));
		pass = getpass("Retype export passphrase:");
		if (pass == NULL || strcmp(prev, pass) != 0)
			errx(1, "passphrase does not match!");
	}

	if (keyname != NULL) {
		snprintf(cmd, sizeof(cmd), "env EXPASS=%s %s pkcs12 -export"
		    " -name %s -CAfile %s/ca.crt -inkey %s/private/%s.key"
		    " -in %s/%s.crt -out %s/private/%s.pfx -passout env:EXPASS"
		    " -passin file:%s", pass, PATH_OPENSSL, keyname,
		    ca->sslpath, ca->sslpath, keyname, ca->sslpath, keyname,
		    ca->sslpath, oname, ca->passfile);
		system(cmd);
	}

	snprintf(cmd, sizeof(cmd), "env EXPASS=%s %s pkcs12 -export"
	    " -caname '%s' -name '%s' -cacerts -nokeys"
	    " -in %s/ca.crt -out %s/ca.pfx -passout env:EXPASS -passin file:%s",
	    pass, PATH_OPENSSL, ca->caname, ca->caname, ca->sslpath,
	    ca->sslpath, ca->passfile);
	system(cmd);

	if ((p = mkdtemp(tpl)) == NULL)
		err(1, "could not create temp dir");

	chmod(p, 0755);

	for (i = 0; i < nitems(hier); i++) {
		strlcpy(dst, p, sizeof(dst));
		strlcat(dst, hier[i].dir, sizeof(dst));
		if (stat(dst, &st) != 0 && errno == ENOENT &&
		    mkdir(dst, hier[i].mode) != 0)
			err(1, "failed to create dir %s", dst);
	}

	/* create a file with the address of the peer to connect to */
	if (myname != NULL) {
		snprintf(dst, sizeof(dst), "%s/export/peer.txt", p);
		if ((fd = open(dst, O_WRONLY|O_CREAT, 0644)) == -1)
			err(1, "open %s", dst);
		write(fd, myname, strlen(myname));
		close(fd);
	}

	snprintf(src, sizeof(src), "%s/ca.pfx", ca->sslpath);
	snprintf(dst, sizeof(dst), "%s/export/ca.pfx", p);
	fcopy(src, dst, 0644);

	snprintf(src, sizeof(src), "%s/ca.crt", ca->sslpath);
	snprintf(dst, sizeof(dst), "%s/ca/ca.crt", p);
	fcopy(src, dst, 0644);

	snprintf(src, sizeof(src), "%s/ca.crl", ca->sslpath);
	if (stat(src, &st) == 0) {
		snprintf(dst, sizeof(dst), "%s/crls/ca.crl", p);
		fcopy(src, dst, 0644);
	}

	if (keyname != NULL) {
		snprintf(src, sizeof(src), "%s/private/%s.pfx", ca->sslpath,
		    oname);
		snprintf(dst, sizeof(dst), "%s/export/%s.pfx", p, oname);
		fcopy(src, dst, 0644);

		snprintf(src, sizeof(src), "%s/private/%s.key", ca->sslpath,
		    keyname);
		snprintf(dst, sizeof(dst), "%s/private/%s.key", p, keyname);
		fcopy(src, dst, 0600);
		snprintf(dst, sizeof(dst), "%s/private/local.key", p);
		fcopy(src, dst, 0600);

		snprintf(src, sizeof(src), "%s/%s.crt", ca->sslpath, keyname);
		snprintf(dst, sizeof(dst), "%s/certs/%s.crt", p, keyname);
		fcopy(src, dst, 0644);

		snprintf(cmd, sizeof(cmd), "%s rsa -out %s/local.pub"
		    " -in %s/private/%s.key -pubout", PATH_OPENSSL, p,
		    ca->sslpath, keyname);
		system(cmd);
	}

	if (stat(PATH_TAR, &st) == 0) {
		if (keyname == NULL)
			snprintf(cmd, sizeof(cmd), "%s -zcf %s.tgz -C %s .",
			    PATH_TAR, oname, ca->sslpath);
		else
			snprintf(cmd, sizeof(cmd), "%s -zcf %s.tgz -C %s .",
			    PATH_TAR, oname, p);
		system(cmd);
		snprintf(src, sizeof(src), "%s.tgz", oname);
		if (realpath(src, dst) != NULL)
			printf("exported files in %s\n", dst);
	}

	if (stat(PATH_ZIP, &st) == 0) {
		dexp = opendir(EXPDIR);
		if (dexp) {
			while ((de = readdir(dexp)) != NULL) {
				if (!strcmp(de->d_name, ".") ||
				    !strcmp(de->d_name, ".."))
					continue;
				snprintf(src, sizeof(src), "%s/%s", EXPDIR,
				    de->d_name);
				snprintf(dst, sizeof(dst), "%s/export/%s", p,
				    de->d_name);
				fcopy(src, dst, 0644);
			}
			closedir(dexp);
		}

		snprintf(dst, sizeof(dst), "%s/export", p);
		if (getcwd(src, sizeof(src)) == NULL)
			err(1, "could not get cwd");

		if (chdir(dst) == -1)
			err(1, "could not change %s", dst);

		snprintf(dst, sizeof(dst), "%s/%s.zip", src, oname);
		snprintf(cmd, sizeof(cmd), "%s -qr %s .", PATH_ZIP, dst);
		system(cmd);
		printf("exported files in %s\n", dst);

		if (chdir(src) == -1)
			err(1, "could not change %s", dst);
	}

	rm_dir(p);

	return (0);
}

char *
ca_readpass(char *path, size_t *len)
{
	FILE		*f;
	char		*p, *r;

	if ((f = fopen(path, "r")) == NULL) {
		warn("fopen %s", path);
		return (NULL);
	}

	if ((p = fgetln(f, len)) != NULL) {
		if ((r = malloc(*len + 1)) == NULL)
			err(1, "malloc");
		memcpy(r, p, *len);
		if (r[*len - 1] == '\n')
			r[*len - 1] = '\0';
		else
			r[*len] = '\0';
	} else
		r = NULL;

	fclose(f);

	return (r);
}

int
ca_revoke(struct ca *ca, char *keyname)
{
	struct stat	 st;
	char		 cmd[PATH_MAX * 2];
	char		 path[PATH_MAX];
	int		 fd;
	char		*pass;
	size_t		 len;

	if (keyname) {
		snprintf(path, sizeof(path), "%s/%s.crt",
		    ca->sslpath, keyname);
		if (stat(path, &st) != 0) {
			warn("Problem with certificate for '%s'", keyname);
			return (1);
		}
	}

	snprintf(path, sizeof(path), "%s/ikeca.passwd", ca->sslpath);
	pass = ca_readpass(path, &len);
	if (pass == NULL)
		errx(1, "could not open passphrase file");

	/* create index if it doesn't already exist */
	snprintf(path, sizeof(path), "%s/index.txt", ca->sslpath);
	if (stat(path, &st) != 0) {
		if  (errno == ENOENT) {
			if ((fd = open(path, O_WRONLY | O_CREAT, 0644)) == -1)
				err(1, "could not create file %s", path);
			close(fd);
		} else
			err(1, "could not access %s", path);
	}

	if (keyname) {
		snprintf(cmd, sizeof(cmd), "env CADB='%s/index.txt' "
		    " %s ca %s-config %s -keyfile %s/private/ca.key"
		    " -key %s"
		    " -cert %s/ca.crt"
		    " -md sha1"
		    " -revoke %s/%s.crt",
		    ca->sslpath, PATH_OPENSSL, ca->batch, ca->sslcnf,
		    ca->sslpath, pass, ca->sslpath, ca->sslpath, keyname);
		system(cmd);
	}

	snprintf(cmd, sizeof(cmd), "env CADB='%s/index.txt' "
	    " %s ca %s-config %s -keyfile %s/private/ca.key"
	    " -key %s"
	    " -gencrl"
	    " -cert %s/ca.crt"
	    " -md sha1"
	    " -crldays 365"
	    " -out %s/ca.crl",
	    ca->sslpath, PATH_OPENSSL, ca->batch, ca->sslcnf, ca->sslpath,
	    pass, ca->sslpath, ca->sslpath);
	system(cmd);

	explicit_bzero(pass, len);
	free(pass);

	return (0);
}

struct ca *
ca_setup(char *caname, int create, int quiet, char *pass)
{
	struct stat	 st;
	struct ca	*ca;
	char		 path[PATH_MAX];

	if (stat(PATH_OPENSSL, &st) == -1)
		err(1, "openssl binary not available");

	if ((ca = calloc(1, sizeof(struct ca))) == NULL)
		err(1, "calloc");

	ca->caname = strdup(caname);
	snprintf(ca->sslpath, sizeof(ca->sslpath), SSLDIR "/%s", caname);
	strlcpy(ca->passfile, ca->sslpath, sizeof(ca->passfile));
	strlcat(ca->passfile, "/ikeca.passwd", sizeof(ca->passfile));

	if (quiet)
		strlcpy(ca->batch, "-batch ", sizeof(ca->batch));

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

	if (create && stat(ca->passfile, &st) == -1 && errno == ENOENT)
		ca_newpass(ca->passfile, pass);

	return (ca);
}
