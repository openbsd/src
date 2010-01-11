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
/*
 * cc -o properties_test -DNO_KANJI=1 properties_test.c properties.c hash.c 
 *
 * ./properties_test 
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

#include "properties.h"

#define	MIN(m,n)	((m) < (n))? (m) : (n)
#define	TEST(f)			\
    {				\
	printf("%-10s .. ", #f);	\
	f();			\
	printf("ok\n");		\
    }

#define ASSERT(x)	\
	if (!(x)) { \
	    fprintf(stderr, \
		"\nASSERT(%s) failed on %s() at %s:%d.\n" \
		, #x, __func__, __FILE__, __LINE__); \
	    abort(); \
	}

static void
set_and_get(const char *key, const char *value)
{
	FILE *f;
	struct properties *props;

	ASSERT((f = fopen("test.properties", "w")) != NULL);
	ASSERT((props = properties_create(512)) != NULL);
	ASSERT(properties_put(props, key, value) != NULL);
	ASSERT(properties_save(props, f) == 0);
	ASSERT(fclose(f) == 0);
	properties_destroy(props);

	ASSERT((props = properties_create(512)) != NULL);
	ASSERT((f = fopen("test.properties", "r")) != NULL);
	ASSERT(properties_load(props, f) == 0);
	ASSERT(strcmp(properties_get(props, key), value) == 0);
	properties_destroy(props);

	ASSERT(fclose(f) == 0);
}

void
test0(void)
{
	set_and_get("hoge", "hogehoge");
	set_and_get("hoge", "hogehogehogehoge");
	set_and_get("hoge", "hogehogehogehogehogehoge");
	set_and_get("hoge", "hogehogehogehogehogehogehogehoge");
	set_and_get("hoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	set_and_get("hoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	set_and_get("hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge", "hogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoehogehogehogehgoe");
}
static void test1()
{
	// [IDGW-DEV 5246] 1024 bytes
	set_and_get("pptpd.listener_in", "PPTP 10.10.10.50 PPTP 100.100.100.100 PPTP 100.100.100.101 PPTP 100.100.100.102 PPTP 100.100.100.103 PPTP 100.100.100.104 PPTP 100.100.100.105 PPTP 100.100.100.106 PPTP 100.100.100.107 PPTP 100.100.100.108 PPTP 100.100.100.109 PPTP 100.100.100.110 PPTP 100.100.100.111 PPTP 100.100.100.112 PPTP 100.100.100.113 PPTP 100.100.100.114 PPTP 100.100.100.115 PPTP 100.100.100.116 PPTP 100.100.100.117 PPTP 100.100.100.118 PPTP 100.100.100.119 PPTP 100.100.100.120 PPTP 100.100.100.121 PPTP 100.100.100.122 PPTP 100.100.100.123 PPTP 100.100.100.124 PPTP 100.100.100.125 PPTP 100.100.100.126 PPTP 100.100.100.127 PPTP 100.100.100.128 PPTP 100.100.100.129 PPTP 100.100.100.130 PPTP 100.100.100.131 PPTP 100.100.100.132 PPTP 100.100.100.133 PPTP 100.100.100.134 PPTP 100.100.100.135 PPTP 100.100.100.136 PPTP 100.100.100.137 PPTP 100.100.100.138 PPTP 100.100.100.139 PPTP 100.100.100.140 PPTP 100.100.100.141 PPTP 100.100.100.142 PPTP 100.100.100.143 PPTP 100.100.100.144 PPTP 100.100.100.145 PPTP 100.100.100.146 PPTP 100.100.100.147 PPTP 100.100.100.148 PPTP 100.100.100.149 PPTP 100.100.100.150 PPTP 100.100.100.151 PPTP 100.100.100.152 PPTP 100.100.100.153 PPTP 100.100.100.154 PPTP 100.100.100.155 PPTP 100.100.100.156 PPTP 100.100.100.157 PPTP 100.100.100.158 PPTP 100.100.100.159 PPTP 100.100.100.160 PPTP 100.100.100.161 PPTP 100.100.100.162 PPTP 100.100.100.163 PPTP 100.100.100.164 PPTP 100.100.100.165 PPTP 100.100.100.166 PPTP 100.100.100.167 PPTP 100.100.100.168 PPTP 100.100.100.169 PPTP 100.100.100.170 PPTP 100.100.100.171 PPTP 100.100.100.172 PPTP 100.100.100.173 PPTP 100.100.100.174 PPTP 100.100.100.175 PPTP 100.100.100.176 PPTP 100.100.100.177 PPTP 100.100.100.178 PPTP 100.100.100.179 PPTP 100.100.100.180 PPTP 100.100.100.181 PPTP 100.100.100.182 PPTP 100.100.100.183 PPTP 100.100.100.184 PPTP 100.100.100.185 PPTP 100.100.100.186 PPTP 100.100.100.187 PPTP 100.100.100.188 PPTP 100.100.100.189 PPTP 100.100.100.190 PPTP 100.100.100.191 PPTP 100.100.100.192 PPTP 100.100.100.193 PPTP 100.100.100.194 PPTP 100.100.100.195 PPTP 100.100.100.196 PPTP 100.100.100.197 PPTP 100.100.100.198 PPTP 100.100.100.200");
	// [IDGW-CVS 9044]
	set_and_get("pptpd.ip4_allow", "pptpd.ip4_allow: 192.168.10.1/32 192.168.10.2/32 192.168.10.3/32 192.168.10.4/32 192.168.10.5/32 192.168.10.6/32 192.168.10.7/32 192.168.10.8/32 192.168.10.9/32 192.168.10.10/32 192.168.10.11/32 192.168.10.12/32 192.168.10.13/32 192.168.10.14/32 192.168.10.15/32 192.168.10.16/32 192.168.10.17/32 192.168.10.18/32 192.168.10.19/32 192.168.10.20/32 192.168.10.21/32 192.168.10.22/32 192.168.10.23/32 192.168.10.24/32 192.168.10.25/32 192.168.10.26/32 192.168.10.27/32 192.168.10.28/32 192.168.10.29/32 192.168.10.30/32 192.168.10.31/32 192.168.10.32/32 192.168.10.33/32 192.168.10.34/32 192.168.10.35/32 192.168.10.36/32 192.168.10.37/32 192.168.10.38/32 192.168.10.39/32 192.168.10.40/32 192.168.10.41/32 192.168.10.42/32 192.168.10.43/32 192.168.10.44/32 192.168.10.45/32 192.168.10.46/32 192.168.10.47/32 192.168.10.48/32 192.168.10.49/32 192.168.10.50/32 192.168.10.51/32 192.168.10.52/32 192.168.10.53/32 192.168.10.54/32 192.168.10.55/32 192.168.10.56/32 192.168.1.57/32 192.168.1.58/32 192.168.1.59/32 192.168.10.60/32 192.168.10.61/32 192.168.10.62/32 192.168.10.63/32 192.168.10.64/32 192.168.10.65/32 192.168.10.66/32 192.168.10.67/32 192.168.10.68/32 192.168.10.69/32 192.168.10.70/32 192.168.10.71/32 192.168.10.72/32 192.168.10.73/32 192.168.10.74/32 192.168.10.75/32 192.168.10.76/32 192.168.10.77/32 192.168.10.78/32 192.168.10.79/32 192.168.10.80/32 192.168.10.81/32 192.168.10.82/32 192.168.10.83/32 192.168.10.84/32 192.168.10.85/32 192.168.10.86/32 192.168.10.87/32 192.168.10.88/32 192.168.10.89/32 192.168.10.90/32 192.168.10.91/32 192.168.10.92/32 192.168.10.93/32 192.168.10.94/32 192.168.10.95/32 192.168.10.96/32 192.168.10.97/32 192.168.10.98/32 192.168.10.99/32 192.168.10.100/32 192.168.10.101/32 192.168.10.102/32 192.168.10.103/32 192.168.10.104/32 192.168.10.105/32 192.168.10.106/32 192.168.10.107/32 192.168.10.108/32 192.168.10.109/32 192.168.10.110/32 192.168.10.111/32 192.168.10.112/32 192.168.10.113/32 192.168.10.114/32 192.168.10.115/32 192.168.10.116/32 192.168.10.117/32 192.168.10.118/32 192.168.1.119/32 192.168.1.120/32 192.168.1.121/32");

}


static void test2()
{
	// [IDGW-DEV 5246] 1024 bytes
	set_and_get("hogehoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	// [IDGW-DEV 5246] 1024 bytes - 1024 bytes
	set_and_get("hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	// [IDGW-DEV 5246] 1024 bytes - 1024 bytes
	set_and_get("hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	// [IDGW-DEV 5246] 2048 bytes
	set_and_get("hogehoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
	// [IDGW-DEV 5246] 2048 bytes
	set_and_get("hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge", "hogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehogehoge");
}
void
test3(void)
{
	FILE *f;
	struct properties *props;

	ASSERT((f = fopen("test.properties", "w")) != NULL);
	ASSERT((props = properties_create(512)) != NULL);
	ASSERT(properties_put(props, " ", "HOGEHOGE") == NULL);
	ASSERT(properties_put(props, "", "HOGEHOGE") == NULL);
	ASSERT(properties_put(props, "HOGEHOGE", "") != NULL);
	ASSERT(properties_save(props, f) == 0);
	ASSERT(fclose(f) == 0);
}

static int test4_off = 0;
static char test4_buf[] =
    "test1: Net\\\n"
    "	BSD\n"
    "test2: Made in\\\n"
    "	\\ Japan\n"
    "test3: bbb\\\n"
    "    aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa    aaaaa  \n"
    "test4: \\r\n"
    ;

static int
test4_read_fn(void *cookie, char *buf, int len)
{
	int rval;

	rval = MIN(len, sizeof(test4_buf) - test4_off);
	if (rval == 0)
		return 0;	// EOF
	memcpy(buf, test4_buf + test4_off, rval);
	test4_off += rval;

	return rval;
}


static void
test4()
{
	FILE *fp;
	struct properties *props;
	const char *val;


	fp = fropen(NULL, test4_read_fn);
	ASSERT((props = properties_create(512)) != NULL);
	ASSERT((properties_load(props, fp)) == 0);
	fclose(fp);

	val = properties_get(props, "test1");
	ASSERT(val != NULL);
	ASSERT(strcmp(val, "NetBSD") == 0);

	val = properties_get(props, "test2");
	ASSERT(val != NULL);
	ASSERT(strcmp(val, "Made in Japan") == 0);
	val = properties_get(props, "test3");
	ASSERT(val != NULL);
	ASSERT(strcmp(val, "bbbaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa    aaaaa") == 0);

	val = properties_get(props, "test4");
	ASSERT(val != NULL);
	ASSERT(strcmp(val, "\r") == 0);
}

int
main(int argc, char *argv[])
{
	TEST(test0);
	TEST(test1);
	TEST(test2);
	TEST(test3);
	TEST(test4);
}
