/* $NetBSD: adwlib.c,v 1.2 1998/09/26 19:54:22 dante Exp $        */

/*
 * Low level routines for the Advanced Systems Inc. SCSI controllers chips
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Ported from:
 */
/*
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *
 * Copyright (c) 1995-1998 Advanced System Products, Inc.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/ic/adwlib.h>
#include <dev/ic/adw.h>
#include <dev/ic/adwmcode.h>


/* Static Functions */

static u_int16_t AdvGetEEPConfig __P((bus_space_tag_t, bus_space_handle_t,
     							ADWEEP_CONFIG *));
static u_int16_t AdvReadEEPWord __P((bus_space_tag_t, bus_space_handle_t,
							int));
static void AdvWaitEEPCmd __P((bus_space_tag_t, bus_space_handle_t));
static void AdvSetEEPConfig __P((bus_space_tag_t, bus_space_handle_t,
					                 ADWEEP_CONFIG *));
static int AdvSendScsiCmd __P((ADW_SOFTC *, ADW_SCSI_REQ_Q *));
static void AdvInquiryHandling __P((ADW_SOFTC *, ADW_SCSI_REQ_Q *));

static void DvcSleepMilliSecond __P((ulong));
static void DvcDelayMicroSecond __P((ulong));


/*
 * EEPROM Configuration.
 *
 * All drivers should use this structure to set the default EEPROM
 * configuration. The BIOS now uses this structure when it is built.
 * Additional structure information can be found in advlib.h where
 * the structure is defined.
 */
static ADWEEP_CONFIG
Default_EEPROM_Config = {
	ADW_EEPROM_BIOS_ENABLE,	/* cfg_msw */
	0x0000,		/* cfg_lsw */
	0xFFFF,		/* disc_enable */
	0xFFFF,		/* wdtr_able */
	0xFFFF,		/* sdtr_able */
	0xFFFF,		/* start_motor */
	0xFFFF,		/* tagqng_able */
	0xFFFF,		/* bios_scan */
	0,		/* scam_tolerant */
	7,		/* adapter_scsi_id */
	0,		/* bios_boot_delay */
	3,		/* scsi_reset_delay */
	0,		/* bios_id_lun */
	0,		/* termination */
	0,		/* reserved1 */
	0xFFEF,		/* bios_ctrl */
	0xFFFF,		/* ultra_able */
	0,		/* reserved2 */
	ASC_DEF_MAX_HOST_QNG,	/* max_host_qng */
	ASC_DEF_MAX_DVC_QNG,	/* max_dvc_qng */
	0,		/* dvc_cntl */
	0,		/* bug_fix */
	0,		/* serial_number_word1 */
	0,		/* serial_number_word2 */
	0,		/* serial_number_word3 */
	0,		/* check_sum */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* oem_name[16] */
	0,		/* dvc_err_code */
	0,		/* adv_err_code */
	0,		/* adv_err_addr */
	0,		/* saved_dvc_err_code */
	0,		/* saved_adv_err_code */
	0,		/* saved_adv_err_addr */
	0		/* num_of_err */
};

/*
 * Initialize the ASC3550.
 *
 * On failure set the ADW_SOFTC field 'err_code' and return ADW_ERROR.
 *
 * For a non-fatal error return a warning code. If there are no warnings
 * then 0 is returned.
 */
