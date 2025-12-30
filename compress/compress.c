#include "compress.h"

// 使用zlib压缩数据
static int compress_data(const uint8_t *input, size_t input_size,
                        uint8_t **output, size_t *output_size,
                        int compression_level) {
    if (compression_level == 0) {
        // 不压缩，直接复制
        *output = malloc(input_size);
        if (!*output) return 0;
        memcpy(*output, input, input_size);
        *output_size = input_size;
        return 1;
    }
    
    // 计算压缩后的最大大小
    uLongf dest_len = compressBound(input_size);
    uint8_t *dest = malloc(dest_len);
    if (!dest) return 0;
    
    int result = compress2(dest, &dest_len, input, input_size, compression_level);
    if (result != Z_OK) {
        free(dest);
        return 0;
    }
    
    *output = dest;
    *output_size = dest_len;
    return 1;
}
// 解压数据
static int decompress_data(const uint8_t *input, size_t input_size,
                          uint8_t **output, size_t output_size) {
    *output = malloc(output_size);
    if (!*output) return 0;
    
    uLongf dest_len = output_size;
    int result = uncompress(*output, &dest_len, input, input_size);
    if (result != Z_OK) {
        free(*output);
        *output = NULL;
        return 0;
    }
    
    return 1;
}