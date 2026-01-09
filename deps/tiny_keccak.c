/* tiny_keccak.c - minimal Keccak-256
   This is a compact implementation adapted for example purposes.
   Public domain.
*/
#include "tiny_keccak.h"
#include <string.h>

/* Keccak-f[1600] implementation adapted from public domain tiny implementations. */
typedef unsigned long long u64;

static inline u64 rol(u64 x, unsigned int y) { return (x << y) | (x >> (64 - y)); }

static void keccakf(u64 st[25]) {
    static const u64 RC[24] = {
        0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
        0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
        0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
        0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
        0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
        0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL
    };
    int round;
    for (round = 0; round < 24; ++round) {
        u64 C[5];
        for (int x = 0; x < 5; ++x) C[x] = st[x] ^ st[x + 5] ^ st[x + 10] ^ st[x + 15] ^ st[x + 20];
        for (int x = 0; x < 5; ++x) {
            u64 d = C[(x + 4) % 5] ^ rol(C[(x + 1) % 5], 1);
            for (int y = 0; y < 25; y += 5) st[y + x] ^= d;
        }
        u64 B[25];
        for (int x = 0; x < 5; ++x) for (int y = 0; y < 5; ++y) B[y*5 + ((2*x + 3*y) % 5)] = rol(st[y*5 + x], (unsigned int)((((x*2)+y*3)%5)*((y*2)+x)%64));
        for (int i = 0; i < 25; ++i) st[i] = B[i];
        for (int j = 0; j < 25; ++j) st[j] ^= RC[round] & (j==0);
        /* Above is intentionally simple and not fully optimized; adequate for demonstration. */
    }
}

void keccak_256(const unsigned char *in, size_t inlen, unsigned char *out) {
    unsigned char temp[200];
    memset(temp, 0, sizeof(temp));
    unsigned long long st[25];
    memset(st, 0, sizeof(st));
    size_t rate = 136; // 1088 bits
    size_t i = 0;
    while (inlen >= rate) {
        for (size_t j = 0; j < rate/8; ++j) {
            u64 t = 0;
            for (int k = 0; k < 8; ++k) t |= (u64)in[i + j*8 + k] << (8*k);
            st[j] ^= t;
        }
        keccakf(st);
        inlen -= rate; i += rate;
    }
    unsigned char block[136]; memset(block, 0, sizeof(block));
    if (inlen) memcpy(block, in + i, inlen);
    block[inlen] = 0x01;
    block[rate-1] |= 0x80;
    for (size_t j = 0; j < rate/8; ++j) {
        u64 t = 0;
        for (int k = 0; k < 8; ++k) t |= (u64)block[j*8 + k] << (8*k);
        st[j] ^= t;
    }
    keccakf(st);
    // squeeze
    for (size_t j = 0; j < 32/8; ++j) {
        u64 t = st[j];
        for (int k = 0; k < 8; ++k) out[j*8 + k] = (unsigned char)((t >> (8*k)) & 0xFF);
    }
}
