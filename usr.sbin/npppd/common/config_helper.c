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
/* $Id: config_helper.c,v 1.1 2010/01/11 04:20:57 yasuoka Exp $ */
/**@file コンフィグヘルパ。
 * <p>
 * しています。</p>
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "properties.h"
#include "config_helper.h"
#include "debugmacro.h"

#define	KEYBUFSZ	512

/**
 * コンフィグキーを作成するための文字列連結
 * (("prefix", "suffix") => "prefix.suffix") を、行います。内部で固定のバッファ
 * 領域を返します。
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
 * 設定を文字列で返します。
 *
 * @param   _this   {@link ::properties}へのポインタ。
 * @param   confKey 設定ファイルの設定項目名
 * @return  設定値。設定が存在しない場合には NULL が返ります。
 */
const char *
config_str(struct properties *_this, const char *confKey)
{
	ASSERT(_this != NULL)

	return properties_get(_this, confKey);
}

/**
 * 設定を int で返します。
 *
 * @param   _this   	{@link ::properties}へのポインタ。
 * @param   confKey 	設定ファイルの設定項目名
 * @param   defValue	設定が省略されている場合のデフォルトの値
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
		/* 関数のインタフェースを変更して、エラーは区別すべきかも */
		return defValue;

	return rval;
}

/**
 * 設定があたえられた文字列と一致するかどうかを返します。
 *
 * @param   _this   	{@link ::properties}へのポインタ。
 * @param   confKey 	設定ファイルの設定項目名
 * @param   defValue	設定が省略されている場合のデフォルトの値
 * @return  一致する場合には 1、一致しない場合には 0 が返ります。
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
 * 設定があたえられた文字列と一致するかどうかを返します。ASCII 文字の
 * 大文字小文字は無視します。
 *
 * @param   _this   	{@link ::properties}へのポインタ。
 * @param   confKey 	設定ファイルの設定項目名
 * @param   defValue	設定が省略されている場合のデフォルトの値
 * @return  一致する場合には 1、一致しない場合には 0 が返ります。
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
 * 設定項目名に指定したプレフィックスをつけて設定を取得し、設定がなければ
 * プレフィックスなしの設定項目で設定を取得するための関数です。
 *
 * たとえば
 * 
 * pppoe.service_name: default_service
 * PPPoE0.pppoe.service_name: my_service
 *
 * という設定があった場合、
 *  config_prefixed_str(prop, "PPPoE0", "service_name")
 * を呼び出すと "my_service" が取得できます。設定に、
 *
 * PPPoE0.pppoe.service_name: my_service
 *
 * がない場合には、"default_service" が取得できます。
 *
 * config_helper.h に定義されている PREFIXED_CONFIG_FUNCTIONS マクロを
 * 使って、プレフィックス部分の指定方法を固定して使うこともできます。
 ***********************************************************************/
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
 * 設定項目名に指定したプレフィックスと指定した名前をつけて設定を取得し、
 * 設定がなければプレフィックスに設定項目で設定を取得するための関数です。
 *
 * たとえば
 * 
 * ipcp.dns_primary: 192.168.0.1
 * ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * という設定があった場合、
 *  config_named_prefix_str(prop, "ipcp", "ipcp0", "dns_primary");
 * を呼び出すと "192.168.0.2" が取得できます。設定に、
 *
 * ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * がない場合には、"192.168.0.1" が取得できます。
 *
 * config_helper.h に定義されている NAMED_PREFIX_CONFIG_FUNCTIONS マクロ
 * を使って、プレフィックス部分の指定方法を固定して使うこともできます。
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
