/*	$OpenBSD: ikeca.c,v 1.49 2019/05/08 23:59:19 tedu Exp $	*/

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
#include <sys/wait.h>
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
	char	 sslpath[PATH_MAX];
	char	 passfile[PATH_MAX + 5]; /* Includes the "file:" prefix */
	char	 index[PATH_MAX];
	char	 serial[PATH_MAX];
	char	 sslcnf[PATH_MAX];
	char	 extcnf[PATH_MAX];
	char	*batch;
	char	*caname;
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

/* explicitly list allowed variables */
char *ca_env[][2] = {
	{ "$ENV::CADB", NULL },
	{ "$ENV::CASERIAL", NULL },
	{ "$ENV::CERTFQDN", NULL },
	{ "$ENV::CERTIP", NULL },
	{ "$ENV::CERTPATHLEN", NULL },
	{ "$ENV::CERTUSAGE", NULL },
	{ "$ENV::CERT_C", NULL },
	{ "$ENV::CERT_CN", NULL },
	{ "$ENV::CERT_EMAIL", NULL },
	{ "$ENV::CERT_L", NULL },
	{ "$ENV::CERT_O", NULL },
	{ "$ENV::CERT_OU", NULL },
	{ "$ENV::CERT_ST", NULL },
	{ "$ENV::EXTCERTUSAGE", NULL },
	{ "$ENV::NSCERTTYPE", NULL },
	{ "$ENV::REQ_EXT", NULL },
	{ NULL }
};

int		 ca_sign(struct ca *, char *, int);
int		 ca_request(struct ca *, char *, int);
void		 ca_newpass(char *, char *);
int		 fcopy(char *, char *, mode_t);
void		 fcopy_env(const char *, const char *, mode_t);
int		 rm_dir(char *);
void		 ca_hier(char *);
void		 ca_setenv(const char *, const char *);
void		 ca_clrenv(void);
void		 ca_setcnf(struct ca *, const char *);
void		 ca_create_index(struct ca *);
int static	 ca_execv(char *const []);

/* util.c */
int		 expand_string(char *, size_t, const char *, const char *);

int
ca_delete(struct ca *ca)
{
	return (rm_dir(ca->sslpath));
}

