/*	$OpenBSD: cryptodev.c,v 1.23 2001/08/28 12:20:43 ben Exp $	*/

/*
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/errno.h>
#include <sys/md5k.h>
#include <dev/rndvar.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>
#include <crypto/cast.h>
#include <crypto/skipjack.h>
#include <crypto/blf.h>
#include <crypto/cryptodev.h>
#include <crypto/xform.h>

struct csession {
	TAILQ_ENTRY(csession) next;
	u_int64_t	sid;
	u_int32_t	ses;

	u_int32_t	cipher;
	struct enc_xform *txform;
	u_int32_t	mac;
	struct auth_hash *thash;

	caddr_t		key;
	int		keylen;
	u_char		tmp_iv[EALG_MAX_BLOCK_LEN];

	caddr_t		mackey;
	int		mackeylen;
	u_char		tmp_mac[CRYPTO_MAX_MAC_LEN];

	struct iovec	iovec[IOV_MAX];
	struct uio	uio;
	int		error;
};

struct fcrypt {
	TAILQ_HEAD(csessionlist, csession) csessions;
	int		sesn;
};

void	cryptoattach __P((int));

int	cryptoopen __P((dev_t, int, int, struct proc *));
int	cryptoclose __P((dev_t, int, int, struct proc *));
int	cryptoread __P((dev_t, struct uio *, int));
int	cryptowrite __P((dev_t, struct uio *, int));
int	cryptoioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	cryptoselect __P((dev_t, int, struct proc *));

int	cryptof_read(struct file *, off_t *, struct uio *, struct ucred *);
int	cryptof_write(struct file *, off_t *, struct uio *, struct ucred *);
int	cryptof_ioctl(struct file *, u_long, caddr_t, struct proc *p);
int	cryptof_select(struct file *, int, struct proc *);
int	cryptof_kqfilter(struct file *, struct knote *);
int	cryptof_stat(struct file *, struct stat *, struct proc *);
int	cryptof_close(struct file *, struct proc *);

static struct fileops cryptofops = {
    cryptof_read,
    cryptof_write,
    cryptof_ioctl,
    cryptof_select,
    cryptof_kqfilter,
    cryptof_stat,
    cryptof_close
};

struct	csession *csefind(struct fcrypt *, u_int);
int	csedelete(struct fcrypt *, struct csession *);
struct	csession *cseadd(struct fcrypt *, struct csession *);
struct	csession *csecreate(struct fcrypt *, u_int64_t, caddr_t, u_int64_t,
    caddr_t, u_int64_t, u_int32_t, u_int32_t, struct enc_xform *,
    struct auth_hash *);
void	csefree(struct csession *);

int	crypto_op(struct csession *, struct crypt_op *, struct proc *);

/* ARGSUSED */
int
cryptof_read(fp, poff, uio, cred)
	struct file *fp;
	off_t *poff;
	struct uio *uio;
	struct ucred *cred;
{
	return (EIO);
}

/* ARGSUSED */
int
cryptof_write(fp, poff, uio, cred)
	struct file *fp;
	off_t *poff;
	struct uio *uio;
	struct ucred *cred;
{
	return (EIO);
}

