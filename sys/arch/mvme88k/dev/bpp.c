/*	$OpenBSD: bpp.c,v 1.2 2009/02/14 17:41:42 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009  Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies. And
 * I won't mind if you keep the disclaimer below.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* BPP cards live in an A24/D16 world */
#define	__BUS_SPACE_RESTRICT_D16__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <mvme88k/dev/bppvar.h>

int	bpp_csr_command(struct bpp_softc *, uint, bus_addr_t);

void
bpp_attach(struct bpp_softc *sc, bus_space_tag_t iot, bus_space_handle_t ioh)
{
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
}

/*
 * CSR Command Protocol
 *
 * This protocol relies on atomic test-and-set instructions to claim the
 * device for ourselves, using the test-and-set register.
 *
 * However, OpenBSD expects to be the only software talking to the BPP
 * device, so it isn't really necessary to perform atomic operations here
 * (such operations are not provided by the bus_space API anyway).
 *
 * Moreover, some MVME197 boards with early BusSwitch version, can not
 * reliably perform test and set operations (xmem) on non-local memory
 * addresses.  Fun fun fun.
 */

/*
 * Send a command to the board through the (limited) CSR command protocol.
 */
int
bpp_csr_command(struct bpp_softc *sc, uint command, bus_addr_t addr)
{
	uint16_t csr, tas;
#if 1
	uint8_t tas8;
	int tmo, tmo2, stat;
	vaddr_t tasaddr;

	tasaddr = (vaddr_t)bus_space_vaddr(sc->sc_iot, sc->sc_ioh) + CSR_TAS;
#else
	int tmo, stat;
#endif

#if 1
	for (tmo = 1000; tmo != 0; tmo--) {
		/* wait for the BUSY bit to clear */
		for (tmo2 = 1000; tmo2 != 0; tmo2--) {
			csr = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    CSR_CONTROL);
			if (!ISSET(csr, CSR_BUSY))
				break;
			delay(100);
		}
		if (tmo2 == 0)
			return STATUS_ERRNO + EAGAIN;

		/*
		 * Claim the TAS register. Note that, since there is no
		 * 16 bits modifier for xmem, we need to use the byte
		 * version.
		 */
		tas8 = TAS_TAS >> 8;
		__asm__ __volatile__ ("xmem.bu %0, %2, r0" :
		    "+r"(tas8), "+m"(tasaddr) : "r"(tasaddr));

		if (!ISSET(tas8, TAS_TAS >> 8))
			break;

		delay(100);
	}
	if (tmo == 0)
		return STATUS_ERRNO + EAGAIN;
#else
	/* wait for the BUSY bit to clear */
	for (tmo = 1000; tmo != 0; tmo--) {
		csr = bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL);
		if (!ISSET(csr, CSR_BUSY))
			break;
		delay(100);
	}
	if (tmo == 0)
		return STATUS_ERRNO + EAGAIN;

	/*
	 * Claim the TAS register.
	 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_TAS, TAS_TAS);
#endif

	/* write the command information */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CSR_ADDR, addr);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CSR_ADDR_MOD, ADRM_EXT_S_D);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CSR_DATA_WIDTH, MEMT_D32);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_TAS,
	    TAS_TAS | TAS_VALID_COMMAND | command);

	/* signal the board */
	bpp_attention(sc);

	/* wait for the command to complete */
	for (tmo = 1000; tmo != 0; tmo--) {
		tas = bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_TAS);
		if (ISSET(tas, TAS_VALID_STATUS))
			break;
		delay(100);
	}
	if (tmo == 0)
		return STATUS_ERRNO + EAGAIN;

	stat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CSR_STATUS);

	/* release the board */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_TAS,
	    TAS_TAS | TAS_COMMAND_COMPLETE |
	    bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_TAS));
	bpp_attention(sc);

	/* wait for the TAS bit to clear */
	for (tmo = 1000; tmo != 0; tmo--) {
		tas = bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_TAS);
		if (!ISSET(tas, TAS_TAS))
			break;
		delay(100);
	}
#ifdef BPP_DEBUG
	if (tmo == 0)
		printf("warning: TAS failed to clear\n");
#endif

	return (stat);
}

/*
 * Interrupt the board.
 */
void
bpp_attention(struct bpp_softc *sc)
{
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL,
	    bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL) |
	    CSR_ATTENTION);
}

/*
 * Reset the board.  This will take several seconds.
 */
int
bpp_reset(struct bpp_softc *sc)
{
	uint16_t csr;
	int tmo;

	/* reset the board */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL, CSR_RESET);
	delay(1000);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL, 0);

	/* wait for BUSY to clear in 10s */
	for (tmo = 10000; tmo != 0; tmo--) {
		csr = bus_space_read_2(sc->sc_iot, sc->sc_ioh, CSR_CONTROL);
		if (!ISSET(csr, CSR_BUSY)) {
			return 0;
		}

		delay(1000);
	}

	return ENXIO;
}

/*
 * CSR operations
 */

