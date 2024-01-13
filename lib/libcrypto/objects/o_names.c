/* $OpenBSD: o_names.c,v 1.26 2024/01/13 11:38:45 tb Exp $ */
#include <openssl/err.h>
#include <openssl/objects.h>

int
OBJ_NAME_init(void)
{
	OBJerror(ERR_R_DISABLED);
	return 0;
}
LCRYPTO_ALIAS(OBJ_NAME_init);

int
OBJ_NAME_new_index(unsigned long (*hash_func)(const char *),
    int (*cmp_func)(const char *, const char *),
    void (*free_func)(const char *, int, const char *))
{
	OBJerror(ERR_R_DISABLED);
	return 0;
}
LCRYPTO_ALIAS(OBJ_NAME_new_index);

const char *
OBJ_NAME_get(const char *name, int type)
{
	OBJerror(ERR_R_DISABLED);
	return NULL;
}
LCRYPTO_ALIAS(OBJ_NAME_get);

int
OBJ_NAME_add(const char *name, int type, const char *data)
{
	/* No error to avoid polluting xca's error stack. */
	return 0;
}
LCRYPTO_ALIAS(OBJ_NAME_add);

int
OBJ_NAME_remove(const char *name, int type)
{
	OBJerror(ERR_R_DISABLED);
	return 0;
}
LCRYPTO_ALIAS(OBJ_NAME_remove);

void
OBJ_NAME_cleanup(int type)
{
}
LCRYPTO_ALIAS(OBJ_NAME_cleanup);
