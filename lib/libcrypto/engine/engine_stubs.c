/*	$OpenBSD: engine_stubs.c,v 1.3 2023/11/19 15:47:40 tb Exp $ */

/*
 * Written by Theo Buehler. Public domain.
 */

#include <openssl/engine.h>

void
ENGINE_load_builtin_engines(void)
{
}

void
ENGINE_load_dynamic(void)
{
}

void
ENGINE_load_openssl(void)
{
}

int
ENGINE_register_all_complete(void)
{
	return 0;
}

void
ENGINE_cleanup(void)
{
}

ENGINE *
ENGINE_new(void)
{
	return NULL;
}

int
ENGINE_free(ENGINE *engine)
{
	return 0;
}

int
ENGINE_init(ENGINE *engine)
{
	return 0;
}

int
ENGINE_finish(ENGINE *engine)
{
	return 0;
}

ENGINE *
ENGINE_by_id(const char *id)
{
	return NULL;
}

const char *
ENGINE_get_id(const ENGINE *engine)
{
	return "";
}

const char *
ENGINE_get_name(const ENGINE *engine)
{
	return "";
}

int
ENGINE_set_default(ENGINE *engine, unsigned int flags)
{
	return 0;
}

ENGINE *
ENGINE_get_default_RSA(void)
{
	return NULL;
}

int
ENGINE_set_default_RSA(ENGINE *engine)
{
	return 0;
}

int
ENGINE_ctrl_cmd(ENGINE *engine, const char *cmd_name, long i, void *p,
    void (*f)(void), int cmd_optional)
{
	return 0;
}

int
ENGINE_ctrl_cmd_string(ENGINE *engine, const char *cmd, const char *arg,
    int cmd_optional)
{
	return 0;
}

EVP_PKEY *
ENGINE_load_private_key(ENGINE *engine, const char *key_id,
    UI_METHOD *ui_method, void *callback_data)
{
	return NULL;
}

EVP_PKEY *
ENGINE_load_public_key(ENGINE *engine, const char *key_id,
    UI_METHOD *ui_method, void *callback_data)
{
	return NULL;
}
