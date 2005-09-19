/*	$OpenBSD: ar5xxx.c,v 1.30 2005/09/19 10:27:08 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
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
 * HAL interface for Atheros Wireless LAN devices.
 * (Please have a look at ar5xxx.h for further information)
 */

#include <dev/pci/pcidevs.h>

#include <dev/ic/ar5xxx.h>

extern ar5k_attach_t ar5k_ar5210_attach;
extern ar5k_attach_t ar5k_ar5211_attach;
extern ar5k_attach_t ar5k_ar5212_attach;

static const struct {
	u_int16_t	vendor;
	u_int16_t	device;
	ar5k_attach_t	(*attach);
} ar5k_known_products[] = {
	/*
	 * From pcidevs_data.h
	 */
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5210,
	    ar5k_ar5210_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5210_AP,
	    ar5k_ar5210_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5210_DEFAULT,
	    ar5k_ar5210_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5211,
	    ar5k_ar5211_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5211_DEFAULT,
	    ar5k_ar5211_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5311,
	    ar5k_ar5211_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5211_FPGA11B,
	    ar5k_ar5211_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5211_LEGACY,
	    ar5k_ar5211_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5212,
	    ar5k_ar5212_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5212_DEFAULT,
	    ar5k_ar5212_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5212_FPGA,
	    ar5k_ar5212_attach },
	{ PCI_VENDOR_ATHEROS, PCI_PRODUCT_ATHEROS_AR5212_IBM,
	    ar5k_ar5212_attach },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRDAG675,
	    ar5k_ar5212_attach },
	{ PCI_VENDOR_3COM2, PCI_PRODUCT_3COM2_3CRPAG175,
	    ar5k_ar5212_attach }
};

static const HAL_RATE_TABLE ar5k_rt_11a = AR5K_RATES_11A;
static const HAL_RATE_TABLE ar5k_rt_11b = AR5K_RATES_11B;
static const HAL_RATE_TABLE ar5k_rt_11g = AR5K_RATES_11G;
static const HAL_RATE_TABLE ar5k_rt_turbo = AR5K_RATES_TURBO;
static const HAL_RATE_TABLE ar5k_rt_xr = AR5K_RATES_XR;

int		 ar5k_eeprom_read_ants(struct ath_hal *, u_int32_t *, u_int);
int		 ar5k_eeprom_read_modes(struct ath_hal *, u_int32_t *, u_int);
u_int16_t	 ar5k_eeprom_bin2freq(struct ath_hal *, u_int16_t, u_int);

HAL_BOOL	 ar5k_ar5110_channel(struct ath_hal *, HAL_CHANNEL *);
u_int32_t	 ar5k_ar5110_chan2athchan(HAL_CHANNEL *);
HAL_BOOL	 ar5k_ar5111_channel(struct ath_hal *, HAL_CHANNEL *);
HAL_BOOL	 ar5k_ar5111_chan2athchan(u_int, struct ar5k_athchan_2ghz *);
HAL_BOOL	 ar5k_ar5112_channel(struct ath_hal *, HAL_CHANNEL *);
HAL_BOOL	 ar5k_check_channel(struct ath_hal *, u_int16_t, u_int flags);

HAL_BOOL	 ar5k_ar5111_rfregs(struct ath_hal *, HAL_CHANNEL *, u_int);
HAL_BOOL	 ar5k_ar5112_rfregs(struct ath_hal *, HAL_CHANNEL *, u_int);
u_int		 ar5k_rfregs_op(u_int32_t *, u_int32_t, u_int32_t, u_int32_t,
    u_int32_t, u_int32_t, HAL_BOOL);

/*
 * Supported channels
 */
static const struct
ieee80211_regchannel ar5k_5ghz_channels[] = IEEE80211_CHANNELS_5GHZ;
static const struct
ieee80211_regchannel ar5k_2ghz_channels[] = IEEE80211_CHANNELS_2GHZ;

/*
 * Initial gain optimization values
 */
static const struct ar5k_gain_opt ar5111_gain_opt = AR5K_AR5111_GAIN_OPT;
static const struct ar5k_gain_opt ar5112_gain_opt = AR5K_AR5112_GAIN_OPT;

/*
 * Initial register for the radio chipsets
 */
static const struct ar5k_ini_rf ar5111_rf[] = AR5K_AR5111_INI_RF;
static const struct ar5k_ini_rf ar5112_rf[] = AR5K_AR5112_INI_RF;
static const struct ar5k_ini_rfgain ar5k_rfg[] = AR5K_INI_RFGAIN;

/*
 * Enable to overwrite the country code (use "00" for debug)
 */
#if 0
#define COUNTRYCODE "00"
#endif

/*
 * Perform a lookup if the device is supported by the HAL
 */
const char *
ath_hal_probe(u_int16_t vendor, u_int16_t device)
{
	int i;

	/*
	 * Perform a linear search on the table of supported devices
	 */
	for (i = 0; i < AR5K_ELEMENTS(ar5k_known_products); i++) {
		if (vendor == ar5k_known_products[i].vendor &&
		    device == ar5k_known_products[i].device)
			return ("");
	}

	return (NULL);
}

/*
 * Fills in the HAL structure and initialises the device
 */