int
AdvInitAsc3550Driver(sc)
ADW_SOFTC      *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t       warn_code;
	u_int32_t       sum;
	int             begin_addr;
	int             end_addr;
	int             code_sum;
	int             word;
	int             rql_addr;	/* RISC Queue List address */
	int             i;
	u_int16_t       scsi_cfg1;
	u_int8_t        biosmem[ASC_MC_BIOSLEN];	/* BIOS RISC Memory
							 * 0x40-0x8F */


	warn_code = 0;

	/*
	 * Save the RISC memory BIOS region before writing the microcode.
	 * The BIOS may already be loaded and using its RISC LRAM region
	 * so its region must be saved and restored.
	 *
	 * Note: This code makes the assumption, which is currently true,
	 * that a chip reset does not clear RISC LRAM.
	 */
	for (i = 0; i < ASC_MC_BIOSLEN; i++) {
		ADW_READ_BYTE_LRAM(iot, ioh, ASC_MC_BIOSMEM + i, biosmem[i]);
	}

	/*
	 * Load the Microcode
	 *
	 * Write the microcode image to RISC memory starting at address 0.
	 */
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_RAM_ADDR, 0);
	for (word = 0; word < adv_mcode_size; word += 2) {
		ADW_WRITE_WORD_AUTO_INC_LRAM(iot, ioh,
				       *((u_int16_t *) (&adv_mcode[word])));
	}

	/*
	 * Clear the rest of Condor's Internal RAM (8KB).
	 */
	for (; word < ADW_CONDOR_MEMSIZE; word += 2) {
		ADW_WRITE_WORD_AUTO_INC_LRAM(iot, ioh, 0);
	}

	/*
	 * Verify the microcode checksum.
	 */
	sum = 0;
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_RAM_ADDR, 0);
	for (word = 0; word < adv_mcode_size; word += 2) {
		sum += ADW_READ_WORD_AUTO_INC_LRAM(iot, ioh);
	}

	if (sum != adv_mcode_chksum)
		return ASC_IERR_MCODE_CHKSUM;

	/*
	 * Restore the RISC memory BIOS region.
	 */
	for (i = 0; i < ASC_MC_BIOSLEN; i++) {
		ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_BIOSMEM + i, biosmem[i]);
	}

	/*
	 * Calculate and write the microcode code checksum to the microcode
	 * code checksum location ASC_MC_CODE_CHK_SUM (0x2C).
	 */
	ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_CODE_BEGIN_ADDR, begin_addr);
	ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_CODE_END_ADDR, end_addr);
	code_sum = 0;
	for (word = begin_addr; word < end_addr; word += 2) {
		code_sum += *((u_int16_t *) (&adv_mcode[word]));
	}
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_CODE_CHK_SUM, code_sum);

	/*
	 * Read microcode version and date.
	 */
	ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_VERSION_DATE, sc->cfg.mcode_date);
	ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_VERSION_NUM, sc->cfg.mcode_version);

	/*
	 * Initialize microcode operating variables
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_ADAPTER_SCSI_ID,
			    sc->chip_scsi_id);

	/*
	 * If the PCI Configuration Command Register "Parity Error Response
	 * Control" Bit was clear (0), then set the microcode variable
	 * 'control_flag' CONTROL_FLAG_IGNORE_PERR flag to tell the microcode
	 * to ignore DMA parity errors.
	 */
	if (sc->cfg.control_flag & CONTROL_FLAG_IGNORE_PERR) {
		ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_CONTROL_FLAG, word);
		word |= CONTROL_FLAG_IGNORE_PERR;
		ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_CONTROL_FLAG, word);
	}
	/*
	 * Set default microcode operating variables for WDTR, SDTR, and
	 * command tag queuing based on the EEPROM configuration values.
	 *
	 * These ADW_DVC_VAR fields and the microcode variables will be
	 * changed in AdvInquiryHandling() if it is found a device is
	 * incapable of a particular feature.
	 */

	/*
	 * Set the microcode ULTRA target mask from EEPROM value. The
	 * SDTR target mask overrides the ULTRA target mask in the
	 * microcode so it is safe to set this value without determining
	 * whether the device supports SDTR.
	 *
	 * Note: There is no way to know whether a device supports ULTRA
	 * speed without attempting a SDTR ULTRA speed negotiation with
	 * the device. The device will reject the speed if it does not
	 * support it by responding with an SDTR message containing a
	 * slower speed.
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_ULTRA_ABLE, sc->ultra_able);
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_DISC_ENABLE, sc->cfg.disc_enable);


	/*
	 * Set SCSI_CFG0 Microcode Default Value.
	 *
	 * The microcode will set the SCSI_CFG0 register using this value
	 * after it is started below.
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_DEFAULT_SCSI_CFG0,
	ADW_PARITY_EN | ADW_SEL_TMO_LONG | ADW_OUR_ID_EN | sc->chip_scsi_id);

	/*
	 * Determine SCSI_CFG1 Microcode Default Value.
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */

	/* Read current SCSI_CFG1 Register value. */
	scsi_cfg1 = ADW_READ_WORD_REGISTER(iot, ioh, IOPW_SCSI_CFG1);

	/*
	 * If all three connectors are in use, return an error.
	 */
	if ((scsi_cfg1 & CABLE_ILLEGAL_A) == 0 ||
	    (scsi_cfg1 & CABLE_ILLEGAL_B) == 0) {
		return ASC_IERR_ILLEGAL_CONNECTION;
	}
	/*
	 * If the internal narrow cable is reversed all of the SCSI_CTRL
	 * register signals will be set. Check for and return an error if
	 * this condition is found.
	 */
	if ((ADW_READ_WORD_REGISTER(iot, ioh, IOPW_SCSI_CTRL) & 0x3F07) ==
			0x3F07) {
	
		return ASC_IERR_REVERSED_CABLE;
	}

	/*
	 * If this is a differential board and a single-ended device
	 * is attached to one of the connectors, return an error.
	 */
	if ((scsi_cfg1 & ADW_DIFF_MODE) && (scsi_cfg1 & ADW_DIFF_SENSE) == 0)
		return ASC_IERR_SINGLE_END_DEVICE;

	/*
	 * If automatic termination control is enabled, then set the
	 * termination value based on a table listed in advlib.h.
	 *
	 * If manual termination was specified with an EEPROM setting
	 * then 'termination' was set-up in AdvInitFromEEP() and
	 * is ready to be 'ored' into SCSI_CFG1.
	 */
	if (sc->cfg.termination == 0) {
		/*
		 * The software always controls termination by setting
		 * ADW_TERM_CTL_SEL.
		 * If ADW_TERM_CTL_SEL were set to 0, the hardware would
		 * set termination.
		 */
		sc->cfg.termination |= ADW_TERM_CTL_SEL;

		switch (scsi_cfg1 & ADW_CABLE_DETECT) {
			/* ADW_TERM_CTL_H: on, ADW_TERM_CTL_L: on */
		case 0x3:
		case 0x7:
		case 0xB:
		case 0xD:
		case 0xE:
		case 0xF:
			sc->cfg.termination |= (ADW_TERM_CTL_H |
					ADW_TERM_CTL_L);
			break;

			/* ADW_TERM_CTL_H: on, ADW_TERM_CTL_L: off */
		case 0x1:
		case 0x5:
		case 0x9:
		case 0xA:
		case 0xC:
			sc->cfg.termination |= ADW_TERM_CTL_H;
			break;

			/* ADW_TERM_CTL_H: off, ADW_TERM_CTL_L: off */
		case 0x2:
		case 0x6:
			break;
		}
	}
	/*
	 * Clear any set ADW_TERM_CTL_H and ADW_TERM_CTL_L bits.
	 */
	scsi_cfg1 &= ~ADW_TERM_CTL;

	/*
	 * Invert the ADW_TERM_CTL_H and ADW_TERM_CTL_L bits and then
	 * set 'scsi_cfg1'. The ADW_TERM_POL bit does not need to be
	 * referenced, because the hardware internally inverts
	 * the Termination High and Low bits if ADW_TERM_POL is set.
	 */
	scsi_cfg1 |= (ADW_TERM_CTL_SEL | (~sc->cfg.termination & ADW_TERM_CTL));

	/*
	 * Set SCSI_CFG1 Microcode Default Value
	 *
	 * Set filter value and possibly modified termination control
	 * bits in the Microcode SCSI_CFG1 Register Value.
	 *
	 * The microcode will set the SCSI_CFG1 register using this value
	 * after it is started below.
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_DEFAULT_SCSI_CFG1,
			    ADW_FLTR_11_TO_20NS | scsi_cfg1);

	/*
	 * Set SEL_MASK Microcode Default Value
	 *
	 * The microcode will set the SEL_MASK register using this value
	 * after it is started below.
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_DEFAULT_SEL_MASK,
			    ADW_TID_TO_TIDMASK(sc->chip_scsi_id));

	/*
	 * Link all the RISC Queue Lists together in a doubly-linked
	 * NULL terminated list.
	 *
	 * Skip the NULL (0) queue which is not used.
	 */
	for (i = 1, rql_addr = ASC_MC_RISC_Q_LIST_BASE+ASC_MC_RISC_Q_LIST_SIZE;
	     i < ASC_MC_RISC_Q_TOTAL_CNT;
	     i++, rql_addr += ASC_MC_RISC_Q_LIST_SIZE) {
		/*
		 * Set the current RISC Queue List's RQL_FWD and
		 * RQL_BWD pointers in a one word write and set
		 * the state (RQL_STATE) to free.
		 */
		ADW_WRITE_WORD_LRAM(iot, ioh, rql_addr,
				((i + 1) + ((i - 1) << 8)));
		ADW_WRITE_BYTE_LRAM(iot, ioh, rql_addr + RQL_STATE,
				ASC_MC_QS_FREE);
	}

	/*
	 * Set the Host and RISC Queue List pointers.
	 *
	 * Both sets of pointers are initialized with the same values:
	 * ASC_MC_RISC_Q_FIRST(0x01) and ASC_MC_RISC_Q_LAST (0xFF).
	 */
	ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_READY,
			ASC_MC_RISC_Q_FIRST);
	ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_DONE,
			ASC_MC_RISC_Q_LAST);

	ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_RISC_NEXT_READY,
			ASC_MC_RISC_Q_FIRST);
	ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_RISC_NEXT_DONE,
			ASC_MC_RISC_Q_LAST);

	/*
	 * Finally, set up the last RISC Queue List (255) with
	 * a NULL forward pointer.
	 */
	ADW_WRITE_WORD_LRAM(iot, ioh, rql_addr,
			(ASC_MC_NULL_Q + ((i - 1) << 8)));
	ADW_WRITE_BYTE_LRAM(iot, ioh, rql_addr + RQL_STATE, ASC_MC_QS_FREE);

	ADW_WRITE_BYTE_REGISTER(iot, ioh, IOPB_INTR_ENABLES,
		 (ADW_INTR_ENABLE_HOST_INTR | ADW_INTR_ENABLE_GLOBAL_INTR));

	ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_CODE_BEGIN_ADDR, word);
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_PC, word);

	/* finally, finally, gentlemen, start your engine */
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_RISC_CSR, ADW_RISC_CSR_RUN);

	return warn_code;
}

