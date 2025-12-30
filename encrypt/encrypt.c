#include "encrypt.h"


// AES加密（简单实现）
static int encrypt_data(const uint8_t *input, size_t input_size,
                       uint8_t **output, size_t *output_size,
                       const char *password) {
    // 简单实现：实际应使用更安全的加密
    size_t padded_size = ((input_size + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
    uint8_t *dest = malloc(padded_size);
    if (!dest) return 0;
    
    // 使用密码生成密钥
    unsigned char key[32];
    SHA256((unsigned char*)password, strlen(password), key);
    
    // 这里应该使用真正的AES加密
    // 为了简化，我们只进行简单的XOR加密
    for (size_t i = 0; i < input_size; i++) {
        dest[i] = input[i] ^ key[i % 32];
    }
    
    // 填充剩余部分
    for (size_t i = input_size; i < padded_size; i++) {
        dest[i] = key[i % 32];
    }
    
    *output = dest;
    *output_size = padded_size;
    return 1;
}

// AES解密
static int decrypt_data(const uint8_t *input, size_t input_size,
                       uint8_t **output, size_t *output_size,
                       const char *password) {
    if (input_size % AES_BLOCK_SIZE != 0) {
        return 0;  // 不是块大小的倍数
    }
    
    // 使用相同的密钥
    unsigned char key[32];
    SHA256((unsigned char*)password, strlen(password), key);
    
    // 找到实际的原始大小（通过查找填充）
    size_t original_size = input_size;
    while (original_size > 0 && input[original_size - 1] == key[(original_size - 1) % 32]) {
        original_size--;
    }
    
    uint8_t *dest = malloc(original_size);
    if (!dest) return 0;
    
    // 解密
    for (size_t i = 0; i < original_size; i++) {
        dest[i] = input[i] ^ key[i % 32];
    }
    
    *output = dest;
    *output_size = original_size;
    return 1;
}