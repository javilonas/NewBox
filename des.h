#ifndef CSCRYPT_DES_H_
#define CSCRYPT_DES_H_

#ifdef  __cplusplus
extern "C" {
#endif

#define CRYPT           0
#define HASH            1
#define UPDATE          2

#define DES_IP              1
#define DES_IP_1            2
#define DES_RIGHT           4
#define DES_HASH            8

#define DES_ECM_CRYPT       0
#define DES_ECM_HASH        DES_HASH
#define DES_ECS2_DECRYPT    (DES_IP | DES_IP_1 | DES_RIGHT)
#define DES_ECS2_CRYPT      (DES_IP | DES_IP_1)

#define F_EURO_S2       0
#define F_TRIPLE_DES    1

#define SetBit(addr, bit) (addr) |= (1 << (bit))
#define UnsetBit(addr, bit) (addr) &= ~(1<< (bit))
#define TestBit(addr, bit) ((addr) & (1 << bit))

typedef unsigned char byte;

extern void EuroDes(byte key1[], byte key2[], byte DesMode, byte OperatingMode, 
                    byte data[]);
extern int des_encrypt(byte *buffer, int len, byte *deskey);
extern int des_decrypt(byte *buffer, int len, byte *deskey);
extern void des_login_key_get(byte *key1, byte *key2, int len, byte *des16);

	extern int des_encrypt(byte *buffer, int len, byte *deskey);
	extern int des_decrypt(byte *buffer, int len, byte *deskey);
	extern void des_login_key_get(byte *key1, byte *key2, int len, byte *des16);
	extern void doPC1(unsigned char data[]);
	extern void des_ede2_cbc_encrypt(byte *data, const byte *iv, const byte *okey1, const byte *okey2, int len);
	extern void des_ede2_cbc_decrypt(byte *data, byte *iv, const byte *okey1, const byte *okey2, int len);
	extern void des(byte key[], byte mode, byte data[]);

#ifdef __cplusplus
}
#endif

extern void doPC1(byte data[]);
extern void des(byte key[], byte mode, byte data[]);

enum { ECM=0, ECS2=1, ECS3=2 };

#endif