/*
 * Read the board's EEPROM configuration. Set fields in ADW_SOFTC and
 * ADW_DVC_CFG based on the EEPROM settings. The chip is stopped while
 * all of this is done.
 *
 * On failure set the ADW_DVC_VAR field 'err_code' and return ADW_ERROR.
 *
 * For a non-fatal error return a warning code. If there are no warnings
 * then 0 is returned.
 *
 * Note: Chip is stopped on entry.
 */
int
AdvInitFromEEP(sc)
	ADW_SOFTC      *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t       warn_code;
	ADWEEP_CONFIG   eep_config;
	int             eep_chksum, i;


	warn_code = 0;

	/*
	 * Read the board's EEPROM configuration.
	 *
	 * Set default values if a bad checksum is found.
	 */
	eep_chksum = AdvGetEEPConfig(iot, ioh, &eep_config);

	if (eep_chksum != eep_config.check_sum) {
		warn_code |= ASC_WARN_EEPROM_CHKSUM;

		/*
		 * Set EEPROM default values.
		 */
		for (i = 0; i < sizeof(ADWEEP_CONFIG); i++) {
			*((u_int8_t *) & eep_config + i) =
				*((u_int8_t *) & Default_EEPROM_Config + i);
		}

		/*
		 * Assume the 6 byte board serial number that was read
		 * from EEPROM is correct even if the EEPROM checksum
		 * failed.
		 */
		eep_config.serial_number_word3 =
			AdvReadEEPWord(iot, ioh, ASC_EEP_DVC_CFG_END - 1);
		eep_config.serial_number_word2 =
			AdvReadEEPWord(iot, ioh, ASC_EEP_DVC_CFG_END - 2);
		eep_config.serial_number_word1 =
			AdvReadEEPWord(iot, ioh, ASC_EEP_DVC_CFG_END - 3);
		AdvSetEEPConfig(iot, ioh, &eep_config);
	}
	/*
	 * Set ADW_DVC_VAR and ADW_DVC_CFG variables from the
	 * EEPROM configuration that was read.
	 *
	 * This is the mapping of EEPROM fields to Adv Library fields.
	 */
	sc->wdtr_able = eep_config.wdtr_able;
	sc->sdtr_able = eep_config.sdtr_able;
	sc->ultra_able = eep_config.ultra_able;
	sc->tagqng_able = eep_config.tagqng_able;
	sc->cfg.disc_enable = eep_config.disc_enable;
	sc->max_host_qng = eep_config.max_host_qng;
	sc->max_dvc_qng = eep_config.max_dvc_qng;
	sc->chip_scsi_id = (eep_config.adapter_scsi_id & ADW_MAX_TID);
	sc->start_motor = eep_config.start_motor;
	sc->scsi_reset_wait = eep_config.scsi_reset_delay;
	sc->cfg.bios_boot_wait = eep_config.bios_boot_delay;
	sc->bios_ctrl = eep_config.bios_ctrl;
	sc->no_scam = eep_config.scam_tolerant;
	sc->cfg.serial1 = eep_config.serial_number_word1;
	sc->cfg.serial2 = eep_config.serial_number_word2;
	sc->cfg.serial3 = eep_config.serial_number_word3;

	/*
	 * Set the host maximum queuing (max. 253, min. 16) and the per device
	 * maximum queuing (max. 63, min. 4).
	 */
	if (eep_config.max_host_qng > ASC_DEF_MAX_HOST_QNG) {
		eep_config.max_host_qng = ASC_DEF_MAX_HOST_QNG;
	} else if (eep_config.max_host_qng < ASC_DEF_MIN_HOST_QNG) {
		/* If the value is zero, assume it is uninitialized. */
		if (eep_config.max_host_qng == 0) {
			eep_config.max_host_qng = ASC_DEF_MAX_HOST_QNG;
		} else {
			eep_config.max_host_qng = ASC_DEF_MIN_HOST_QNG;
		}
	}
	if (eep_config.max_dvc_qng > ASC_DEF_MAX_DVC_QNG) {
		eep_config.max_dvc_qng = ASC_DEF_MAX_DVC_QNG;
	} else if (eep_config.max_dvc_qng < ASC_DEF_MIN_DVC_QNG) {
		/* If the value is zero, assume it is uninitialized. */
		if (eep_config.max_dvc_qng == 0) {
			eep_config.max_dvc_qng = ASC_DEF_MAX_DVC_QNG;
		} else {
			eep_config.max_dvc_qng = ASC_DEF_MIN_DVC_QNG;
		}
	}
	/*
	 * If 'max_dvc_qng' is greater than 'max_host_qng', then
	 * set 'max_dvc_qng' to 'max_host_qng'.
	 */
	if (eep_config.max_dvc_qng > eep_config.max_host_qng) {
		eep_config.max_dvc_qng = eep_config.max_host_qng;
	}
	/*
	 * Set ADW_DVC_VAR 'max_host_qng' and ADW_DVC_CFG 'max_dvc_qng'
	 * values based on possibly adjusted EEPROM values.
	 */
	sc->max_host_qng = eep_config.max_host_qng;
	sc->max_dvc_qng = eep_config.max_dvc_qng;


	/*
	 * If the EEPROM 'termination' field is set to automatic (0), then set
	 * the ADW_DVC_CFG 'termination' field to automatic also.
	 *
	 * If the termination is specified with a non-zero 'termination'
	 * value check that a legal value is set and set the ADW_DVC_CFG
	 * 'termination' field appropriately.
	 */
	if (eep_config.termination == 0) {
		sc->cfg.termination = 0;	/* auto termination */
	} else {
		/* Enable manual control with low off / high off. */
		if (eep_config.termination == 1) {
			sc->cfg.termination = ADW_TERM_CTL_SEL;

			/* Enable manual control with low off / high on. */
		} else if (eep_config.termination == 2) {
			sc->cfg.termination = ADW_TERM_CTL_SEL | ADW_TERM_CTL_H;

			/* Enable manual control with low on / high on. */
		} else if (eep_config.termination == 3) {
			sc->cfg.termination = ADW_TERM_CTL_SEL |
					ADW_TERM_CTL_H | ADW_TERM_CTL_L;
		} else {
			/*
			 * The EEPROM 'termination' field contains a bad value.
			 * Use automatic termination instead.
			 */
			sc->cfg.termination = 0;
			warn_code |= ASC_WARN_EEPROM_TERMINATION;
		}
	}

	return warn_code;
}

