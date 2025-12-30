#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h> 
#include <zlib.h>

// 使用zlib压缩数据
static int compress_data(const uint8_t *input, size_t input_size,
                        uint8_t **output, size_t *output_size,
                        int compression_level);
// 解压数据
static int decompress_data(const uint8_t *input, size_t input_size,
                          uint8_t **output, size_t output_size);


#endif // COMPRESS_H