struct ath_hal *
ath_hal_attach(u_int16_t device, void *sc, bus_space_tag_t st,
    bus_space_handle_t sh, int *status)
{
	ieee80211_regdomain_t ieee_regdomain;
	u_int16_t regdomain;
	struct ath_hal *hal = NULL;
	ar5k_attach_t *attach = NULL;
	u_int8_t mac[IEEE80211_ADDR_LEN];
	int i;

	*status = EINVAL;

	/*
	 * Call the chipset-dependent attach routine by device id
	 */
	for (i = 0; i < AR5K_ELEMENTS(ar5k_known_products); i++) {
		if (device == ar5k_known_products[i].device &&
		    ar5k_known_products[i].attach != NULL)
			attach = ar5k_known_products[i].attach;
	}

	if (attach == NULL) {
		*status = ENXIO;
		AR5K_PRINTF("device not supported: 0x%04x\n", device);
		return (NULL);
	}

	if ((hal = malloc(sizeof(struct ath_hal),
		 M_DEVBUF, M_NOWAIT)) == NULL) {
		*status = ENOMEM;
		AR5K_PRINT("out of memory\n");
		return (NULL);
	}

	bzero(hal, sizeof(struct ath_hal));

	hal->ah_sc = sc;
	hal->ah_st = st;
	hal->ah_sh = sh;
	hal->ah_device = device;
	hal->ah_sub_vendor = 0; /* XXX unknown?! */

	/*
	 * HAL information
	 */
	hal->ah_abi = HAL_ABI_VERSION;
	hal->ah_country_code = CTRY_DEFAULT;
	hal->ah_capabilities.cap_regdomain.reg_current = AR5K_TUNE_REGDOMAIN;
	hal->ah_op_mode = HAL_M_STA;
	hal->ah_radar.r_enabled = AR5K_TUNE_RADAR_ALERT;
	hal->ah_turbo = AH_FALSE;
	hal->ah_txpower.txp_tpc = AR5K_TUNE_TPC_TXPOWER;
	hal->ah_imr = 0;
	hal->ah_atim_window = 0;
	hal->ah_aifs = AR5K_TUNE_AIFS;
	hal->ah_cw_min = AR5K_TUNE_CWMIN;
	hal->ah_limit_tx_retries = AR5K_INIT_TX_RETRY;
	hal->ah_software_retry = AH_FALSE;
	hal->ah_ant_diversity = AR5K_TUNE_ANT_DIVERSITY;

	if ((attach)(device, hal, st, sh, status) == NULL)
		goto failed;

#ifdef AR5K_DEBUG
	hal->ah_dump_state(hal);
#endif

	/*
	 * Get card capabilities, values, ...
	 */

	if (ar5k_eeprom_init(hal) != 0) {
		AR5K_PRINT("unable to init EEPROM\n");
		goto failed;
	}

	/* Set regulation domain */
	if ((regdomain =
		hal->ah_capabilities.cap_eeprom.ee_regdomain) != 0) {
		hal->ah_capabilities.cap_regdomain.reg_current =
		    ieee_regdomain = ar5k_regdomain_to_ieee(regdomain);
	} else {
		ieee_regdomain =
		    hal->ah_capabilities.cap_regdomain.reg_current;

		/* Try to write default regulation domain to EEPROM */
		ar5k_eeprom_regulation_domain(hal, AH_TRUE, &ieee_regdomain);
	}

	hal->ah_capabilities.cap_regdomain.reg_hw = ieee_regdomain;

	/* Get misc capabilities */
	if (hal->ah_get_capabilities(hal) != AH_TRUE) {
		AR5K_PRINTF("unable to get device capabilities: 0x%04x\n",
		    device);
		goto failed;
	}

	/* Get MAC address */
	if ((*status = ar5k_eeprom_read_mac(hal, mac)) != 0) {
		AR5K_PRINTF("unable to read address from EEPROM: 0x%04x\n",
		    device);
		goto failed;
	}

	hal->ah_set_lladdr(hal, mac);

	/* Get rate tables */
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11A)
		ar5k_rt_copy(&hal->ah_rt_11a, &ar5k_rt_11a);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11B)
		ar5k_rt_copy(&hal->ah_rt_11b, &ar5k_rt_11b);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11G)
		ar5k_rt_copy(&hal->ah_rt_11g, &ar5k_rt_11g);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_TURBO)
		ar5k_rt_copy(&hal->ah_rt_turbo, &ar5k_rt_turbo);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_XR)
		ar5k_rt_copy(&hal->ah_rt_xr, &ar5k_rt_xr);

	/* Initialize the gain optimization values */
	if (hal->ah_radio == AR5K_AR5111) {
		hal->ah_gain.g_step_idx = ar5111_gain_opt.go_default;
		hal->ah_gain.g_step =
		    &ar5111_gain_opt.go_step[hal->ah_gain.g_step_idx];
		hal->ah_gain.g_low = 20;
		hal->ah_gain.g_high = 35;
		hal->ah_gain.g_active = 1;
	} else if (hal->ah_radio == AR5K_AR5112) {
		hal->ah_gain.g_step_idx = ar5112_gain_opt.go_default;
		hal->ah_gain.g_step =
		    &ar5111_gain_opt.go_step[hal->ah_gain.g_step_idx];
		hal->ah_gain.g_low = 20;
		hal->ah_gain.g_high = 85;
		hal->ah_gain.g_active = 1;
	}

	*status = HAL_OK;

	return (hal);

 failed:
	free(hal, M_DEVBUF);
	return (NULL);
}

u_int16_t
ath_hal_computetxtime(struct ath_hal *hal, const HAL_RATE_TABLE *rates,
    u_int32_t frame_length, u_int16_t rate_index, HAL_BOOL short_preamble)
{
	const HAL_RATE *rate;
	u_int32_t value;

	AR5K_ASSERT_ENTRY(rate_index, rates->rateCount);

	/*
	 * Get rate by index
	 */
	rate = &rates->info[rate_index];

	/*
	 * Calculate the transmission time by operation (PHY) mode
	 */
	switch (rate->phy) {
	case IEEE80211_T_CCK:
		/*
		 * CCK / DS mode (802.11b)
		 */
		value = AR5K_CCK_TX_TIME(rate->rateKbps, frame_length,
		    (short_preamble && rate->shortPreamble));
		break;

	case IEEE80211_T_OFDM:
		/*
		 * Orthogonal Frequency Division Multiplexing
		 */
		if (AR5K_OFDM_NUM_BITS_PER_SYM(rate->rateKbps) == 0)
			return (0);
		value = AR5K_OFDM_TX_TIME(rate->rateKbps, frame_length);
		break;

	case IEEE80211_T_TURBO:
		/*
		 * Orthogonal Frequency Division Multiplexing
		 * Atheros "Turbo Mode" (doubled rates)
		 */
		if (AR5K_TURBO_NUM_BITS_PER_SYM(rate->rateKbps) == 0)
			return (0);
		value = AR5K_TURBO_TX_TIME(rate->rateKbps, frame_length);
		break;

	case IEEE80211_T_XR:
		/*
		 * Orthogonal Frequency Division Multiplexing
		 * Atheros "eXtended Range" (XR)
		 */
		if (AR5K_XR_NUM_BITS_PER_SYM(rate->rateKbps) == 0)
			return (0);
		value = AR5K_XR_TX_TIME(rate->rateKbps, frame_length);
		break;

	default:
		return (0);
	}

	return (value);
}

u_int
ath_hal_mhz2ieee(u_int mhz, u_int flags)
{
	return (ieee80211_mhz2ieee(mhz, flags));
}

u_int
ath_hal_ieee2mhz(u_int ieee, u_int flags)
{
	return (ieee80211_ieee2mhz(ieee, flags));
}

HAL_BOOL
ar5k_check_channel(struct ath_hal *hal, u_int16_t freq, u_int flags)
{
	/* Check if the channel is in our supported range */
	if (flags & IEEE80211_CHAN_2GHZ) {
		if ((freq >= hal->ah_capabilities.cap_range.range_2ghz_min) &&
		    (freq <= hal->ah_capabilities.cap_range.range_2ghz_max))
			return (AH_TRUE);
	} else if (flags & IEEE80211_CHAN_5GHZ) {
		if ((freq >= hal->ah_capabilities.cap_range.range_5ghz_min) &&
		    (freq <= hal->ah_capabilities.cap_range.range_5ghz_max))
			return (AH_TRUE);
	}

	return (AH_FALSE);
}