/*
 * Read EEPROM configuration into the specified buffer.
 *
 * Return a checksum based on the EEPROM configuration read.
 */
static          u_int16_t
AdvGetEEPConfig(iot, ioh, cfg_buf)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	ADWEEP_CONFIG  *cfg_buf;
{
	u_int16_t       wval, chksum;
	u_int16_t      *wbuf;
	int             eep_addr;

	wbuf = (u_int16_t *) cfg_buf;
	chksum = 0;

	for (eep_addr = ASC_EEP_DVC_CFG_BEGIN;
	     eep_addr < ASC_EEP_DVC_CFG_END;
	     eep_addr++, wbuf++) {
		wval = AdvReadEEPWord(iot, ioh, eep_addr);
		chksum += wval;
		*wbuf = wval;
	}
	*wbuf = AdvReadEEPWord(iot, ioh, eep_addr);
	wbuf++;
	for (eep_addr = ASC_EEP_DVC_CTL_BEGIN;
	     eep_addr < ASC_EEP_MAX_WORD_ADDR;
	     eep_addr++, wbuf++) {
		*wbuf = AdvReadEEPWord(iot, ioh, eep_addr);
	}
	return chksum;
}

/*
 * Read the EEPROM from specified location
 */
static          u_int16_t
AdvReadEEPWord(iot, ioh, eep_word_addr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int             eep_word_addr;
{
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_CMD,
				ASC_EEP_CMD_READ | eep_word_addr);
	AdvWaitEEPCmd(iot, iot);
	return ADW_READ_WORD_REGISTER(iot, ioh, IOPW_EE_DATA);
}

/*
 * Wait for EEPROM command to complete
 */
static void
AdvWaitEEPCmd(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	DvcSleepMilliSecond(1);

	for (;;) {
		if (ADW_READ_WORD_REGISTER(iot, ioh, IOPW_EE_CMD) &
		    ASC_EEP_CMD_DONE) {
			break;
		}
		DvcSleepMilliSecond(1);
	}

	return;
}

/*
 * Write the EEPROM from 'cfg_buf'.
 */