/* ARGSUSED */
int
cryptof_ioctl(fp, cmd, data, p)
	struct file *fp;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	struct cryptoini cria, crie;
	struct fcrypt *fcr = (struct fcrypt *)fp->f_data;
	struct csession *cse;
	struct session_op *sop;
	struct crypt_op *cop;
	struct enc_xform *txform = NULL;
	struct auth_hash *thash = NULL;
	u_int64_t sid;
	u_int32_t ses;
	int error = 0;

	switch (cmd) {
	case CIOCGSESSION:
		sop = (struct session_op *)data;
		switch (sop->cipher) {
		case 0:
			break;
		case CRYPTO_DES_CBC:
			txform = &enc_xform_des;
			break;
		case CRYPTO_3DES_CBC:
			txform = &enc_xform_3des;
			break;
		case CRYPTO_BLF_CBC:
			txform = &enc_xform_blf;
			break;
		case CRYPTO_CAST_CBC:
			txform = &enc_xform_cast5;
			break;
		case CRYPTO_SKIPJACK_CBC:
			txform = &enc_xform_skipjack;
			break;
		case CRYPTO_AES_CBC:
			txform = &enc_xform_rijndael128;
			break;
		case CRYPTO_ARC4:
			txform = &enc_xform_arc4;
			break;
		default:
			return (EINVAL);
		}

		switch (sop->mac) {
		case 0:
			break;
		case CRYPTO_MD5_HMAC:
			thash = &auth_hash_hmac_md5_96;
			break;
		case CRYPTO_SHA1_HMAC:
			thash = &auth_hash_hmac_sha1_96;
			break;
		case CRYPTO_RIPEMD160_HMAC:
			thash = &auth_hash_hmac_ripemd_160_96;
			break;
		case CRYPTO_MD5:
			thash = &auth_hash_md5;
			break;
		case CRYPTO_SHA1:
			thash = &auth_hash_sha1;
			break;
		default:
			return (EINVAL);
		}

		bzero(&crie, sizeof(crie));
		bzero(&cria, sizeof(cria));

		if (txform) {
			crie.cri_alg = txform->type;
			crie.cri_klen = sop->keylen * 8;
			if (sop->keylen > txform->maxkey
			    || sop->keylen < txform->minkey) {
				error = EINVAL;
				goto bail;
			}

			MALLOC(crie.cri_key, u_int8_t *,
			    crie.cri_klen / 8, M_XDATA, M_WAITOK);
			if ((error = copyin(sop->key, crie.cri_key,
			    crie.cri_klen / 8)))
				goto bail;
			if (thash)
				crie.cri_next = &cria;
		}

		if (thash) {
			cria.cri_alg = thash->type;
			cria.cri_klen = sop->mackeylen * 8;
			if (sop->mackeylen != thash->keysize) {
				error = EINVAL;
				goto bail;
			}

			MALLOC(cria.cri_key, u_int8_t *,
			    cria.cri_klen / 8, M_XDATA, M_WAITOK);
			if ((error = copyin(sop->mackey, cria.cri_key,
			    cria.cri_klen / 8)))
				goto bail;
		}

		error = crypto_newsession(&sid, (txform ? &crie : &cria), 1);

bail:
		if (error) {
			if (crie.cri_key)
				FREE(crie.cri_key, M_XDATA);
			if (cria.cri_key)
				FREE(cria.cri_key, M_XDATA);
			return (error);
		}

		cse = csecreate(fcr, sid, crie.cri_key, crie.cri_klen,
		    cria.cri_key, cria.cri_klen, sop->cipher, sop->mac, txform,
		    thash);
		sop->ses = cse->ses;
		break;
	case CIOCFSESSION:
		ses = *(u_int32_t *)data;
		cse = csefind(fcr, ses);
		if (cse == NULL)
			return (EINVAL);
		error = crypto_freesession(cse->sid);
		csedelete(fcr, cse);
		csefree(cse);
		break;
	case CIOCCRYPT:
		cop = (struct crypt_op *)data;
		cse = csefind(fcr, cop->ses);
		if (cse == NULL)
			return (EINVAL);
		error = crypto_op(cse, cop, p);
		break;
	default:
		error = EINVAL;
	}
	return (error);
}

int	cryptodev_cb(void *);


