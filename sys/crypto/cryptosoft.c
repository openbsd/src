/*	$OpenBSD: cryptosoft.c,v 1.38 2003/02/21 20:33:35 jason Exp $	*/

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/md5k.h>
#include <dev/rndvar.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>
#include <crypto/cast.h>
#include <crypto/skipjack.h>
#include <crypto/blf.h>
#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <crypto/xform.h>

u_int8_t hmac_ipad_buffer[64] = {
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

u_int8_t hmac_opad_buffer[64] = {
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
	0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C
};


struct swcr_data **swcr_sessions = NULL;
u_int32_t swcr_sesnum = 0;
int32_t swcr_id = -1;

#define COPYBACK(x, a, b, c, d) \
	(x) == CRYPTO_BUF_MBUF ? m_copyback((struct mbuf *)a,b,c,d) \
	: cuio_copyback((struct uio *)a,b,c,d)
#define COPYDATA(x, a, b, c, d) \
	(x) == CRYPTO_BUF_MBUF ? m_copydata((struct mbuf *)a,b,c,d) \
	: cuio_copydata((struct uio *)a,b,c,d)

/*
 * Apply a symmetric encryption/decryption algorithm.
 */
int
swcr_encdec(struct cryptodesc *crd, struct swcr_data *sw, caddr_t buf,
    int outtype)
{
	unsigned char iv[EALG_MAX_BLOCK_LEN], blk[EALG_MAX_BLOCK_LEN], *idat;
	unsigned char *ivp, piv[EALG_MAX_BLOCK_LEN];
	struct enc_xform *exf;
	int i, k, j, blks, ind, count;
	struct mbuf *m = NULL;
	struct uio *uio = NULL;


	exf = sw->sw_exf;
	blks = exf->blocksize;

	/* Check for non-padded data */
	if (crd->crd_len % blks)
		return EINVAL;

	if (outtype == CRYPTO_BUF_MBUF)
		m = (struct mbuf *) buf;
	else
		uio = (struct uio *) buf;

	/* Initialize the IV */
	if (crd->crd_flags & CRD_F_ENCRYPT) {
		/* IV explicitly provided ? */
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, iv, blks);
		else {
			/* Get random IV */
			for (i = 0;
			    i + sizeof (u_int32_t) < EALG_MAX_BLOCK_LEN;
			    i += sizeof (u_int32_t)) {
				u_int32_t temp = arc4random();

				bcopy(&temp, iv + i, sizeof(u_int32_t));
			}
			/*
			 * What if the block size is not a multiple
			 * of sizeof (u_int32_t), which is the size of
			 * what arc4random() returns ?
			 */
			if (EALG_MAX_BLOCK_LEN % sizeof (u_int32_t) != 0) {
				u_int32_t temp = arc4random();

				bcopy (&temp, iv + i,
				    EALG_MAX_BLOCK_LEN - i);
			}
		}

		/* Do we need to write the IV */
		if (!(crd->crd_flags & CRD_F_IV_PRESENT)) {
			COPYBACK(outtype, buf, crd->crd_inject, blks, iv);
		}

	} else {	/* Decryption */
			/* IV explicitly provided ? */
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, iv, blks);
		else {
			/* Get IV off buf */
			COPYDATA(outtype, buf, crd->crd_inject, blks, iv);
		}
	}

	ivp = iv;

	if (outtype == CRYPTO_BUF_MBUF) {
		/* Find beginning of data */
		m = m_getptr(m, crd->crd_skip, &k);
		if (m == NULL)
			return EINVAL;

		i = crd->crd_len;

		while (i > 0) {
			/*
			 * If there's insufficient data at the end of
			 * an mbuf, we have to do some copying.
			 */
			if (m->m_len < k + blks && m->m_len != k) {
				m_copydata(m, k, blks, blk);

				/* Actual encryption/decryption */
				if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, blk);

					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					bcopy(blk, iv, blks);
					ivp = iv;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					if (ivp == iv)
						bcopy(blk, piv, blks);
					else
						bcopy(blk, iv, blks);

					exf->decrypt(sw->sw_kschedule, blk);

					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					if (ivp == iv)
						bcopy(piv, iv, blks);
					else
						ivp = iv;
				}

				/* Copy back decrypted block */
				m_copyback(m, k, blks, blk);

				/* Advance pointer */
				m = m_getptr(m, k + blks, &k);
				if (m == NULL)
					return EINVAL;

				i -= blks;

				/* Could be done... */
				if (i == 0)
					break;
			}

			/* Skip possibly empty mbufs */
			if (k == m->m_len) {
				for (m = m->m_next; m && m->m_len == 0;
				    m = m->m_next)
					;
				k = 0;
			}

			/* Sanity check */
			if (m == NULL)
				return EINVAL;

			/*
			 * Warning: idat may point to garbage here, but
			 * we only use it in the while() loop, only if
			 * there are indeed enough data.
			 */
			idat = mtod(m, unsigned char *) + k;

			while (m->m_len >= k + blks && i > 0) {
				if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, idat);
					ivp = idat;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block to be used
					 * in next block's processing.
					 */
					if (ivp == iv)
						bcopy(idat, piv, blks);
					else
						bcopy(idat, iv, blks);

					exf->decrypt(sw->sw_kschedule, idat);

					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					if (ivp == iv)
						bcopy(piv, iv, blks);
					else
						ivp = iv;
				}

				idat += blks;
				k += blks;
				i -= blks;
			}
		}
	} else {
		/* Find beginning of data */
		count = crd->crd_skip;
		ind = cuio_getptr(uio, count, &k);
		if (ind == -1)
			return EINVAL;

		i = crd->crd_len;

		while (i > 0) {
			/*
			 * If there's insufficient data at the end,
			 * we have to do some copying.
			 */
			if (uio->uio_iov[ind].iov_len < k + blks &&
			    uio->uio_iov[ind].iov_len != k) {
				cuio_copydata(uio, k, blks, blk);

				/* Actual encryption/decryption */
				if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, blk);

					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					bcopy(blk, iv, blks);
					ivp = iv;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					if (ivp == iv)
						bcopy(blk, piv, blks);
					else
						bcopy(blk, iv, blks);

					exf->decrypt(sw->sw_kschedule, blk);

					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					if (ivp == iv)
						bcopy(piv, iv, blks);
					else
						ivp = iv;
				}

				/* Copy back decrypted block */
				cuio_copyback(uio, k, blks, blk);

				count += blks;

				/* Advance pointer */
				ind = cuio_getptr(uio, count, &k);
				if (ind == -1)
					return (EINVAL);

				i -= blks;

				/* Could be done... */
				if (i == 0)
					break;
			}

			/*
			 * Warning: idat may point to garbage here, but
			 * we only use it in the while() loop, only if
			 * there are indeed enough data.
			 */
			idat = uio->uio_iov[ind].iov_base + k;

			while (uio->uio_iov[ind].iov_len >= k + blks &&
			    i > 0) {
				if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, idat);
					ivp = idat;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block to be used
					 * in next block's processing.
					 */
					if (ivp == iv)
						bcopy(idat, piv, blks);
					else
						bcopy(idat, iv, blks);

					exf->decrypt(sw->sw_kschedule, idat);

					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					if (ivp == iv)
						bcopy(piv, iv, blks);
					else
						ivp = iv;
				}

				idat += blks;
				count += blks;
				k += blks;
				i -= blks;
			}
		}
	}

	return 0; /* Done with encryption/decryption */
}