static void
AdvSetEEPConfig(iot, ioh, cfg_buf)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	ADWEEP_CONFIG  *cfg_buf;
{
	u_int16_t      *wbuf;
	u_int16_t       addr, chksum;

	wbuf = (u_int16_t *) cfg_buf;
	chksum = 0;

	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_CMD, ASC_EEP_CMD_WRITE_ABLE);
	AdvWaitEEPCmd(iot, ioh);

	/*
	 * Write EEPROM from word 0 to word 15
	 */
	for (addr = ASC_EEP_DVC_CFG_BEGIN;
	     addr < ASC_EEP_DVC_CFG_END; addr++, wbuf++) {
		chksum += *wbuf;
		ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_DATA, *wbuf);
		ADW_WRITE_WORD_REGISTER(iot, ioh,
				     IOPW_EE_CMD, ASC_EEP_CMD_WRITE | addr);
		AdvWaitEEPCmd(iot, ioh);
		DvcSleepMilliSecond(ASC_EEP_DELAY_MS);
	}

	/*
	 * Write EEPROM checksum at word 18
	 */
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_DATA, chksum);
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_CMD,
				ASC_EEP_CMD_WRITE | addr);
	AdvWaitEEPCmd(iot, ioh);
	wbuf++;			/* skip over check_sum */

	/*
	 * Write EEPROM OEM name at words 19 to 26
	 */
	for (addr = ASC_EEP_DVC_CTL_BEGIN;
	     addr < ASC_EEP_MAX_WORD_ADDR; addr++, wbuf++) {
		ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_DATA, *wbuf);
		ADW_WRITE_WORD_REGISTER(iot, ioh,
				     IOPW_EE_CMD, ASC_EEP_CMD_WRITE | addr);
		AdvWaitEEPCmd(iot, ioh);
	}
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_EE_CMD,
				ASC_EEP_CMD_WRITE_DISABLE);
	AdvWaitEEPCmd(iot, ioh);
	return;
}

/*
 * This function resets the chip and SCSI bus
 *
 * It is up to the caller to add a delay to let the bus settle after
 * calling this function.
 *
 * The SCSI_CFG0, SCSI_CFG1, and MEM_CFG registers are set-up in
 * AdvInitAsc3550Driver(). Here when doing a write to one of these
 * registers read first and then write.
 *
 * Note: A SCSI Bus Reset can not be done until after the EEPROM
 * configuration is read to determine whether SCSI Bus Resets
 * should be performed.
 */
void
AdvResetChip(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_int16_t       word;
	u_int8_t        byte;


	/*
	 * Reset Chip.
	 */
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_CTRL_REG,
			ADW_CTRL_REG_CMD_RESET);
	DvcSleepMilliSecond(100);
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_CTRL_REG,
			ADW_CTRL_REG_CMD_WR_IO_REG);

	/*
	 * Initialize Chip registers.
	 *
	 * Note: Don't remove the use of a temporary variable in the following
	 * code, otherwise the Microsoft C compiler will turn the following
	 * lines into a no-op.
	 */
	byte = ADW_READ_BYTE_REGISTER(iot, ioh, IOPB_MEM_CFG);
	byte |= RAM_SZ_8KB;
	ADW_WRITE_BYTE_REGISTER(iot, ioh, IOPB_MEM_CFG, byte);

	word = ADW_READ_WORD_REGISTER(iot, ioh, IOPW_SCSI_CFG1);
	word &= ~BIG_ENDIAN;
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_SCSI_CFG1, word);

	/*
	 * Setting the START_CTL_EMFU 3:2 bits sets a FIFO threshold
	 * of 128 bytes. This register is only accessible to the host.
	 */
	ADW_WRITE_BYTE_REGISTER(iot, ioh, IOPB_DMA_CFG0,
				START_CTL_EMFU | READ_CMD_MRM);
}

/*
 * Description:
 *      Send a SCSI request to the ASC3550 chip
 *
 * If there is no SG list for the request, set 'sg_entry_cnt' to 0.
 *
 * If 'sg_real_addr' is non-zero on entry, AscGetSGList() will not be
 * called. It is assumed the caller has already initialized 'sg_real_addr'.
 *
 * Return:
 *      ADW_SUCCESS(1) - the request is in the mailbox
 *      ADW_BUSY(0) - total request count > 253, try later
 *      ADW_ERROR(-1) - invalid scsi request Q
 */
int
AdvExeScsiQueue(sc, scsiq)
	ADW_SOFTC      *sc;
	ADW_SCSI_REQ_Q *scsiq;
{
	return AdvSendScsiCmd(sc, scsiq);
}

/*
 * Reset SCSI Bus and purge all outstanding requests.
 *
 * Return Value:
 *      ADW_TRUE(1) - All requests are purged and SCSI Bus is reset.
 *
 * Note: Should always return ADW_TRUE.
 */
int
AdvResetCCB(sc)
	ADW_SOFTC      *sc;
{
	int             status;

	status = AdvSendIdleCmd(sc, (u_int16_t) IDLE_CMD_SCSI_RESET, 0L, 0);

	AdvResetSCSIBus(sc);

	return status;
}

/*
 * Reset SCSI Bus and delay.
 */
void
AdvResetSCSIBus(sc)
	ADW_SOFTC      *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t       scsi_ctrl;



	/*
	 * The microcode currently sets the SCSI Bus Reset signal while
	 * handling the AdvSendIdleCmd() IDLE_CMD_SCSI_RESET command above.
	 * But the SCSI Bus Reset Hold Time in the uCode is not deterministic
	 * (it may in fact be for less than the SCSI Spec. minimum of 25 us).
	 * Therefore on return the Adv Library sets the SCSI Bus Reset signal
	 * for ASC_SCSI_RESET_HOLD_TIME_US, which is defined to be greater
	 * than 25 us.
	 */
	scsi_ctrl = ADW_READ_WORD_REGISTER(iot, ioh, IOPW_SCSI_CTRL);
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_SCSI_CTRL,
				scsi_ctrl | ADW_SCSI_CTRL_RSTOUT);
	DvcDelayMicroSecond((u_int16_t) ASC_SCSI_RESET_HOLD_TIME_US);
	ADW_WRITE_WORD_REGISTER(iot, ioh, IOPW_SCSI_CTRL,
				scsi_ctrl & ~ADW_SCSI_CTRL_RSTOUT);

	DvcSleepMilliSecond((ulong) sc->scsi_reset_wait * 1000);
}