int
ca_key_create(struct ca *ca, char *keyname)
{
	struct stat		 st;
	char			 path[PATH_MAX];

	snprintf(path, sizeof(path), "%s/private/%s.key", ca->sslpath, keyname);

	/* don't recreate key if one is already present */
	if (stat(path, &st) == 0) {
		return (0);
	}

	char *cmd[] = { PATH_OPENSSL, "genrsa", "-out", path, "2048", NULL };
	ca_execv(cmd);
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
ca_request(struct ca *ca, char *keyname, int type)
{
	char		hostname[HOST_NAME_MAX+1];
	char		name[128];
	char		key[PATH_MAX];
	char		path[PATH_MAX];

	ca_setenv("$ENV::CERT_CN", keyname);

	strlcpy(name, keyname, sizeof(name));

	if (type == HOST_IPADDR) {
		ca_setenv("$ENV::CERTIP", name);
		ca_setenv("$ENV::REQ_EXT", "x509v3_IPAddr");
	} else if (type == HOST_FQDN) {
		if (!strcmp(keyname, "local")) {
			if (gethostname(hostname, sizeof(hostname)))
				err(1, "gethostname");
			strlcpy(name, hostname, sizeof(name));
		}
		ca_setenv("$ENV::CERTFQDN", name);
		ca_setenv("$ENV::REQ_EXT", "x509v3_FQDN");
	} else {
		errx(1, "unknown host type %d", type);
	}

	ca_setcnf(ca, keyname);

	snprintf(key, sizeof(key), "%s/private/%s.key", ca->sslpath, keyname);
	snprintf(path, sizeof(path), "%s/private/%s.csr", ca->sslpath, keyname);

	char *cmd[] = { PATH_OPENSSL, "req", "-new", "-key", key, "-out", path,
	    "-config", ca->sslcnf, ca->batch, NULL };
	ca_execv(cmd);
	chmod(path, 0600);

	return (0);
}

int
ca_sign(struct ca *ca, char *keyname, int type)
{
	char		cakey[PATH_MAX];
	char		cacrt[PATH_MAX];
	char		out[PATH_MAX];
	char		in[PATH_MAX];
	char		*extensions = NULL;

	if (type == HOST_IPADDR) {
		extensions = "x509v3_IPAddr";
	} else if (type == HOST_FQDN) {
		extensions = "x509v3_FQDN";
	} else {
		errx(1, "unknown host type %d", type);
	}

	ca_create_index(ca);

	ca_setenv("$ENV::CADB", ca->index);
	ca_setenv("$ENV::CASERIAL", ca->serial);
	ca_setcnf(ca, keyname);

	snprintf(cakey, sizeof(cakey), "%s/private/ca.key", ca->sslpath);
	snprintf(cacrt, sizeof(cacrt), "%s/ca.crt", ca->sslpath);
	snprintf(out, sizeof(out), "%s/%s.crt", ca->sslpath, keyname);
	snprintf(in, sizeof(in), "%s/private/%s.csr", ca->sslpath, keyname);

	char *cmd[] = { PATH_OPENSSL, "ca", "-config", ca->sslcnf,
	    "-keyfile", cakey, "-cert", cacrt, "-extfile", ca->extcnf,
	    "-extensions", extensions, "-out", out, "-in", in,
	    "-passin", ca->passfile, "-outdir", ca->sslpath, "-batch", NULL };
	ca_execv(cmd);

	return (0);
}

int
ca_certificate(struct ca *ca, char *keyname, int type, int action)
{
	ca_clrenv();

	switch (action) {
	case CA_SERVER:
		ca_setenv("$ENV::EXTCERTUSAGE", "serverAuth");
		ca_setenv("$ENV::NSCERTTYPE", "server");
		ca_setenv("$ENV::CERTUSAGE",
		    "digitalSignature,keyEncipherment");
		break;
	case CA_CLIENT:
		ca_setenv("$ENV::EXTCERTUSAGE", "clientAuth");
		ca_setenv("$ENV::NSCERTTYPE", "client");
		ca_setenv("$ENV::CERTUSAGE",
		    "digitalSignature,keyAgreement");
		break;
	case CA_OCSP:
		ca_setenv("$ENV::EXTCERTUSAGE", "OCSPSigning");
		ca_setenv("$ENV::CERTUSAGE",
		    "nonRepudiation,digitalSignature,keyEncipherment");
		break;
	default:
		break;
	}

	ca_key_create(ca, keyname);
	ca_request(ca, keyname, type);
	ca_sign(ca, keyname, type);

	return (0);
}

int
ca_key_install(struct ca *ca, char *keyname, char *dir)
{
	struct stat	 st;
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		 out[PATH_MAX];
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

	snprintf(out, sizeof(out), "%s/local.pub", dir);

	char *cmd[] = { PATH_OPENSSL, "rsa", "-out", out, "-in", dst,
	    "-pubout", NULL };
	ca_execv(cmd);

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

void
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
}

int
ca_create(struct ca *ca)
{
	char			 key[PATH_MAX];
	char			 csr[PATH_MAX];
	char			 crt[PATH_MAX];

	ca_clrenv();

	snprintf(key, sizeof(key), "%s/private/ca.key", ca->sslpath);
	char *genrsa[] = { PATH_OPENSSL, "genrsa", "-aes256", "-out", key,
	    "-passout", ca->passfile, "2048", NULL };
	ca_execv(genrsa);

	chmod(key, 0600);

	ca_setenv("$ENV::CERT_CN", "VPN CA");
	ca_setenv("$ENV::REQ_EXT", "x509v3_CA");
	ca_setcnf(ca, "ca");

	snprintf(csr, sizeof(csr), "%s/private/ca.csr", ca->sslpath);
	char *reqcmd[] = { PATH_OPENSSL, "req", "-new", "-key", key,
	    "-config", ca->sslcnf, "-out", csr,
	    "-passin", ca->passfile, ca->batch, NULL };
	ca_execv(reqcmd);
	chmod(csr, 0600);

	snprintf(crt, sizeof(crt), "%s/ca.crt", ca->sslpath);
	char *x509[] = { PATH_OPENSSL, "x509", "-req", "-days", "4500",
	    "-in", csr, "-signkey", key, "-sha256",
	    "-extfile", ca->extcnf, "-extensions", "x509v3_CA",
	    "-out", crt, "-passin", ca->passfile, NULL };
	ca_execv(x509);

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
	char		 path[PATH_MAX];
	char		*p;
	struct stat	 st;

	if (name != NULL) {
		snprintf(path, sizeof(path), "%s/%s.crt",
		    ca->sslpath, name);
		if (stat(path, &st) != 0)
			err(1, "could not open file %s.crt", name);
		char *cmd[] = { PATH_OPENSSL, "x509", "-text",
		    "-in", path, NULL };
		ca_execv(cmd);
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
			char *cmd[] = { PATH_OPENSSL, "x509", "-subject",
			    "-fingerprint", "-dates", "-noout", "-in", path,
			    NULL };
			ca_execv(cmd);
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
	uint8_t		buf[BUFSIZ];
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

void
fcopy_env(const char *src, const char *dst, mode_t mode)
{
	int		 ofd = -1, i;
	uint8_t		 buf[BUFSIZ];
	ssize_t		 r = -1, len;
	FILE		*ifp = NULL;
	int		 saved_errno;

	if ((ifp = fopen(src, "r")) == NULL)
		err(1, "fopen %s", src);

	if ((ofd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, mode)) == -1)
		goto done;

	while (fgets(buf, sizeof(buf), ifp) != NULL) {
		for (i = 0; ca_env[i][0] != NULL; i++) {
			if (ca_env[i][1] == NULL)
				continue;
			if (expand_string(buf, sizeof(buf),
			    ca_env[i][0], ca_env[i][1]) == -1)
				errx(1, "env %s value too long", ca_env[i][0]);
		}
		len = strlen(buf);
		if (write(ofd, buf, len) != len)
			goto done;
	}

	r = 0;

 done:
	saved_errno = errno;
	close(ofd);
	if (ifp != NULL)
		fclose(ifp);
	if (r == -1)
		errc(1, saved_errno, "open %s", dst);
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

void
ca_hier(char *path)
{
	struct stat	 st;
	char		 dst[PATH_MAX];
	unsigned int	 i;

	for (i = 0; i < nitems(hier); i++) {
		strlcpy(dst, path, sizeof(dst));
		strlcat(dst, hier[i].dir, sizeof(dst));
		if (stat(dst, &st) != 0 && errno == ENOENT &&
		    mkdir(dst, hier[i].mode) != 0)
			err(1, "failed to create dir %s", dst);
	}
}

int
ca_export(struct ca *ca, char *keyname, char *myname, char *password)
{
	DIR		*dexp;
	struct dirent	*de;
	struct stat	 st;
	char		*pass;
	char		 prev[_PASSWORD_LEN + 1];
	char		 passenv[_PASSWORD_LEN + 8];
	char		 oname[PATH_MAX];
	char		 src[PATH_MAX];
	char		 dst[PATH_MAX];
	char		 cacrt[PATH_MAX];
	char		 capfx[PATH_MAX];
	char		 key[PATH_MAX];
	char		 crt[PATH_MAX];
	char		 pfx[PATH_MAX];
	char		*p;
	char		 tpl[] = "/tmp/ikectl.XXXXXXXXXX";
	unsigned int	 i;
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

	snprintf(cacrt, sizeof(cacrt), "%s/ca.crt", ca->sslpath);
	snprintf(capfx, sizeof(capfx), "%s/ca.pfx", ca->sslpath);
	snprintf(key, sizeof(key), "%s/private/%s.key", ca->sslpath, keyname);
	snprintf(crt, sizeof(crt), "%s/%s.crt", ca->sslpath, keyname);
	snprintf(pfx, sizeof(pfx), "%s/private/%s.pfx", ca->sslpath, oname);

	snprintf(passenv, sizeof(passenv), "EXPASS=%s", pass);
	putenv(passenv);

	if (keyname != NULL) {
		char *cmd[] = { PATH_OPENSSL, "pkcs12", "-export",
		    "-name", keyname, "-CAfile", cacrt, "-inkey", key,
		    "-in", crt, "-out", pfx, "-passout", "env:EXPASS",
		    "-passin", ca->passfile, NULL };
		ca_execv(cmd);
	}

	char *pkcscmd[] = { PATH_OPENSSL, "pkcs12", "-export",
	    "-caname", ca->caname, "-name", ca->caname, "-cacerts",
	    "-nokeys", "-in", cacrt, "-out", capfx,
	    "-passout", "env:EXPASS", "-passin", ca->passfile, NULL };
	ca_execv(pkcscmd);

	unsetenv("EXPASS");
	explicit_bzero(passenv, sizeof(passenv));

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

		snprintf(dst, sizeof(dst), "%s/local.pub", p);
		char *cmd[] = { PATH_OPENSSL, "rsa", "-out", dst, "-in", key,
		    "-pubout", NULL };
		ca_execv(cmd);
	}

	if (stat(PATH_TAR, &st) == 0) {
		snprintf(src, sizeof(src), "%s.tgz", oname);
		if (keyname == NULL) {
			char *cmd[] = { PATH_TAR, "-zcf", src,
			    "-C", ca->sslpath, ".", NULL };
			ca_execv(cmd);
		} else {
			char *cmd[] = { PATH_TAR, "-zcf", src, "-C", p, ".",
			    NULL };
			ca_execv(cmd);
		}
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
		char *cmd[] = { PATH_ZIP, "-qr", dst, ".", NULL };
		ca_execv(cmd);
		printf("exported files in %s\n", dst);

		if (chdir(src) == -1)
			err(1, "could not change %s", dst);
	}

	rm_dir(p);

	return (0);
}

/* create index if it doesn't already exist */
void
ca_create_index(struct ca *ca)
{
	struct stat	 st;
	int		 fd;

	if (snprintf(ca->index, sizeof(ca->index), "%s/index.txt",
	    ca->sslpath) < 0)
		err(1, "snprintf");
	if (stat(ca->index, &st) != 0) {
		if  (errno == ENOENT) {
			if ((fd = open(ca->index, O_WRONLY | O_CREAT, 0644))
			    == -1)
				err(1, "could not create file %s", ca->index);
			close(fd);
		} else
			err(1, "could not access %s", ca->index);
	}

	if (snprintf(ca->serial, sizeof(ca->serial), "%s/serial.txt",
	    ca->sslpath) < 0)
		err(1, "snprintf");
	if (stat(ca->serial, &st) != 0) {
		if  (errno == ENOENT) {
			if ((fd = open(ca->serial, O_WRONLY | O_CREAT, 0644))
			    == -1)
				err(1, "could not create file %s", ca->serial);
			/* serial file must be created with a number */
			if (write(fd, "01\n", 3) != 3)
				err(1, "write %s", ca->serial);
			close(fd);
		} else
			err(1, "could not access %s", ca->serial);
	}
}

int
ca_revoke(struct ca *ca, char *keyname)
{
	struct stat	 st;
	char		 path[PATH_MAX];
	char		 cakey[PATH_MAX];
	char		 cacrt[PATH_MAX];

	if (keyname) {
		snprintf(path, sizeof(path), "%s/%s.crt",
		    ca->sslpath, keyname);
		if (stat(path, &st) != 0) {
			warn("Problem with certificate for '%s'", keyname);
			return (1);
		}
	}

	ca_create_index(ca);

	ca_setenv("$ENV::CADB", ca->index);
	ca_setenv("$ENV::CASERIAL", ca->serial);
	if (keyname)
		ca_setenv("$ENV::REQ_EXT", "");

	ca_setcnf(ca, "ca-revoke");

	snprintf(cakey, sizeof(cakey), "%s/private/ca.key", ca->sslpath);
	snprintf(cacrt, sizeof(cacrt), "%s/ca.crt", ca->sslpath);

	if (keyname) {
		char *cmd[] = { PATH_OPENSSL, "ca", "-config", ca->sslcnf,
		    "-keyfile", cakey, "-passin", ca->passfile, "-cert", cacrt,
		    "-revoke", path, ca->batch, NULL };
		ca_execv(cmd);
	}

	snprintf(path, sizeof(path), "%s/ca.crl", ca->sslpath);
	char *cmd[] = { PATH_OPENSSL, "ca", "-config", ca->sslcnf,
	    "-keyfile", cakey, "-passin", ca->passfile, "-gencrl",
	    "-cert", cacrt, "-crldays", "365", "-out", path, ca->batch, NULL };
	ca_execv(cmd);

	return (0);
}

void
ca_clrenv(void)
{
	int	 i;
	for (i = 0; ca_env[i][0] != NULL; i++) {
		free(ca_env[i][1]);
		ca_env[i][1] = NULL;
	}
}

void
ca_setenv(const char *key, const char *value)
{
	int	 i;
	char	*p = NULL;

	for (i = 0; ca_env[i][0] != NULL; i++) {
		if (strcmp(ca_env[i][0], key) == 0) {
			if (ca_env[i][1] != NULL)
				errx(1, "env %s already set: %s", key, value);
			p = strdup(value);
			if (p == NULL)
				err(1, NULL);
			ca_env[i][1] = p;
			return;
		}
	}
	errx(1, "env %s invalid", key);
}

void
ca_setcnf(struct ca *ca, const char *keyname)
{
	struct stat	 st;
	const char	*extcnf, *sslcnf;

	if (stat(IKECA_CNF, &st) == 0) {
		extcnf = IKECA_CNF;
		sslcnf = IKECA_CNF;
	} else {
		extcnf = X509_CNF;
		sslcnf = SSL_CNF;
	}

	snprintf(ca->extcnf, sizeof(ca->extcnf), "%s/%s-ext.cnf",
	    ca->sslpath, keyname);
	snprintf(ca->sslcnf, sizeof(ca->sslcnf), "%s/%s-ssl.cnf",
	    ca->sslpath, keyname);

	fcopy_env(extcnf, ca->extcnf, 0400);
	fcopy_env(sslcnf, ca->sslcnf, 0400);
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

	if (quiet)
		ca->batch = "-batch";

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

	snprintf(path, sizeof(path), "%s/ikeca.passwd", ca->sslpath);
	if (create && stat(path, &st) == -1 && errno == ENOENT)
		ca_newpass(path, pass);
	snprintf(ca->passfile, sizeof(ca->passfile), "file:%s", path);

	return (ca);
}

int static
ca_execv(char *const argv[])
{
	pid_t pid, cpid;
	int status;

	switch (cpid = fork()) {
	case -1:
		return -1;
	case 0:
		execv(argv[0], argv);
		_exit(127);
	}

	do {
		pid = waitpid(cpid, &status, 0);
	} while (pid == -1 && errno == EINTR);

	return (pid == -1 ? -1 : WEXITSTATUS(status));
}