HAL_BOOL
ath_hal_init_channels(struct ath_hal *hal, HAL_CHANNEL *channels,
    u_int max_channels, u_int *channels_size, HAL_CTRY_CODE country,
    u_int16_t mode, HAL_BOOL outdoor, HAL_BOOL extended)
{
	u_int i, c;
	u_int32_t domain_current;
	u_int domain_5ghz, domain_2ghz;
	HAL_CHANNEL *all_channels;

	if ((all_channels = malloc(sizeof(HAL_CHANNEL) * max_channels,
	    M_TEMP, M_NOWAIT)) == NULL)
		return (AH_FALSE);

	i = c = 0;
	domain_current = hal->ah_get_regdomain(hal);

	/*
	 * In debugging mode, enable all channels supported by the chipset
	 */
	if (domain_current == DMN_DEFAULT) {
		int min, max, freq;
		u_int flags;

		min = ieee80211_mhz2ieee(IEEE80211_CHANNELS_2GHZ_MIN,
		    IEEE80211_CHAN_2GHZ);
		max = ieee80211_mhz2ieee(IEEE80211_CHANNELS_2GHZ_MAX,
		    IEEE80211_CHAN_2GHZ);
		flags = CHANNEL_B | CHANNEL_TG |
		    (hal->ah_version == AR5K_AR5211 ?
		    CHANNEL_PUREG : CHANNEL_G);

 debugchan:
		for (i = min; i <= max && c < max_channels; i++) {
			freq = ieee80211_ieee2mhz(i, flags);
			if (ar5k_check_channel(hal, freq, flags) == AH_FALSE)
				continue;
			all_channels[c].c_channel = freq;
			all_channels[c++].c_channel_flags = flags;
		}

		if (flags & IEEE80211_CHAN_2GHZ) {
			min = ieee80211_mhz2ieee(IEEE80211_CHANNELS_5GHZ_MIN,
			    IEEE80211_CHAN_5GHZ);
			max = ieee80211_mhz2ieee(IEEE80211_CHANNELS_5GHZ_MAX,
			    IEEE80211_CHAN_5GHZ);
			flags = CHANNEL_A | CHANNEL_T | CHANNEL_XR;
			goto debugchan;
		}

		goto done;
	}

	domain_5ghz = ieee80211_regdomain2flag(domain_current,
	    IEEE80211_CHANNELS_5GHZ_MIN);
	domain_2ghz = ieee80211_regdomain2flag(domain_current,
	    IEEE80211_CHANNELS_2GHZ_MIN);

	/*
	 * Create channel list based on chipset capabilities, regulation domain
	 * and mode. 5GHz...
	 */
	for (i = 0; (hal->ah_capabilities.cap_range.range_5ghz_max > 0) &&
		 (i < AR5K_ELEMENTS(ar5k_5ghz_channels)) &&
		 (c < max_channels); i++) {
		/* Check if channel is supported by the chipset */
		if (ar5k_check_channel(hal,
		    ar5k_5ghz_channels[i].rc_channel,
		    IEEE80211_CHAN_5GHZ) == AH_FALSE)
			continue;

		/* Match regulation domain */
		if ((IEEE80211_DMN(ar5k_5ghz_channels[i].rc_domain) &
			IEEE80211_DMN(domain_5ghz)) == 0)
			continue;

		/* Match modes */
		if (ar5k_5ghz_channels[i].rc_mode & IEEE80211_CHAN_TURBO) {
			all_channels[c].c_channel_flags = CHANNEL_T;
		} else if (ar5k_5ghz_channels[i].rc_mode &
		    IEEE80211_CHAN_OFDM) {
			all_channels[c].c_channel_flags = CHANNEL_A;
		} else
			continue;

		/* Write channel and increment counter */
		all_channels[c++].channel = ar5k_5ghz_channels[i].rc_channel;
	}

	/*
	 * ...and 2GHz.
	 */
	for (i = 0; (hal->ah_capabilities.cap_range.range_2ghz_max > 0) &&
		 (i < AR5K_ELEMENTS(ar5k_2ghz_channels)) &&
		 (c < max_channels); i++) {
		/* Check if channel is supported by the chipset */
		if (ar5k_check_channel(hal,
		    ar5k_2ghz_channels[i].rc_channel,
		    IEEE80211_CHAN_2GHZ) == AH_FALSE)
			continue;

		/* Match regulation domain */
		if ((IEEE80211_DMN(ar5k_2ghz_channels[i].rc_domain) &
			IEEE80211_DMN(domain_2ghz)) == 0)
			continue;

		/* Match modes */
		if (ar5k_2ghz_channels[i].rc_mode & IEEE80211_CHAN_CCK)
			all_channels[c].c_channel_flags = CHANNEL_B;

		if (ar5k_2ghz_channels[i].rc_mode & IEEE80211_CHAN_OFDM) {
			all_channels[c].c_channel_flags |=
			    hal->ah_version == AR5K_AR5211 ?
			    CHANNEL_PUREG : CHANNEL_G;
			if (ar5k_2ghz_channels[i].rc_mode &
			    IEEE80211_CHAN_TURBO)
				all_channels[c].c_channel_flags |= CHANNEL_TG;
		}

		/* Write channel and increment counter */
		all_channels[c++].channel = ar5k_2ghz_channels[i].rc_channel;
	}

 done:
	bcopy(all_channels, channels, sizeof(HAL_CHANNEL) * max_channels);
	*channels_size = c;
	free(all_channels, M_TEMP);
	return (AH_TRUE);
}

/*
 * Common internal functions
 */

const char *
ar5k_printver(enum ar5k_srev_type type, u_int32_t val)
{
	struct ar5k_srev_name names[] = AR5K_SREV_NAME;
	const char *name = "xxxx";
	int i;

	for (i = 0; i < AR5K_ELEMENTS(names); i++) {
		if (names[i].sr_type != type ||
		    names[i].sr_val == AR5K_SREV_UNKNOWN)
			continue;
		if ((val & 0xff) < names[i + 1].sr_val) {
			name = names[i].sr_name;
			break;
		}
	}

	return (name);
}

void
ar5k_radar_alert(struct ath_hal *hal)
{
	/*
	 * Limit ~1/s
	 */
	if (hal->ah_radar.r_last_channel.channel ==
	    hal->ah_current_channel.channel &&
	    tick < (hal->ah_radar.r_last_alert + hz))
		return;

	hal->ah_radar.r_last_channel.channel =
	    hal->ah_current_channel.channel;
	hal->ah_radar.r_last_channel.c_channel_flags =
	    hal->ah_current_channel.c_channel_flags;
	hal->ah_radar.r_last_alert = tick;

	AR5K_PRINTF("Possible radar activity detected at %u MHz (tick %u)\n",
	    hal->ah_radar.r_last_alert, hal->ah_current_channel.channel);
}

u_int16_t
ar5k_regdomain_from_ieee(ieee80211_regdomain_t ieee)
{
	u_int32_t regdomain = (u_int32_t)ieee;

	if (regdomain & 0xf0000000)
		return ((u_int16_t)AR5K_TUNE_REGDOMAIN);

	return (regdomain & 0xff);
}

ieee80211_regdomain_t
ar5k_regdomain_to_ieee(u_int16_t regdomain)
{
	ieee80211_regdomain_t ieee = (ieee80211_regdomain_t)regdomain & 0xff;

	return (ieee);
}

u_int16_t
ar5k_get_regdomain(struct ath_hal *hal)
{
#ifndef COUNTRYCODE
	/*
	 * Use the regulation domain found in the EEPROM, if not
	 * forced by a static country code.
	 */
	u_int16_t regdomain;
	ieee80211_regdomain_t ieee_regdomain;

	if (ar5k_eeprom_regulation_domain(hal,
	    AH_FALSE, &ieee_regdomain) == AH_TRUE) {
		if ((regdomain = ar5k_regdomain_from_ieee(ieee_regdomain)))
			return (regdomain);
	}

	return (hal->ah_regdomain);
#else
	/*
	 * Get the regulation domain by country code. This will ignore
	 * the settings found in the EEPROM.
	 */
	u_int16_t code;

	code = ieee80211_name2countrycode(COUNTRYCODE);
	return (ieee80211_countrycode2regdomain(code));
#endif
}

u_int32_t
ar5k_bitswap(u_int32_t val, u_int bits)
{
	u_int32_t retval = 0, bit, i;

	for (i = 0; i < bits; i++) {
		bit = (val >> i) & 1;
		retval = (retval << 1) | bit;
	}

	return (retval);
}

u_int
ar5k_htoclock(u_int usec, HAL_BOOL turbo)
{
	return (turbo == AH_TRUE ? (usec * 80) : (usec * 40));
}

u_int
ar5k_clocktoh(u_int clock, HAL_BOOL turbo)
{
	return (turbo == AH_TRUE ? (clock / 80) : (clock / 40));
}

void
ar5k_rt_copy(HAL_RATE_TABLE *dst, const HAL_RATE_TABLE *src)
{
	bzero(dst, sizeof(HAL_RATE_TABLE));
	dst->rateCount = src->rateCount;
	bcopy(src->info, dst->info, sizeof(dst->info));
}

