#include <stdio.h>
#include <crypto/rijndael.h>
#include <crypto/key_wrap.h>

void
print_hex(const char *str, unsigned char *buf, int len)
{
	int i;

	printf("%s", str);
	for (i = 0; i < len; i++) {
		if ((i % 8) == 0)
			printf(" ");
		printf("%02X", buf[i]);
	}
	printf("\n");
}

void
ovbcopy(const void *src, void *dst, size_t len)
{
	/* userspace does not have ovbcopy: fake it */
	memmove(dst, src, len);
}

void
do_test(u_int kek_len, u_int data_len)
{
	aes_key_wrap_ctx ctx;
	u_int8_t kek[32], data[32];
	u_int8_t output[64];
	int i;

	for (i = 0; i < kek_len; i++)
		kek[i] = i;
	printf("Input:\n");
	print_hex("KEK:\n  ", kek, kek_len);
	for (i = 0; i < 16; i++)
		data[i] = i * 16 + i;
	for (; i < data_len; i++)
		data[i] = i - 16;
	print_hex("Key Data:\n  ", data, data_len);
	aes_key_wrap_set_key(&ctx, kek, kek_len);
	aes_key_wrap(&ctx, data, data_len / 8, output);
	print_hex("Ciphertext:\n  ", output, data_len + 8);
	aes_key_unwrap(&ctx, output, output, data_len / 8);
	printf("Output:\n");
	print_hex("Key Data:\n  ", output, data_len);
	printf("====\n");
}

int
main(void)
{
	do_test(16, 16);
	do_test(24, 16);
	do_test(32, 16);
	do_test(24, 24);
	do_test(32, 24);
	do_test(32, 32);

	return 0;
}
