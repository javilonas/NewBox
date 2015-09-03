/*
 * This code implements the RC6-32/20 block cipher.
 *
 * The algorithm is due to Ron Rivest and RSA Labs. This code was written
 * by Martin Hinner <mhi@penguin.cz> in 1999, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * During the AES evaluation period RC6 can be freely used for research
 * and evaluation purposes. For more information see www.rsa.com
 * and AES homepage.
 *
 * RC6 is block cipher using blocks of size 128 bits. To encrypt or
 * decrypt block initialize key structure using rc6keyinit. Then use
 * rc6encrypt/rc6decrypt to encrypt/decrypt block.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <byteswap.h>
#include <endian.h>

#include "rc6.h"


/*
 * Initialize rc6key structure. 'userkey' is user-supplied key of length 'len'.
 */

unsigned int rol(unsigned int  v, unsigned int  cnt)
{
	cnt &= (RC6_WORDSIZE - 1);
	while (cnt--)
		v = (v << 1) | (v >> (RC6_WORDSIZE - 1));
	return v;
}

unsigned int ror(unsigned int  v, unsigned int  cnt)
{
	cnt &= (RC6_WORDSIZE - 1);
	while (cnt--)
		v = (v >> 1) | (v << (RC6_WORDSIZE - 1));
	return v;
}

int rc6initkey(struct rc6key *key, void *userkey, int len)
{
	int i=0, j=0, v=0, s=0;
	unsigned int  a=0, b=0;
	unsigned int  *l = (unsigned int  *) userkey;
#if __BYTE_ORDER == __BIG_ENDIAN
	l[0]=bswap_32(l[0]);l[1]=bswap_32(l[1]);l[2]=bswap_32(l[2]);l[3]=bswap_32(l[3]);
#endif
	key->key[0] = RC6_P32;
	for (i = 1; i < RC6_ROUNDS * 2 + 4; i++)
		key->key[i] = key->key[i - 1] + RC6_Q32;

	a = b = i = j = 0;
	v = 3 * (len / 4 > RC6_ROUNDS * 2 + 4 ? len / 4 : RC6_ROUNDS * 2 + 4);
	for (s = 0; s < v; s++)
	{
		a = key->key[i] = rol (key->key[i] + a + b, 3);
		b = l[j] = rol (l[j] + a + b, a + b);
		i++; i %= RC6_ROUNDS * 2 + 4;
		j++;j %= len / 4;
	}
	return 0;
}

int rc6decrypt(struct rc6key *key, void *inout)
{
	unsigned int  *inl = (unsigned int  *) inout;
	unsigned int  a=0, b=0, c=0, d=0, t=0, u=0;
	int i=0;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	a = inl[0];b = inl[1];c = inl[2];d = inl[3];
#else
	a = bswap_32(inl[0]);b = bswap_32(inl[1]);c = bswap_32(inl[2]);d = bswap_32(inl[3]);
#endif
	c -= key->key[RC6_ROUNDS * 2 + 3];
	a -= key->key[RC6_ROUNDS * 2 + 2];

	for (i = RC6_ROUNDS; i > 0; i--)
	{
		t = a;u = b;
		a = d;d = c;b = t;c = u;

		u = rol ((d * (2 * d + 1)), 5);
		t = rol ((b * (2 * b + 1)), 5);
		c = ror (c - key->key[2 * i + 1], t) ^ u;
		a = ror (a - key->key[2 * i], u) ^ t;
	}
	d -= key->key[1];
	b -= key->key[0];

#if __BYTE_ORDER == __LITTLE_ENDIAN
	inl[0] = a;inl[1] = b;inl[2] = c;inl[3] = d;
#else
	inl[0] = bswap_32(a);inl[1] = bswap_32(b);inl[2] = bswap_32(c);inl[3] = bswap_32(d);
#endif

	return 0;
}