/*
 * Compute keyed-hash authenticator.
 */
int
swcr_authcompute(struct cryptop *crp, struct cryptodesc *crd,
    struct swcr_data *sw, caddr_t buf, int outtype)
{
	unsigned char aalg[AALG_MAX_RESULT_LEN];
	struct auth_hash *axf;
	union authctx ctx;
	int err;

	if (sw->sw_ictx == 0)
		return EINVAL;

	axf = sw->sw_axf;

	bcopy(sw->sw_ictx, &ctx, axf->ctxsize);

	if (outtype == CRYPTO_BUF_MBUF)
		err = m_apply((struct mbuf *) buf, crd->crd_skip, crd->crd_len,
		    (int (*)(caddr_t, caddr_t, unsigned int)) axf->Update,
		    (caddr_t) &ctx);
	else
		err = cuio_apply((struct uio *) buf, crd->crd_skip,
		    crd->crd_len,
		    (int (*)(caddr_t, caddr_t, unsigned int)) axf->Update,
		    (caddr_t) &ctx);

	if (err)
		return err;

	switch (sw->sw_alg) {
	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
		if (sw->sw_octx == NULL)
			return EINVAL;

		axf->Final(aalg, &ctx);
		bcopy(sw->sw_octx, &ctx, axf->ctxsize);
		axf->Update(&ctx, aalg, axf->hashsize);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
		if (sw->sw_octx == NULL)
			return EINVAL;

		axf->Update(&ctx, sw->sw_octx, sw->sw_klen);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_MD5:
	case CRYPTO_SHA1:
		axf->Final(aalg, &ctx);
		break;
	}

