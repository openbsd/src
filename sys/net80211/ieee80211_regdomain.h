/*	$OpenBSD: ieee80211_regdomain.h,v 1.2 2004/11/06 18:31:41 reyk Exp $	*/

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

#ifndef _NET80211_IEEE80211_REGDOMAIN_H_
#define _NET80211_IEEE80211_REGDOMAIN_H_

typedef u_int32_t ieee80211_regdomain_t;

enum ieee80211_regdomain {
	DMN_DEFAULT		= 0x00,
	DMN_NULL1_WORLD		= 0x03,
	DMN_NULL1_ETSIB		= 0x07,
	DMN_NULL1_ETSIC		= 0x08,
	DMN_FCC1_FCCA		= 0x10,
	DMN_FCC1_WORLD		= 0x11,
	DMN_FCC2_FCCA		= 0x20,
	DMN_FCC2_WORLD		= 0x21,
	DMN_FCC2_ETSIC		= 0x22,
	DMN_FRANCE_RES		= 0x31,
	DMN_FCC3_FCCA		= 0x3A,
	DMN_ETSI1_WORLD		= 0x37,
	DMN_ETSI3_ETSIA		= 0x32,
	DMN_ETSI2_WORLD		= 0x35,
	DMN_ETSI3_WORLD		= 0x36,
	DMN_ETSI4_WORLD		= 0x30,
	DMN_ETSI4_ETSIC		= 0x38,
	DMN_ETSI5_WORLD		= 0x39,
	DMN_ETSI6_WORLD		= 0x34,
	DMN_ETSI_RESERVED       = 0x33,
	DMN_MKK1_MKKA		= 0x40,
	DMN_MKK1_MKKB		= 0x41,
	DMN_APL4_WORLD		= 0x42,
	DMN_MKK2_MKKA		= 0x43,
	DMN_APL_RESERVED	= 0x44,
	DMN_APL2_WORLD		= 0x45,
	DMN_APL2_APLC		= 0x46,
	DMN_APL3_WORLD		= 0x47,
	DMN_MKK1_FCCA		= 0x48,
	DMN_APL2_APLD		= 0x49,
	DMN_MKK1_MKKA1		= 0x4A,
	DMN_MKK1_MKKA2		= 0x4B,
	DMN_APL1_WORLD		= 0x52,
	DMN_APL1_FCCA		= 0x53,
	DMN_APL1_APLA		= 0x54,
	DMN_APL1_ETSIC		= 0x55,
	DMN_APL2_ETSIC		= 0x56,
	DMN_APL5_WORLD		= 0x58,
	DMN_WOR0_WORLD		= 0x60,
	DMN_WOR1_WORLD		= 0x61,
	DMN_WOR2_WORLD		= 0x62,
	DMN_WOR3_WORLD		= 0x63,
	DMN_WOR4_WORLD		= 0x64,
	DMN_WOR5_ETSIC		= 0x65,
	DMN_WOR01_WORLD		= 0x66,
	DMN_WOR02_WORLD		= 0x67,
	DMN_EU1_WORLD		= 0x68,
	DMN_WOR9_WORLD		= 0x69,
	DMN_WORA_WORLD		= 0x6A,

	DMN_APL1		= 0xf0000001,
	DMN_APL2		= 0xf0000002,
	DMN_APL3		= 0xf0000004,
	DMN_APL4		= 0xf0000008,
	DMN_APL5		= 0xf0000010,
	DMN_ETSI1		= 0xf0000020,
	DMN_ETSI2		= 0xf0000040,
	DMN_ETSI3		= 0xf0000080,
	DMN_ETSI4		= 0xf0000100,
	DMN_ETSI5		= 0xf0000200,
	DMN_ETSI6		= 0xf0000400,
	DMN_ETSIA		= 0xf0000800,
	DMN_ETSIB		= 0xf0001000,
	DMN_ETSIC		= 0xf0002000,
	DMN_FCC1		= 0xf0004000,
	DMN_FCC2		= 0xf0008000,
	DMN_FCC3		= 0xf0010000,
	DMN_FCCA		= 0xf0020000,
	DMN_APLD		= 0xf0040000,
	DMN_MKK1		= 0xf0080000,
	DMN_MKK2		= 0xf0100000,
	DMN_MKKA		= 0xf0200000,
	DMN_NULL1		= 0xf0400000,
	DMN_WORLD		= 0xf0800000,
	DMN_DEBUG               = 0xf1000000,  /* used for debugging */
};

#define IEEE80211_DMN(_d)	((_d) & ~0xf0000000)

struct ieee80211_regdomainname {
	u_int32_t	rn_domain;
	const char 	*rn_name;
};

