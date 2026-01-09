/* tiny_keccak.h - minimal Keccak / Keccak-256 interface
   Public domain minimal implementation used for hashing
*/
#ifndef TINY_KECCAK_H
#define TINY_KECCAK_H

#include <stddef.h>

void keccak_256(const unsigned char *in, size_t inlen, unsigned char *out);

#endif