	/* Inject the authentication data */
	if (outtype == CRYPTO_BUF_MBUF)
		COPYBACK(outtype, buf, crd->crd_inject, axf->authsize, aalg);
	else
		bcopy(aalg, crp->crp_mac, axf->authsize);

	return 0;
}

/*
 * Apply a compression/decompression algorithm
 */
int
swcr_compdec(struct cryptodesc *crd, struct swcr_data *sw,
    caddr_t buf, int outtype)
{
	u_int8_t *data, *out;
	struct comp_algo *cxf;
	int adj;
	u_int32_t result;

	cxf = sw->sw_cxf;

	/* We must handle the whole buffer of data in one time
	 * then if there is not all the data in the mbuf, we must
	 * copy in a buffer.
	 */

	MALLOC(data, u_int8_t *, crd->crd_len, M_CRYPTO_DATA,  M_NOWAIT);
	if (data == NULL)
		return (EINVAL);
	COPYDATA(outtype, buf, crd->crd_skip, crd->crd_len, data);

	if (crd->crd_flags & CRD_F_COMP)
		result = cxf->compress(data, crd->crd_len, &out);
	else
		result = cxf->decompress(data, crd->crd_len, &out);

	FREE(data, M_CRYPTO_DATA);
	if (result == 0)
		return EINVAL;

	/* Copy back the (de)compressed data. m_copyback is
	 * extending the mbuf as necessary.
	 */
	sw->sw_size = result;
	/* Check the compressed size when doing compression */
	if (crd->crd_flags & CRD_F_COMP) {
		if (result > crd->crd_len) {
			/* Compression was useless, we lost time */
			FREE(out, M_CRYPTO_DATA);
			return 0;
		}
	}

	COPYBACK(outtype, buf, crd->crd_skip, result, out);
	if (result < crd->crd_len) {
		adj = result - crd->crd_len;
		if (outtype == CRYPTO_BUF_MBUF) {
			adj = result - crd->crd_len;
			m_adj((struct mbuf *)buf, adj);
		} else {
			struct uio *uio = (struct uio *)buf;
			int ind;

			adj = crd->crd_len - result;
			ind = uio->uio_iovcnt - 1;

			while (adj > 0 && ind >= 0) {
				if (adj < uio->uio_iov[ind].iov_len) {
					uio->uio_iov[ind].iov_len -= adj;
					break;
				}

				adj -= uio->uio_iov[ind].iov_len;
				uio->uio_iov[ind].iov_len = 0;
				ind--;
				uio->uio_iovcnt--;
			}
		}
	}
	FREE(out, M_CRYPTO_DATA);
	return 0;
}

/*
 * Generate a new software session.
 */
