#include <stdio.h>

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define IV_SIZE 16
#define GCM_IV_SIZE 12
#define TAG_SIZE 16

// Generates a random string of `size` bytes, used for AES keys and IVs
int generate_rand_bytes(int size, unsigned char *bytes);

// Encrypts the plaintext using AES-256 in cipher block chaining (CBC) mode. 
// Each block is 16 bytes.
int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
            unsigned char *iv, unsigned char *ciphertext);

// Encrypts plaintext using AES-256 in GCM mode, where a unique tag is generated
// by the encryption scheme, to be sent and validated to ensure authenticity of source.
int gcm_encrypt(unsigned char *plaintext, int plaintext_len,
                unsigned char *aad, int aad_len,
                unsigned char *key,
                unsigned char *iv, int iv_len,
                unsigned char *ciphertext,
                unsigned char *tag);

// Decrypts ciphertext using iv and tag unique to the sender. Ensures the tag
// is not modified; else returns -1
int gcm_decrypt(unsigned char *ciphertext, int ciphertext_len,
                unsigned char *aad, int aad_len,
                unsigned char *tag,
                unsigned char *key,
                unsigned char *iv, int iv_len,
                unsigned char *plaintext);