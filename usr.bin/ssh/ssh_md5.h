#ifndef MD5_H
#define MD5_H

typedef word32 uint32;

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

#define MD5Init ssh_MD5Init
void MD5Init(struct MD5Context *context);
#define MD5Update ssh_MD5Update
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
#define MD5Final ssh_MD5Final
void MD5Final(unsigned char digest[16], struct MD5Context *context);
#define MD5Transform ssh_MD5Transform
void MD5Transform(uint32 buf[4], const unsigned char in[64]);

#endif /* !MD5_H */
