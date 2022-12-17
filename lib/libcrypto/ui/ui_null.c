/*	$OpenBSD: ui_null.c,v 1.1 2022/12/17 21:59:39 tb Exp $ */

/*
 * Written by Theo Buehler. Public domain.
 */

#include "ui_local.h"

static const UI_METHOD ui_null = {
	.name = "OpenSSL NULL UI",
};

const UI_METHOD *
UI_null(void)
{
	return &ui_null;
}
LCRYPTO_ALIAS(UI_null)
