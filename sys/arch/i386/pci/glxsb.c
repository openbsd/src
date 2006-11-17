/*	$OpenBSD: glxsb.c,v 1.2 2006/11/17 16:06:16 tom Exp $	*/

/*
 * Copyright (c) 2006 Tom Cosgrove <tom@openbsd.org>
 * Copyright (c) 2003, 2004 Theo de Raadt
 * Copyright (c) 2003 Jason Wright
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

/*
 * Driver for the security block on the AMD Geode LX processors
 * http://www.amd.com/files/connectivitysolutions/geode/geode_lx/33234d_lx_ds.pdf
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/types.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/pctr.h>

#include <dev/rndvar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#undef CRYPTO
#ifdef CRYPTO
#include <crypto/cryptodev.h>
#include <crypto/rijndael.h>
#endif

#define SB_GLD_MSR_CAP		0x58002000	/* RO - Capabilities */
#define SB_GLD_MSR_CONFIG	0x58002001	/* RW - Master Config */
#define SB_GLD_MSR_SMI		0x58002002	/* RW - SMI */
#define SB_GLD_MSR_ERROR	0x58002003	/* RW - Error */
#define SB_GLD_MSR_PM		0x58002004	/* RW - Power Mgmt */
#define SB_GLD_MSR_DIAG		0x58002005	/* RW - Diagnostic */
#define SB_GLD_MSR_CTRL		0x58002006	/* RW - Security Block Cntrl */

						/* For GLD_MSR_CTRL: */
#define SB_GMC_DIV0		0x0000		/* AES update divisor values */
#define SB_GMC_DIV1		0x0001
#define SB_GMC_DIV2		0x0002
#define SB_GMC_DIV3		0x0003
#define SB_GMC_DIV_MASK		0x0003
#define SB_GMC_SBI		0x0004		/* AES swap bits */
#define SB_GMC_SBY		0x0008		/* AES swap bytes */
#define SB_GMC_TW		0x0010		/* Time write (EEPROM) */
#define SB_GMC_T_SEL0		0x0000		/* RNG post-proc: none */
#define SB_GMC_T_SEL1		0x0100		/* RNG post-proc: LFSR */
#define SB_GMC_T_SEL2		0x0200		/* RNG post-proc: whitener */
#define SB_GMC_T_SEL3		0x0300		/* RNG LFSR+whitener */
#define SB_GMC_T_SEL_MASK	0x0300
#define SB_GMC_T_NE		0x0400		/* Noise (generator) Enable */
#define SB_GMC_T_TM		0x0800		/* RNG test mode */
						/*     (deterministic) */

/* Security Block configuration/control registers (offsets from base) */

#define SB_CTL_A		0x0000		/* RW - SB Control A */
#define SB_CTL_B		0x0004		/* RW - SB Control B */
#define SB_AES_INT		0x0008		/* RW - SB AES Interrupt */
#define SB_SOURCE_A		0x0010		/* RW - Source A */
#define SB_DEST_A		0x0014		/* RW - Destination A */
#define SB_LENGTH_A		0x0018		/* RW - Length A */
#define SB_SOURCE_B		0x0020		/* RW - Source B */
#define SB_DEST_B		0x0024		/* RW - Destination B */
#define SB_LENGTH_B		0x0028		/* RW - Length B */
#define SB_WKEY			0x0030		/* WO - Writable Key 0-3 */
#define SB_WKEY_0		0x0030		/* WO - Writable Key 0 */
#define SB_WKEY_1		0x0034		/* WO - Writable Key 1 */
#define SB_WKEY_2		0x0038		/* WO - Writable Key 2 */
#define SB_WKEY_3		0x003C		/* WO - Writable Key 3 */
#define SB_CBC_IV		0x0040		/* RW - CBC IV 0-3 */
#define SB_CBC_IV_0		0x0040		/* RW - CBC IV 0 */
#define SB_CBC_IV_1		0x0044		/* RW - CBC IV 1 */
#define SB_CBC_IV_2		0x0048		/* RW - CBC IV 2 */
#define SB_CBC_IV_3		0x004C		/* RW - CBC IV 3 */
#define SB_RANDOM_NUM		0x0050		/* RW - Random Number */
#define SB_RANDOM_NUM_STATUS	0x0054		/* RW - Random Number Status */
#define SB_EEPROM_COMM		0x0800		/* RW - EEPROM Command */
#define SB_EEPROM_ADDR		0x0804		/* RW - EEPROM Address */
#define SB_EEPROM_DATA		0x0808		/* RW - EEPROM Data */
#define SB_EEPROM_SEC_STATE	0x080C		/* RW - EEPROM Security State */

						/* For SB_CTL_A and _B */
