/*	$OpenBSD: ieee80211_regdomain.c,v 1.3 2005/02/17 18:28:05 reyk Exp $	*/

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
 * Basic regulation domain extensions for the IEEE 802.11 stack
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>

int	 ieee80211_regdomain_compare_cn(const void *, const void *);
int	 ieee80211_regdomain_compare_rn(const void *, const void *);

static const struct ieee80211_regdomainname
ieee80211_r_names[] = IEEE80211_REGDOMAIN_NAMES;

static const struct ieee80211_regdomainmap
ieee80211_r_map[] = IEEE80211_REGDOMAIN_MAP;

static const struct ieee80211_countryname
ieee80211_r_ctry[] = IEEE80211_REGDOMAIN_COUNTRY_NAMES;

#ifndef bsearch
const void *bsearch(const void *, const void *, size_t, size_t,
    int (*)(const void *, const void *));

const void *
bsearch(const void *key, const void *base0, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *))
{
	const char *base = base0;
	int lim, cmp;
	const void *p;
	
	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		cmp = (*compar)(key, p);
		if (cmp == 0)
			return ((const void *)p);
		if (cmp > 0) {  /* key > p: move right */
			base = (const char *)p + size;
			lim--;
		} /* else move left */
	}
	return (NULL);
}
#endif

int
ieee80211_regdomain_compare_cn(const void *a, const void *b)
{
	return(strcmp(((const struct ieee80211_countryname*)a)->cn_name, 
		   ((const struct ieee80211_countryname*)b)->cn_name));
}

int
ieee80211_regdomain_compare_rn(const void *a, const void *b)
{
	return(strcmp(((const struct ieee80211_regdomainname*)a)->rn_name, 
		   ((const struct ieee80211_regdomainname*)b)->rn_name));
}

u_int16_t
ieee80211_name2countrycode(const char *name)
{
	const struct ieee80211_countryname key = { CTRY_DEFAULT, name }, *value;

	if((value = bsearch(&key, &ieee80211_r_ctry,
		sizeof(ieee80211_r_ctry) / sizeof(ieee80211_r_ctry[0]),
		sizeof(struct ieee80211_countryname),
		ieee80211_regdomain_compare_cn)) != NULL)
		return(value->cn_code);

	return(CTRY_DEFAULT);
}

u_int32_t
ieee80211_name2regdomain(const char *name)
{
	const struct ieee80211_regdomainname key = { DMN_DEFAULT, name }, *value;

	if((value = bsearch(&key, &ieee80211_r_names,
		sizeof(ieee80211_r_names) / sizeof(ieee80211_r_names[0]),
		sizeof(struct ieee80211_regdomainname),
		ieee80211_regdomain_compare_rn)) != NULL)
		return((u_int32_t)value->rn_domain);

	return((u_int32_t)DMN_DEFAULT);
}

const char *
ieee80211_countrycode2name(u_int16_t code)
{
	int i;

	/* Linear search over the table */
	for(i = 0; i < (sizeof(ieee80211_r_ctry) / sizeof(ieee80211_r_ctry[0])); i++)
		if(ieee80211_r_ctry[i].cn_code == code)
			return(ieee80211_r_ctry[i].cn_name);

	return(NULL);
}

const char *
ieee80211_regdomain2name(u_int32_t regdomain)
{
	int i;

	/* Linear search over the table */
	for(i = 0; i < (sizeof(ieee80211_r_names) /
		sizeof(ieee80211_r_names[0])); i++)
		if(ieee80211_r_names[i].rn_domain == regdomain)
			return(ieee80211_r_names[i].rn_name);

	return(ieee80211_r_names[0].rn_name);
}

u_int32_t
ieee80211_regdomain2flag(u_int16_t regdomain, u_int16_t mhz)
{
	int i;
	
	for(i = 0; i < (sizeof(ieee80211_r_map) / 
		sizeof(ieee80211_r_map[0])); i++) {
		if(ieee80211_r_map[i].rm_domain == regdomain) {
			if(mhz >= 2000 && mhz <= 3000)
				return((u_int32_t)ieee80211_r_map[i].rm_domain_2ghz);
			if(mhz >= IEEE80211_CHANNELS_5GHZ_MIN && 
			    mhz <= IEEE80211_CHANNELS_5GHZ_MAX)
				return((u_int32_t)ieee80211_r_map[i].rm_domain_5ghz);
		}
	}

	return((u_int32_t)DMN_DEBUG);
}

u_int32_t
ieee80211_countrycode2regdomain(u_int16_t code)
{
	int i;

	for (i = 0;
	     i < (sizeof(ieee80211_r_ctry) / sizeof(ieee80211_r_ctry[0])); i++)
		if (ieee80211_r_ctry[i].cn_code == code)
			return (ieee80211_r_ctry[i].cn_domain);

	return((u_int32_t)DMN_DEFAULT);
}