HAL_BOOL
ar5k_register_timeout(struct ath_hal *hal, u_int32_t reg, u_int32_t flag,
    u_int32_t val, HAL_BOOL is_set)
{
	int i;
	u_int32_t data;

	for (i = AR5K_TUNE_REGISTER_TIMEOUT; i > 0; i--) {
		data = AR5K_REG_READ(reg);
		if ((is_set == AH_TRUE) && (data & flag))
			break;
		else if ((data & flag) == val)
			break;
		AR5K_DELAY(15);
	}

	if (i <= 0)
		return (AH_FALSE);

	return (AH_TRUE);
}

/*
 * Common ar5xx EEPROM access functions
 */

u_int16_t
ar5k_eeprom_bin2freq(struct ath_hal *hal, u_int16_t bin, u_int mode)
{
	u_int16_t val;

	if (bin == AR5K_EEPROM_CHANNEL_DIS)
		return (bin);

	if (mode == AR5K_EEPROM_MODE_11A) {
		if (hal->ah_ee_version > AR5K_EEPROM_VERSION_3_2)
			val = (5 * bin) + 4800;
		else
			val = bin > 62 ?
			    (10 * 62) + (5 * (bin - 62)) + 5100 :
			    (bin * 10) + 5100;
	} else {
		if (hal->ah_ee_version > AR5K_EEPROM_VERSION_3_2)
			val = bin + 2300;
		else
			val = bin + 2400;
	}

	return (val);
}

int
ar5k_eeprom_read_ants(struct ath_hal *hal, u_int32_t *offset, u_int mode)
{
	struct ar5k_eeprom_info *ee = &hal->ah_capabilities.cap_eeprom;
	u_int32_t o = *offset;
	u_int16_t val;
	int ret, i = 0;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_switch_settling[mode]	= (val >> 8) & 0x7f;
	ee->ee_ant_tx_rx[mode]		= (val >> 2) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 4) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 12) & 0xf;
	ee->ee_ant_control[mode][i++]	= (val >> 6) & 0x3f;
	ee->ee_ant_control[mode][i++]	= val & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	= (val >> 10) & 0x3f;
	ee->ee_ant_control[mode][i++]	= (val >> 4) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 2) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 14) & 0x3;
	ee->ee_ant_control[mode][i++]	= (val >> 8) & 0x3f;
	ee->ee_ant_control[mode][i++]	= (val >> 2) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 4) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 12) & 0xf;
	ee->ee_ant_control[mode][i++]	= (val >> 6) & 0x3f;
	ee->ee_ant_control[mode][i++]	= val & 0x3f;

	/* Get antenna modes */
	hal->ah_antenna[mode][0] =
	    (ee->ee_ant_control[mode][0] << 4) | 0x1;
	hal->ah_antenna[mode][HAL_ANT_FIXED_A] =
	    ee->ee_ant_control[mode][1] |
	    (ee->ee_ant_control[mode][2] << 6) |
	    (ee->ee_ant_control[mode][3] << 12) |
	    (ee->ee_ant_control[mode][4] << 18) |
	    (ee->ee_ant_control[mode][5] << 24);
	hal->ah_antenna[mode][HAL_ANT_FIXED_B] =
	    ee->ee_ant_control[mode][6] |
	    (ee->ee_ant_control[mode][7] << 6) |
	    (ee->ee_ant_control[mode][8] << 12) |
	    (ee->ee_ant_control[mode][9] << 18) |
	    (ee->ee_ant_control[mode][10] << 24);

	/* return new offset */
	*offset = o;

	return (0);
}

int
ar5k_eeprom_read_modes(struct ath_hal *hal, u_int32_t *offset, u_int mode)
{
	struct ar5k_eeprom_info *ee = &hal->ah_capabilities.cap_eeprom;
	u_int32_t o = *offset;
	u_int16_t val;
	int ret;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_tx_end2xlna_enable[mode]	= (val >> 8) & 0xff;
	ee->ee_thr_62[mode]		= val & 0xff;

	if (hal->ah_ee_version <= AR5K_EEPROM_VERSION_3_2)
		ee->ee_thr_62[mode] =
		    mode == AR5K_EEPROM_MODE_11A ? 15 : 28;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_tx_end2xpa_disable[mode]	= (val >> 8) & 0xff;
	ee->ee_tx_frm2xpa_enable[mode]	= val & 0xff;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_pga_desired_size[mode]	= (val >> 8) & 0xff;

	if ((val & 0xff) & 0x80)
		ee->ee_noise_floor_thr[mode] = -((((val & 0xff) ^ 0xff)) + 1);
	else
		ee->ee_noise_floor_thr[mode] = val & 0xff;

	if (hal->ah_ee_version <= AR5K_EEPROM_VERSION_3_2)
		ee->ee_noise_floor_thr[mode] =
		    mode == AR5K_EEPROM_MODE_11A ? -54 : -1;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_xlna_gain[mode]		= (val >> 5) & 0xff;
	ee->ee_x_gain[mode]		= (val >> 1) & 0xf;
	ee->ee_xpd[mode]		= val & 0x1;

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_0)
		ee->ee_fixed_bias[mode] = (val >> 13) & 0x1;

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_3_3) {
		AR5K_EEPROM_READ(o++, val);
		ee->ee_false_detect[mode] = (val >> 6) & 0x7f;

		if (mode == AR5K_EEPROM_MODE_11A)
			ee->ee_xr_power[mode] = val & 0x3f;
		else {
			ee->ee_ob[mode][0] = val & 0x7;
			ee->ee_db[mode][0] = (val >> 3) & 0x7;
		}
	}

	if (hal->ah_ee_version < AR5K_EEPROM_VERSION_3_4) {
		ee->ee_i_gain[mode] = AR5K_EEPROM_I_GAIN;
		ee->ee_cck_ofdm_power_delta = AR5K_EEPROM_CCK_OFDM_DELTA;
	} else {
		ee->ee_i_gain[mode] = (val >> 13) & 0x7;

		AR5K_EEPROM_READ(o++, val);
		ee->ee_i_gain[mode] |= (val << 3) & 0x38;

		if (mode == AR5K_EEPROM_MODE_11G)
			ee->ee_cck_ofdm_power_delta = (val >> 3) & 0xff;
	}

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_0 &&
	    mode == AR5K_EEPROM_MODE_11A) {
		ee->ee_i_cal[mode] = (val >> 8) & 0x3f;
		ee->ee_q_cal[mode] = (val >> 3) & 0x1f;
	}

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_6 &&
	    mode == AR5K_EEPROM_MODE_11G)
		ee->ee_scaled_cck_delta = (val >> 11) & 0x1f;

	/* return new offset */
	*offset = o;

	return (0);
}

