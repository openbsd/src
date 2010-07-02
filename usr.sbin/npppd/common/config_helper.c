/* $OpenBSD: config_helper.c,v 1.3 2010/07/02 21:20:57 yasuoka Exp $ */
/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: config_helper.c,v 1.3 2010/07/02 21:20:57 yasuoka Exp $ */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "properties.h"
#include "config_helper.h"
#include "debugmacro.h"

#define	KEYBUFSZ	512

/**
 * This function concatenates given prefix and the give suffix for making
 * configuration key (("prefix", "suffix") => "prefix.suffix").  The string
 * returned by this function resides in a static memory area.
 */
const char *
config_key_prefix(const char *prefix, const char *suffix)
{
	static char keybuf[KEYBUFSZ];

	strlcpy(keybuf, prefix, sizeof(keybuf));
	strlcat(keybuf, ".", sizeof(keybuf));
	strlcat(keybuf, suffix, sizeof(keybuf));

	return keybuf;
}

/**
 * Retrieve the configuration value as a 'string' that is specified by
 * given configuration key.
 * @param   _this   The pointer to {@link ::properties}
 * @param   confKey configuration key name.
 * @return pointer to the configuration value.  If no configuration value
 * exists then it returns NULL.
 */
const char *
config_str(struct properties *_this, const char *confKey)
{
	ASSERT(_this != NULL)

	return properties_get(_this, confKey);
}

/**
 * Retrieve the configuration value as a 'int' that is specified by
 * given configuration key.
 * @param   _this   The pointer to {@link ::properties}
 * @param   confKey configuration key name.
 * @param   defValue	The default value.  This function will return this
 * value in case no configuration exists
 */
int
config_int(struct properties *_this, const char *confKey, int defValue)
{
	int rval, x;
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	x = sscanf(val, "%d", &rval);

	if (x != 1)
		return defValue;

	return rval;
}

/**
 * Checks whether the configuration value equals given string.
 * @param   _this   The pointer to {@link ::properties}
 * @param   confKey configuration key name.
 * @param   defValue	The default value.  This function will return this
 * value in case no configuration exists
 * @return  return 1 if given string matches the configuration value,
 * otherwise return 0.
 */
int
config_str_equal(struct properties *_this, const char *confKey,
    const char *str, int defValue)
{
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	return (strcmp(val, str) == 0)? 1 : 0;
}

/**
 * Checks whether the configuration value equals given string ignoring
 * case.
 * @param   _this   The pointer to {@link ::properties}
 * @param   confKey configuration key name.
 * @param   defValue	The default value.  This function will return this
 * value in case no configuration exists
 * @return  return 1 if given string equals the configuration value,
 * otherwise return 0.
 */
int
config_str_equali(struct properties *_this, const char *confKey,
    const char *str, int defValue)
{
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	return (strcasecmp(val, str) == 0)? 1 : 0;
}

/***********************************************************************
 * Following functions are to get configuration value by given
 * configuration key.  At first the function will try to get the value
 * by the key with the prefix, if it fails, then it will try to get the
 * value by the key without the prefix.
 *
 * For example, we have following configuration
 *
 *	pppoe.service_name: default_service
 *	PPPoE0.pppoe.service_name: my_service
 *
 * calling
 *
 *	config_prefixed_str(prop, "PPPoE0", "service_name")
 *
 * returns "my_service".  If
 *
 *	PPPoE0.pppoe.service_name: my_service
 *
 * does not exist, then it returns "default_service".
 *
 * Functions that have fixed prefix can be generated by
 * PREFIXED_CONFIG_FUNCTIONS macro that is defined in config_helper.h.
 */
const char  *
config_prefixed_str(struct properties *_this, const char *prefix, const char *confKey)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return val;
	}

	return config_str(_this, confKey);
}

int
config_prefixed_int(struct properties *_this, const char *prefix, const char *confKey, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_int(_this, keybuf, defValue);
	}

	return config_int(_this, confKey, defValue);
}

int
config_prefixed_str_equal(struct properties *_this, const char *prefix, const char *confKey, const char *str,
    int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_str_equal(_this, keybuf, str,
			    defValue);
	}

	return config_str_equal(_this, confKey, str, defValue);
}

int
config_prefixed_str_equali(struct properties *_this, const char *prefix,
    const char *confKey, const char *str, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	ASSERT(_this != NULL);

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_str_equali(_this, keybuf, str,
			    defValue);
	}

	return config_str_equali(_this, confKey, str, defValue);
}

/***********************************************************************
 * Following functions are to get configuration value by given
 * configuration key.  At first the function will try to get the value
 * by the key with the prefix and given label, if it fail, then it will
 * try to get the value by the key without the label.
 *
 * For example, we have following configuration
 *
 *	ipcp.dns_primary: 192.168.0.1
 *	ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * calling
 *
 *  config_named_prefix_str(prop, "ipcp", "ipcp0", "dns_primary");
 *
 * will returns "192.168.0.2".  If
 *
 *	ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * was not exists, then it returns "default_service".
 *
 * Functions that has fixed prefix can be generated by
 * NAMED_PREFIXED_CONFIG_FUNCTIONS macro that is defined in
 * config_helper.h.
 ***********************************************************************/
const char  *
config_named_prefix_str(struct properties *_this, const char *prefix,
    const char *name, const char *confKey)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (name != NULL && name[0] != '\0') {
		snprintf(keybuf, sizeof(keybuf), "%s.%s.%s", prefix, name,
		    confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return val;
	}

	snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
	return config_str(_this, keybuf);
}

int
config_named_prefix_int(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (name != NULL && name[0] != '\0') {
		snprintf(keybuf, sizeof(keybuf), "%s.%s.%s", prefix, name,
		    confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_int(_this, keybuf, defValue);
	}

	snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
	return config_int(_this, keybuf, defValue);
}

int
config_named_prefix_str_equal(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, const char *str, int defValue)
{
	const char *val;

	val = config_named_prefix_str(_this, prefix, name, confKey);
	if (val == NULL)
		return defValue;

	return (strcmp(val, str) == 0)? 1 : 0;
}

int
config_named_prefix_str_equali(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, const char *str, int defValue)
{
	const char *val;

	val = config_named_prefix_str(_this, prefix, name, confKey);
	if (val == NULL)
		return defValue;

	return (strcasecmp(val, str) == 0)? 1 : 0;
}