int
crypto_op(struct csession *cse, struct crypt_op *cop, struct proc *p)
{
	struct cryptop *crp = NULL;
	struct cryptodesc *crde = NULL, *crda = NULL;
	int i, error;

	if (cop->len > 256*1024-4)
		return (E2BIG);

	if (cse->txform && (cop->len % cse->txform->blocksize) != 0)
		return (EINVAL);

	bzero(&cse->uio, sizeof(cse->uio));
	cse->uio.uio_iovcnt = 1;
	cse->uio.uio_resid = 0;
	cse->uio.uio_segflg = UIO_SYSSPACE;
	cse->uio.uio_rw = UIO_WRITE;
	cse->uio.uio_procp = p;
	cse->uio.uio_iov = cse->iovec;
	bzero(&cse->iovec, sizeof(cse->iovec));
	cse->uio.uio_iov[0].iov_len = cop->len;
	cse->uio.uio_iov[0].iov_base = malloc(cop->len, M_XDATA, M_WAITOK);
	for (i = 0; i < cse->uio.uio_iovcnt; i++)
		cse->uio.uio_resid += cse->uio.uio_iov[0].iov_len;

	crp = crypto_getreq((cse->txform != NULL) + (cse->thash != NULL));
	if (crp == NULL) {
		error = ENOMEM;
		goto bail;
	}

	if (cse->thash) {
		crda = crp->crp_desc;
		if (cse->txform)
			crde = crda->crd_next;
	} else {
		if (cse->txform)
			crde = crp->crp_desc;
		else {
			error = EINVAL;
			goto bail;
		}
	}

	if ((error = copyin(cop->src, cse->uio.uio_iov[0].iov_base, cop->len)))
		goto bail;

	if (crda) {
		crda->crd_skip = 0;
		crda->crd_len = cop->len;
		crda->crd_inject = 0;	/* ??? */

		crda->crd_alg = cse->mac;
		crda->crd_key = cse->mackey;
		crda->crd_klen = cse->mackeylen * 8;
	}

	if (crde) {
		if (cop->op == COP_ENCRYPT)
			crde->crd_flags |= CRD_F_ENCRYPT;
		else
			crde->crd_flags &= ~CRD_F_ENCRYPT;
		crde->crd_len = cop->len;
		crde->crd_inject = 0;

		crde->crd_alg = cse->cipher;
		crde->crd_key = cse->key;
		crde->crd_klen = cse->keylen * 8;
	}

	crp->crp_ilen = cop->len;
	crp->crp_flags = CRYPTO_F_IOV;
	crp->crp_buf = (caddr_t)&cse->uio;
	crp->crp_callback = (int (*) (struct cryptop *)) cryptodev_cb;
	crp->crp_sid = cse->sid;
	crp->crp_opaque = (void *)cse;

	if (cop->iv) {
		if (crde == NULL) {
			error = EINVAL;
			goto bail;
		}
		if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
			error = EINVAL;
			goto bail;
		}
		if ((error = copyin(cop->iv, cse->tmp_iv, cse->txform->blocksize)))
			goto bail;
		bcopy(cse->tmp_iv, crde->crd_iv, cse->txform->blocksize);
		crde->crd_flags |= CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT;
		crde->crd_skip = 0;
	} else if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
		crde->crd_skip = 0;
	} else if (crde) {
		crde->crd_flags |= CRD_F_IV_PRESENT;
		crde->crd_skip = cse->txform->blocksize;
		crde->crd_len -= cse->txform->blocksize;
	}

	if (cop->mac) {
		if (crda == NULL) {
			error = EINVAL;
			goto bail;
		}
		crp->crp_mac=cse->tmp_mac;
	}

	crypto_dispatch(crp);
	error = tsleep(cse, PSOCK, "crydev", 0);
	if (error) {
		/* XXX can this happen?  if so, how do we recover? */
		goto bail;
	}

	if (cse->error) {
		error = cse->error;
		goto bail;
	}

	if ((error = copyout(cse->uio.uio_iov[0].iov_base, cop->dst, cop->len)))
		goto bail;

	if (cop->mac &&
	    (error = copyout(crp->crp_mac, cop->mac, cse->thash->hashsize)))
		goto bail;

bail:
	if (crp)
		crypto_freereq(crp);
	if (cse->uio.uio_iov[0].iov_base)
		free(cse->uio.uio_iov[0].iov_base, M_XDATA);

	return (error);
}

int
cryptodev_cb(void *op)
{
	struct cryptop *crp = (struct cryptop *) op;
	struct csession *cse = (struct csession *)crp->crp_opaque;

	cse->error = crp->crp_etype;
	if (crp->crp_etype == EAGAIN)
		return crypto_dispatch(crp);
	wakeup(cse);
	return (0);
}

/* ARGSUSED */
int
cryptof_select(fp, which, p)
	struct file *fp;
	int which;
	struct proc *p;
{
	return (0);
}

/* ARGSUSED */
int
cryptof_kqfilter(fp, kn)
	struct file *fp;
	struct knote *kn;
{
	return (0);
}

