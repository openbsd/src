/*	$OpenBSD: aeon.c,v 1.1 1999/02/19 02:52:19 deraadt Exp $	*/

/*
 * Invertex AEON driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 *
 * This driver is based on a previous driver by Invertex, for which they
 * requested:  Please send any comments, feedback, bug-fixes, or feature
 * requests to software@invertex.com.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/aeonvar.h>
#include <dev/pci/aeonreg.h>

#define AEON_DEBUG

/*
 * Prototypes and count for the pci_device structure
 */
int aeon_probe		__P((struct device *, void *, void *));
void aeon_attach	__P((struct device *, struct device *, void *));

void aeon_reset_board	__P((struct aeon_softc *));
int aeon_enable_crypto	__P((struct aeon_softc *));
void aeon_init_dma	__P((struct aeon_softc *));
void aeon_init_pci_registers __P((struct aeon_softc *));
int aeon_ram_setting_okay __P((struct aeon_softc *));
int aeon_intr		__P((void *));
u_int32_t aeon_write_command __P((const struct aeon_command_buf_data *,
    u_int8_t *));
int aeon_build_command __P((const struct aeon_command * cmd,
    struct aeon_command_buf_data *));
void aeon_intr_process_ring __P((struct aeon_softc *, struct aeon_dma *));

struct cfattach aeon_ca = {
	sizeof(struct aeon_softc), aeon_probe, aeon_attach,
};

struct cfdriver aeon_cd = {
	0, "aeon", DV_DULL
};

/*
 * Used for round robin crypto requests
 */
int aeon_num_devices = 0;
struct aeon_softc *aeon_devices[AEON_MAX_DEVICES];


int
aeon_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INVERTEX &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INVERTEX_AEON)
		return (1);
	return (0);
}

/*
 * Purpose:  One time initialization for the device performed at bootup.
 */
void 
aeon_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct aeon_softc *sc = (struct aeon_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t iobase;
	bus_size_t iosize;
	u_int32_t cmd;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (pci_mem_find(pc, pa->pa_tag, PCI_BASE_ADDRESS_0, &iobase, &iosize,
	    NULL)){
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh0)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_st0 = pa->pa_memt;

	if (pci_mem_find(pc, pa->pa_tag, PCI_BASE_ADDRESS_1, &iobase, &iosize,
	    NULL)){
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh1)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_st1 = pa->pa_memt;
	printf(" mem %x %x", sc->sc_sh0, sc->sc_sh1);

	sc->sc_dma = (struct aeon_dma *)vm_page_alloc_contig(sizeof(*sc->sc_dma),
	    0x100000, 0xffffffff, PAGE_SIZE);
	bzero(sc->sc_dma, sizeof(*sc->sc_dma));

	aeon_reset_board(sc);

	if (aeon_enable_crypto(sc) != 0) {
		printf("%s: crypto enabling failed\n",
		    sc->sc_dv.dv_xname);
		return;
	}

	aeon_init_dma(sc);
	aeon_init_pci_registers(sc);

	if (aeon_ram_setting_okay(sc) != 0)
		sc->is_dram_model = 1;

	/*
	 * Reinitialize again, since the DRAM/SRAM detection shifted our ring
	 * pointers and may have changed the value we send to the RAM Config
	 * Register.
	 */
	aeon_reset_board(sc);
	aeon_init_dma(sc);
	aeon_init_pci_registers(sc);

	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, aeon_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt\n");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	aeon_devices[aeon_num_devices] = sc;
	aeon_num_devices++;

	printf(": %s\n", intrstr);
}

/*
 * Purpose:  Resets the board.  Values in the regesters are left as is
 *           from the reset (i.e. initial values are assigned elsewhere).
 */
void
aeon_reset_board(sc)
	struct aeon_softc *sc;
{
	/*
	 * Set polling in the DMA configuration register to zero.  0x7 avoids
	 * resetting the board and zeros out the other fields.
	 */
	WRITE_REG_1(sc, AEON_DMA_CFG, AEON_DMA_CFG_NOBOARDRESET |
	    AEON_DMA_CFG_NODMARESET | AEON_DMA_CFG_NEED);