int
ar5k_eeprom_init(struct ath_hal *hal)
{
	struct ar5k_eeprom_info *ee = &hal->ah_capabilities.cap_eeprom;
	u_int32_t offset;
	u_int16_t val;
	int ret, i;
	u_int mode;

	/* Initial TX thermal adjustment values */
	ee->ee_tx_clip = 4;
	ee->ee_pwd_84 = ee->ee_pwd_90 = 1;
	ee->ee_gain_select = 1;

	/*
	 * Read values from EEPROM and store them in the capability structure
	 */
	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_MAGIC, ee_magic);
	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_PROTECT, ee_protect);
	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_REG_DOMAIN, ee_regdomain);
	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_VERSION, ee_version);
	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_HDR, ee_header);

	/* Return if we have an old EEPROM */
	if (hal->ah_ee_version < AR5K_EEPROM_VERSION_3_0)
		return (0);

	AR5K_EEPROM_READ_HDR(AR5K_EEPROM_ANT_GAIN(hal->ah_ee_version),
	    ee_ant_gain);

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_0) {
		AR5K_EEPROM_READ_HDR(AR5K_EEPROM_MISC0, ee_misc0);
		AR5K_EEPROM_READ_HDR(AR5K_EEPROM_MISC1, ee_misc1);
	}

	if (hal->ah_ee_version < AR5K_EEPROM_VERSION_3_3) {
		AR5K_EEPROM_READ(AR5K_EEPROM_OBDB0_2GHZ, val);
		ee->ee_ob[AR5K_EEPROM_MODE_11B][0] = val & 0x7;
		ee->ee_db[AR5K_EEPROM_MODE_11B][0] = (val >> 3) & 0x7;

		AR5K_EEPROM_READ(AR5K_EEPROM_OBDB1_2GHZ, val);
		ee->ee_ob[AR5K_EEPROM_MODE_11G][0] = val & 0x7;
		ee->ee_db[AR5K_EEPROM_MODE_11G][0] = (val >> 3) & 0x7;
	}

	/*
	 * Get conformance test limit values
	 */
	offset = AR5K_EEPROM_CTL(hal->ah_ee_version);
	ee->ee_ctls = AR5K_EEPROM_N_CTLS(hal->ah_ee_version);

	for (i = 0; i < ee->ee_ctls; i++) {
		AR5K_EEPROM_READ(offset++, val);
		ee->ee_ctl[i] = (val >> 8) & 0xff;
		ee->ee_ctl[i + 1] = val & 0xff;
	}

	/*
	 * Get values for 802.11a (5GHz)
	 */
	mode = AR5K_EEPROM_MODE_11A;

	ee->ee_turbo_max_power[mode] =
	    AR5K_EEPROM_HDR_T_5GHZ_DBM(ee->ee_header);

	offset = AR5K_EEPROM_MODES_11A(hal->ah_ee_version);

	if ((ret = ar5k_eeprom_read_ants(hal, &offset, mode)) != 0)
		return (ret);

	AR5K_EEPROM_READ(offset++, val);
	ee->ee_adc_desired_size[mode]	= (int8_t)((val >> 8) & 0xff);
	ee->ee_ob[mode][3]		= (val >> 5) & 0x7;
	ee->ee_db[mode][3]		= (val >> 2) & 0x7;
	ee->ee_ob[mode][2]		= (val << 1) & 0x7;

	AR5K_EEPROM_READ(offset++, val);
	ee->ee_ob[mode][2]		|= (val >> 15) & 0x1;
	ee->ee_db[mode][2]		= (val >> 12) & 0x7;
	ee->ee_ob[mode][1]		= (val >> 9) & 0x7;
	ee->ee_db[mode][1]		= (val >> 6) & 0x7;
	ee->ee_ob[mode][0]		= (val >> 3) & 0x7;
	ee->ee_db[mode][0]		= val & 0x7;

	if ((ret = ar5k_eeprom_read_modes(hal, &offset, mode)) != 0)
		return (ret);

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_1) {
		AR5K_EEPROM_READ(offset++, val);
		ee->ee_margin_tx_rx[mode] = val & 0x3f;
	}

	/*
	 * Get values for 802.11b (2.4GHz)
	 */
	mode = AR5K_EEPROM_MODE_11B;
	offset = AR5K_EEPROM_MODES_11B(hal->ah_ee_version);

	if ((ret = ar5k_eeprom_read_ants(hal, &offset, mode)) != 0)
		return (ret);

	AR5K_EEPROM_READ(offset++, val);
	ee->ee_adc_desired_size[mode]	= (int8_t)((val >> 8) & 0xff);
	ee->ee_ob[mode][1]		= (val >> 4) & 0x7;
	ee->ee_db[mode][1]		= val & 0x7;

	if ((ret = ar5k_eeprom_read_modes(hal, &offset, mode)) != 0)
		return (ret);

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_0) {
		AR5K_EEPROM_READ(offset++, val);
		ee->ee_cal_pier[mode][0] =
		    ar5k_eeprom_bin2freq(hal, val & 0xff, mode);
		ee->ee_cal_pier[mode][1] =
		    ar5k_eeprom_bin2freq(hal, (val >> 8) & 0xff, mode);

		AR5K_EEPROM_READ(offset++, val);
		ee->ee_cal_pier[mode][2] =
		    ar5k_eeprom_bin2freq(hal, val & 0xff, mode);
	}

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_1) {
		ee->ee_margin_tx_rx[mode] = (val >> 8) & 0x3f;
	}

	/*
	 * Get values for 802.11g (2.4GHz)
	 */
	mode = AR5K_EEPROM_MODE_11G;
	offset = AR5K_EEPROM_MODES_11G(hal->ah_ee_version);

	if ((ret = ar5k_eeprom_read_ants(hal, &offset, mode)) != 0)
		return (ret);

	AR5K_EEPROM_READ(offset++, val);
	ee->ee_adc_desired_size[mode]	= (int8_t)((val >> 8) & 0xff);
	ee->ee_ob[mode][1]		= (val >> 4) & 0x7;
	ee->ee_db[mode][1]		= val & 0x7;

	if ((ret = ar5k_eeprom_read_modes(hal, &offset, mode)) != 0)
		return (ret);

	if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_0) {
		AR5K_EEPROM_READ(offset++, val);
		ee->ee_cal_pier[mode][0] =
		    ar5k_eeprom_bin2freq(hal, val & 0xff, mode);
		ee->ee_cal_pier[mode][1] =
		    ar5k_eeprom_bin2freq(hal, (val >> 8) & 0xff, mode);

		AR5K_EEPROM_READ(offset++, val);
		ee->ee_turbo_max_power[mode] = val & 0x7f;
		ee->ee_xr_power[mode] = (val >> 7) & 0x3f;

		AR5K_EEPROM_READ(offset++, val);
		ee->ee_cal_pier[mode][2] =
		    ar5k_eeprom_bin2freq(hal, val & 0xff, mode);

		if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_1) {
			ee->ee_margin_tx_rx[mode] = (val >> 8) & 0x3f;
		}

		AR5K_EEPROM_READ(offset++, val);
		ee->ee_i_cal[mode] = (val >> 8) & 0x3f;
		ee->ee_q_cal[mode] = (val >> 3) & 0x1f;

		if (hal->ah_ee_version >= AR5K_EEPROM_VERSION_4_2) {
			AR5K_EEPROM_READ(offset++, val);
			ee->ee_cck_ofdm_gain_delta = val & 0xff;
		}
	}

	/*
	 * Read 5GHz EEPROM channels
	 */

	return (0);
}

int
ar5k_eeprom_read_mac(struct ath_hal *hal, u_int8_t *mac)
{
	u_int32_t total, offset;
	u_int16_t data;
	int octet;
	u_int8_t mac_d[IEEE80211_ADDR_LEN];

	bzero(mac, IEEE80211_ADDR_LEN);
	bzero(&mac_d, IEEE80211_ADDR_LEN);

	if (hal->ah_eeprom_read(hal, 0x20, &data) != 0)
		return (EIO);

	for (offset = 0x1f, octet = 0, total = 0;
	     offset >= 0x1d; offset--) {
		if (hal->ah_eeprom_read(hal, offset, &data) != 0)
			return (EIO);

		total += data;
		mac_d[octet + 1] = data & 0xff;
		mac_d[octet] = data >> 8;
		octet += 2;
	}

	bcopy(mac_d, mac, IEEE80211_ADDR_LEN);

	if ((!total) || total == (3 * 0xffff))
		return (EINVAL);

	return (0);
}