int
swcr_newsession(u_int32_t *sid, struct cryptoini *cri)
{
	struct swcr_data **swd;
	struct auth_hash *axf;
	struct enc_xform *txf;
	struct comp_algo *cxf;
	u_int32_t i;
	int k;

	if (sid == NULL || cri == NULL)
		return EINVAL;

	if (swcr_sessions) {
		for (i = 1; i < swcr_sesnum; i++)
			if (swcr_sessions[i] == NULL)
				break;
	}

	if (swcr_sessions == NULL || i == swcr_sesnum) {
		if (swcr_sessions == NULL) {
			i = 1; /* We leave swcr_sessions[0] empty */
			swcr_sesnum = CRYPTO_SW_SESSIONS;
		} else
			swcr_sesnum *= 2;

		swd = malloc(swcr_sesnum * sizeof(struct swcr_data *),
		    M_CRYPTO_DATA, M_NOWAIT);
		if (swd == NULL) {
			/* Reset session number */
			if (swcr_sesnum == CRYPTO_SW_SESSIONS)
				swcr_sesnum = 0;
			else
				swcr_sesnum /= 2;
			return ENOBUFS;
		}

		bzero(swd, swcr_sesnum * sizeof(struct swcr_data *));

		/* Copy existing sessions */
		if (swcr_sessions) {
			bcopy(swcr_sessions, swd,
			    (swcr_sesnum / 2) * sizeof(struct swcr_data *));
			free(swcr_sessions, M_CRYPTO_DATA);
		}

		swcr_sessions = swd;
	}

	swd = &swcr_sessions[i];
	*sid = i;

	while (cri) {
		MALLOC(*swd, struct swcr_data *, sizeof(struct swcr_data),
		    M_CRYPTO_DATA, M_NOWAIT);
		if (*swd == NULL) {
			swcr_freesession(i);
			return ENOBUFS;
		}
		bzero(*swd, sizeof(struct swcr_data));

		switch (cri->cri_alg) {
		case CRYPTO_DES_CBC:
			txf = &enc_xform_des;
			goto enccommon;
		case CRYPTO_3DES_CBC:
			txf = &enc_xform_3des;
			goto enccommon;
		case CRYPTO_BLF_CBC:
			txf = &enc_xform_blf;
			goto enccommon;
		case CRYPTO_CAST_CBC:
			txf = &enc_xform_cast5;
			goto enccommon;
		case CRYPTO_SKIPJACK_CBC:
			txf = &enc_xform_skipjack;
			goto enccommon;
		case CRYPTO_RIJNDAEL128_CBC:
			txf = &enc_xform_rijndael128;
			goto enccommon;
		case CRYPTO_NULL:
			txf = &enc_xform_null;
			goto enccommon;
		enccommon:
			txf->setkey(&((*swd)->sw_kschedule), cri->cri_key,
			    cri->cri_klen / 8);
			(*swd)->sw_exf = txf;
			break;

		case CRYPTO_MD5_HMAC:
			axf = &auth_hash_hmac_md5_96;
			goto authcommon;
		case CRYPTO_SHA1_HMAC:
			axf = &auth_hash_hmac_sha1_96;
			goto authcommon;
		case CRYPTO_RIPEMD160_HMAC:
			axf = &auth_hash_hmac_ripemd_160_96;
		authcommon:
			(*swd)->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(i);
				return ENOBUFS;
			}

			(*swd)->sw_octx = malloc(axf->ctxsize, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_octx == NULL) {
				swcr_freesession(i);
				return ENOBUFS;
			}

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= HMAC_IPAD_VAL;

			axf->Init((*swd)->sw_ictx);
			axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Update((*swd)->sw_ictx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (cri->cri_klen / 8));

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

			axf->Init((*swd)->sw_octx);
			axf->Update((*swd)->sw_octx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Update((*swd)->sw_octx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (cri->cri_klen / 8));

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= HMAC_OPAD_VAL;
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_MD5_KPDK:
			axf = &auth_hash_key_md5;
			goto auth2common;

		case CRYPTO_SHA1_KPDK:
			axf = &auth_hash_key_sha1;
		auth2common:
			(*swd)->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(i);
				return ENOBUFS;
			}

			/* Store the key so we can "append" it to the payload */
			(*swd)->sw_octx = malloc(cri->cri_klen / 8, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_octx == NULL) {
				swcr_freesession(i);
				return ENOBUFS;
			}

			(*swd)->sw_klen = cri->cri_klen / 8;
			bcopy(cri->cri_key, (*swd)->sw_octx, cri->cri_klen / 8);
			axf->Init((*swd)->sw_ictx);
			axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Final(NULL, (*swd)->sw_ictx);
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_MD5:
			axf = &auth_hash_md5;
			goto auth3common;

		case CRYPTO_SHA1:
			axf = &auth_hash_sha1;
		auth3common:
			(*swd)->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(i);
				return ENOBUFS;
			}

			axf->Init((*swd)->sw_ictx);
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_DEFLATE_COMP:
			cxf = &comp_algo_deflate;
			(*swd)->sw_cxf = cxf;
			break;
		default:
			swcr_freesession(i);
			return EINVAL;
		}

		(*swd)->sw_alg = cri->cri_alg;
		cri = cri->cri_next;
		swd = &((*swd)->sw_next);
	}
	return 0;
}

/*
 * Free a session.
 */
int
swcr_freesession(u_int64_t tid)
{
	struct swcr_data *swd;
	struct enc_xform *txf;
	struct auth_hash *axf;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	if (sid > swcr_sesnum || swcr_sessions == NULL ||
	    swcr_sessions[sid] == NULL)
		return EINVAL;

	/* Silently accept and return */
	if (sid == 0)
		return 0;

	while ((swd = swcr_sessions[sid]) != NULL) {
		swcr_sessions[sid] = swd->sw_next;

		switch (swd->sw_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_BLF_CBC:
		case CRYPTO_CAST_CBC:
		case CRYPTO_SKIPJACK_CBC:
		case CRYPTO_RIJNDAEL128_CBC:
		case CRYPTO_NULL:
			txf = swd->sw_exf;

			if (swd->sw_kschedule)
				txf->zerokey(&(swd->sw_kschedule));
			break;

		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
			axf = swd->sw_axf;

			if (swd->sw_ictx) {
				bzero(swd->sw_ictx, axf->ctxsize);
				free(swd->sw_ictx, M_CRYPTO_DATA);
			}
			if (swd->sw_octx) {
				bzero(swd->sw_octx, axf->ctxsize);
				free(swd->sw_octx, M_CRYPTO_DATA);
			}
			break;

		case CRYPTO_MD5_KPDK:
		case CRYPTO_SHA1_KPDK:
			axf = swd->sw_axf;

			if (swd->sw_ictx) {
				bzero(swd->sw_ictx, axf->ctxsize);
				free(swd->sw_ictx, M_CRYPTO_DATA);
			}
			if (swd->sw_octx) {
				bzero(swd->sw_octx, swd->sw_klen);
				free(swd->sw_octx, M_CRYPTO_DATA);
			}
			break;

		case CRYPTO_MD5:
		case CRYPTO_SHA1:
			axf = swd->sw_axf;

			if (swd->sw_ictx)
				free(swd->sw_ictx, M_CRYPTO_DATA);
			break;
		}

		FREE(swd, M_CRYPTO_DATA);
	}
	return 0;
}

