/* 
 * Created by Martin Rinman, rinman@erlang.ericsson.se 
 * Copyright (C) 1998 Ericsson Radio Systems AB 
 */


#include <assert.h>
#include <sys/types.h>

#include "cast.h"


void
cast_setkey(cast_key* key, u_int8_t* rawkey, int keybytes) {
	assert("cast_setkey not implemented yet");
}

void
cast_encrypt(cast_key* key, u_int8_t* inblock, u_int8_t* outblock) {
	assert("cast_encrypt not implemented yet");
}

void
cast_decrypt(cast_key* key, u_int8_t* inblock, u_int8_t* outblock) {
	assert("cast_decrypt not implemented yet");
}