	/*
	 * Now that polling has been disabled, we have to wait 1 ms
	 * before resetting the board.
	 */
	DELAY(1000);

	/* Reset the board.  We do this by writing zeros to the DMA reset
	 * field, the BRD reset field, and the manditory 1 at position 2.
	 * Every other field is set to zero.
	 */
	WRITE_REG_1(sc, AEON_DMA_CFG, AEON_DMA_CFG_NEED);

	/*
	 * Wait another millisecond for the board to reset.
	 */
	DELAY(1000);

	/*
	 * Turn off the reset!  (No joke.)
	 */
	WRITE_REG_1(sc, AEON_DMA_CFG, AEON_DMA_CFG_NOBOARDRESET |
	    AEON_DMA_CFG_NODMARESET | AEON_DMA_CFG_NEED);
}

/*
 * Purpose:   Checks to see if crypto is already enabled.  If crypto
 *            isn't enable, "aeon_enable_crypto" is called to enable it.
 *            The check is important, as enabling crypto twice will lock
 *            the board.
 *
 * Returns:   0 value on success, -1 if we were not able to unlock the
 *            cryptographic engine.
 */
int 
aeon_enable_crypto(sc)
	struct aeon_softc *sc;
{
	u_int32_t encryption_level;

	/*
	 * The RAM config register's encrypt level bit needs to be set before
	 * every read performed on the encryption level register.
	 */
	WRITE_REG_0(sc, AEON_RAM_CONFIG,
	    READ_REG_0(sc, AEON_RAM_CONFIG) | (0x1 << 5));

	encryption_level = READ_REG_0(sc, AEON_ENCRYPTION_LEVEL);

	/*
	 * Make sure we don't re-unlock.  Two unlocks kills chip until the
	 * next reboot.
	 */
	if (encryption_level == 0x1020 || encryption_level == 0x1120) {
#ifdef AEON_DEBUG
		printf("%s: Strong Crypto already enabled!\n",
		    sc->sc_dv.dv_xname);
#endif
		return 0;	/* success */
	}

	/**
	 **
	 **   Rest of unlock procedure removed.
	 **
	 **
	 **/

	switch(encryption_level) {
	case 0x3020:
		printf(" no encr/auth");
		break;
	case 0x1020:
		printf(" DES");
		break;
	case 0x1120:
		printf(" FULL");
		break;
	default:
		printf(" disabled");
		break;
	}

	return 0;
}

/*
 *  Purpose:   Give initial values to the registers listed in the
 *             "Register Space" section of the AEON Software Development
 *             reference manual.
 */
void 
aeon_init_pci_registers(sc)
	struct aeon_softc *sc;
{
	u_int32_t ram_config;

	/*
	 *  Write fixed values needed by the Initialization registers
	 */
	WRITE_REG_0(sc, AEON_INIT_1, 0x2);
	WRITE_REG_0(sc, AEON_INIT_2, 0x400);
	WRITE_REG_0(sc, AEON_INIT_3, 0x200);

	/*
	 *  Write all 4 ring address registers
	 */
	WRITE_REG_1(sc, AEON_COMMAND_RING_ADDR,
	    vtophys(sc->sc_dma->command_ring));
	WRITE_REG_1(sc, AEON_SOURCE_RING_ADDR,
	    vtophys(sc->sc_dma->source_ring));
	WRITE_REG_1(sc, AEON_DEST_RING_ADDR,
	    vtophys(sc->sc_dma->dest_ring));
	WRITE_REG_1(sc, AEON_RESULT_RING_ADDR,
	    vtophys(sc->sc_dma->result_ring));

	/*
	 *  Write status register
	 */
	WRITE_REG_1(sc, AEON_STATUS, AEON_INIT_STATUS_REG);

