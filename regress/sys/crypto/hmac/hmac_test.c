#include <stdio.h>
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/hmac.h>

void
print_hex(unsigned char *buf, int len)
{
	int i;

	printf("digest = 0x");
	for (i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

int
main(void)
{
	HMAC_MD5_CTX md5;
	HMAC_SHA1_CTX sha1;
	HMAC_SHA256_CTX sha256;
	u_int8_t data[50], output[32];
	int i;

	HMAC_MD5_Init(&md5, "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 16);
	HMAC_MD5_Update(&md5, "Hi There", 8);
	HMAC_MD5_Final(output, &md5);
	print_hex(output, MD5_DIGEST_LENGTH);

	HMAC_MD5_Init(&md5, "Jefe", 4);
	HMAC_MD5_Update(&md5, "what do ya want for nothing?", 28);
	HMAC_MD5_Final(output, &md5);
	print_hex(output, MD5_DIGEST_LENGTH);

	HMAC_MD5_Init(&md5, "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 16);
	memset(data, 0xDD, sizeof data);
	HMAC_MD5_Update(&md5, data, sizeof data);
	HMAC_MD5_Final(output, &md5);
	print_hex(output, MD5_DIGEST_LENGTH);

	HMAC_SHA1_Init(&sha1, "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 16);
	HMAC_SHA1_Update(&sha1, "Hi There", 8);
	HMAC_SHA1_Final(output, &sha1);
	print_hex(output, SHA1_DIGEST_LENGTH);

	HMAC_SHA1_Init(&sha1, "Jefe", 4);
	HMAC_SHA1_Update(&sha1, "what do ya want for nothing?", 28);
	HMAC_SHA1_Final(output, &sha1);
	print_hex(output, SHA1_DIGEST_LENGTH);

	HMAC_SHA1_Init(&sha1, "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 16);
	memset(data, 0xDD, sizeof data);
	HMAC_SHA1_Update(&sha1, data, sizeof data);
	HMAC_SHA1_Final(output, &sha1);
	print_hex(output, SHA1_DIGEST_LENGTH);

	HMAC_SHA256_Init(&sha256, "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 16);
	HMAC_SHA256_Update(&sha256, "Hi There", 8);
	HMAC_SHA256_Final(output, &sha256);
	print_hex(output, SHA256_DIGEST_LENGTH);

	HMAC_SHA256_Init(&sha256, "Jefe", 4);
	HMAC_SHA256_Update(&sha256, "what do ya want for nothing?", 28);
	HMAC_SHA256_Final(output, &sha256);
	print_hex(output, SHA256_DIGEST_LENGTH);

	HMAC_SHA256_Init(&sha256, "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 16);
	memset(data, 0xDD, sizeof data);
	HMAC_SHA256_Update(&sha256, data, sizeof data);
	HMAC_SHA256_Final(output, &sha256);
	print_hex(output, SHA256_DIGEST_LENGTH);

	return 0;
}
