/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/

typedef struct {
    unsigned long state[5];
    unsigned long count[2];  
    unsigned char buffer[64];
} SHA1_CTX;
  
void SHA1Transform(unsigned long state[5], unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned int len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);
