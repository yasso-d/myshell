#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/sha.h>

// AES加密（简单实现）
static int encrypt_data(const uint8_t *input, size_t input_size,
                       uint8_t **output, size_t *output_size,
                       const char *password);
// AES解密
static int decrypt_data(const uint8_t *input, size_t input_size,
                       uint8_t **output, size_t *output_size,
                       const char *password);
#endif // ENCRYPT_H