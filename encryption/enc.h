#include <stdio.h>

int calculate_block_size(int command_len);

int generate_rand_bytes(int size, unsigned char *bytes);

int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
            unsigned char *iv, unsigned char *ciphertext);

int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
            unsigned char *iv, unsigned char *plaintext);