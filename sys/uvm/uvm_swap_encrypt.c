/*
 * Copyright 1999 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <dev/rndvar.h>
#include <crypto/blf.h>

#include <uvm/uvm_swap_encrypt.h>

blf_ctx swap_key;

int uvm_doswapencrypt = 0;
int swap_encrypt_initalized = 0;

/*
 * Initalize the key from the kernel random number generator.  This is
 * done once on startup.
 */

void
swap_encrypt_init(caddr_t data, size_t len)
{
	int i;
	u_int32_t *key = (u_int32_t *)data;

	if (swap_encrypt_initalized)
		return;

	for (i = 0; i < len / sizeof(u_int32_t); i++)
		*key++ = arc4random();

	swap_encrypt_initalized = 1;
}

/*
 * Encrypt the data before it goes to swap, the size should be 64-bit
 * aligned.
 */

void
swap_encrypt(caddr_t src, caddr_t dst, size_t count)
{
  u_int32_t *dsrc = (u_int32_t *)src;
  u_int32_t *ddst = (u_int32_t *)dst;
  u_int32_t iv1, iv2;

  if (!swap_encrypt_initalized)
	swap_encrypt_init((caddr_t)&swap_key, sizeof(swap_key));

  count /= sizeof(u_int32_t);

  iv1 = iv2 = 0;
  for (; count > 0; count -= 2) {
    ddst[0] = dsrc[0] ^ iv1;
    ddst[1] = dsrc[1] ^ iv2;
    /*
     * Do not worry about endianess, it only needs to decrypt on this machine
     */
    Blowfish_encipher(&swap_key, ddst);
    iv1 = ddst[0];
    iv2 = ddst[1];

    dsrc += 2;
    ddst += 2;
  }
}

/*
 * Decrypt the data after we retrieved it from swap, the size should be 64-bit
 * aligned.
 */

void
swap_decrypt(caddr_t src, caddr_t dst, size_t count)
{
  u_int32_t *dsrc = (u_int32_t *)src;
  u_int32_t *ddst = (u_int32_t *)dst;
  u_int32_t iv1, iv2, niv1, niv2;

  if (!swap_encrypt_initalized)
    panic("swap_decrypt: key not initalized");

  count /= sizeof(u_int32_t);

  iv1 = iv2 = 0;
  for (; count > 0; count -= 2) {
    ddst[0] = niv1 = dsrc[0];
    ddst[1] = niv2 = dsrc[1];
    Blowfish_decipher(&swap_key, ddst);
    ddst[0] ^= iv1;
    ddst[1] ^= iv2;

    iv1 = niv1;
    iv2 = niv2;

    dsrc += 2;
    ddst += 2;
  }
}
