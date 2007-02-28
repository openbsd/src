/*	$OpenBSD: onewiredevs_data.h,v 1.5 2007/02/28 22:31:35 deraadt Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * Generated from:
 *	OpenBSD: onewiredevs,v 1.3 2007/02/28 21:20:22 grange Exp 
 */

struct onewire_family {
	int		of_type;
	const char	*of_name;
};

static const struct onewire_family onewire_famtab[] = {
	{ ONEWIRE_FAMILY_DS1990, "ID" },
	{ ONEWIRE_FAMILY_DS1991, "MultiKey" },
	{ ONEWIRE_FAMILY_DS1994, "4kb NVRAM + RTC" },
	{ ONEWIRE_FAMILY_DS1993, "4kb NVRAM" },
	{ ONEWIRE_FAMILY_DS1992, "1kb NVRAM" },
	{ ONEWIRE_FAMILY_DS1982, "1kb EPROM" },
	{ ONEWIRE_FAMILY_DS1995, "16kb NVRAM" },
	{ ONEWIRE_FAMILY_DS1996, "64kb NVRAM" },
	{ ONEWIRE_FAMILY_DS1920, "Temperature" },
	{ ONEWIRE_FAMILY_DS2438, "Smart Battery Monitor" },
	{ ONEWIRE_FAMILY_DS195X, "Java" },
	{ 0, NULL }
};
