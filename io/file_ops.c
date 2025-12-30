#include "file_ops.h"

// 写入数据到缓冲区
static int write_to_buffer(MemoryBuffer *buf, const void *data, size_t size) {
    if (buf->size + size > buf->capacity) {
        if (!expand_buffer(buf, buf->size + size)) {
            return 0;
        }
    }
    
    memcpy(buf->buffer + buf->size, data, size);
    buf->size += size;
    return 1;
}

// 计算CRC32校验和
static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    //CRC取反
    return ~crc;
}


// 实际写入文件到归档
int write_file_to_archive(FILE *archive_fp, const char *filename,
                         CompressionLevel compression_level,
                         const char *password) {
    FILE *file_fp = fopen(filename, "rb");
    if (!file_fp) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return 0;
    }
    
    // 获取文件大小
    fseek(file_fp, 0, SEEK_END);
    long file_size = ftell(file_fp);
    fseek(file_fp, 0, SEEK_SET);
    
    // 读取文件内容
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(file_fp);
        return 0;
    }
    
    fread(file_data, 1, file_size, file_fp);
    fclose(file_fp);
    
    // 计算原始CRC32
    uint32_t original_crc = calculate_crc32(file_data, file_size);
    
    // 压缩数据
    uint8_t *compressed_data = NULL;
    size_t compressed_size = 0;
    uint16_t flags = 0;
    
    if (compression_level > 0) {
        if (compress_data(file_data, file_size, &compressed_data, &compressed_size, compression_level)) {
            flags |= FLAG_COMPRESSED;
        } else {
            compressed_data = file_data;
            compressed_size = file_size;
        }
    } else {
        compressed_data = file_data;
        compressed_size = file_size;
    }
    
    // 加密数据
    uint8_t *encrypted_data = NULL;
    size_t encrypted_size = 0;
    
    if (password && *password) {
        if (encrypt_data(compressed_data, compressed_size, &encrypted_data, &encrypted_size, password)) {
            flags |= FLAG_ENCRYPTED;
            free(compressed_data);
            compressed_data = encrypted_data;
            compressed_size = encrypted_size;
        }
    }
    
    // 获取文件信息
    struct stat file_stat;
    stat(filename, &file_stat);
    
    // 创建文件条目头
    FileEntry entry;
    memset(&entry, 0, sizeof(FileEntry));
    strncpy(entry.filename, filename, sizeof(entry.filename) - 1);
    entry.file_size = file_size;
    entry.stored_size = compressed_size;
    entry.offset = ftell(archive_fp) + sizeof(FileEntry);
    entry.mtime = file_stat.st_mtime;
    entry.atime = file_stat.st_atime;
    entry.mode = file_stat.st_mode;
    entry.flags = flags;
    entry.crc32 = original_crc;
    
    // 写入条目头
    fwrite(&entry, sizeof(FileEntry), 1, archive_fp);
    
    // 写入文件数据
    fwrite(compressed_data, 1, compressed_size, archive_fp);
    
    // 清理
    if (compressed_data != file_data) {
        free(compressed_data);
    }
    free(file_data);
    
    return 1;
}

// 从归档读取文件
int read_file_from_archive(FILE *archive_fp, const FileEntry *entry,
                          const char *dest_path, const char *password) {
    // 保存当前位置
    long current_pos = ftell(archive_fp);
    
    // 跳转到文件数据开始处
    fseek(archive_fp, entry->offset, SEEK_SET);
    
    // 读取文件数据
    uint8_t *stored_data = malloc(entry->stored_size);
    if (!stored_data) {
        fseek(archive_fp, current_pos, SEEK_SET);
        return 0;
    }
    
    fread(stored_data, 1, entry->stored_size, archive_fp);
    
    // 解密数据
    uint8_t *decrypted_data = stored_data;
    size_t decrypted_size = entry->stored_size;
    
    if (entry->flags & FLAG_ENCRYPTED) {
        if (!password || !*password) {
            fprintf(stderr, "File is encrypted, password required\n");
            free(stored_data);
            fseek(archive_fp, current_pos, SEEK_SET);
            return 0;
        }
        
        if (!decrypt_data(stored_data, entry->stored_size, &decrypted_data, &decrypted_size, password)) {
            fprintf(stderr, "Decryption failed\n");
            free(stored_data);
            fseek(archive_fp, current_pos, SEEK_SET);
            return 0;
        }
        
        free(stored_data);
    }
    
    // 解压数据
    uint8_t *decompressed_data = decrypted_data;
    size_t decompressed_size = decrypted_size;
    
    if (entry->flags & FLAG_COMPRESSED) {
        if (!decompress_data(decrypted_data, decrypted_size, &decompressed_data, entry->file_size)) {
            fprintf(stderr, "Decompression failed\n");
            if (decrypted_data != stored_data) {
                free(decrypted_data);
            }
            fseek(archive_fp, current_pos, SEEK_SET);
            return 0;
        }
        
        if (decrypted_data != stored_data) {
            free(decrypted_data);
        }
    }
    
    // 验证CRC32
    uint32_t calculated_crc = calculate_crc32(decompressed_data, entry->file_size);
    if (calculated_crc != entry->crc32) {
        fprintf(stderr, "CRC32 mismatch for file: %s\n", entry->filename);
        free(decompressed_data);
        fseek(archive_fp, current_pos, SEEK_SET);
        return 0;
    }
    
    // 构建目标路径
    char full_path[512];
    if (dest_path && *dest_path) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, entry->filename);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", entry->filename);
    }
    
    // 创建目录（如果需要）
    char *last_slash = strrchr(full_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        // 创建目录
        #ifdef _WIN32
            _mkdir(full_path);
        #else
            mkdir(full_path, 0755);
        #endif
        *last_slash = '/';
    }
    
    // 写入文件
    FILE *dest_fp = fopen(full_path, "wb");
    if (!dest_fp) {
        fprintf(stderr, "Cannot create file: %s\n", full_path);
        free(decompressed_data);
        fseek(archive_fp, current_pos, SEEK_SET);
        return 0;
    }
    
    fwrite(decompressed_data, 1, entry->file_size, dest_fp);
    fclose(dest_fp);
    
    // 恢复文件属性
    #ifndef _WIN32
        chmod(full_path, entry->mode & 0777);
        struct utimbuf times;
        times.actime = entry->atime;
        times.modtime = entry->mtime;
        utime(full_path, &times);
    #endif
    
    free(decompressed_data);
    fseek(archive_fp, current_pos, SEEK_SET);
    
    return 1;
}