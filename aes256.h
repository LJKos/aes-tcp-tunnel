#ifndef AES256_H
#define AES256_H

#include <stdio.h>
#include <string.h>

void cipher256(const unsigned char *in, unsigned char *out, unsigned int *w);
void inv_cipher256(const unsigned char *in, unsigned char *out, unsigned int *w);
void key_expansion256(const unsigned char *key, unsigned int *w);

#endif