HAL_BOOL
ar5k_eeprom_regulation_domain(struct ath_hal *hal, HAL_BOOL write,
    ieee80211_regdomain_t *regdomain)
{
	/* Read current value */
	if (write != AH_TRUE) {
		*regdomain = hal->ah_capabilities.cap_regdomain.reg_current;
		return (AH_TRUE);
	}

	/* Try to write a new value */
	hal->ah_capabilities.cap_regdomain.reg_current = *regdomain;

	if (hal->ah_capabilities.cap_eeprom.ee_protect &
	    AR5K_EEPROM_PROTECT_WR_128_191)
		return (AH_FALSE);

	if (hal->ah_eeprom_write(hal, AR5K_EEPROM_REG_DOMAIN,
	    hal->ah_capabilities.cap_eeprom.ee_regdomain) != 0)
		return (AH_FALSE);

	hal->ah_capabilities.cap_eeprom.ee_regdomain =
	    ar5k_regdomain_from_ieee(*regdomain);

	return (AH_TRUE);
}

/*
 * PHY/RF access functions
 */

HAL_BOOL
ar5k_channel(struct ath_hal *hal, HAL_CHANNEL *channel)
{
	HAL_BOOL ret;

	/*
	 * Check bounds supported by the PHY
	 * (don't care about regulation restrictions at this point)
	 */
	if ((channel->channel < hal->ah_capabilities.cap_range.range_2ghz_min ||
	    channel->channel > hal->ah_capabilities.cap_range.range_2ghz_max) &&
	    (channel->channel < hal->ah_capabilities.cap_range.range_5ghz_min ||
	    channel->channel > hal->ah_capabilities.cap_range.range_5ghz_max)) {
		AR5K_PRINTF("channel out of supported range (%u MHz)\n",
		    channel->channel);
		return (AH_FALSE);
	}

	/*
	 * Set the channel and wait
	 */
	if (hal->ah_radio == AR5K_AR5110)
		ret = ar5k_ar5110_channel(hal, channel);
	else if (hal->ah_radio == AR5K_AR5111)
		ret = ar5k_ar5111_channel(hal, channel);
	else
		ret = ar5k_ar5112_channel(hal, channel);

	if (ret == AH_FALSE)
		return (ret);

	hal->ah_current_channel.c_channel = channel->c_channel;
	hal->ah_current_channel.c_channel_flags = channel->c_channel_flags;
	hal->ah_turbo = channel->c_channel_flags == CHANNEL_T ?
	    AH_TRUE : AH_FALSE;

	return (AH_TRUE);
}

u_int32_t
ar5k_ar5110_chan2athchan(HAL_CHANNEL *channel)
{
	u_int32_t athchan;

	/*
	 * Convert IEEE channel/MHz to an internal channel value used
	 * by the AR5210 chipset. This has not been verified with
	 * newer chipsets like the AR5212A who have a completely
	 * different RF/PHY part.
	 */
	athchan = (ar5k_bitswap((ieee80211_mhz2ieee(channel->c_channel,
	    channel->c_channel_flags) - 24) / 2, 5) << 1) |
	    (1 << 6) | 0x1;

	return (athchan);
}

HAL_BOOL
ar5k_ar5110_channel(struct ath_hal *hal, HAL_CHANNEL *channel)
{
	u_int32_t data;

	/*
	 * Set the channel and wait
	 */
	data = ar5k_ar5110_chan2athchan(channel);
	AR5K_PHY_WRITE(0x27, data);
	AR5K_PHY_WRITE(0x30, 0);
	AR5K_DELAY(1000);

	return (AH_TRUE);
}

HAL_BOOL
ar5k_ar5111_chan2athchan(u_int ieee, struct ar5k_athchan_2ghz *athchan)
{
	int channel;

	/* Cast this value to catch negative channel numbers (>= -19) */ 
	channel = (int)ieee;

	/*
	 * Map 2GHz IEEE channel to 5GHz Atheros channel
	 */
	if (channel <= 13) {
		athchan->a2_athchan = 115 + channel;
		athchan->a2_flags = 0x46;
	} else if (channel == 14) {
		athchan->a2_athchan = 124;
		athchan->a2_flags = 0x44;
	} else if (channel >= 15 && channel <= 26) {
		athchan->a2_athchan = ((channel - 14) * 4) + 132;
		athchan->a2_flags = 0x46;
	} else
		return (AH_FALSE);

	return (AH_TRUE);
}

HAL_BOOL
ar5k_ar5111_channel(struct ath_hal *hal, HAL_CHANNEL *channel)
{
	u_int ieee_channel, ath_channel;
	u_int32_t data0, data1, clock;
	struct ar5k_athchan_2ghz ath_channel_2ghz;

	/*
	 * Set the channel on the AR5111 radio
	 */
	data0 = data1 = 0;
	ath_channel = ieee_channel = ath_hal_mhz2ieee(channel->c_channel,
	    channel->c_channel_flags);

	if (channel->c_channel_flags & IEEE80211_CHAN_2GHZ) {
		/* Map 2GHz channel to 5GHz Atheros channel ID */
		if (ar5k_ar5111_chan2athchan(ieee_channel,
			&ath_channel_2ghz) == AH_FALSE)
			return (AH_FALSE);

		ath_channel = ath_channel_2ghz.a2_athchan;
		data0 = ((ar5k_bitswap(ath_channel_2ghz.a2_flags, 8) & 0xff)
		    << 5) | (1 << 4);
	}

	if (ath_channel < 145 || !(ath_channel & 1)) {
		clock = 1;
		data1 = ((ar5k_bitswap(ath_channel - 24, 8) & 0xff) << 2)
		    | (clock << 1) | (1 << 10) | 1;
	} else {
		clock = 0;
		data1 = ((ar5k_bitswap((ath_channel - 24) / 2, 8) & 0xff) << 2)
		    | (clock << 1) | (1 << 10) | 1;
	}

	AR5K_PHY_WRITE(0x27, (data1 & 0xff) | ((data0 & 0xff) << 8));
	AR5K_PHY_WRITE(0x34, ((data1 >> 8) & 0xff) | (data0 & 0xff00));

	return (AH_TRUE);
}

HAL_BOOL
ar5k_ar5112_channel(struct ath_hal *hal, HAL_CHANNEL *channel)
{
	u_int32_t data, data0, data1, data2;
	u_int16_t c;

	data = data0 = data1 = data2 = 0;
	c = channel->c_channel;

	/*
	 * Set the channel on the AR5112 or newer
	 */
	if (c < 4800) {
		if (!((c - 2224) % 5)) {
			data0 = ((2 * (c - 704)) - 3040) / 10;
			data1 = 1;
		} else if (!((c - 2192) % 5)) {
			data0 = ((2 * (c - 672)) - 3040) / 10;
			data1 = 0;
		} else
			return (AH_FALSE);

		data0 = ar5k_bitswap((data0 << 2) & 0xff, 8);
	} else {
		if (!(c % 20) && c >= 5120) {
			data0 = ar5k_bitswap(((c - 4800) / 20 << 2), 8);
			data2 = ar5k_bitswap(3, 2);
		} else if (!(c % 10)) {
			data0 = ar5k_bitswap(((c - 4800) / 10 << 1), 8);
			data2 = ar5k_bitswap(2, 2);
		} else if (!(c % 5)) {
			data0 = ar5k_bitswap((c - 4800) / 5, 8);
			data2 = ar5k_bitswap(1, 2);
		} else
			return (AH_FALSE);
	}

	data = (data0 << 4) | (data1 << 1) | (data2 << 2) | 0x1001;

	AR5K_PHY_WRITE(0x27, data & 0xff);
	AR5K_PHY_WRITE(0x36, (data >> 8) & 0x7f);

	return (AH_TRUE);
}

