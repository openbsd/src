#ifndef MD5_H
#define MD5_H

struct MD5Context {
	u_int32_t buf[4];
	u_int32_t bits[2];
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
void MD5Transform(u_int32_t buf[4], const unsigned char in[64]);

#endif /* !MD5_H */