#define IEEE80211_REGDOMAIN_NAMES {					\
	{ DMN_APL1,		    "APL1" }, 				\
	{ DMN_APL1_APLA,	    "APL1_APLA" }, 			\
	{ DMN_APL1_ETSIC,	    "APL1_ETSIC" }, 			\
	{ DMN_APL1_FCCA,	    "APL1_FCCA" }, 			\
	{ DMN_APL1_WORLD,	    "APL1_WORLD" }, 			\
	{ DMN_APL2,		    "APL2" }, 				\
	{ DMN_APL2_APLC,	    "APL2_APLC" }, 			\
	{ DMN_APL2_APLD,	    "APL2_APLD" }, 			\
	{ DMN_APL2_ETSIC,	    "APL2_ETSIC" }, 			\
	{ DMN_APL2_WORLD,	    "APL2_WORLD" }, 			\
	{ DMN_APL3,		    "APL3" },				\
	{ DMN_APL3_WORLD,	    "APL3_WORLD" },			\
	{ DMN_APL4,		    "APL4" },				\
	{ DMN_APL4_WORLD,	    "APL4_WORLD" },			\
	{ DMN_APL5,		    "APL5" },				\
	{ DMN_APL5_WORLD,	    "APL5_WORLD" },			\
	{ DMN_APLD,		    "APLD" },				\
	{ DMN_APL_RESERVED,	    "APL_RESERVED" },			\
	{ DMN_DEBUG,		    "DEBUG" },				\
	{ DMN_ETSI1,		    "ETSI1" },				\
	{ DMN_ETSI1_WORLD,	    "ETSI1_WORLD" },			\
	{ DMN_ETSI2,		    "ETSI2" },				\
	{ DMN_ETSI2_WORLD,	    "ETSI2_WORLD" },			\
	{ DMN_ETSI3,		    "ETSI3" },				\
	{ DMN_ETSI3_ETSIA,	    "ETSI3_ETSIA" },			\
	{ DMN_ETSI3_WORLD,	    "ETSI3_WORLD," },			\
	{ DMN_ETSI4,		    "ETSI4" },				\
	{ DMN_ETSI4_ETSIC,	    "ETSI4_ETSIC" },			\
	{ DMN_ETSI4_WORLD,	    "ETSI4_WORLD" },			\
	{ DMN_ETSI5,		    "ETSI5" },				\
	{ DMN_ETSI5_WORLD,	    "ETSI5_WORLD" },			\
	{ DMN_ETSI6,		    "ETSI6" },				\
	{ DMN_ETSI6_WORLD,	    "ETSI6_WORLD" },			\
	{ DMN_ETSIA,		    "ETSIA" },				\
	{ DMN_ETSIB,		    "ETSIB" },				\
	{ DMN_ETSIC,		    "ETSIC" },				\
	{ DMN_ETSI_RESERVED,	    "ETSI_RESERVED" },			\
	{ DMN_EU1_WORLD,	    "EU1_WORLD" },			\
	{ DMN_FCC1,		    "FCC1" },				\
	{ DMN_FCC1_FCCA,	    "FCC1_FCCA" },			\
	{ DMN_FCC1_WORLD,	    "FCC1_WORLD" },			\
	{ DMN_FCC2,		    "FCC2" },				\
	{ DMN_FCC2_ETSIC,	    "FCC2_ETSIC" },			\
	{ DMN_FCC2_FCCA,	    "FCC2_FCCA" },			\
	{ DMN_FCC2_WORLD,	    "FCC2_WORLD" },			\
	{ DMN_FCC3,		    "FCC3" },				\
	{ DMN_FCC3_FCCA,	    "FCC3_FCCA" },			\
	{ DMN_FCCA,		    "FCCA" },				\
	{ DMN_FRANCE_RES,	    "FRANCE_RES" },			\
	{ DMN_MKK1,		    "MKK1" },				\
	{ DMN_MKK1_FCCA,	    "MKK1_FCCA" },			\
	{ DMN_MKK1_MKKA,	    "MKK1_MKKA" },			\
	{ DMN_MKK1_MKKA1,	    "MKK1_MKKA" },			\
	{ DMN_MKK1_MKKA2,	    "MKK1_MKKA2" },			\
	{ DMN_MKK1_MKKB,	    "MKK1_MKKB" },			\
	{ DMN_MKK2,		    "MKK2" },				\
	{ DMN_MKK2_MKKA,	    "MKK2_MKKA" },			\
	{ DMN_MKKA,		    "MKKA" },				\
	{ DMN_DEFAULT,		    "NONE" },				\
	{ DMN_NULL1,		    "NULL1" },				\
	{ DMN_NULL1_ETSIB,	    "NULL1_ETSIB" },			\
	{ DMN_NULL1_ETSIC,	    "NULL1_ETSIC" },			\
	{ DMN_WOR01_WORLD,	    "WOR01_WORLD" },			\
	{ DMN_WOR02_WORLD,	    "WOR02_WORLD" },			\
	{ DMN_WOR0_WORLD,	    "WOR0_WORLD" },			\
	{ DMN_WOR1_WORLD,	    "WOR1_WORLd" },			\
	{ DMN_WOR2_WORLD,	    "WOR2_WORLD" },			\
	{ DMN_WOR3_WORLD,	    "WOR3_WORLD" },			\
	{ DMN_WOR4_WORLD,	    "WOR4_WORLD" },			\
	{ DMN_WOR5_ETSIC,	    "WOR5_ETSIC" },			\
	{ DMN_WOR9_WORLD,	    "WOR9_WORLD" },			\
	{ DMN_WORA_WORLD,	    "WORA_WORLD" },			\
	{ DMN_NULL1_WORLD,	    "WORLD" },				\
	{ DMN_WORLD,		    "WORLD" },				\
}

