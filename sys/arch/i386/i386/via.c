/*	$OpenBSD: via.c,v 1.2 2004/06/15 19:19:03 deraadt Exp $	*/
/*	$NetBSD: machdep.c,v 1.214 1996/11/10 03:16:17 thorpej Exp $	*/

/*-
 * Copyright (c) 2003 Jason Wright
 * Copyright (c) 2003, 2004 Theo de Raadt
 * All rights reserved.
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
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/extent.h>
#include <sys/sysctl.h>

#ifdef CRYPTO
#include <crypto/cryptodev.h>
#include <crypto/rijndael.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/bus.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/biosvar.h>

#include <dev/rndvar.h>

void	viac3_rnd(void *);
int viac3_crypto_present;


#ifdef CRYPTO

struct viac3_session {
	u_int32_t	ses_ekey[4 * (MAXNR + 1) + 4];	/* 128 bit aligned */
	u_int32_t	ses_dkey[4 * (MAXNR + 1) + 4];	/* 128 bit aligned */
	u_int8_t	ses_iv[16];			/* 128 bit aligned */
	u_int32_t	ses_cw0;
	int		ses_klen;
	int		ses_used;
	int		ses_pad;			/* to multiple of 16 */
};

struct viac3_softc {
	u_int32_t		op_cw[4];		/* 128 bit aligned */
	u_int8_t		op_iv[16];		/* 128 bit aligned */
	void			*op_buf;

	/* normal softc stuff */
	int32_t			sc_cid;
	int			sc_nsessions;
	struct viac3_session	*sc_sessions;
};

#define VIAC3_SESSION(sid)		((sid) & 0x0fffffff)
#define	VIAC3_SID(crd,ses)		(((crd) << 28) | ((ses) & 0x0fffffff))

static struct viac3_softc *vc3_sc;
extern int i386_has_xcrypt;

void viac3_crypto_setup(void);
int viac3_crypto_newsession(u_int32_t *, struct cryptoini *);
int viac3_crypto_process(struct cryptop *);
int viac3_crypto_freesession(u_int64_t);
static __inline void viac3_cbc(void *, void *, void *, void *, int, void *);