#define SB_CTL_ST		0x0001		/* Start operation (enc/dec) */
#define SB_CTL_ENC		0x0002		/* Encrypt (0 is decrypt) */
#define SB_CTL_DEC		0x0000		/* Decrypt */
#define SB_CTL_WK		0x0004		/* Use writable key (we set) */
#define SB_CTL_DC		0x0008		/* Destination coherent */
#define SB_CTL_SC		0x0010		/* Source coherent */
#define SB_CTL_CBC		0x0020		/* CBC (0 is ECB) */

						/* For SB_AES_INT */
#define SB_AI_DISABLE_AES_A	0x0001		/* Disable AES A compl int */
#define SB_AI_ENABLE_AES_A	0x0000		/* Enable AES A compl int */
#define SB_AI_DISABLE_AES_B	0x0002		/* Disable AES B compl int */
#define SB_AI_ENABLE_AES_B	0x0000		/* Enable AES B compl int */
#define SB_AI_DISABLE_EEPROM	0x0004		/* Disable EEPROM op comp int */
#define SB_AI_ENABLE_EEPROM	0x0000		/* Enable EEPROM op compl int */
#define SB_AI_AES_A_COMPLETE	0x0100		/* AES A operation complete */
#define SB_AI_AES_B_COMPLETE	0x0200		/* AES B operation complete */
#define SB_AI_EEPROM_COMPLETE	0x0400		/* EEPROM operation complete */

#define SB_RNS_TRNG_VALID	0x0001		/* in SB_RANDOM_NUM_STATUS */

#define SB_MEM_SIZE		0x0810		/* Size of memory block */

#ifdef CRYPTO
struct glxsb_session {
	uint32_t	ses_key[4];
	uint8_t		ses_iv[16];
	int		ses_klen;
	int		ses_used;
};
#endif /* CRYPTO */

struct glxsb_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct timeout		sc_to;

#ifdef CRYPTO
	int32_t			sc_cid;
	int			sc_nsessions;
	struct glxsb_session	*sc_sessions;

	int			maxpolls;	/* XXX */
#endif /* CRYPTO */
};

int	glxsb_match(struct device *, void *, void *);
void	glxsb_attach(struct device *, struct device *, void *);
void	glxsb_rnd(void *);

struct cfattach glxsb_ca = {
	sizeof(struct glxsb_softc), glxsb_match, glxsb_attach
};

struct cfdriver glxsb_cd = {
	NULL, "glxsb", DV_DULL
};


#ifdef CRYPTO

#define GLXSB_SESSION(sid)		((sid) & 0x0fffffff)
#define	GLXSB_SID(crd,ses)		(((crd) << 28) | ((ses) & 0x0fffffff))

static struct glxsb_softc *glxsb_sc;
extern int i386_has_xcrypt;

int glxsb_crypto_setup(struct glxsb_softc *);
int glxsb_crypto_newsession(uint32_t *, struct cryptoini *);
int glxsb_crypto_process(struct cryptop *);
int glxsb_crypto_freesession(uint64_t);
static void glxsb_bus_space_write_consec_16(bus_space_tag_t,
    bus_space_handle_t, bus_size_t, uint32_t *);
static __inline void glxsb_aes(struct glxsb_softc *, uint32_t, void *, void *,
    void *, int, void *);

#endif /* CRYPTO */


int
glxsb_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_GEODE_LX_CRYPTO)
		return (1);

	return (0);
}

void
glxsb_attach(struct device *parent, struct device *self, void *aux)
{
	struct glxsb_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	bus_addr_t membase;
	bus_size_t memsize;
	uint64_t msr;
#ifdef CRYPTO
	uint32_t intr;
#endif

	msr = rdmsr(SB_GLD_MSR_CAP);
	if ((msr & 0xFFFF00) != 0x130400) {
		printf(": unknown ID 0x%x\n", (int) ((msr & 0xFFFF00) >> 16));
		return;
	}

	/* printf(": revision %d", (int) (msr & 0xFF)); */

	/* Map in the security block configuration/control registers */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_iot,
	    &sc->sc_ioh, &membase, &memsize, SB_MEM_SIZE)) {
		printf(": can't find mem space\n");
		return;
	}

	/*
	 * Configure the Security Block.
	 *
	 * We want to enable the noise generator (T_NE), and enable the
	 * linear feedback shift register and whitener post-processing
	 * (T_SEL = 3).  Also ensure that test mode (deterministic values)
	 * is disabled.
	 */
	msr = rdmsr(SB_GLD_MSR_CTRL);
	msr &= ~(SB_GMC_T_TM | SB_GMC_T_SEL_MASK);
	msr |= SB_GMC_T_NE | SB_GMC_T_SEL3;