	/*
	 *  Write registers which had thier initial values defined
	 *  elsewhere.  The "Encryption level register" is the only
	 *  documented register not initialized by this routine (it's read
	 *  only).
	 */
	WRITE_REG_1(sc, AEON_INTERRUPT_ENABLE, AEON_INIT_INTERRUPT_ENABLE_REG);

	ram_config = AEON_INIT_RAM_CONFIG_REG
#if BYTE_ORDER == BIG_ENDIAN
	    | (0x1 << 7)
#endif
	    | (sc->is_dram_model << 4);
	WRITE_REG_0(sc, AEON_RAM_CONFIG, ram_config);
	WRITE_REG_0(sc, AEON_EXPAND, AEON_INIT_EXPAND_REG);
	WRITE_REG_1(sc, AEON_DMA_CFG, AEON_INIT_DMA_CONFIG_REG);
}

/*
 *  Purpose:   There are both DRAM and SRAM models of the aeon board.
 *             A bit in the "ram configuration register" needs to be
 *             set according to the model.  The driver will guess one
 *             way or the other -- and then call this routine to verify.
 *  Returns:
 *     0: RAM setting okay
 *    -1: Current RAM setting in error
 */
int 
aeon_ram_setting_okay(sc)
	struct aeon_softc *sc;
{
	aeon_base_command_t write_command = {(0x3 << 13), 0, 8, 0};
	aeon_base_command_t read_command = {(0x2 << 13), 0, 0, 8};
	u_int8_t data[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
	u_int8_t *source_buf, *dest_buf;
	struct aeon_dma *dma = sc->sc_dma;

	const u_int32_t masks = AEON_DESCRIPT_VALID | AEON_DESCRIPT_LAST |
	    AEON_DESCRIPT_MASK_DONE_IRQ;

#if (AEON_DESCRIPT_RING_SIZE < 3)
#error "descriptor ring size too small DRAM/SRAM check"
#endif

	/*
	 *  We steal the 8 bytes needed for both the source and dest buffers
	 *  from the 3rd slot that the DRAM/SRAM test won't use.
	 */
	source_buf = sc->sc_dma->command_bufs[2];
	dest_buf = sc->sc_dma->result_bufs[2];

	/*
	 *  Build write command
	 */
	*(aeon_base_command_t *) sc->sc_dma->command_bufs[0] = write_command;
	bcopy(data, source_buf, sizeof(data));

	dma->source_ring[0].pointer = vtophys(source_buf);
	dma->dest_ring[0].pointer = vtophys(dest_buf);

	dma->command_ring[0].length = 16 | masks;
	dma->source_ring[0].length = 8 | masks;
	dma->dest_ring[0].length = 8 | masks;
	dma->result_ring[0].length = AEON_MAX_RESULT_LENGTH | masks;

	/*
	 *  Let write command execute
	 */
	DELAY(1000);

	if (dma->result_ring[0].length & AEON_DESCRIPT_VALID)
		printf("%s: SRAM/DRAM detection error -- result[0] valid still set\n",
		    sc->sc_dv.dv_xname);

	/*
	 *  Build read command
	 */
	*(aeon_base_command_t *) sc->sc_dma->command_bufs[1] = read_command;

	dma->source_ring[1].pointer = vtophys(source_buf);
	dma->dest_ring[1].pointer = vtophys(dest_buf);

	dma->command_ring[1].length = 16 | masks;
	dma->source_ring[1].length = 8 | masks;
	dma->dest_ring[1].length = 8 | masks;
	dma->result_ring[1].length = AEON_MAX_RESULT_LENGTH | masks;

	/*
	 *  Let read command execute
	 */
	DELAY(1000);

	if (dma->result_ring[1].length & AEON_DESCRIPT_VALID)
		printf("%s: SRAM/DRAM detection error -- result[1] valid still set\n",
		    sc->sc_dv.dv_xname);

	return (memcmp(dest_buf, data, sizeof(data)) == 0) ? 0 : -1;
}

/*
 *  Purpose:   Initialize the descriptor rings.
 */
void 
aeon_init_dma(sc)
	struct aeon_softc *sc;
{
	int i;
	struct aeon_dma *dma = sc->sc_dma;

