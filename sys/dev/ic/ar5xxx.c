/*	$OpenBSD: ar5xxx.c,v 1.4 2004/11/06 03:05:20 reyk Exp $	*/

/*
 * Copyright (c) 2004 Reyk Floeter <reyk@vantronix.net>.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY
 * SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * HAL interface for Atheros Wireless LAN devices.
 * (Please have a look at ar5k.h for further information)
 */

#include <dev/pci/pcidevs.h>

#include <dev/ic/ar5xxx.h>

extern ar5k_attach_t ar5k_ar5210_attach;

#ifdef notyet
extern ar5k_attach_t ar5k_ar5211_attach;
extern ar5k_attach_t ar5k_ar5212_attach;
#endif

static const struct
ieee80211_regchannel ar5k_5ghz_channels[] = IEEE80211_CHANNELS_5GHZ;

static const struct
ieee80211_regchannel ar5k_2ghz_channels[] = IEEE80211_CHANNELS_2GHZ;

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
#ifdef notyet
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
	  ar5k_ar5212_attach },
#endif
};

/*
 * Perform a lookup if the device is supported by the HAL
 */
const char *
ath_hal_probe(vendor, device)
	u_int16_t vendor;
	u_int16_t device;
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
ath_hal_attach(device, sc, st, sh, status)
	u_int16_t device;
	void *sc;
	bus_space_tag_t st;
	bus_space_handle_t sh;
	int *status;
{
	HAL_RATE_TABLE rt_11a = AR5K_RATES_11A;
	HAL_RATE_TABLE rt_11b = AR5K_RATES_11B;
	HAL_RATE_TABLE rt_11g = AR5K_RATES_11G;
	HAL_RATE_TABLE rt_turbo = AR5K_RATES_TURBO;
	struct ath_hal *hal = NULL;
	ar5k_attach_t *attach;
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
		AR5K_PRINTF("device not supported\n");
		return (NULL);
	}

        if ((hal = malloc(sizeof(struct ath_hal),
		 M_DEVBUF, M_NOWAIT)) == NULL) {
		*status = ENOMEM;
		AR5K_PRINTF("out of memory\n");
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
	hal->ah_op_mode = HAL_M_STA;
	hal->ah_radar.r_enabled = AR5K_TUNE_RADAR_ALERT;
	hal->ah_capabilities.cap_eeprom.ee_regdomain = DMN_DEFAULT;
	hal->ah_turbo = AH_FALSE;
	hal->ah_imr = 0;
	hal->ah_atim_window = 0;
	hal->ah_aifs = AR5K_TUNE_AIFS;
	hal->ah_cw_min = AR5K_TUNE_CWMIN;
	hal->ah_limit_tx_retries = AR5K_INIT_TX_RETRY;
	hal->ah_software_retry = AH_FALSE;

	if (attach(device, hal, st, sh, status) == NULL)
		goto failed;

	/*
	 * Get card capabilities, values, ...
	 */

	if (hal->ah_get_capabilities(hal) != AH_TRUE) {
		AR5K_PRINTF("unable to get device capabilities\n");
		goto failed;
	}

	if ((*status = ar5k_eeprom_read_mac(hal, mac)) != HAL_OK) {
		AR5K_PRINTF("unable to read address from EEPROM\n");
		goto failed;
	}

	hal->ah_setMacAddress(hal, mac);

	if (hal->ah_capabilities.cap_mode & HAL_MODE_11A)
		ar5k_rt_copy(&hal->ah_rt_11a, &rt_11a);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11B)
		ar5k_rt_copy(&hal->ah_rt_11b, &rt_11b);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_11G)
		ar5k_rt_copy(&hal->ah_rt_11g, &rt_11g);
	if (hal->ah_capabilities.cap_mode & HAL_MODE_TURBO)
		ar5k_rt_copy(&hal->ah_rt_turbo, &rt_turbo);

	*status = HAL_OK;

	return (hal);

 failed:
	free(hal, M_DEVBUF);
	return (NULL);
}