#if 0
	msr |= SB_GMC_SBI | SB_GMC_SBY;		/* for AES, if necessary */
#endif
	wrmsr(SB_GLD_MSR_CTRL, msr);

	/* Install a periodic collector for the "true" (AMD's word) RNG */
	timeout_set(&sc->sc_to, glxsb_rnd, sc);
	glxsb_rnd(sc);
	printf(": RNG");

#ifdef CRYPTO
	/* We don't have an interrupt handler, so disable completion INTs */
	intr = SB_AI_DISABLE_AES_A | SB_AI_DISABLE_AES_B |
	    SB_AI_DISABLE_EEPROM | SB_AI_AES_A_COMPLETE |
	    SB_AI_AES_B_COMPLETE | SB_AI_EEPROM_COMPLETE;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_AES_INT, intr);

	if (glxsb_crypto_setup(sc))
		printf(" AES");
#endif

	printf("\n");
}

void
glxsb_rnd(void *v)
{
	struct glxsb_softc *sc = v;
	uint32_t status, value;
	extern int hz;

	status = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SB_RANDOM_NUM_STATUS);
	if (status & SB_RNS_TRNG_VALID) {
		value = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SB_RANDOM_NUM);
		add_true_randomness(value);
	}

	timeout_add(&sc->sc_to, (hz > 100) ? (hz / 100) : 1);
}

#ifdef CRYPTO
int
glxsb_crypto_setup(struct glxsb_softc *sc)
{
	int algs[CRYPTO_ALGORITHM_MAX + 1];

	bzero(algs, sizeof(algs));
	algs[CRYPTO_AES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0)
		return 0;

	crypto_register(sc->sc_cid, algs, glxsb_crypto_newsession,
	    glxsb_crypto_freesession, glxsb_crypto_process);

	sc->sc_nsessions = 0;

	glxsb_sc = sc;

	return 1;
}

int
glxsb_crypto_newsession(uint32_t *sidp, struct cryptoini *cri)
{
	struct glxsb_softc *sc = glxsb_sc;
	struct glxsb_session *ses = NULL;
	int sesn;

	if (sc == NULL || sidp == NULL || cri == NULL ||
	    cri->cri_next != NULL || cri->cri_alg != CRYPTO_AES_CBC ||
	    cri->cri_klen != 128)
		return (EINVAL);

	for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
		if (sc->sc_sessions[sesn].ses_used == 0) {
			ses = &sc->sc_sessions[sesn];
			break;
		}
	}

	if (ses == NULL) {
		sesn = sc->sc_nsessions;
		ses = malloc((sesn + 1) * sizeof(*ses), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);
		if (sesn != 0) {
			bcopy(sc->sc_sessions, ses, sesn * sizeof(*ses));
			bzero(sc->sc_sessions, sesn * sizeof(*ses));
			free(sc->sc_sessions, M_DEVBUF);
		}
		sc->sc_sessions = ses;
		ses = &sc->sc_sessions[sesn];
		sc->sc_nsessions++;
	}

	bzero(ses, sizeof(*ses));
	ses->ses_used = 1;

	get_random_bytes(ses->ses_iv, sizeof(ses->ses_iv));
	ses->ses_klen = cri->cri_klen;

	/* Copy the key (Geode LX wants the primary key only) */
	bcopy(cri->cri_key, ses->ses_key, sizeof(ses->ses_key));

	*sidp = GLXSB_SID(0, sesn);
	return (0);
}

int
glxsb_crypto_freesession(uint64_t tid)
{
	struct glxsb_softc *sc = glxsb_sc;
	int sesn;
	uint32_t sid = ((uint32_t)tid) & 0xffffffff;

	if (sc == NULL)
		return (EINVAL);
	sesn = GLXSB_SESSION(sid);
	if (sesn >= sc->sc_nsessions)
		return (EINVAL);
	bzero(&sc->sc_sessions[sesn], sizeof(sc->sc_sessions[sesn]));
	return (0);
}

static void
glxsb_bus_space_write_consec_16(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_size_t offset, uint32_t *dp)
{
	bus_space_write_4(iot, ioh, offset +  0, dp[0]);
	bus_space_write_4(iot, ioh, offset +  4, dp[1]);
	bus_space_write_4(iot, ioh, offset +  8, dp[2]);
	bus_space_write_4(iot, ioh, offset + 12, dp[3]);
}

/*
 * Must be called at splnet() or higher
 */