	/*
	 *  Initialize static pointer values.
	 */
	for (i = 0; i < AEON_DESCRIPT_RING_SIZE; i++) {
		dma->command_ring[i].pointer = vtophys(dma->command_bufs[i]);
		dma->result_ring[i].pointer = vtophys(dma->result_bufs[i]);
	}

	dma->command_ring[AEON_DESCRIPT_RING_SIZE].pointer =
	    vtophys(dma->command_ring);

	dma->source_ring[AEON_DESCRIPT_RING_SIZE].pointer =
	    vtophys(dma->source_ring);

	dma->dest_ring[AEON_DESCRIPT_RING_SIZE].pointer =
	    vtophys(dma->dest_ring);

	dma->result_ring[AEON_DESCRIPT_RING_SIZE].pointer =
	    vtophys(dma->result_ring);
}

/*
 *  Purpose:   Writes out the raw command buffer space.  Returns the
 *             command buffer size.
 */
u_int32_t 
aeon_write_command(
    const struct aeon_command_buf_data * cmd_data,
    u_int8_t * command_buf
)
{
	u_int8_t *command_buf_pos = command_buf;
	const aeon_base_command_t *base_cmd = &cmd_data->base_cmd;
	const aeon_mac_command_t *mac_cmd = &cmd_data->mac_cmd;
	const aeon_crypt_command_t *crypt_cmd = &cmd_data->crypt_cmd;

	int     using_mac = base_cmd->masks & AEON_BASE_CMD_MAC;
	int     using_crypt = base_cmd->masks & AEON_BASE_CMD_CRYPT;

	/*
	 *  Write base command structure
	 */
	*((aeon_base_command_t *) command_buf_pos) = *base_cmd;
	command_buf_pos += sizeof(aeon_base_command_t);

	/*
	 *  Write MAC command structure
	 */
	if (using_mac) {
		*((aeon_mac_command_t *) command_buf_pos) = *mac_cmd;
		command_buf_pos += sizeof(aeon_mac_command_t);
	}
	/*
	 *  Write encryption command structure
	 */
	if (using_crypt) {
		*((aeon_crypt_command_t *) command_buf_pos) = *crypt_cmd;
		command_buf_pos += sizeof(aeon_crypt_command_t);
	}
	/*
	 *  Write MAC key
	 */
	if (mac_cmd->masks & AEON_MAC_CMD_NEW_KEY) {
		bcopy(cmd_data->mac_key, command_buf_pos, AEON_MAC_KEY_LENGTH);
		command_buf_pos += AEON_MAC_KEY_LENGTH;
	}
	/*
	 *  Write crypto key
	 */
	if (crypt_cmd->masks & AEON_CRYPT_CMD_NEW_KEY) {
		u_int32_t alg = crypt_cmd->masks & AEON_CRYPT_CMD_ALG_MASK;
		u_int32_t key_len = (alg == AEON_CRYPT_CMD_ALG_DES) ?
		AEON_DES_KEY_LENGTH : AEON_3DES_KEY_LENGTH;
		bcopy(cmd_data->crypt_key, command_buf_pos, key_len);
		command_buf_pos += key_len;
	}
	/*
	 *  Write crypto iv
	 */
	if (crypt_cmd->masks & AEON_CRYPT_CMD_NEW_IV) {
		bcopy(cmd_data->initial_vector, command_buf_pos, AEON_IV_LENGTH);
		command_buf_pos += AEON_IV_LENGTH;
	}
	/*
	 *  Write 8 bytes of zero's if we're not sending crypt or MAC
	 *  structures
	 */
	if (!(base_cmd->masks & AEON_BASE_CMD_MAC) &&
	    !(base_cmd->masks & AEON_BASE_CMD_CRYPT)) {
		*((u_int32_t *) command_buf_pos) = 0;
		command_buf_pos += 4;
		*((u_int32_t *) command_buf_pos) = 0;
		command_buf_pos += 4;
	}
#if 0
	if ((command_buf_pos - command_buf) > AEON_MAX_COMMAND_LENGTH)
		printf("aeon: Internal Error -- Command buffer overflow.\n");
#endif

	return command_buf_pos - command_buf;

}

/*
 *  Purpose:   Check command input and build up structure to write
 *             the command buffer later.  Returns 0 on success and
 *             -1 if given bad command input was given.
 */
int 
aeon_build_command(
    const struct aeon_command * cmd,
    struct aeon_command_buf_data * cmd_buf_data
)
{
#define AEON_COMMAND_CHECKING

	u_int32_t flags = cmd->flags;
	aeon_base_command_t *base_cmd = &cmd_buf_data->base_cmd;
	aeon_mac_command_t *mac_cmd = &cmd_buf_data->mac_cmd;
	aeon_crypt_command_t *crypt_cmd = &cmd_buf_data->crypt_cmd;
	u_int   mac_length;

#ifdef AEON_COMMAND_CHECKING
	int     dest_diff;
#endif

	bzero(cmd_buf_data, sizeof(struct aeon_command_buf_data));


#ifdef AEON_COMMAND_CHECKING
	if (!(!!(flags & AEON_DECODE) ^ !!(flags & AEON_ENCODE))) {
		printf("aeon: encode/decode setting error\n");
		return -1;
	}
	if ((flags & AEON_CRYPT_DES) && (flags & AEON_CRYPT_3DES)) {
		printf("aeon: Too many crypto algorithms set in command\n");
		return -1;
	}
	if ((flags & AEON_MAC_SHA1) && (flags & AEON_MAC_MD5)) {
		printf("aeon: Too many MAC algorithms set in command\n");
		return -1;
	}
#endif


	/*
	 *  Compute the mac value length -- leave at zero if not MAC'ing
	 */
	mac_length = 0;
	if (AEON_USING_MAC(flags)) {
		mac_length = (flags & AEON_MAC_TRUNC) ? AEON_MAC_TRUNC_LENGTH :
		    ((flags & AEON_MAC_MD5) ? AEON_MD5_LENGTH : AEON_SHA1_LENGTH);
	}
#ifdef AEON_COMMAND_CHECKING
	/*
	 *  Check for valid src/dest buf sizes
	 */

	/*
	 *   XXX XXX  We need to include header counts into all these
	 *            checks!!!!
	 */

	if (cmd->source_length <= mac_length) {
		printf("aeon:  command source buffer has no data\n");
		return -1;
	}
	dest_diff = (flags & AEON_ENCODE) ? mac_length : -mac_length;
	if (cmd->dest_length < cmd->source_length + dest_diff) {
		printf("aeon:  command dest length %u too short -- needed %u\n",
		    cmd->dest_length, cmd->source_length + dest_diff);
		return -1;
	}
#endif


	/**
	 **  Building up base command
	 **
	 **/

	/*
	 *  Set MAC bit
	 */
	if (AEON_USING_MAC(flags))
		base_cmd->masks |= AEON_BASE_CMD_MAC;

	/* Set Encrypt bit */
	if (AEON_USING_CRYPT(flags))
		base_cmd->masks |= AEON_BASE_CMD_CRYPT;

	/*
	 *  Set Decode bit
	 */
	if (flags & AEON_DECODE)
		base_cmd->masks |= AEON_BASE_CMD_DECODE;

	/*
	 *  Set total source and dest counts.  These values are the same as the
	 *  values set in the length field of the source and dest descriptor rings.
	 */
	base_cmd->total_source_count = cmd->source_length;
	base_cmd->total_dest_count = cmd->dest_length;

	/*
	 *  XXX -- We need session number range checking...
	 */
	base_cmd->session_num = cmd->session_num;

	/**
	 **  Building up mac command
	 **
	 **/
	if (AEON_USING_MAC(flags)) {

		/*
		 *  Set the MAC algorithm and trunc setting
		 */
		mac_cmd->masks |= (flags & AEON_MAC_MD5) ?
		    AEON_MAC_CMD_ALG_MD5 : AEON_MAC_CMD_ALG_SHA1;
		if (flags & AEON_MAC_TRUNC)
			mac_cmd->masks |= AEON_MAC_CMD_TRUNC;

		/*
	         *  We always use HMAC mode, assume MAC values are appended to the
	         *  source buffer on decodes and we append them to the dest buffer
	         *  on encodes, and order auth/encryption engines as needed by
	         *  IPSEC
	         */
		mac_cmd->masks |= AEON_MAC_CMD_MODE_HMAC | AEON_MAC_CMD_APPEND |
		    AEON_MAC_CMD_POS_IPSEC;

		/*
	         *  Setup to send new MAC key if needed.
	         */
		if (flags & AEON_MAC_CMD_NEW_KEY) {
			mac_cmd->masks |= AEON_MAC_CMD_NEW_KEY;
			cmd_buf_data->mac_key = cmd->mac_key;
		}
		/*
	         *  Set the mac header skip and source count.
	         */
		mac_cmd->source_count = cmd->source_length - cmd->mac_header_skip;
		if (flags & AEON_DECODE)
			mac_cmd->source_count -= mac_length;
	}
	/**
	 **  Building up crypto command
	 **
	 **/
	if (AEON_USING_CRYPT(flags)) {

		/*
	         *  Set the encryption algorithm bits.
	         */
		crypt_cmd->masks |= (flags & AEON_CRYPT_DES) ?
		    AEON_CRYPT_CMD_ALG_DES : AEON_CRYPT_CMD_ALG_3DES;

		/* We always use CBC mode and send a new IV (as needed by
		 * IPSec). */
		crypt_cmd->masks |= AEON_CRYPT_CMD_MODE_CBC | AEON_CRYPT_CMD_NEW_IV;

		/*
	         *  Setup to send new encrypt key if needed.
	         */
		if (flags & AEON_CRYPT_CMD_NEW_KEY) {
			crypt_cmd->masks |= AEON_CRYPT_CMD_NEW_KEY;
			cmd_buf_data->crypt_key = cmd->crypt_key;
		}
		/*
	         *  Set the encrypt header skip and source count.
	         */
		crypt_cmd->header_skip = cmd->crypt_header_skip;
		crypt_cmd->source_count = cmd->source_length - cmd->crypt_header_skip;
		if (flags & AEON_DECODE)
			crypt_cmd->source_count -= mac_length;


#ifdef AEON_COMMAND_CHECKING
		if (crypt_cmd->source_count % 8 != 0) {
			printf("aeon:  Error -- encryption source %u not a multiple of 8!\n",
			    crypt_cmd->source_count);
			return -1;
		}
#endif
	}
	cmd_buf_data->initial_vector = cmd->initial_vector;


#if 0
	printf("aeon: command parameters"
	    " -- session num %u"
	    " -- base t.s.c: %u"
	    " -- base t.d.c: %u"
	    " -- mac h.s. %u  s.c. %u"
	    " -- crypt h.s. %u  s.c. %u\n",
	    base_cmd->session_num,
	    base_cmd->total_source_count,
	    base_cmd->total_dest_count,
	    mac_cmd->header_skip,
	    mac_cmd->source_count,
	    crypt_cmd->header_skip,
	    crypt_cmd->source_count
	    );
#endif

	return 0;		/* success */
}


/*
 *  Function:  aeon_process_command
 */
int 
aeon_crypto(struct aeon_command * cmd)
{
	u_int32_t command_length;

	u_int32_t local_ring_pos;
	int     err;
	int     oldint;
	static u_int32_t current_device = 0;
	struct aeon_softc *sc;
	struct aeon_dma *dma;
	const u_int32_t masks = AEON_DESCRIPT_VALID | AEON_DESCRIPT_LAST |
	AEON_DESCRIPT_MASK_DONE_IRQ;

	struct aeon_command_buf_data cmd_buf_data;

	if (aeon_build_command(cmd, &cmd_buf_data) != 0)
		return AEON_CRYPTO_BAD_INPUT;

	/*
	 *  Turning off interrupts
	 */
	oldint = splimp();

	/* Pick the aeon board to send the data to.  Right now we use a round
	 * robin approach. */
	sc = aeon_devices[current_device++];
	if (current_device == aeon_num_devices)
		current_device = 0;
	dma = sc->sc_dma;

#if 0
	printf("%s: Entering command"
	    " -- Status Reg 0x%08x"
	    " -- Interrupt Enable Reg 0x%08x"
	    " -- slots in use %u"
	    " -- source length %u"
	    " -- dest length %u\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, AEON_STATUS),
	    READ_REG_1(sc, AEON_INTERRUPT_ENABLE),
	    dma->slots_in_use,
	    cmd->source_length,
	    cmd->dest_length
	    );
#endif


	if (dma->slots_in_use == AEON_DESCRIPT_RING_SIZE) {

		if (cmd->flags & AEON_DMA_FULL_NOBLOCK)
			return AEON_CRYPTO_RINGS_FULL;

		do {
#ifdef AEON_DEBUG
			printf("%s: Waiting for unused ring.\n",
			    sc->sc_dv.dv_xname);
#endif
			/* sleep for minimum timeout */
			tsleep((caddr_t) dma, PZERO, "QFULL", 1);

		} while (dma->slots_in_use == AEON_DESCRIPT_RING_SIZE);
	}
	dma->slots_in_use++;


	if (dma->ring_pos == AEON_DESCRIPT_RING_SIZE) {
		local_ring_pos = 0;
		dma->ring_pos = 1;
	} else {
		local_ring_pos = dma->ring_pos++;
	}


	command_length =
	    aeon_write_command(&cmd_buf_data, dma->command_bufs[local_ring_pos]);

	dma->aeon_commands[local_ring_pos] = cmd;

	/*
	 *  If we wrapped to the begining of the ring, validate the jump
	 *  descriptor.  (Not needed on the very first time command -- but it
	 *  doesn't hurt.)
	 */
	if (local_ring_pos == 0) {
		const u_int32_t jmp_masks = masks | AEON_DESCRIPT_JUMP;

		dma->command_ring[AEON_DESCRIPT_RING_SIZE].length = jmp_masks;
		dma->source_ring[AEON_DESCRIPT_RING_SIZE].length = jmp_masks;
		dma->dest_ring[AEON_DESCRIPT_RING_SIZE].length = jmp_masks;
		dma->result_ring[AEON_DESCRIPT_RING_SIZE].length = jmp_masks;
	}
	/*
	 *  "pointer" values for command and result descriptors are already set
	 */
	dma->command_ring[local_ring_pos].length = command_length | masks;

	dma->source_ring[local_ring_pos].pointer = vtophys(cmd->source_buf);
	dma->source_ring[local_ring_pos].length = cmd->source_length | masks;

	dma->dest_ring[local_ring_pos].pointer = vtophys(cmd->dest_buf);
	dma->dest_ring[local_ring_pos].length = cmd->dest_length | masks;


	/*
	 *  Unlike other descriptors, we don't mask done interrupt from
	 *  result descriptor.
	 */
	dma->result_ring[local_ring_pos].length =
	    AEON_MAX_RESULT_LENGTH | AEON_DESCRIPT_VALID | AEON_DESCRIPT_LAST;

	/*
	 *  We don't worry about missing an interrupt (which a waiting
	 *  on command interrupt salvages us from), unless there is more
	 *  than one command in the queue.
	 */
	if (dma->slots_in_use > 1) {
		WRITE_REG_1(sc, AEON_INTERRUPT_ENABLE,
		    AEON_INTR_ON_RESULT_DONE | AEON_INTR_ON_COMMAND_WAITING);
	}
	/*
	 *  If not given a callback routine, we block until the dest data is
	 *  ready.  (Setting interrupt timeout at 3 seconds.)
	 */
	if (cmd->dest_ready_callback == NULL) {
#if 0
		printf("%s: no callback -- we're sleeping\n",
		    sc->sc_dv.dv_xname);
#endif
		err = tsleep((caddr_t) & dma->result_ring[local_ring_pos], PZERO, "CRYPT",
		    hz * 3);
		if (err != 0)
			printf("%s: timed out waiting for interrupt"
			    " -- tsleep() exited with %d\n",
			    sc->sc_dv.dv_xname, err);
	}
#if 0
	printf("%s: command executed"
	    " -- Status Register 0x%08x"
	    " -- Interrupt Enable Reg 0x%08x\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, AEON_STATUS),
	    READ_REG_1(sc, AEON_INTERRUPT_ENABLE));
#endif

	/*
	 *  Turning interupts back on
	 */
	splx(oldint);

	return 0;		/* success */
}

/*
 *  Part of interrupt handler--cleans out done jobs from rings
 */
void
aeon_intr_process_ring(sc, dma)
	struct aeon_softc *sc;
	struct aeon_dma *dma;
{
	if (dma->slots_in_use > AEON_DESCRIPT_RING_SIZE)
		printf("%s: Internal Error -- ring overflow\n",
		    sc->sc_dv.dv_xname);