/*
 * Adv Library Interrupt Service Routine
 *
 *  This function is called by a driver's interrupt service routine.
 *  The function disables and re-enables interrupts.
 *
 *  When a microcode idle command is completed, the ADW_DVC_VAR
 *  'idle_cmd_done' field is set to ADW_TRUE.
 *
 *  Note: AdvISR() can be called when interrupts are disabled or even
 *  when there is no hardware interrupt condition present. It will
 *  always check for completed idle commands and microcode requests.
 *  This is an important feature that shouldn't be changed because it
 *  allows commands to be completed from polling mode loops.
 *
 * Return:
 *   ADW_TRUE(1) - interrupt was pending
 *   ADW_FALSE(0) - no interrupt was pending
 */
int
AdvISR(sc)
	ADW_SOFTC      *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t        int_stat;
	u_int16_t       next_done_loc, target_bit;
	int             completed_q;
	ADW_SCSI_REQ_Q *scsiq;
	ASC_REQ_SENSE  *sense_data;
	int             ret;


	ret = (ADW_IS_INT_PENDING(iot, ioh)) ? ADW_TRUE : ADW_FALSE;

	/* Reading the register clears the interrupt. */
	int_stat = ADW_READ_BYTE_REGISTER(iot, ioh, IOPB_INTR_STATUS_REG);

	if (int_stat & ADW_INTR_STATUS_INTRB) {
		sc->idle_cmd_done = ADW_TRUE;
	}
	/*
	 * Notify the driver of a hardware detected SCSI Bus Reset.
	 */
	if (int_stat & ADW_INTR_STATUS_INTRC) {
		if (sc->sbreset_callback) {
			(*(ADW_SBRESET_CALLBACK) sc->sbreset_callback) (sc);
		}
	}
	/*
	 * ASC_MC_HOST_NEXT_DONE (0x129) is actually the last completed RISC
	 * Queue List request. Its forward pointer (RQL_FWD) points to the
	 * current completed RISC Queue List request.
	 */
	ADW_READ_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_DONE, next_done_loc);
	next_done_loc = ASC_MC_RISC_Q_LIST_BASE +
		(next_done_loc * ASC_MC_RISC_Q_LIST_SIZE) + RQL_FWD;

	ADW_READ_BYTE_LRAM(iot, ioh, next_done_loc, completed_q);

	/* Loop until all completed Q's are processed. */
	while (completed_q != ASC_MC_NULL_Q) {
		ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_DONE,
				    completed_q);

		next_done_loc = ASC_MC_RISC_Q_LIST_BASE +
			(completed_q * ASC_MC_RISC_Q_LIST_SIZE);

		/*
		 * Read the ADW_SCSI_REQ_Q virtual address pointer from
		 * the RISC list entry. The microcode has changed the
		 * ADW_SCSI_REQ_Q physical address to its virtual address.
		 *
		 * Refer to comments at the end of AdvSendScsiCmd() for
		 * more information on the RISC list structure.
		 */
		{
			ushort          lsw, msw;
			ADW_READ_WORD_LRAM(iot, ioh,
					   next_done_loc + RQL_PHYADDR, lsw);
			ADW_READ_WORD_LRAM(iot, ioh,
				      next_done_loc + RQL_PHYADDR + 2, msw);

			scsiq = (ADW_SCSI_REQ_Q *)
				(((u_int32_t) msw << 16) | lsw);
		}

		target_bit = ADW_TID_TO_TIDMASK(scsiq->target_id);

		/*
		 * Clear request microcode control flag.
		 */
		scsiq->cntl = 0;

		/*
		 * Check Condition handling
		 */
		if ((scsiq->done_status == QD_WITH_ERROR) &&
		    (scsiq->scsi_status == SS_CHK_CONDITION) &&
		 (sense_data = (ASC_REQ_SENSE *) scsiq->vsense_addr) != 0 &&
		    (scsiq->orig_sense_len - scsiq->sense_len) >=
			ASC_MIN_SENSE_LEN) {
			/*
			 * Command returned with a check condition and valid
			 * sense data.
			 */
		}
		/*
		 * If the command that completed was a SCSI INQUIRY and
		 * LUN 0 was sent the command, then process the INQUIRY
		 * command information for the device.
		 */
		else if (scsiq->done_status == QD_NO_ERROR &&
			 scsiq->cdb[0] == INQUIRY &&
			 scsiq->target_lun == 0) {
			AdvInquiryHandling(sc, scsiq);
		}
		/* Change the RISC Queue List state to free. */
		ADW_WRITE_BYTE_LRAM(iot, ioh,
				 next_done_loc + RQL_STATE, ASC_MC_QS_FREE);

		/* Get the RISC Queue List forward pointer. */
		ADW_READ_BYTE_LRAM(iot, ioh,
				   next_done_loc + RQL_FWD, completed_q);

		/*
		 * Notify the driver of the completed request by passing
		 * the ADW_SCSI_REQ_Q pointer to its callback function.
		 */
		sc->cur_host_qng--;
		scsiq->a_flag |= ADW_SCSIQ_DONE;
		(*(ADW_ISR_CALLBACK) sc->isr_callback) (sc, scsiq);
		/*
		 * Note: After the driver callback function is called, 'scsiq'
		 * can no longer be referenced.
		 *
		 * Fall through and continue processing other completed
		 * requests...
		 */
	}
	return ret;
}

