#ifndef __RC6_H
#define __RC6_H

#ifdef  __cplusplus
extern "C" {
#endif

#define RC6_ROUNDS     20
#define RC6_WORDSIZE   32

#define RC6_P32 0xB7E15163L
#define RC6_Q32 0x9E3779B9L

struct rc6key {
	unsigned int key[RC6_ROUNDS * 2 + 4];
};

int rc6decrypt (struct rc6key *key, void *inout);
int rc6initkey (struct rc6key *key, void *userkey, int len);

#ifdef  __cplusplus
}
#endif

#endif