	while (dma->slots_in_use > 0) {
		u_int32_t wake_pos = dma->wakeup_ring_pos;
		struct aeon_command *cmd = dma->aeon_commands[wake_pos];

		/*
	         *  If still valid, stop processing
	         */
		if (dma->result_ring[wake_pos].length & AEON_DESCRIPT_VALID)
			break;

		if (AEON_USING_MAC(cmd->flags) && (cmd->flags & AEON_DECODE)) {
			u_int8_t *result_buf = dma->result_bufs[wake_pos];
			cmd->result_status = (result_buf[8] & 0x2) ? AEON_MAC_BAD : 0;
			printf("%s: byte index 8 of result 0x%02x\n",
			    sc->sc_dv.dv_xname, (u_int32_t) result_buf[8]);
		}
		/*
	         *  Position is done, notify producer with wakup or callback
	         */
		if (cmd->dest_ready_callback == NULL) {
			wakeup((caddr_t) &dma->result_ring[wake_pos]);
		} else {
			cmd->dest_ready_callback(cmd);
		}

		if (++dma->wakeup_ring_pos == AEON_DESCRIPT_RING_SIZE)
			dma->wakeup_ring_pos = 0;
		dma->slots_in_use--;
	}

}

/*
 *  Purpose:   Interrupt handler.  The argument passed is the device
 *             structure for the board that generated the interrupt.
 *             XXX:  Remove hardcoded status checking/setting values.
 */
int 
aeon_intr(arg)
	void *arg;
{
	struct aeon_softc *sc = arg;
	struct aeon_dma *dma = sc->sc_dma;
	int r;

#if 0
	printf("%s: Processing Interrupt"
	    " -- Status Reg 0x%08x"
	    " -- Interrupt Enable Reg 0x%08x"
	    " -- slots in use %u\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, AEON_STATUS),
	    READ_REG_1(sc, AEON_INTERRUPT_ENABLE),
	    dma->slots_in_use
	    );
#endif

	if (dma->slots_in_use == 0 && (READ_REG_1(sc, AEON_STATUS) & (1 << 2))) {
		/*
		 * If no slots to process and we received a "waiting on
		 * result" interrupt, we disable the "waiting on result" (by
		 * clearing it).
		 */
		WRITE_REG_1(sc, AEON_INTERRUPT_ENABLE,
		    AEON_INTR_ON_RESULT_DONE);
		r  = 1;
	} else {
		aeon_intr_process_ring(sc, dma);
		r = 1;
	}

#if 0
	printf("%s: exiting interrupt handler -- slots in use %u\n",
	    sc->sc_dv.dv_xname, dma->slots_in_use);
#endif

	/*
	 *  Clear "result done" and "waiting on command ring" flags in status
	 *  register.  If we still have slots to process and we received a
	 *  waiting interrupt, this will interupt us again.
	 */
	WRITE_REG_1(sc, AEON_STATUS, (1 << 20) | (1 << 2));
	return (r);
}