/*
 * Send an idle command to the chip and wait for completion.
 *
 * Interrupts do not have to be enabled on entry.
 *
 * Return Values:
 *   ADW_TRUE - command completed successfully
 *   ADW_FALSE - command failed
 */
int
AdvSendIdleCmd(sc, idle_cmd, idle_cmd_parameter, flags)
	ADW_SOFTC      *sc;
	u_int16_t       idle_cmd;
	u_int32_t       idle_cmd_parameter;
	int             flags;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t       i;
	int             ret;

	sc->idle_cmd_done = 0;

	/*
	 * Write the idle command value after the idle command parameter
	 * has been written to avoid a race condition. If the order is not
	 * followed, the microcode may process the idle command before the
	 * parameters have been written to LRAM.
	 */
	ADW_WRITE_DWORD_LRAM(iot, ioh, ASC_MC_IDLE_PARA_STAT,
			     idle_cmd_parameter);
	ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_IDLE_CMD, idle_cmd);

	/*
	 * If the 'flags' argument contains the ADW_NOWAIT flag, then
	 * return with success.
	 */
	if (flags & ADW_NOWAIT)
		return ADW_TRUE;

	for (i = 0; i < SCSI_WAIT_10_SEC * SCSI_MS_PER_SEC; i++) {
		/*
		 * 'idle_cmd_done' is set by AdvISR().
		 */
		if (sc->idle_cmd_done)
			break;

		DvcSleepMilliSecond(1);

		/*
		 * If interrupts were disabled on entry to AdvSendIdleCmd(),
		 * then they will still be disabled here. Call AdvISR() to
		 * check for the idle command completion.
		 */
		(void) AdvISR(sc);
	}

	if (sc->idle_cmd_done == ADW_FALSE) {
		return ADW_FALSE;
	} else {
		ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_IDLE_PARA_STAT, ret);
		return ret;
	}
}

/*
 * Send the SCSI request block to the adapter
 *
 * Each of the 255 Adv Library/Microcode RISC Lists or mailboxes has the
 * following structure:
 *
 * 0: RQL_FWD - RISC list forward pointer (1 byte)
 * 1: RQL_BWD - RISC list backward pointer (1 byte)
 * 2: RQL_STATE - RISC list state byte - free, ready, done, aborted (1 byte)
 * 3: RQL_TID - request target id (1 byte)
 * 4: RQL_PHYADDR - ADW_SCSI_REQ_Q physical pointer (4 bytes)
 *
 * Return:
 *      ADW_SUCCESS(1) - the request is in the mailbox
 *      ADW_BUSY(0) - total request count > 253, try later
 */
static int
AdvSendScsiCmd(sc, scsiq)
	ADW_SOFTC      *sc;
	ADW_SCSI_REQ_Q *scsiq;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	ADW_CCB        *ccb = (ADW_CCB *) scsiq->ccb_ptr;
	u_int16_t       next_ready_loc;
	u_int8_t        next_ready_loc_fwd;
	long            req_size;
	u_int32_t       q_phy_addr;


	if (sc->cur_host_qng >= sc->max_host_qng) {
		return ADW_BUSY;
	} else {
		sc->cur_host_qng++;
	}

	/*
	 * Clear the ADW_SCSI_REQ_Q done flag.
	 */
	scsiq->a_flag &= ~ADW_SCSIQ_DONE;

	/*
	 * Save the original sense buffer length.
	 *
	 * After the request completes 'sense_len' will be set to the residual
	 * byte count of the Auto-Request Sense if a command returns CHECK
	 * CONDITION and the Sense Data is valid indicated by 'host_status' not
	 * being set to QHSTA_M_AUTO_REQ_SENSE_FAIL. To determine the valid
	 * Sense Data Length subtract 'sense_len' from 'orig_sense_len'.
	 */
	scsiq->orig_sense_len = scsiq->sense_len;

	ADW_READ_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_READY, next_ready_loc);
	next_ready_loc = ASC_MC_RISC_Q_LIST_BASE +
		(next_ready_loc * ASC_MC_RISC_Q_LIST_SIZE);

	/*
	 * Write the physical address of the Q to the mailbox.
	 * We need to skip the first four bytes, because the microcode
	 * uses them internally for linking Q's together.
	 */
	req_size = sizeof(ADW_SCSI_REQ_Q);
	q_phy_addr = sc->sc_dmamap_control->dm_segs[0].ds_addr +
		ADW_CCB_OFF(ccb) + offsetof(struct adw_ccb, scsiq);

	scsiq->scsiq_ptr = scsiq;

	/*
	 * The RISC list structure, which 'next_ready_loc' is a pointer
	 * to in microcode LRAM, has the format detailed in the comment
	 * header for this function.
	 *
	 * Write the ADW_SCSI_REQ_Q physical pointer to
	 * 'next_ready_loc' request.
	 */
	ADW_WRITE_DWORD_LRAM(iot, ioh, next_ready_loc + RQL_PHYADDR,
			q_phy_addr);

	/* Write target_id to 'next_ready_loc' request. */
	ADW_WRITE_BYTE_LRAM(iot, ioh, next_ready_loc + RQL_TID,
			scsiq->target_id);

	/*
	 * Set the ASC_MC_HOST_NEXT_READY (0x128) microcode variable to
	 * the 'next_ready_loc' request forward pointer.
	 *
	 * Do this *before* changing the 'next_ready_loc' queue to QS_READY.
	 * After the state is changed to QS_READY 'RQL_FWD' will be changed
	 * by the microcode.
	 *
	 * NOTE: The temporary variable 'next_ready_loc_fwd' is required to
	 * prevent some compilers from optimizing out 'AdvReadByteLram()' if
	 * it were used as the 3rd argument to 'AdvWriteByteLram()'.
	 */
	ADW_READ_BYTE_LRAM(iot, ioh, next_ready_loc + RQL_FWD,
			   next_ready_loc_fwd);
	ADW_WRITE_BYTE_LRAM(iot, ioh, ASC_MC_HOST_NEXT_READY,
			    next_ready_loc_fwd);

	/*
	 * Change the state of 'next_ready_loc' request from QS_FREE to
	 * QS_READY which will cause the microcode to pick it up and
	 * execute it.
	 *
	 * Can't reference 'next_ready_loc' after changing the request
	 * state to QS_READY. The microcode now owns the request.
	 */
	ADW_WRITE_BYTE_LRAM(iot, ioh, next_ready_loc + RQL_STATE,
			    ASC_MC_QS_READY);

	return ADW_SUCCESS;
}