u_int16_t
ath_hal_computetxtime(hal, rates, frame_length, rate_index, short_preamble)
	struct ath_hal *hal;
	const HAL_RATE_TABLE *rates;
	u_int32_t frame_length;
	u_int16_t rate_index;
	HAL_BOOL short_preamble;
{
	HAL_RATE *rate;
	u_int32_t value;

	AR5K_ASSERT_ENTRY(rate_index, rates->rateCount);

	/*
	 * Get rate by index
	 */
	rate = (HAL_RATE*)&rates->info[rate_index];

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
ath_hal_mhz2ieee(mhz, flags)
	u_int mhz;
	u_int flags;
{
	return (ieee80211_mhz2ieee(mhz, flags));
}

u_int
ath_hal_ieee2mhz(ieee, flags)
	u_int ieee;
	u_int flags;
{
	return (ieee80211_ieee2mhz(ieee, flags));
}

HAL_BOOL
ath_hal_init_channels(hal, channels, max_channels, channels_size, country, mode,
    outdoor, extended)
	struct ath_hal *hal;
	HAL_CHANNEL *channels;
	u_int max_channels;
	u_int *channels_size;
	HAL_CTRY_CODE country;
	u_int16_t mode;
	HAL_BOOL outdoor;
	HAL_BOOL extended;
{
	u_int i, c;
	u_int32_t domain_current;
	u_int domain_5ghz, domain_2ghz;
	HAL_CHANNEL all_channels[max_channels];

	c = 0;
	domain_current = hal->ah_getRegDomain(hal);
	domain_5ghz = ieee80211_regdomain2flag(domain_current,
	    IEEE80211_CHANNELS_5GHZ_MIN);
	domain_2ghz = ieee80211_regdomain2flag(domain_current,
	    IEEE80211_CHANNELS_2GHZ_MIN);

	/*
	 * Create channel list based on chipset capabilities, regulation domain
	 * and mode. 5GHz...
	 */
	for (i = 0; (hal->ah_capabilities.cap_range.range_5ghz_max > 0) &&
		 (i < (sizeof(ar5k_5ghz_channels) /
		     sizeof(ar5k_5ghz_channels[0]))) &&
		 (c < max_channels); i++) {
		/* Check if channel is supported by the chipset */
		if ((ar5k_5ghz_channels[i].rc_channel <
			hal->ah_capabilities.cap_range.range_5ghz_min) ||
		    (ar5k_5ghz_channels[i].rc_channel >
			hal->ah_capabilities.cap_range.range_5ghz_max))
			continue;

		/* Match regulation domain */
		if ((IEEE80211_DMN(ar5k_5ghz_channels[i].rc_domains) &
			IEEE80211_DMN(domain_5ghz)) == 0)
			continue;

		/* Match modes */
		if (ar5k_5ghz_channels[i].rc_mode & IEEE80211_CHAN_TURBO)
			all_channels[c].channelFlags = CHANNEL_T;
		else if (ar5k_5ghz_channels[i].rc_mode & IEEE80211_CHAN_OFDM)
			all_channels[c].channelFlags = CHANNEL_A;
		else
			continue;

		/* Write channel and increment counter */
		all_channels[c++].channel = ar5k_5ghz_channels[i].rc_channel;
	}

	/*
	 * ...and 2GHz.
	 */
	for (i = 0; (hal->ah_capabilities.cap_range.range_2ghz_max > 0) &&
		 (i < (sizeof(ar5k_2ghz_channels) /
		     sizeof(ar5k_2ghz_channels[0]))) &&
		 (c < max_channels); i++) {
		/* Check if channel is supported by the chipset */
		if ((ar5k_2ghz_channels[i].rc_channel <
			hal->ah_capabilities.cap_range.range_2ghz_min) ||
		    (ar5k_2ghz_channels[i].rc_channel >
			hal->ah_capabilities.cap_range.range_2ghz_max))
			continue;

		/* Match regulation domain */
		if ((IEEE80211_DMN(ar5k_2ghz_channels[i].rc_domains) &
			IEEE80211_DMN(domain_2ghz)) == 0)
			continue;

		/* Match modes */
		if (ar5k_2ghz_channels[i].rc_mode & IEEE80211_CHAN_CCK)
			all_channels[c].channelFlags = CHANNEL_B;
		else if (ar5k_2ghz_channels[i].rc_mode & IEEE80211_CHAN_TURBO)
			all_channels[c].channelFlags = CHANNEL_TG;
		else if (ar5k_2ghz_channels[i].rc_mode & IEEE80211_CHAN_OFDM)
			all_channels[c].channelFlags = CHANNEL_G;
		else
			continue;

		/* Write channel and increment counter */
		all_channels[c++].channel = ar5k_2ghz_channels[i].rc_channel;
	}

	memcpy(channels, &all_channels, sizeof(all_channels));
	*channels_size = c;

	return (AH_TRUE);
}

/*
 * Common internal functions
 */

void
ar5k_radar_alert(hal)
	struct ath_hal *hal;
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
	hal->ah_radar.r_last_channel.channelFlags =
	    hal->ah_current_channel.channelFlags;
	hal->ah_radar.r_last_alert = tick;

	AR5K_PRINTF("Possible radar activity detected at %u MHz (tick %u)\n",
	    hal->ah_radar.r_last_alert, hal->ah_current_channel.channel);
}