struct ieee80211_regdomainmap {
	u_int16_t	rm_domain;
	u_int32_t	rm_domain_5ghz;
	u_int32_t	rm_domain_2ghz;
};

#define IEEE80211_REGDOMAIN_MAP {					\
	{ DMN_DEFAULT,	        DMN_DEBUG,	DMN_DEBUG }, 		\
	{ DMN_NULL1_WORLD,	DMN_NULL1,	DMN_WORLD },		\
	{ DMN_NULL1_ETSIB,	DMN_NULL1,	DMN_ETSIB },		\
	{ DMN_NULL1_ETSIC,	DMN_NULL1,	DMN_ETSIC },		\
	{ DMN_FCC2_FCCA,	DMN_FCC2,	DMN_FCCA  },		\
	{ DMN_FCC2_WORLD,	DMN_FCC2,	DMN_WORLD },		\
	{ DMN_FCC2_ETSIC,	DMN_FCC2,	DMN_ETSIC },		\
	{ DMN_FCC3_FCCA,	DMN_FCC3,	DMN_FCCA },		\
	{ DMN_ETSI1_WORLD,	DMN_ETSI1,	DMN_WORLD },		\
	{ DMN_ETSI2_WORLD,	DMN_ETSI2,	DMN_WORLD },		\
	{ DMN_ETSI3_WORLD,	DMN_ETSI3,	DMN_WORLD },		\
	{ DMN_ETSI4_WORLD,	DMN_ETSI4,	DMN_WORLD },		\
	{ DMN_ETSI5_WORLD,	DMN_ETSI5,	DMN_WORLD },		\
	{ DMN_ETSI6_WORLD,	DMN_ETSI6,	DMN_WORLD },		\
	{ DMN_ETSI3_ETSIA,	DMN_ETSI3,	DMN_WORLD },		\
	{ DMN_FRANCE_RES,	DMN_ETSI3,	DMN_WORLD },		\
	{ DMN_FCC1_WORLD,	DMN_FCC1,	DMN_WORLD },		\
	{ DMN_FCC1_FCCA,	DMN_FCC1,	DMN_FCCA },		\
	{ DMN_APL1_WORLD,	DMN_APL1,	DMN_WORLD },		\
	{ DMN_APL2_WORLD,	DMN_APL2,	DMN_WORLD },		\
	{ DMN_APL3_WORLD,	DMN_APL3,	DMN_WORLD },		\
	{ DMN_APL4_WORLD,	DMN_APL4,	DMN_WORLD },		\
	{ DMN_APL5_WORLD,	DMN_APL5,	DMN_WORLD },		\
	{ DMN_APL1_ETSIC,	DMN_APL1,	DMN_ETSIC },		\
	{ DMN_APL2_ETSIC,	DMN_APL2,	DMN_ETSIC },		\
	{ DMN_APL2_APLD,	DMN_APL2,	DMN_APLD },		\
	{ DMN_MKK1_MKKA,	DMN_MKK1,	DMN_MKKA },		\
	{ DMN_MKK1_MKKB,	DMN_MKK1,	DMN_MKKA },		\
	{ DMN_MKK2_MKKA,	DMN_MKK2,	DMN_MKKA },		\
	{ DMN_MKK1_FCCA,	DMN_MKK1,	DMN_FCCA },		\
	{ DMN_MKK1_MKKA1,	DMN_MKK1,	DMN_MKKA },		\
	{ DMN_MKK1_MKKA2,	DMN_MKK1,	DMN_MKKA },		\
}