/*
 * Inquiry Information Byte 7 Handling
 *
 * Handle SCSI Inquiry Command information for a device by setting
 * microcode operating variables that affect WDTR, SDTR, and Tag
 * Queuing.
 */
static void
AdvInquiryHandling(sc, scsiq)
	ADW_SOFTC      *sc;
	ADW_SCSI_REQ_Q *scsiq;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	ASC_SCSI_INQUIRY *inq;
	u_int16_t       cfg_word;
	u_int16_t       tidmask;
	u_int8_t        tid;

	/*
	 * AdvInquiryHandling() requires up to INQUIRY information Byte 7
	 * to be available.
	 *
	 * If less than 8 bytes of INQUIRY information were requested or less
	 * than 8 bytes were transferred, then return. cdb[4] is the request
	 * length and the ADW_SCSI_REQ_Q 'data_cnt' field is set by the
	 * microcode to the transfer residual count.
	 */
	if (scsiq->cdb[4] < 8 || (scsiq->cdb[4] - scsiq->data_cnt) < 8) {
		return;
	}
	tid = scsiq->target_id;
	inq = (ASC_SCSI_INQUIRY *) scsiq->vdata_addr;

	/*
	 * WDTR, SDTR, and Tag Queuing cannot be enabled for old devices.
	 */
	if (inq->byte3.rsp_data_fmt < 2 && inq->byte2.ansi_apr_ver < 2) {
		return;
	} else {
		/*
		 * INQUIRY Byte 7 Handling
		 *
		 * Use a device's INQUIRY byte 7 to determine whether it
		 * supports WDTR, SDTR, and Tag Queuing. If the feature
		 * is enabled in the EEPROM and the device supports the
		 * feature, then enable it in the microcode.
		 */

		tidmask = ADW_TID_TO_TIDMASK(tid);
		/*
		 * Wide Transfers
		 *
		 * If the EEPROM enabled WDTR for the device and the device
		 * supports wide bus (16 bit) transfers, then turn on the
		 * device's 'wdtr_able' bit and write the new value to the
		 * microcode.
		 */
		if ((sc->wdtr_able & tidmask) && inq->byte7.WBus16) {
			ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_WDTR_ABLE,
					   cfg_word);
			if ((cfg_word & tidmask) == 0) {
				cfg_word |= tidmask;
				ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_WDTR_ABLE,
						    cfg_word);

				/*
				 * Clear the microcode "WDTR negotiation" done
				 * indicator for the target to cause it
				 * to negotiate with the new setting set above.
				 */
				ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_WDTR_DONE,
						   cfg_word);
				cfg_word &= ~tidmask;
				ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_WDTR_DONE,
						    cfg_word);
			}
		}
		/*
		 * Synchronous Transfers
		 *
		 * If the EEPROM enabled SDTR for the device and the device
		 * supports synchronous transfers, then turn on the device's
		 * 'sdtr_able' bit. Write the new value to the microcode.
		 */
		if ((sc->sdtr_able & tidmask) && inq->byte7.Sync) {
			ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_SDTR_ABLE,
					   cfg_word);
			if ((cfg_word & tidmask) == 0) {
				cfg_word |= tidmask;
				ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_SDTR_ABLE,
						    cfg_word);

				/*
				 * Clear the microcode "SDTR negotiation" done
				 * indicator for the target to cause it
				 * to negotiate with the new setting set above.
				 */
				ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_SDTR_DONE,
						   cfg_word);
				cfg_word &= ~tidmask;
				ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_SDTR_DONE,
						    cfg_word);
			}
		}
		/*
		 * If the EEPROM enabled Tag Queuing for device and the
		 * device supports Tag Queuing, then turn on the device's
		 * 'tagqng_enable' bit in the microcode and set the microcode
		 * maximum command count to the ADW_DVC_VAR 'max_dvc_qng'
		 * value.
		 *
		 * Tag Queuing is disabled for the BIOS which runs in polled
		 * mode and would see no benefit from Tag Queuing. Also by
		 * disabling Tag Queuing in the BIOS devices with Tag Queuing
		 * bugs will at least work with the BIOS.
		 */
		if ((sc->tagqng_able & tidmask) && inq->byte7.CmdQue) {
			ADW_READ_WORD_LRAM(iot, ioh, ASC_MC_TAGQNG_ABLE,
					   cfg_word);
			cfg_word |= tidmask;
			ADW_WRITE_WORD_LRAM(iot, ioh, ASC_MC_TAGQNG_ABLE,
					    cfg_word);
			ADW_WRITE_BYTE_LRAM(iot, ioh,
					    ASC_MC_NUMBER_OF_MAX_CMD + tid,
					    sc->max_dvc_qng);
		}
	}
}

static void
DvcSleepMilliSecond(n)
	ulong           n;
{

	DELAY(n * 1000);
}

static void
DvcDelayMicroSecond(n)
	ulong           n;
{

	DELAY(n);
}