u_int
ar5k_rfregs_op(u_int32_t *rf, u_int32_t offset, u_int32_t reg, u_int32_t bits,
    u_int32_t first, u_int32_t col, HAL_BOOL set)
{
	u_int32_t mask, entry, last, data, shift, position;
	int32_t left;
	int i;

	if (rf == NULL) {
		/* should not happen */
		return (0);
	}

	if (!(col <= 3 && bits <= 32 && first + bits <= 319)) {
		AR5K_PRINTF("invalid values at offset %u\n", offset);
		return (0);
	}

	entry = ((first - 1) / 8) + offset;
	position = (first - 1) % 8;

	if (set == AH_TRUE)
		data = ar5k_bitswap(reg, bits);

	for (i = shift = 0, left = bits; left > 0; position = 0, entry++, i++) {
		last = (position + left > 8) ? 8 : position + left;
		mask = (((1 << last) - 1) ^ ((1 << position) - 1)) <<
		    (col * 8);

		if (set == AH_TRUE) {
			rf[entry] &= ~mask;
			rf[entry] |= ((data << position) << (col * 8)) & mask;
			data >>= (8 - position);
		} else {
			data = (((rf[entry] & mask) >> (col * 8)) >>
			    position) << shift;
			shift += last - position;
		}

		left -= 8 - position;
	}

	data = set == AH_TRUE ? 1 : ar5k_bitswap(data, bits);

	return (data);
}

u_int32_t
ar5k_rfregs_gainf_corr(struct ath_hal *hal)
{
	u_int32_t mix, step;
	u_int32_t *rf;

	if (hal->ah_rf_banks == NULL)
		return (0);

	rf = hal->ah_rf_banks;
	hal->ah_gain.g_f_corr = 0;

	if (ar5k_rfregs_op(rf, hal->ah_offset[7], 0, 1, 36, 0, AH_FALSE) != 1)
		return (0);

	step = ar5k_rfregs_op(rf, hal->ah_offset[7], 0, 4, 32, 0, AH_FALSE);
	mix = hal->ah_gain.g_step->gos_param[0];

	switch (mix) {
	case 3:
		hal->ah_gain.g_f_corr = step * 2;
		break;
	case 2:
		hal->ah_gain.g_f_corr = (step - 5) * 2;
		break;
	case 1:
		hal->ah_gain.g_f_corr = step;
		break;
	default:
		hal->ah_gain.g_f_corr = 0;
		break;
	}

	return (hal->ah_gain.g_f_corr);
}

HAL_BOOL
ar5k_rfregs_gain_readback(struct ath_hal *hal)
{
	u_int32_t step, mix, level[4];
	u_int32_t *rf;

	if (hal->ah_rf_banks == NULL)
		return (0);

	rf = hal->ah_rf_banks;

	if (hal->ah_radio == AR5K_AR5111) {
		step = ar5k_rfregs_op(rf, hal->ah_offset[7],
		    0, 6, 37, 0, AH_FALSE);
		level[0] = 0;
		level[1] = (step == 0x3f) ? 0x32 : step + 4;
		level[2] = (step != 0x3f) ? 0x40 : level[0];
		level[3] = level[2] + 0x32;

		hal->ah_gain.g_high = level[3] -
		    (step == 0x3f ? AR5K_GAIN_DYN_ADJUST_HI_MARGIN : -5);
		hal->ah_gain.g_low = level[0] +
		    (step == 0x3f ? AR5K_GAIN_DYN_ADJUST_LO_MARGIN : 0);
	} else {
		mix = ar5k_rfregs_op(rf, hal->ah_offset[7],
		    0, 1, 36, 0, AH_FALSE);
		level[0] = level[2] = 0;

		if (mix == 1) {
			level[1] = level[3] = 83;
		} else {
			level[1] = level[3] = 107;
			hal->ah_gain.g_high = 55;
		}
	}

	return ((hal->ah_gain.g_current >= level[0] &&
	    hal->ah_gain.g_current <= level[1]) ||
	    (hal->ah_gain.g_current >= level[2] &&
	    hal->ah_gain.g_current <= level[3]));
}

int32_t
ar5k_rfregs_gain_adjust(struct ath_hal *hal)
{
	int ret = 0;
	const struct ar5k_gain_opt *go;

	go = hal->ah_radio == AR5K_AR5111 ?
	    &ar5111_gain_opt : &ar5112_gain_opt;

	hal->ah_gain.g_step = &go->go_step[hal->ah_gain.g_step_idx];

	if (hal->ah_gain.g_current >= hal->ah_gain.g_high) {
		if (hal->ah_gain.g_step_idx == 0)
			return (-1);
		for (hal->ah_gain.g_target = hal->ah_gain.g_current;
		    hal->ah_gain.g_target >=  hal->ah_gain.g_high &&
		    hal->ah_gain.g_step_idx > 0;
		    hal->ah_gain.g_step =
		    &go->go_step[hal->ah_gain.g_step_idx]) {
			hal->ah_gain.g_target -= 2 *
			    (go->go_step[--(hal->ah_gain.g_step_idx)].gos_gain -
			    hal->ah_gain.g_step->gos_gain);
		}

		ret = 1;
		goto done;
	}

	if (hal->ah_gain.g_current <= hal->ah_gain.g_low) {
		if (hal->ah_gain.g_step_idx == (go->go_steps_count - 1))
			return (-2);
		for (hal->ah_gain.g_target = hal->ah_gain.g_current;
		    hal->ah_gain.g_target <=  hal->ah_gain.g_low &&
		    hal->ah_gain.g_step_idx < (go->go_steps_count - 1);
		    hal->ah_gain.g_step =
		    &go->go_step[hal->ah_gain.g_step_idx]) {
			hal->ah_gain.g_target -= 2 *
			    (go->go_step[++(hal->ah_gain.g_step_idx)].gos_gain -
			    hal->ah_gain.g_step->gos_gain);
		}

		ret = 2;
		goto done;
	}

 done:
#ifdef AR5K_DEBUG
	AR5K_PRINTF("ret %d, gain step %u, current gain %u, target gain %u\n",
	    ret,
	    hal->ah_gain.g_step_idx,
	    hal->ah_gain.g_current,
	    hal->ah_gain.g_target);
#endif

	return (ret);
}

HAL_BOOL
ar5k_rfregs(struct ath_hal *hal, HAL_CHANNEL *channel, u_int mode)
{
	ar5k_rfgain_t *func = NULL;
	HAL_BOOL ret;

	if (hal->ah_radio == AR5K_AR5111) {
		hal->ah_rf_banks_size = sizeof(ar5111_rf);
		func = ar5k_ar5111_rfregs;
	} else if (hal->ah_radio == AR5K_AR5112) {
		hal->ah_rf_banks_size = sizeof(ar5112_rf);
		func = ar5k_ar5112_rfregs;
	} else
		return (AH_FALSE);

	if (hal->ah_rf_banks == NULL) {
		/* XXX do extra checks? */
		if ((hal->ah_rf_banks = malloc(hal->ah_rf_banks_size,
		    M_DEVBUF, M_NOWAIT)) == NULL) {
			AR5K_PRINT("out of memory\n");
			return (AH_FALSE);
		}
	}

	ret = (func)(hal, channel, mode);

	if (ret == AH_TRUE)
		hal->ah_rf_gain = HAL_RFGAIN_INACTIVE;

	return (ret);
}

