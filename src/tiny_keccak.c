/* tiny_keccak.c - Correct Keccak-256 implementation
   Based on the official Keccak specification
   Public domain.
*/
#include "tiny_keccak.h"
#include <string.h>

typedef unsigned long long u64;

#define ROL64(a, offset) ((a << offset) ^ (a >> (64-offset)))

static const u64 keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

static const int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

static void keccakf(u64 s[25]) {
    int i, j, round;
    u64 t, bc[5];

    for (round = 0; round < 24; round++) {
        // Theta
        for (i = 0; i < 5; i++)
            bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                s[j + i] ^= t;
        }

        // Rho Pi
        t = s[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = s[j];
            s[j] = ROL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        // Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = s[j + i];
            for (i = 0; i < 5; i++)
                s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        // Iota
        s[0] ^= keccakf_rndc[round];
    }
}

void keccak_256(const unsigned char *in, size_t inlen, unsigned char *out) {
    u64 s[25];
    unsigned char temp[144];
    size_t rate = 136; // 1088 / 8 for Keccak-256
    size_t i;

    memset(s, 0, sizeof(s));

    // Absorb
    while (inlen >= rate) {
        for (i = 0; i < rate / 8; i++) {
            s[i] ^= ((u64*)in)[i];
        }
        keccakf(s);
        in += rate;
        inlen -= rate;
    }

    // Last block
    memset(temp, 0, sizeof(temp));
    memcpy(temp, in, inlen);
    temp[inlen] = 0x01;
    temp[rate - 1] |= 0x80;

    for (i = 0; i < rate / 8; i++) {
        s[i] ^= ((u64*)temp)[i];
    }
    keccakf(s);

    // Squeeze
    memcpy(out, s, 32);
}