/*
 * Process a software request.
 */
int
swcr_process(struct cryptop *crp)
{
	struct cryptodesc *crd;
	struct swcr_data *sw;
	u_int32_t lid;
	int type;

	/* Sanity check */
	if (crp == NULL)
		return EINVAL;

	if (crp->crp_desc == NULL || crp->crp_buf == NULL) {
		crp->crp_etype = EINVAL;
		goto done;
	}

	lid = crp->crp_sid & 0xffffffff;
	if (lid >= swcr_sesnum || lid == 0 || swcr_sessions[lid] == NULL) {
		crp->crp_etype = ENOENT;
		goto done;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		type = CRYPTO_BUF_MBUF;
	else
		type = CRYPTO_BUF_IOV;

	/* Go through crypto descriptors, processing as we go */
	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		/*
		 * Find the crypto context.
		 *
		 * XXX Note that the logic here prevents us from having
		 * XXX the same algorithm multiple times in a session
		 * XXX (or rather, we can but it won't give us the right
		 * XXX results). To do that, we'd need some way of differentiating
		 * XXX between the various instances of an algorithm (so we can
		 * XXX locate the correct crypto context).
		 */
		for (sw = swcr_sessions[lid];
		    sw && sw->sw_alg != crd->crd_alg;
		    sw = sw->sw_next)
			;

		/* No such context ? */
		if (sw == NULL) {
			crp->crp_etype = EINVAL;
			goto done;
		}

		switch (sw->sw_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_BLF_CBC:
		case CRYPTO_CAST_CBC:
		case CRYPTO_SKIPJACK_CBC:
		case CRYPTO_RIJNDAEL128_CBC:
		case CRYPTO_NULL:
			if ((crp->crp_etype = swcr_encdec(crd, sw,
			    crp->crp_buf, type)) != 0)
				goto done;
			break;
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
		case CRYPTO_MD5_KPDK:
		case CRYPTO_SHA1_KPDK:
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
			if ((crp->crp_etype = swcr_authcompute(crp, crd, sw,
			    crp->crp_buf, type)) != 0)
				goto done;
			break;

		case CRYPTO_DEFLATE_COMP:
			if ((crp->crp_etype = swcr_compdec(crd, sw,
			    crp->crp_buf, type)) != 0)
				goto done;
			else
				crp->crp_olen = (int)sw->sw_size;
			break;

		default:
			/* Unknown/unsupported algorithm */
			crp->crp_etype = EINVAL;
			goto done;
		}
	}

done:
	crypto_done(crp);
	return 0;
}

/*
 * Initialize the driver, called from the kernel main().
 */
void
swcr_init(void)
{
	int algs[CRYPTO_ALGORITHM_MAX + 1];
	int flags = CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_ENCRYPT_MAC |
	    CRYPTOCAP_F_MAC_ENCRYPT;

	swcr_id = crypto_get_driverid(flags);
	if (swcr_id < 0) {
		/* This should never happen */
		panic("Software crypto device cannot initialize!");
	}

	bzero(algs, sizeof(algs));

	algs[CRYPTO_DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_3DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_BLF_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_CAST_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_SKIPJACK_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_MD5_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_RIPEMD160_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_MD5_KPDK] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_SHA1_KPDK] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_MD5] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_SHA1] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_RIJNDAEL128_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_DEFLATE_COMP] = CRYPTO_ALG_FLAG_SUPPORTED;
	algs[CRYPTO_NULL] = CRYPTO_ALG_FLAG_SUPPORTED;

	crypto_register(swcr_id, algs, swcr_newsession,
	    swcr_freesession, swcr_process);
}