int
ar5k_eeprom_read_mac(hal, mac)
	struct ath_hal *hal;
	u_int8_t *mac;
{
	u_int32_t total, offset;
	u_int16_t data;
	int octet;
	u_int8_t mac_d[IEEE80211_ADDR_LEN];

	bzero(mac, IEEE80211_ADDR_LEN);
	bzero(&mac_d, IEEE80211_ADDR_LEN);

	if (hal->ah_eeprom_is_busy(hal))
		return (EBUSY);

	/*
	 * XXX Does this work with newer EEPROMs?
	 */
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

	memcpy(mac, &mac_d, IEEE80211_ADDR_LEN);

	if ((!total) || total == (3 * 0xffff))
		return (EINVAL);

	return (0);
}

u_int8_t
ar5k_regdomain_from_ieee(regdomain)
	ieee80211_regdomain_t *regdomain;
{
	/*
	 * XXX Fix
	 */
	return ((u_int8_t)*regdomain);
}

ieee80211_regdomain_t *
ar5k_regdomain_to_ieee(regdomain)
	u_int8_t regdomain;
{
	/*
	 * XXX Fix
	 */
	return ((ieee80211_regdomain_t*)&regdomain);
}

u_int32_t
ar5k_bitswap(val, bits)
	u_int32_t val;
	u_int bits;
{
	u_int32_t retval = 0, bit, i;

	for (i = 0; i < bits; i++) {
		bit = (val >> i) & 1;
		retval = (retval << 1) | bit;
	}

	return (retval);
}

u_int
ar5k_htoclock(usec, turbo)
	u_int usec;
	HAL_BOOL turbo;
{
	return (turbo == AH_TRUE ? (usec * 80) : (usec * 40));
}

u_int
ar5k_clocktoh(clock, turbo)
	u_int clock;
	HAL_BOOL turbo;
{
	return (turbo == AH_TRUE ? (clock / 80) : (clock / 40));
}

void
ar5k_rt_copy(dst, src)
	HAL_RATE_TABLE *dst;
	HAL_RATE_TABLE *src;
{
	memset(dst, 0, sizeof(HAL_RATE_TABLE));
	dst->rateCount = src->rateCount;
	memcpy(&dst->info, &src->info, sizeof(dst->info));
}

HAL_BOOL
ar5k_register_timeout(hal, reg, flag, val, is_set)
	struct ath_hal *hal;
	u_int32_t reg;
	u_int32_t flag;
	u_int32_t val;
	HAL_BOOL is_set;
{
	int i;

	for (i = AR5K_TUNE_REGISTER_TIMEOUT; i > 0; i--) {
		if ((is_set == AH_TRUE) && (AR5K_REG_READ(reg) & flag))
			break;
		else if ((AR5K_REG_READ(reg) & flag) == val)
			break;
		AR5K_DELAY(15);
	}

	if (i <= 0)
		return (AH_FALSE);

	return (AH_TRUE);
}
