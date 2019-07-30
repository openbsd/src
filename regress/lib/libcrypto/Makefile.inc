# Use this variable when the test needs internal symbols from libcrypto
CRYPTO_INT=  -Wl,-Bstatic -lcrypto -Wl,-Bdynamic