enum ieee80211_countrycode {
        CTRY_DEFAULT            = 0,   /* Default domain (NA) */
	CTRY_ALBANIA            = 8,   /* Albania */
	CTRY_ALGERIA            = 12,  /* Algeria */
	CTRY_ARGENTINA          = 32,  /* Argentina */
	CTRY_ARMENIA            = 51,  /* Armenia */
	CTRY_AUSTRALIA          = 36,  /* Australia */
	CTRY_AUSTRIA            = 40,  /* Austria */
	CTRY_AZERBAIJAN         = 31,  /* Azerbaijan */
	CTRY_BAHRAIN            = 48,  /* Bahrain */
	CTRY_BELARUS            = 112, /* Belarus */
	CTRY_BELGIUM            = 56,  /* Belgium */
	CTRY_BELIZE             = 84,  /* Belize */
	CTRY_BOLIVIA            = 68,  /* Bolivia */
	CTRY_BRAZIL             = 76,  /* Brazil */
	CTRY_BRUNEI_DARUSSALAM  = 96,  /* Brunei Darussalam */
	CTRY_BULGARIA           = 100, /* Bulgaria */
	CTRY_CANADA             = 124, /* Canada */
	CTRY_CHILE              = 152, /* Chile */
	CTRY_CHINA              = 156, /* People's Republic of China */
	CTRY_COLOMBIA           = 170, /* Colombia */
	CTRY_COSTA_RICA         = 188, /* Costa Rica */
	CTRY_CROATIA            = 191, /* Croatia */
	CTRY_CYPRUS             = 196, /* Cyprus */
	CTRY_CZECH              = 203, /* Czech Republic */
	CTRY_DENMARK            = 208, /* Denmark */
	CTRY_DOMINICAN_REPUBLIC = 214, /* Dominican Republic */
	CTRY_ECUADOR            = 218, /* Ecuador */
	CTRY_EGYPT              = 818, /* Egypt */
	CTRY_EL_SALVADOR        = 222, /* El Salvador */
	CTRY_ESTONIA            = 233, /* Estonia */
	CTRY_FAEROE_ISLANDS     = 234, /* Faeroe Islands */
	CTRY_FINLAND            = 246, /* Finland */
	CTRY_FRANCE             = 250, /* France */
	CTRY_FRANCE2            = 255, /* France2 */
	CTRY_GEORGIA            = 268, /* Georgia */
	CTRY_GERMANY            = 276, /* Germany */
	CTRY_GREECE             = 300, /* Greece */
	CTRY_GUATEMALA          = 320, /* Guatemala */
	CTRY_HONDURAS           = 340, /* Honduras */
	CTRY_HONG_KONG          = 344, /* Hong Kong S.A.R., P.R.C. */
	CTRY_HUNGARY            = 348, /* Hungary */
	CTRY_ICELAND            = 352, /* Iceland */
	CTRY_INDIA              = 356, /* India */
	CTRY_INDONESIA          = 360, /* Indonesia */
	CTRY_IRAN               = 364, /* Iran */
	CTRY_IRAQ               = 368, /* Iraq */
	CTRY_IRELAND            = 372, /* Ireland */
	CTRY_ISRAEL             = 376, /* Israel */
	CTRY_ITALY              = 380, /* Italy */
	CTRY_JAMAICA            = 388, /* Jamaica */
	CTRY_JAPAN              = 392, /* Japan */
	CTRY_JAPAN1             = 393, /* Japan (JP1) */
	CTRY_JAPAN2             = 394, /* Japan (JP0) */
	CTRY_JAPAN3             = 395, /* Japan (JP1-1) */
	CTRY_JAPAN4             = 396, /* Japan (JE1) */
	CTRY_JAPAN5             = 397, /* Japan (JE2) */
	CTRY_JORDAN             = 400, /* Jordan */
	CTRY_KAZAKHSTAN         = 398, /* Kazakhstan */
	CTRY_KENYA              = 404, /* Kenya */
	CTRY_KOREA_NORTH        = 408, /* North Korea */
	CTRY_KOREA_ROC          = 410, /* South Korea */
	CTRY_KOREA_ROC2         = 411, /* South Korea */
	CTRY_KUWAIT             = 414, /* Kuwait */
	CTRY_LATVIA             = 428, /* Latvia */
	CTRY_LEBANON            = 422, /* Lebanon */
	CTRY_LIBYA              = 434, /* Libya */
	CTRY_LIECHTENSTEIN      = 438, /* Liechtenstein */
	CTRY_LITHUANIA          = 440, /* Lithuania */
	CTRY_LUXEMBOURG         = 442, /* Luxembourg */
	CTRY_MACAU              = 446, /* Macau */
	CTRY_MACEDONIA          = 807, /* the Former Yugoslav Republic of Macedonia */
	CTRY_MALAYSIA           = 458, /* Malaysia */
	CTRY_MEXICO             = 484, /* Mexico */
	CTRY_MONACO             = 492, /* Principality of Monaco */
	CTRY_MOROCCO            = 504, /* Morocco */
	CTRY_NETHERLANDS        = 528, /* Netherlands */
	CTRY_NEW_ZEALAND        = 554, /* New Zealand */
	CTRY_NICARAGUA          = 558, /* Nicaragua */
	CTRY_NORWAY             = 578, /* Norway */
	CTRY_OMAN               = 512, /* Oman */
	CTRY_PAKISTAN           = 586, /* Islamic Republic of Pakistan */
	CTRY_PANAMA             = 591, /* Panama */
	CTRY_PARAGUAY           = 600, /* Paraguay */
	CTRY_PERU               = 604, /* Peru */
	CTRY_PHILIPPINES        = 608, /* Republic of the Philippines */
	CTRY_POLAND             = 616, /* Poland */
	CTRY_PORTUGAL           = 620, /* Portugal */
	CTRY_PUERTO_RICO        = 630, /* Puerto Rico */
	CTRY_QATAR              = 634, /* Qatar */
	CTRY_ROMANIA            = 642, /* Romania */
	CTRY_RUSSIA             = 643, /* Russia */
	CTRY_SAUDI_ARABIA       = 682, /* Saudi Arabia */
	CTRY_SINGAPORE          = 702, /* Singapore */
	CTRY_SLOVAKIA           = 703, /* Slovak Republic */
	CTRY_SLOVENIA           = 705, /* Slovenia */
	CTRY_SOUTH_AFRICA       = 710, /* South Africa */
	CTRY_SPAIN              = 724, /* Spain */
	CTRY_SRI_LANKA          = 728, /* Sri Lanka */
	CTRY_SWEDEN             = 752, /* Sweden */
	CTRY_SWITZERLAND        = 756, /* Switzerland */
	CTRY_SYRIA              = 760, /* Syria */
	CTRY_TAIWAN             = 158, /* Taiwan */
	CTRY_THAILAND           = 764, /* Thailand */
	CTRY_TRINIDAD_Y_TOBAGO  = 780, /* Trinidad y Tobago */
	CTRY_TUNISIA            = 788, /* Tunisia */
	CTRY_TURKEY             = 792, /* Turkey */
	CTRY_UAE                = 784, /* U.A.E. */
	CTRY_UKRAINE            = 804, /* Ukraine */
	CTRY_UNITED_KINGDOM     = 826, /* United Kingdom */
	CTRY_UNITED_STATES      = 840, /* United States */
	CTRY_URUGUAY            = 858, /* Uruguay */
	CTRY_UZBEKISTAN         = 860, /* Uzbekistan */
	CTRY_VENEZUELA          = 862, /* Venezuela */
	CTRY_VIET_NAM           = 704, /* Viet Nam */
	CTRY_YEMEN              = 887, /* Yemen */
	CTRY_ZIMBABWE           = 716, /* Zimbabwe */
        CTRY_DEBUG              = 0x1ff, /* for debugging */
};

