/*	$OpenBSD: yptest.c,v 1.12 2002/07/19 16:28:48 maja Exp $ */

/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: yptest.c,v 1.12 2002/07/19 16:28:48 maja Exp $";
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>

static int
yptest_foreach(u_long status, char *key, int keylen, char *val, int vallen,
    void *data)
{
	if (status == 2)
		return(0);

	key[keylen] = '\0';
	val[vallen] = '\0';
	printf("%s %s\n", key, val);
	return 0;
}

int
main(int argc, char *argv[])
{
	char *Domain, *Value, *Key2;
	char *Map = "passwd.byname", *Key = "root";
	int ValLen, Status, Order, KeyLen;
	struct ypall_callback Callback;
	struct ypmaplist *ypml, *y;

	Status = yp_get_default_domain(&Domain);
	printf("Status %d\n", Status);
	printf("dom %s\n", Domain);
	printf("dom %d\n", Domain[0]);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));

	printf("Test 1: yp_match\n");
	KeyLen = strlen(Key);
	Status = yp_match(Domain, Map, Key, KeyLen, &Value, &ValLen);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));
	else
		printf("%*.*s\n", ValLen, ValLen, Value);

	printf("\nTest 2: yp_first\n");
	Status = yp_first(Domain, Map, &Key2, &KeyLen, &Value, &ValLen);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));
	else
		printf("%*.*s %*.*s\n", KeyLen, KeyLen, Key2, ValLen, ValLen, Value);

	printf("\nTest 3: yp_next\n");
	while (Status == 0) {
		Status = yp_next(Domain, Map, Key2, KeyLen, &Key2,
		    &KeyLen, &Value, &ValLen);
		if (Status)
			printf("yp error: %s\n", yperr_string(Status));
		else
			printf("%*.*s %*.*s\n", KeyLen, KeyLen, Key2,
			    ValLen, ValLen, Value);
	}

	printf("\nTest 4: yp_master\n");
	Status = yp_master(Domain, Map, &Key2);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));
	else
		printf("%s\n", Key2);

	printf("\nTest 5: yp_order\n");
	Status = yp_order(Domain, Map, &Order);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));
	else
		printf("%d\n", Order);

	printf("\nTest 6: yp_maplist\n");
	ypml = NULL;
	switch (yp_maplist(Domain, &ypml)) {
	case 0:
		for (y = ypml; y; ) {
			ypml = y;
			printf("%s\n", ypml->map);
			y = ypml->next;
		}
		break;
	default:
		printf("yp error: %s\n", yperr_string(Status));
		break;
	}

	printf("\nTest 7: yp_all\n");
	Callback.foreach = yptest_foreach;
	Status = yp_all(Domain, Map, &Callback);
	if (Status)
		printf("yp error: %s\n", yperr_string(Status));
	return 0;
}