HAL_BOOL
ar5k_ar5111_rfregs(struct ath_hal *hal, HAL_CHANNEL *channel, u_int mode)
{
	struct ar5k_eeprom_info *ee = &hal->ah_capabilities.cap_eeprom;
	const u_int rf_size = AR5K_ELEMENTS(ar5111_rf);
	u_int32_t *rf;
	int i, obdb = -1, bank = -1;
	u_int32_t ee_mode;

	AR5K_ASSERT_ENTRY(mode, AR5K_INI_VAL_MAX);

	rf = hal->ah_rf_banks;

	/* Copy values to modify them */
	for (i = 0; i < rf_size; i++) {
		if (ar5111_rf[i].rf_bank >=
		    AR5K_AR5111_INI_RF_MAX_BANKS) {
			AR5K_PRINT("invalid bank\n");
			return (AH_FALSE);
		}

		if (bank != ar5111_rf[i].rf_bank) {
			bank = ar5111_rf[i].rf_bank;
			hal->ah_offset[bank] = i;
		}

		rf[i] = ar5111_rf[i].rf_value[mode];
	}

	if (channel->c_channel_flags & IEEE80211_CHAN_2GHZ) {
		if (channel->c_channel_flags & IEEE80211_CHAN_B)
			ee_mode = AR5K_EEPROM_MODE_11B;
		else
			ee_mode = AR5K_EEPROM_MODE_11G;
		obdb = 0;

		if (!ar5k_rfregs_op(rf, hal->ah_offset[0],
			ee->ee_ob[ee_mode][obdb], 3, 119, 0, AH_TRUE))
			return (AH_FALSE);

		if (!ar5k_rfregs_op(rf, hal->ah_offset[0],
			ee->ee_ob[ee_mode][obdb], 3, 122, 0, AH_TRUE))
			return (AH_FALSE);

		obdb = 1;
	} else {
		/* For 11a, Turbo and XR */
		ee_mode = AR5K_EEPROM_MODE_11A;
		obdb = channel->c_channel >= 5725 ? 3 :
		    (channel->c_channel >= 5500 ? 2 :
			(channel->c_channel >= 5260 ? 1 :
			    (channel->c_channel > 4000 ? 0 : -1)));

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_pwd_84, 1, 51, 3, AH_TRUE))
			return (AH_FALSE);

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_pwd_90, 1, 45, 3, AH_TRUE))
			return (AH_FALSE);
	}

	if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
		!ee->ee_xpd[ee_mode], 1, 95, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
		ee->ee_x_gain[ee_mode], 4, 96, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
		obdb >= 0 ? ee->ee_ob[ee_mode][obdb] : 0, 3, 104, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
		obdb >= 0 ? ee->ee_db[ee_mode][obdb] : 0, 3, 107, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[7],
		ee->ee_i_gain[ee_mode], 6, 29, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[7],
		ee->ee_xpd[ee_mode], 1, 4, 0, AH_TRUE))
		return (AH_FALSE);

	/* Write RF values */
	for (i = 0; i < rf_size; i++) {
		AR5K_REG_WAIT(i);
		AR5K_REG_WRITE(ar5111_rf[i].rf_register, rf[i]);
	}

	return (AH_TRUE);
}

HAL_BOOL
ar5k_ar5112_rfregs(struct ath_hal *hal, HAL_CHANNEL *channel, u_int mode)
{
	struct ar5k_eeprom_info *ee = &hal->ah_capabilities.cap_eeprom;
	const u_int rf_size = AR5K_ELEMENTS(ar5112_rf);
	u_int32_t *rf;
	int i, obdb = -1, bank = -1;
	u_int32_t ee_mode;

	AR5K_ASSERT_ENTRY(mode, AR5K_INI_VAL_MAX);

	rf = hal->ah_rf_banks;

	/* Copy values to modify them */
	for (i = 0; i < rf_size; i++) {
		if (ar5112_rf[i].rf_bank >=
		    AR5K_AR5112_INI_RF_MAX_BANKS) {
			AR5K_PRINT("invalid bank\n");
			return (AH_FALSE);
		}

		if (bank != ar5112_rf[i].rf_bank) {
			bank = ar5112_rf[i].rf_bank;
			hal->ah_offset[bank] = i;
		}

		rf[i] = ar5112_rf[i].rf_value[mode];
	}

	if (channel->c_channel_flags & IEEE80211_CHAN_2GHZ) {
		if (channel->c_channel_flags & IEEE80211_CHAN_B)
			ee_mode = AR5K_EEPROM_MODE_11B;
		else
			ee_mode = AR5K_EEPROM_MODE_11G;
		obdb = 0;

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_ob[ee_mode][obdb], 3, 287, 0, AH_TRUE))
			return (AH_FALSE);

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_ob[ee_mode][obdb], 3, 290, 0, AH_TRUE))
			return (AH_FALSE);
	} else {
		/* For 11a, Turbo and XR */
		ee_mode = AR5K_EEPROM_MODE_11A;
		obdb = channel->c_channel >= 5725 ? 3 :
		    (channel->c_channel >= 5500 ? 2 :
			(channel->c_channel >= 5260 ? 1 :
			    (channel->c_channel > 4000 ? 0 : -1)));

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_ob[ee_mode][obdb], 3, 279, 0, AH_TRUE))
			return (AH_FALSE);

		if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
			ee->ee_ob[ee_mode][obdb], 3, 282, 0, AH_TRUE))
			return (AH_FALSE);
	}

#ifdef notyet
	ar5k_rfregs_op(rf, hal->ah_offset[6],
	    ee->ee_x_gain[ee_mode], 2, 270, 0, AH_TRUE);
	ar5k_rfregs_op(rf, hal->ah_offset[6],
	    ee->ee_x_gain[ee_mode], 2, 257, 0, AH_TRUE);
#endif

	if (!ar5k_rfregs_op(rf, hal->ah_offset[6],
		ee->ee_xpd[ee_mode], 1, 302, 0, AH_TRUE))
		return (AH_FALSE);

	if (!ar5k_rfregs_op(rf, hal->ah_offset[7],
		ee->ee_i_gain[ee_mode], 6, 14, 0, AH_TRUE))
		return (AH_FALSE);

	/* Write RF values */
	for (i = 0; i < rf_size; i++)
		AR5K_REG_WRITE(ar5112_rf[i].rf_register, rf[i]);

	return (AH_TRUE);
}

HAL_BOOL
ar5k_rfgain(struct ath_hal *hal, u_int phy, u_int freq)
{
	int i;

	switch (phy) {
	case AR5K_INI_PHY_5111:
	case AR5K_INI_PHY_5112:
		break;
	default:
		return (AH_FALSE);
	}

	switch (freq) {
	case AR5K_INI_RFGAIN_2GHZ:
	case AR5K_INI_RFGAIN_5GHZ:
		break;
	default:
		return (AH_FALSE);
	}

	for (i = 0; i < AR5K_ELEMENTS(ar5k_rfg); i++) {
		AR5K_REG_WAIT(i);
		AR5K_REG_WRITE((u_int32_t)ar5k_rfg[i].rfg_register,
		    ar5k_rfg[i].rfg_value[phy][freq]);
	}

	return (AH_TRUE);
}

/*
 * Common TX power setup
 */
void
ar5k_txpower_table(struct ath_hal *hal, HAL_CHANNEL *channel, int16_t max_power)
{
	u_int16_t txpower, *rates;
	int i;

	rates = hal->ah_txpower.txp_rates;

	txpower = AR5K_TUNE_DEFAULT_TXPOWER * 2;
	if (max_power > txpower) {
		txpower = max_power > AR5K_TUNE_MAX_TXPOWER ?
		    AR5K_TUNE_MAX_TXPOWER : max_power;
	}

	for (i = 0; i < AR5K_MAX_RATES; i++)
		rates[i] = txpower;

	/* XXX setup target powers by rate */

	hal->ah_txpower.txp_min = rates[7];
	hal->ah_txpower.txp_max = rates[0];
	hal->ah_txpower.txp_ofdm = rates[0];

	for (i = 0; i < AR5K_ELEMENTS(hal->ah_txpower.txp_pcdac); i++)
		hal->ah_txpower.txp_pcdac[i] = AR5K_EEPROM_PCDAC_START;
}