struct ieee80211_countryname {
	u_int16_t	cn_code;
	const char 	*cn_name;
	u_int32_t 	cn_domain;
};

#define IEEE80211_REGDOMAIN_COUNTRY_NAMES { 				\
	{ CTRY_DEFAULT,            "00", DMN_DEBUG }, 			\
	{ CTRY_UAE,                "ae", DMN_NULL1_WORLD }, 		\
	{ CTRY_ALBANIA,            "al", DMN_NULL1_WORLD }, 		\
	{ CTRY_ARMENIA,            "am", DMN_ETSI4_WORLD },		\
	{ CTRY_ARGENTINA,          "ar", DMN_APL3_WORLD },		\
	{ CTRY_AUSTRIA,            "at", DMN_ETSI5_WORLD },		\
	{ CTRY_AUSTRALIA,          "au", DMN_FCC2_WORLD },		\
	{ CTRY_AZERBAIJAN,         "az", DMN_ETSI4_WORLD },		\
	{ CTRY_BELGIUM,            "be", DMN_ETSI4_WORLD },		\
	{ CTRY_BULGARIA,           "bg", DMN_ETSI6_WORLD },		\
	{ CTRY_BAHRAIN,            "bh", DMN_NULL1_WORLD },		\
	{ CTRY_BRUNEI_DARUSSALAM,  "bn", DMN_APL1_WORLD },		\
	{ CTRY_BOLIVIA,            "bo", DMN_APL1_ETSIC },		\
	{ CTRY_BRAZIL,             "br", DMN_NULL1_ETSIC },		\
	{ CTRY_BELARUS,            "by", DMN_NULL1_WORLD },		\
	{ CTRY_BELIZE,             "bz", DMN_NULL1_ETSIC },		\
	{ CTRY_CANADA,             "ca", DMN_FCC2_FCCA },		\
	{ CTRY_SWITZERLAND,        "ch", DMN_ETSI2_WORLD },		\
	{ CTRY_CHILE,              "cl", DMN_APL5_WORLD },		\
	{ CTRY_CHINA,              "cn", DMN_APL1_WORLD },		\
	{ CTRY_COLOMBIA,           "co", DMN_FCC1_FCCA },		\
	{ CTRY_COSTA_RICA,         "cr", DMN_NULL1_WORLD },		\
	{ CTRY_CYPRUS,             "cy", DMN_ETSI1_WORLD },		\
	{ CTRY_CZECH,              "cz", DMN_ETSI3_WORLD },		\
	{ CTRY_GERMANY,            "de", DMN_ETSI1_WORLD },		\
	{ CTRY_DENMARK,            "dk", DMN_ETSI1_WORLD },		\
        { CTRY_DOMINICAN_REPUBLIC, "do", DMN_FCC1_FCCA },		\
	{ CTRY_ALGERIA,            "dz", DMN_NULL1_WORLD },		\
	{ CTRY_ECUADOR,            "ec", DMN_NULL1_WORLD },		\
	{ CTRY_ESTONIA,            "ee", DMN_ETSI1_WORLD },		\
	{ CTRY_EGYPT,              "eg", DMN_NULL1_WORLD },		\
	{ CTRY_SPAIN,              "es", DMN_ETSI1_WORLD },		\
	{ CTRY_FRANCE2,            "f2", DMN_ETSI3_WORLD },		\
	{ CTRY_FINLAND,            "fi", DMN_ETSI1_WORLD },		\
	{ CTRY_FAEROE_ISLANDS,     "fo", DMN_NULL1_WORLD },		\
	{ CTRY_FRANCE,             "fr", DMN_ETSI3_WORLD },		\
	{ CTRY_GEORGIA,            "ge", DMN_ETSI4_WORLD },		\
	{ CTRY_GREECE,             "gr", DMN_NULL1_WORLD },		\
	{ CTRY_GUATEMALA,          "gt", DMN_FCC1_FCCA },		\
	{ CTRY_HONG_KONG,          "hk", DMN_FCC2_WORLD },		\
	{ CTRY_HONDURAS,           "hn", DMN_NULL1_WORLD },		\
	{ CTRY_CROATIA,            "hr", DMN_ETSI3_WORLD },		\
	{ CTRY_HUNGARY,            "hu", DMN_ETSI2_WORLD },		\
	{ CTRY_INDONESIA,          "id", DMN_NULL1_WORLD },		\
	{ CTRY_IRELAND,            "ie", DMN_ETSI1_WORLD },		\
	{ CTRY_ISRAEL,             "il", DMN_NULL1_WORLD },		\
	{ CTRY_INDIA,              "in", DMN_NULL1_WORLD },		\
	{ CTRY_IRAQ,               "iq", DMN_NULL1_WORLD },		\
	{ CTRY_IRAN,               "ir", DMN_APL1_WORLD },		\
	{ CTRY_ICELAND,            "is", DMN_ETSI1_WORLD },		\
	{ CTRY_ITALY,              "it", DMN_ETSI1_WORLD },		\
	{ CTRY_JAPAN1,             "j1", DMN_MKK1_MKKB },		\
	{ CTRY_JAPAN2,             "j2", DMN_MKK1_FCCA },		\
	{ CTRY_JAPAN3,             "j3", DMN_MKK2_MKKA },		\
	{ CTRY_JAPAN4,             "j4", DMN_MKK1_MKKA1 },		\
	{ CTRY_JAPAN5,             "j5", DMN_MKK1_MKKA2 },		\
	{ CTRY_JAMAICA,            "jm", DMN_NULL1_WORLD },		\
	{ CTRY_JORDAN,             "jo", DMN_NULL1_WORLD },		\
	{ CTRY_JAPAN,              "jp", DMN_MKK1_MKKA },		\
	{ CTRY_KOREA_ROC2,         "k2", DMN_APL2_APLD },		\
	{ CTRY_KENYA,              "ke", DMN_NULL1_WORLD },		\
	{ CTRY_KOREA_NORTH,        "kp", DMN_APL2_WORLD },		\
	{ CTRY_KOREA_ROC,          "kr", DMN_APL2_WORLD },		\
	{ CTRY_KUWAIT,             "kw", DMN_NULL1_WORLD },		\
	{ CTRY_KAZAKHSTAN,         "kz", DMN_NULL1_WORLD },		\
	{ CTRY_LEBANON,            "lb", DMN_NULL1_WORLD },		\
	{ CTRY_LIECHTENSTEIN,      "li", DMN_ETSI2_WORLD },		\
	{ CTRY_SRI_LANKA,          "lk", DMN_NULL1_WORLD },		\
	{ CTRY_LITHUANIA,          "lt", DMN_ETSI1_WORLD },		\
	{ CTRY_LUXEMBOURG,         "lu", DMN_ETSI1_WORLD },		\
	{ CTRY_LATVIA,             "lv", DMN_NULL1_WORLD },		\
	{ CTRY_LIBYA,              "ly", DMN_NULL1_WORLD },		\
	{ CTRY_MOROCCO,            "ma", DMN_NULL1_WORLD },		\
	{ CTRY_MONACO,             "mc", DMN_ETSI4_WORLD },		\
	{ CTRY_MACEDONIA,          "mk", DMN_NULL1_WORLD },		\
	{ CTRY_MACAU,              "mo", DMN_FCC2_WORLD },		\
	{ CTRY_MEXICO,             "mx", DMN_FCC1_FCCA },		\
	{ CTRY_MALAYSIA,           "my", DMN_NULL1_WORLD },		\
	{ CTRY_NICARAGUA,          "ni", DMN_NULL1_WORLD },		\
	{ CTRY_NETHERLANDS,        "nl", DMN_ETSI1_WORLD },		\
	{ CTRY_NORWAY,             "no", DMN_ETSI1_WORLD },		\
	{ CTRY_NEW_ZEALAND,        "nz", DMN_FCC2_ETSIC },		\
	{ CTRY_OMAN,               "om", DMN_NULL1_WORLD },		\
	{ CTRY_PANAMA,             "pa", DMN_FCC1_FCCA },		\
	{ CTRY_PERU,               "pe", DMN_NULL1_WORLD },		\
	{ CTRY_PHILIPPINES,        "ph", DMN_FCC1_WORLD },		\
	{ CTRY_PAKISTAN,           "pk", DMN_NULL1_WORLD },		\
	{ CTRY_POLAND,             "pl", DMN_ETSI1_WORLD },		\
	{ CTRY_PUERTO_RICO,        "pr", DMN_FCC1_FCCA },		\
	{ CTRY_PORTUGAL,           "pt", DMN_ETSI1_WORLD },		\
	{ CTRY_PARAGUAY,           "py", DMN_NULL1_WORLD },		\
	{ CTRY_QATAR,              "qa", DMN_NULL1_WORLD },		\
	{ CTRY_ROMANIA,            "ro", DMN_NULL1_WORLD },		\
	{ CTRY_RUSSIA,             "ru", DMN_NULL1_WORLD },		\
	{ CTRY_SAUDI_ARABIA,       "sa", DMN_NULL1_WORLD },		\
	{ CTRY_SWEDEN,             "se", DMN_ETSI1_WORLD },		\
	{ CTRY_SINGAPORE,          "sg", DMN_APL4_WORLD },		\
	{ CTRY_SLOVENIA,           "si", DMN_ETSI1_WORLD },		\
	{ CTRY_SLOVAKIA,           "sk", DMN_ETSI3_WORLD },		\
	{ CTRY_EL_SALVADOR,        "sv", DMN_NULL1_WORLD },		\
	{ CTRY_SYRIA,              "sy", DMN_NULL1_WORLD },		\
	{ CTRY_THAILAND,           "th", DMN_APL2_WORLD },		\
	{ CTRY_TUNISIA,            "tn", DMN_ETSI3_WORLD },		\
	{ CTRY_TURKEY,             "tr", DMN_ETSI3_WORLD },		\
	{ CTRY_TRINIDAD_Y_TOBAGO,  "tt", DMN_ETSI4_WORLD },		\
	{ CTRY_TAIWAN,             "tw", DMN_APL3_WORLD },		\
	{ CTRY_UKRAINE,            "ua", DMN_NULL1_WORLD },		\
	{ CTRY_UNITED_KINGDOM,     "uk", DMN_ETSI1_WORLD },		\
	{ CTRY_UNITED_STATES,      "us", DMN_FCC1_FCCA },		\
	{ CTRY_URUGUAY,            "uy", DMN_APL2_WORLD },		\
	{ CTRY_UZBEKISTAN,         "uz", DMN_FCC3_FCCA },		\
	{ CTRY_VENEZUELA,          "ve", DMN_APL2_ETSIC },		\
	{ CTRY_VIET_NAM,           "vn", DMN_NULL1_WORLD },		\
	{ CTRY_YEMEN,              "ye", DMN_NULL1_WORLD },		\
	{ CTRY_SOUTH_AFRICA,       "za", DMN_ETSI1_WORLD },		\
	{ CTRY_ZIMBABWE,           "zw", DMN_NULL1_WORLD },		\
	{ CTRY_DEBUG,              "zz", DMN_DEBUG },			\
}

