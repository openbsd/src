/*	$Id: random_key.c,v 1.2 1995/12/17 19:12:05 tholo Exp $	*/

/* Copyright (C) 1992 Eric Young - see COPYING for more details */
#include "des_locl.h"

void des_random_key(ret)
des_cblock ret;
	{
	des_key_schedule ks;
	static u_int32_t c=0;
	static pid_t pid=0;
	static des_cblock data={0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
	des_cblock key;

#ifdef MSDOS
	pid=1;
#else
	if (!pid) pid=getpid();
#endif
	((u_int32_t *)key)[0]=(u_int32_t)time(NULL);
	((u_int32_t *)key)[1]=(u_int32_t)((pid)|((c++)<<16));

	des_set_odd_parity((des_cblock *)data);
	des_set_key_schedule((des_cblock *)data,ks);
	des_cbc_cksum((des_cblock *)key,(des_cblock *)key,
		(int32_t)sizeof(key),ks,(des_cblock *)data);
	des_set_odd_parity((des_cblock *)key);
	des_cbc_cksum((des_cblock *)key,(des_cblock *)key,
		(int32_t)sizeof(key),ks,(des_cblock *)data);

	bcopy(key,ret,sizeof(key));
	bzero(key,sizeof(key));
	bzero(ks,sizeof(ks));
	}