void
viac3_crypto_setup(void)
{
	int algs[CRYPTO_ALGORITHM_MAX + 1];

	if ((vc3_sc = malloc(sizeof(*vc3_sc), M_DEVBUF, M_NOWAIT)) == NULL)
		return;		/* YYY bitch? */
	bzero(vc3_sc, sizeof(*vc3_sc));

	bzero(algs, sizeof(algs));
	algs[CRYPTO_AES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;

	vc3_sc->sc_cid = crypto_get_driverid(0);
	if (vc3_sc->sc_cid < 0)
		return;		/* YYY bitch? */

	crypto_register(vc3_sc->sc_cid, algs, viac3_crypto_newsession,
	    viac3_crypto_freesession, viac3_crypto_process);
	i386_has_xcrypt = viac3_crypto_present;
}

int
viac3_crypto_newsession(u_int32_t *sidp, struct cryptoini *cri)
{
	struct viac3_softc *sc = vc3_sc;
	struct viac3_session *ses = NULL;
	int sesn, i, cw0;

	if (sc == NULL || sidp == NULL || cri == NULL ||
	    cri->cri_next != NULL || cri->cri_alg != CRYPTO_AES_CBC)
		return (EINVAL);

	switch (cri->cri_klen) {
	case 128:
		cw0 = C3_CRYPT_CWLO_KEY128;
		break;
	case 192:
		cw0 = C3_CRYPT_CWLO_KEY192;
		break;
	case 256:
		cw0 = C3_CRYPT_CWLO_KEY256;
		break;
	default:
		return (EINVAL);
	}
	cw0 |= C3_CRYPT_CWLO_ALG_AES | C3_CRYPT_CWLO_KEYGEN_SW |
	    C3_CRYPT_CWLO_NORMAL;

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct viac3_session *)malloc(
		    sizeof(*ses), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (sc->sc_sessions[sesn].ses_used == 0) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			ses = (struct viac3_session *)malloc((sesn + 1) *
			    sizeof(*ses), M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			bcopy(sc->sc_sessions, ses, sesn * sizeof(*ses));
			bzero(sc->sc_sessions, sesn * sizeof(*ses));
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}

	bzero(ses, sizeof(*ses));
	ses->ses_used = 1;

	get_random_bytes(ses->ses_iv, sizeof(ses->ses_iv));
	ses->ses_klen = cri->cri_klen;
	ses->ses_cw0 = cw0;

	/* Build expanded keys for both directions */
	rijndaelKeySetupEnc(ses->ses_ekey, cri->cri_key, cri->cri_klen);
	rijndaelKeySetupDec(ses->ses_dkey, cri->cri_key, cri->cri_klen);
	for (i = 0; i < 4 * (MAXNR + 1); i++) {
		ses->ses_ekey[i] = ntohl(ses->ses_ekey[i]);
		ses->ses_dkey[i] = ntohl(ses->ses_dkey[i]);
	}

	*sidp = VIAC3_SID(0, sesn);
	return (0);
}

int
viac3_crypto_freesession(u_int64_t tid)
{
	struct viac3_softc *sc = vc3_sc;
	int sesn;
	u_int32_t sid = ((u_int32_t)tid) & 0xffffffff;

	if (sc == NULL)
		return (EINVAL);
	sesn = VIAC3_SESSION(sid);
	if (sesn >= sc->sc_nsessions)
		return (EINVAL);
	bzero(&sc->sc_sessions[sesn], sizeof(sc->sc_sessions[sesn]));
	return (0);
}

static __inline void
viac3_cbc(void *cw, void *src, void *dst, void *key, int rep,
    void *iv)
{
	unsigned int creg0;

	creg0 = rcr0();		/* Permit access to SIMD/FPU path */
	lcr0(creg0 & ~(CR0_EM|CR0_TS));

	/* Do the deed */
	__asm __volatile("pushfl; popfl");
	__asm __volatile("rep xcrypt-cbc" :
	    : "a" (iv), "b" (key), "c" (rep), "d" (cw), "S" (src), "D" (dst)
	    : "memory", "cc");

	lcr0(creg0);
}

int
viac3_crypto_process(struct cryptop *crp)
{
	struct viac3_softc *sc = vc3_sc;
	struct viac3_session *ses;
	struct cryptodesc *crd;
	int sesn, err = 0;
	u_int32_t *key;

	if (crp == NULL || crp->crp_callback == NULL) {
		err = EINVAL;
		goto out;
	}
	crd = crp->crp_desc;
	if (crd == NULL || crd->crd_next != NULL ||
	    crd->crd_alg != CRYPTO_AES_CBC || 
	    (crd->crd_len % 16) != 0) {
		err = EINVAL;
		goto out;
	}

	sesn = VIAC3_SESSION(crp->crp_sid);
	if (sesn >= sc->sc_nsessions) {
		err = EINVAL;
		goto out;
	}
	ses = &sc->sc_sessions[sesn];

	sc->op_buf = (char *)malloc(crd->crd_len, M_DEVBUF, M_NOWAIT);
	if (sc->op_buf == NULL) {
		err = ENOMEM;
		goto out;
	}

	if (crd->crd_flags & CRD_F_ENCRYPT) {
		sc->op_cw[0] = ses->ses_cw0 | C3_CRYPT_CWLO_ENCRYPT;
		key = ses->ses_ekey;
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, sc->op_iv, 16);
		else
			bcopy(ses->ses_iv, sc->op_iv, 16);

		if ((crd->crd_flags & CRD_F_IV_PRESENT) == 0) {
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copyback((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, sc->op_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copyback((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, sc->op_iv);
			else
				bcopy(sc->op_iv,
				    crp->crp_buf + crd->crd_inject, 16);
		}
	} else {
		sc->op_cw[0] = ses->ses_cw0 | C3_CRYPT_CWLO_DECRYPT;
		key = ses->ses_dkey;
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, sc->op_iv, 16);
		else {
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, sc->op_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, sc->op_iv);
			else
				bcopy(crp->crp_buf + crd->crd_inject,
				    sc->op_iv, 16);
		}
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m_copydata((struct mbuf *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, sc->op_buf);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		cuio_copydata((struct uio *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, sc->op_buf);
	else
		bcopy(crp->crp_buf + crd->crd_skip, sc->op_buf, crd->crd_len);

	sc->op_cw[1] = sc->op_cw[2] = sc->op_cw[3] = 0;
	viac3_cbc(&sc->op_cw, sc->op_buf, sc->op_buf, key,
	    crd->crd_len / 16, sc->op_iv);

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m_copyback((struct mbuf *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, sc->op_buf);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		cuio_copyback((struct uio *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, sc->op_buf);
	else
		bcopy(sc->op_buf, crp->crp_buf + crd->crd_skip, crd->crd_len);

	/* copy out last block for use as next session IV */
	if (crd->crd_flags & CRD_F_ENCRYPT) {
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copydata((struct mbuf *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, ses->ses_iv);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			cuio_copydata((struct uio *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, sc->op_iv);
		else
			bcopy(crp->crp_buf + crd->crd_skip + crd->crd_len - 16,
			    sc->op_iv, 16);
	}

out:
	if (sc->op_buf != NULL) {
		bzero(sc->op_buf, crd->crd_len);
		free(sc->op_buf, M_DEVBUF);
		sc->op_buf = NULL;
	}
	crp->crp_etype = err;
	crypto_done(crp);
	return (err);
}

#endif /* CRYPTO */

#if defined(I686_CPU)
/*
 * Note, the VIA C3 Nehemiah provides 4 internal 8-byte buffers, which
 * store random data, and can be accessed a lot quicker than waiting
 * for new data to be generated.  As we are using every 8th bit only
 * due to whitening. Since the RNG generates in excess of 21KB/s at
 * it's worst, collecting 64 bytes worth of entropy should not affect
 * things significantly.
 *
 * Note, due to some weirdness in the RNG, we need at least 7 bytes
 * extra on the end of our buffer.  Also, there is an outside chance
 * that the VIA RNG can "wedge", as the generated bit-rate is variable.
 * We could do all sorts of startup testing and things, but
 * frankly, I don't really see the point.  If the RNG wedges, then the
 * chances of you having a defective CPU are very high.  Let it wedge.
 *
 * Adding to the whole confusion, in order to access the RNG, we need
 * to have FXSR support enabled, and the correct FPU enable bits must
 * be there to enable the FPU in kernel.  It would be nice if all this
 * mumbo-jumbo was not needed in order to use the RNG.  Oh well, life
 * does go on...
 */
#define VIAC3_RNG_BUFSIZ	16		/* 32bit words */
struct timeout viac3_rnd_tmo;
int viac3_rnd_present;

void
viac3_rnd(void *v)
{
	struct timeout *tmo = v;
	unsigned int *p, i, rv, creg0, len = VIAC3_RNG_BUFSIZ;
	static int buffer[VIAC3_RNG_BUFSIZ + 2];	/* XXX why + 2? */

	creg0 = rcr0();		/* Permit access to SIMD/FPU path */
	lcr0(creg0 & ~(CR0_EM|CR0_TS));

	/*
	 * Here we collect the random data from the VIA C3 RNG.  We make
	 * sure that we turn on maximum whitening (%edx[0,1] == "11"), so
	 * that we get the best random data possible.
	 */
	__asm __volatile("rep xstore-rng"
	    : "=a" (rv) : "d" (3), "D" (buffer), "c" (len*sizeof(int))
	    : "memory", "cc");

	lcr0(creg0);

	for (i = 0, p = buffer; i < VIAC3_RNG_BUFSIZ; i++, p++)
		add_true_randomness(*p);

	timeout_add(tmo, (hz > 100) ? (hz / 100) : 1);
}

#endif /* defined(I686_CPU) */