static __inline void
glxsb_aes(struct glxsb_softc *sc, uint32_t control, void *src, void *dst,
    void *key, int len, void *iv)
{
	uint32_t intr;
	int i;
	extern paddr_t vtophys(vaddr_t);
	static int re_check = 0;

	if (re_check) {
		panic("glxsb: call again :(\n");
	} else {
		re_check = 1;
	}

	if (len & 0xF) {
		printf("glxsb: len must be a multiple of 16 (not %d)\n", len);
		re_check = 0;
		return;
	}

	/* Set the source */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_SOURCE_A,
	    (uint32_t) vtophys((vaddr_t) src));

	/* Set the destination address */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_DEST_A,
	    (uint32_t) vtophys((vaddr_t) dst));

	/* Set the data length */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_LENGTH_A, len);

	/* Set the IV */
	if (iv != NULL) {
		glxsb_bus_space_write_consec_16(sc->sc_iot, sc->sc_ioh,
		    SB_CBC_IV, iv);
		control |= SB_CTL_CBC;
	}

	/* Set the key */
	glxsb_bus_space_write_consec_16(sc->sc_iot, sc->sc_ioh, SB_WKEY, key);

	/* Ask the security block to do it */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_CTL_A,
	    control | SB_CTL_WK | SB_CTL_DC | SB_CTL_SC | SB_CTL_ST);

	/*
	 * Now wait until it is done.
	 *
	 * We do a busy wait: typically the SB completes after 7 or 8
	 * iterations (yet to see more than 9).  Wait up to a hundred
	 * just in case.
	 */
	for (i = 0; i < 100; i++) {
		intr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SB_AES_INT);

		if (intr & SB_AI_AES_A_COMPLETE) {	/* Done */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, SB_AES_INT,
			    intr);

			if (i > sc->maxpolls)	/* XXX */
				sc->maxpolls = i;
			re_check = 0;
			return;
		}
	}

	re_check = 0;
	printf("glxsb: operation failed to complete\n");
}

int
glxsb_crypto_process(struct cryptop *crp)
{
	struct glxsb_softc *sc = glxsb_sc;
	struct glxsb_session *ses;
	struct cryptodesc *crd;
	char *op_buf = NULL;
	char *op_src;			/* Source and dest buffers must */
	char *op_dst;			/* be 16-byte aligned */
	uint8_t op_iv[16];
	int sesn, err = 0;
	uint32_t control;
	int s;

	s = splnet();

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

	sesn = GLXSB_SESSION(crp->crp_sid);
	if (sesn >= sc->sc_nsessions) {
		err = EINVAL;
		goto out;
	}
	ses = &sc->sc_sessions[sesn];

	/*
	 * XXX Check if we can have input == output on Geode LX.
	 * XXX In the meantime, allocate space for two separate 
	 *     (adjacent) buffers
	 */
	op_buf = malloc(crd->crd_len * 2, M_DEVBUF, M_NOWAIT);
	if (op_buf == NULL) {
		err = ENOMEM;
		goto out;
	}
	op_src = op_buf;
	op_dst = op_buf + crd->crd_len;

	if (crd->crd_flags & CRD_F_ENCRYPT) {
		control = SB_CTL_ENC;
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, op_iv, 16);
		else
			bcopy(ses->ses_iv, op_iv, 16);

		if ((crd->crd_flags & CRD_F_IV_PRESENT) == 0) {
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copyback((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, op_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copyback((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, op_iv);
			else
				bcopy(op_iv,
				    crp->crp_buf + crd->crd_inject, 16);
		}
	} else {
		control = SB_CTL_DEC;
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			bcopy(crd->crd_iv, op_iv, 16);
		else {
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, 16, op_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_inject, 16, op_iv);
			else
				bcopy(crp->crp_buf + crd->crd_inject,
				    op_iv, 16);
		}
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m_copydata((struct mbuf *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, op_src);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		cuio_copydata((struct uio *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, op_src);
	else
		bcopy(crp->crp_buf + crd->crd_skip, op_src, crd->crd_len);

	glxsb_aes(sc, control, op_src, op_dst, ses->ses_key,
	    crd->crd_len, op_iv);

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m_copyback((struct mbuf *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, op_dst);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		cuio_copyback((struct uio *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, op_dst);
	else
		bcopy(op_dst, crp->crp_buf + crd->crd_skip, crd->crd_len);

	/* copy out last block for use as next session IV */
	if (crd->crd_flags & CRD_F_ENCRYPT) {
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copydata((struct mbuf *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, ses->ses_iv);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			cuio_copydata((struct uio *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 16, 16, ses->ses_iv);
		else
			bcopy(crp->crp_buf + crd->crd_skip + crd->crd_len - 16,
			    ses->ses_iv, 16);
	}

out:
	if (op_buf != NULL) {
		bzero(op_buf, crd->crd_len * 2);
		free(op_buf, M_DEVBUF);
	}
	crp->crp_etype = err;
	crypto_done(crp);
	splx(s);
	return (err);
}

#endif /* CRYPTO */