/* ARGSUSED */
int
cryptof_stat(fp, sb, p)
	struct file *fp;
	struct stat *sb;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

/* ARGSUSED */
int
cryptof_close(fp, p)
	struct file *fp;
	struct proc *p;
{
	struct fcrypt *fcr = (struct fcrypt *)fp->f_data;
	struct csession *cse;

	while ((cse = TAILQ_FIRST(&fcr->csessions))) {
		TAILQ_REMOVE(&fcr->csessions, cse, next);
		csefree(cse);
	}
	FREE(fcr, M_XDATA);
	fp->f_data = NULL;
	return 0;
}

void
cryptoattach(int n)
{
}

int
cryptoopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag;
	int	mode;
	struct proc *p;
{
	return (0);
}

int
cryptoclose(dev, flag, mode, p)
	dev_t	dev;
	int	flag;
	int	mode;
	struct proc *p;
{
	return (0);
}

int
cryptoread(dev, uio, ioflag)
	dev_t	dev;
	struct uio *uio;
	int	ioflag;
{
	return (EIO);
}

int
cryptowrite(dev, uio, ioflag)
	dev_t	dev;
	struct uio *uio;
	int	ioflag;
{
	return (EIO);
}

int
cryptoioctl(dev, cmd, data, flag, p)
	dev_t	dev;
	u_long	cmd;
	caddr_t	data;
	int	flag;
	struct proc *p;
{
	struct file *f;
	struct fcrypt *fcr;
	int fd, error;

	switch (cmd) {
	case CRIOGET:
		MALLOC(fcr, struct fcrypt *,
		    sizeof(struct fcrypt), M_XDATA, M_WAITOK);
		TAILQ_INIT(&fcr->csessions);
		fcr->sesn = 0;

		error = falloc(p, &f, &fd);

		if (error) {
			FREE(fcr, M_XDATA);
			return (error);
		}
		f->f_flag = FREAD | FWRITE;
		f->f_type = DTYPE_CRYPTO;
		f->f_ops = &cryptofops;
		f->f_data = (caddr_t)fcr;
		*(u_int32_t *)data = fd;
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
cryptoselect(dev, rw, p)
	dev_t	dev;
	int	rw;
	struct proc *p;
{
	return (0);
}

struct csession *
csefind(struct fcrypt *fcr, u_int ses)
{
	struct csession *cse;

	TAILQ_FOREACH(cse, &fcr->csessions, next)
		if (cse->ses == ses)
			return (cse);
	return (NULL);
}

int
csedelete(struct fcrypt *fcr, struct csession *cse_del)
{
	struct csession *cse;

	TAILQ_FOREACH(cse, &fcr->csessions, next) {
		if (cse == cse_del) {
			TAILQ_REMOVE(&fcr->csessions, cse, next);
			return (1);
		}
	}
	return (0);
}
	
struct csession *
cseadd(struct fcrypt *fcr, struct csession *cse)
{
	TAILQ_INSERT_TAIL(&fcr->csessions, cse, next);
	cse->ses = fcr->sesn++;
	return (cse);
}

struct csession *
csecreate(struct fcrypt *fcr, u_int64_t sid, caddr_t key, u_int64_t keylen,
    caddr_t mackey, u_int64_t mackeylen, u_int32_t cipher, u_int32_t mac,
    struct enc_xform *txform, struct auth_hash *thash)
{
	struct csession *cse;

	MALLOC(cse, struct csession *, sizeof(struct csession),
	    M_XDATA, M_NOWAIT);
	cse->key = key;
	cse->keylen = keylen/8;
	cse->mackey = mackey;
	cse->mackeylen = mackeylen/8;
	cse->sid = sid;
	cse->cipher = cipher;
	cse->mac = mac;
	cse->txform = txform;
	cse->thash = thash;
	cseadd(fcr, cse);
	return (cse);
}

void
csefree(struct csession *cse)
{
	if (cse->key)
		FREE(cse->key, M_XDATA);
	if (cse->mackey)
		FREE(cse->mackey, M_XDATA);
	(void) crypto_freesession(cse->sid);
	FREE(cse, M_XDATA);
}