struct ieee80211_regchannel {
	u_int16_t	rc_channel;
	u_int32_t 	rc_mode;
	u_int32_t 	rc_domains;
};

#define IEEE80211_CHANNELS_2GHZ_MIN	2312
#define IEEE80211_CHANNELS_2GHZ_MAX 	2732
#define IEEE80211_CHANNELS_2GHZ { 					\
        { 2312 /* -19 */, IEEE80211_CHAN_CCK,	DMN_DEBUG }, 		\
        { 2317 /* -18 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2322 /* -17 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2327 /* -16 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2332 /* -15 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2337 /* -14 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2342 /* -13 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2347 /* -12 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2352 /* -11 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2357 /* -10 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2362 /*  -9 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2367 /*  -8 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2372 /*  -7 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2377 /*  -6 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2382 /*  -5 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2387 /*  -4 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2392 /*  -3 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2397 /*  -2 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2402 /*  -1 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2407 /*   0 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2412 /*   1 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2417 /*   2 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2422 /*   3 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2427 /*   4 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2432 /*   5 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2437 /*   6 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2442 /*   7 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2447 /*   8 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2452 /*   9 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2457 /*  10 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2462 /*  11 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2467 /*  12 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2472 /*  13 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2484 /*  14 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2512 /*  15 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2532 /*  16 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2552 /*  17 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2572 /*  18 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2592 /*  19 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2612 /*  20 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2632 /*  21 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2652 /*  22 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2672 /*  23 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2692 /*  24 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2712 /*  25 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
        { 2732 /*  26 */, IEEE80211_CHAN_CCK,	DMN_DEBUG },		\
}