int
bpp_create_channel(struct bpp_softc *sc, struct bpp_chan *chan, int pri,
    int ipl, int vec)
{
	struct bpp_channel *channel = chan->ch;
	struct bpp_envelope *envcmd, *envsts;

	/*
	 * Setup the initial null envelopes.
	 */

	envcmd = bpp_get_envelope(sc);
	if (envcmd == NULL)
		return STATUS_ERRNO + ENOMEM;
	envsts = bpp_get_envelope(sc);
	if (envsts == NULL)
		return STATUS_ERRNO + ENOMEM;

	(*sc->bpp_env_sync)(sc, envcmd, BUS_DMASYNC_PREWRITE);
	(*sc->bpp_env_sync)(sc, envsts, BUS_DMASYNC_PREWRITE);

	/*
	 * Setup channel header.
	 */

	memset(channel, 0, sizeof (*channel));
	channel->ch_cmd_head = channel->ch_cmd_tail =
	    htobe32((*sc->bpp_env_pa)(sc, envcmd));
	channel->ch_status_head = channel->ch_status_tail =
	    htobe32((*sc->bpp_env_pa)(sc, envsts));
	channel->ch_ipl = ipl;
	channel->ch_ivec = vec;
	channel->ch_priority = pri;
	channel->ch_addrmod = ADRM_EXT_S_D;
	channel->ch_buswidth = MEMT_D32;

	(*sc->bpp_chan_sync)(sc, channel, BUS_DMASYNC_PREWRITE);

	chan->envcmd = envcmd;
	chan->envsts = envsts;

	return bpp_csr_command(sc, COMMAND_CHANNEL_CREATE,
	    (*sc->bpp_chan_pa)(sc, channel));
}
	
/*
 * BPP operations and envelope management
 */

/*
 * Initialize the free list as a set of contiguous envelopes.
 */
void
bpp_initialize_envelopes(struct bpp_softc *sc, struct bpp_envelope *env,
    uint cnt)
{
	sc->sc_env_free = NULL;
	while (cnt-- != 0)
		bpp_put_envelope(sc, env++);
}

/*
 * Get a new envelope from the free list.
 */
struct bpp_envelope *
bpp_get_envelope(struct bpp_softc *sc)
{
	struct bpp_envelope *env;

	env = sc->sc_env_free;
	if (env != NULL) {
		sc->sc_env_free = (struct bpp_envelope *)env->env_link;
		memset(env, 0, sizeof(*env));
		/* env->env_valid = ENVELOPE_INVALID; */
	}

	return env;
}

/*
 * Put an envelope into the free list.
 */
void
bpp_put_envelope(struct bpp_softc *sc, struct bpp_envelope *env)
{
	env->env_link = (uint32_t)sc->sc_env_free;
	sc->sc_env_free = env;
}

/*
 * Send a command packet, replacing the NULL envelope with the given one,
 * and enqueue it to the command pipe.
 */
void
bpp_queue_envelope(struct bpp_softc *sc, struct bpp_chan *chan,
    struct bpp_envelope *tail, paddr_t cmdpa)
{
	struct bpp_envelope *env;
	struct bpp_channel *channel;
	paddr_t tailpa;

	/* tail assumed to be unmodified since bpp_get_envelope(),
	   no need to memset() */
	(*sc->bpp_env_sync)(sc, tail, BUS_DMASYNC_PREWRITE);
	tailpa = (*sc->bpp_env_pa)(sc, tail);

	/*
	 * Update the existing NULL envelope with data.
	 */

	env = chan->envcmd;
	env->env_link = htobe32(tailpa);
	env->env_pkt = htobe32(cmdpa);
	(*sc->bpp_env_sync)(sc, env, BUS_DMASYNC_PREWRITE);	/* paranoia */
	env->env_valid = ENVELOPE_VALID;
	(*sc->bpp_env_sync)(sc, env, BUS_DMASYNC_PREWRITE);

	/*
	 * Update the channel's command tail pointer to point to the
	 * new envelope.
	 */

	channel = chan->ch;
	channel->ch_cmd_tail = htobe32(tailpa);
	/* really necessary? */
	(*sc->bpp_chan_sync)(sc, channel, BUS_DMASYNC_PREWRITE);

	chan->envcmd = tail;

	bpp_attention(sc);
}

/*
 * Dequeue an envelope from the status pipe, and return the command it
 * was carrying. The envelope itself is returned to the free list.
 */
int
bpp_dequeue_envelope(struct bpp_softc *sc, struct bpp_chan *chan,
    paddr_t *cmdpa)
{
	struct bpp_envelope *env;
	struct bpp_channel *channel;
	paddr_t headpa;

	/*
	 * Look at the head envelope.
	 */

	env = chan->envsts;
	(*sc->bpp_env_sync)(sc, env, BUS_DMASYNC_POSTREAD);

	if (env->env_valid == ENVELOPE_INVALID)
		return EAGAIN;

	/*
	 * Dequeue the envelope, compute the new head envelope index, and
	 * finally release the envelope.
	 */

	headpa = betoh32(env->env_link);
	if (cmdpa != NULL)
		*cmdpa = betoh32(env->env_pkt);
	chan->envsts = (*sc->bpp_env_va)(sc, headpa);

	channel = chan->ch;
	channel->ch_status_head = htobe32(headpa);
	/* really necessary? */
	(*sc->bpp_chan_sync)(sc, channel, BUS_DMASYNC_PREWRITE);

	bpp_put_envelope(sc, env);

	return 0;
}