#define IEEE80211_CHANNELS_5GHZ_MIN	4800
#define IEEE80211_CHANNELS_5GHZ_MAX	6100
#define IEEE80211_CHANNELS_5GHZ { 					\
        { 4800, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4805, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4810, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4815, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4820, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4825, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4830, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4835, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4840, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4845, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4850, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4855, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4860, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4865, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4870, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4875, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4880, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4885, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4890, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4895, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4900, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4905, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4910, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4915, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4920, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4925, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4930, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4935, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4940, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4945, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4950, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4955, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4960, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4965, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4970, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4975, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4980, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4985, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4990, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 4995, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5000, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5005, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5010, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5015, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5020, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5025, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5030, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5035, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5040, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5045, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5050, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5055, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5060, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5065, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5070, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5075, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5080, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5085, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5090, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5095, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5100, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5105, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5110, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5115, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5120, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5125, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5130, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5135, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5140, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5145, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5150, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5155, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5160, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5165, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5170, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5175, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5180, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5185, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5190, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5195, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5200, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5205, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5210, IEEE80211_CHAN_TURBO,	DMN_DEBUG | DMN_FCC1 },		\
        { 5215, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5220, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5225, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5230, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5235, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5240, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 | 		\
                                        DMN_FCC3 },			\
        { 5245, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5250, IEEE80211_CHAN_TURBO,	DMN_DEBUG | DMN_FCC1 },		\
        { 5255, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5260, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5265, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5270, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5275, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5280, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5285, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5290, IEEE80211_CHAN_TURBO,	DMN_DEBUG | DMN_FCC1 },		\
        { 5295, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5300, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5305, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5310, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5315, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5320, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5325, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5330, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5335, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5340, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5345, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5350, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5355, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5360, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5365, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5370, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5375, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5380, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5385, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5390, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5395, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5400, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5405, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5410, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5415, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5420, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5425, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5430, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5435, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5440, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5445, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5450, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5455, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5460, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5465, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5470, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5475, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5480, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5485, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5490, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5495, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5500, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5505, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5510, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5515, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5520, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5525, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5530, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5535, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5540, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5545, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5550, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5555, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5560, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5565, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5570, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5575, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5580, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5585, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5590, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5595, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5600, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5605, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5610, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5615, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5620, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5625, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5630, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5635, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5640, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5645, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5650, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5655, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5660, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5665, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5670, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5675, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5680, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5685, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5690, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5695, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5700, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5705, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5710, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5715, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5720, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5725, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5730, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5735, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5740, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5745, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5750, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5755, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5760, IEEE80211_CHAN_TURBO,	DMN_DEBUG | DMN_FCC1 },		\
        { 5765, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5770, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5775, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5780, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5785, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5790, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5795, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5800, IEEE80211_CHAN_TURBO,	DMN_DEBUG | DMN_FCC1 },		\
        { 5805, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC3 },		\
        { 5810, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5815, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5820, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5825, IEEE80211_CHAN_OFDM,	DMN_DEBUG | DMN_FCC1 },		\
        { 5830, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5835, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5840, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5845, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5850, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5855, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5860, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5865, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5870, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5875, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5880, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5885, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5890, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5895, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5900, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5905, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5910, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5915, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5920, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5925, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5930, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5935, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5940, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5945, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5950, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5955, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5960, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5965, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5970, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5975, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5980, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5985, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5990, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 5995, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6000, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6005, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6010, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6015, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6020, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6025, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6030, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6035, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6040, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6045, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6050, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6055, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6060, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6065, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6070, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6075, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6080, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6085, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6090, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6095, IEEE80211_CHAN_OFDM,	DMN_DEBUG },			\
        { 6100, IEEE80211_CHAN_OFDM,    DMN_DEBUG },			\
}

__BEGIN_DECLS

extern u_int16_t	 ieee80211_name2countrycode(const char *);
extern u_int32_t	 ieee80211_name2regdomain(const char *);
extern const char 	*ieee80211_countrycode2name(u_int16_t);
extern const char 	*ieee80211_regdomain2name(u_int32_t);
extern u_int32_t 	 ieee80211_regdomain2flag(u_int16_t, u_int16_t);

__END_DECLS

#endif /* _NET80211_IEEE80211_REGDOMAIN_H_ */
