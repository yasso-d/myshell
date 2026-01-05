#include "../include/archiver.h"

 int quiet = 0;
 int progress = 0;

//初始化archive_init函数
ArchiveAPI* archive_init(void) {
    ArchiveAPI *api = malloc(sizeof(ArchiveAPI));
    if (!api) return NULL;
    
    ArchiveContext *ctx = malloc(sizeof(ArchiveContext));
    if (!ctx) {
        free(api);
        return NULL;
    }
    
    // 初始化上下文
    memset(ctx, 0, sizeof(ArchiveContext));
    ctx->compression_level = COMPRESSION_DEFAULT;
    
    // 设置函数指针
    api->create = archive_create;
    api->extract = archive_extract;
    api->list = archive_list;
    api->add = archive_add;

    api->remove = archive_remove; // 未实现
    api->verify = archive_verify;
    api->update = archive_update; // 未实现
    api->test = archive_test;   // 未实现
    api->set_compression = compress_data;
    api->set_encryption = encrypt_data;

    api->progress_callback = progress_callback;
    api->error_callback = error_callback;
    api->context = ctx; // 设置上下文指针
    
    return api;
}

int archive_cleanup(ArchiveAPI *api) {
    if (!api) return -1;
    
    if (api->context) {
        free(api->context);
    }
    
    free(api);
    return 0;
}

const char* archive_strerror(int error_code) {
    switch (error_code) {
        case ARCHIVE_OK:
            return "No error";
        case ARCHIVE_ERROR_OPEN:
            return "Failed to open archive file";
        case ARCHIVE_ERROR_READ:
            return "Failed to read from archive file";
        case ARCHIVE_ERROR_WRITE:
            return "Failed to write to archive file";
        case ARCHIVE_ERROR_MEMORY:
            return "Memory allocation error";
        case ARCHIVE_ERROR_INVALID:
            return "Invalid archive format";
        case ARCHIVE_ERROR_NOT_FOUND:
            return "File not found in archive";
        case ARCHIVE_ERROR_COMPRESSION:
            return "Compression/decompression error";
        case ARCHIVE_ERROR_ENCRYPTION:
            return "Encryption/decryption error";
        case ARCHIVE_ERROR_CORRUPTED:
            return "Archive is corrupted";
        default:
            return "Unknown error";
    }
}
// 打开归档文件（内部使用）
  ArchiveFile* open_archive_file(const char *filename, const char *mode) {
    ArchiveFile *af = malloc(sizeof(ArchiveFile));
    if (!af) return NULL;
    
    af->fp = fopen(filename, mode);
    if (!af->fp) {
        free(af);
        return NULL;
    }
    
    af->filename = strdup(filename);
    af->is_modified = 0;
    af->entries = NULL;
    
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        // 读取归档头
        fread(&af->header, sizeof(ArchiveHeader), 1, af->fp);
        
        // 验证魔数
        if (af->header.magic != 0x48435241) { // "ARCH"
            fclose(af->fp);
            free(af->filename);
            free(af);
            return NULL;
        }
        
        // 读取文件条目
        if (af->header.file_count > 0) {
            af->entries = malloc(sizeof(FileEntry) * af->header.file_count);
            fread(af->entries, sizeof(FileEntry), af->header.file_count, af->fp);
        }
    } else {
        // 创建新的归档头
        af->header.magic = 0x48435241; // "ARCH"
        af->header.version = ARCHIVE_FORMAT_VERSION;
        af->header.header_size = sizeof(ArchiveHeader);
        af->header.file_count = 0;
        af->header.total_size = 0;
        af->header.archive_size = 0;
        af->header.create_time = time(NULL);
        af->header.flags = 0;
        memset(af->header.reserved, 0, sizeof(af->header.reserved));
        
        // 写入归档头（稍后会更新）
        fwrite(&af->header, sizeof(ArchiveHeader), 1, af->fp);
    }
    
    return af;
}

// 关闭归档文件
  void close_archive_file(ArchiveFile *af) {
    if (!af) return;
    
    if (af->fp) {
        if (af->is_modified) {
            // 更新归档头并写入
            fseek(af->fp, 0, SEEK_SET);
            fwrite(&af->header, sizeof(ArchiveHeader), 1, af->fp);
        }
        fclose(af->fp);
    }
    
    if (af->filename) free(af->filename);
    if (af->entries) free(af->entries);
    free(af);
}
void report_progress(ArchiveContext *ctx, int percentage, const char *filename) {
    if (ctx && ctx->api && ctx->api->progress_callback) {
        ctx->api->progress_callback(percentage, filename);
    }
}
void report_error(ArchiveContext *ctx, const char *message) {
    if (ctx && ctx->api && ctx->api->error_callback) {
        ctx->api->error_callback(message);
    }
}

// 实际的create函数实现
  int archive_create(ArchiveContext *ctx, const char *archive, char **files, int count) {
        // 检查参数
    if (!ctx) {
        // 如果没有上下文，无法报告错误，直接返回错误码
        return ARCHIVE_ERROR_INVALID;
    }
    
    if (!archive || !files || count <= 0) {
        report_error(ctx, "Invalid parameters for create");
        return ARCHIVE_ERROR_INVALID;
    }
    
    // 检查文件是否存在
    for (int i = 0; i < count; i++) {
        if (access(files[i], F_OK) != 0) {
            report_error(ctx, "File does not exist");
            return ARCHIVE_ERROR_NOT_FOUND;
        }
    }
    
    // 创建归档文件
    ArchiveFile *af = open_archive_file(archive, "wb");
    if (!af) {
        report_error(ctx, "Failed to create archive file");
        return ARCHIVE_ERROR_OPEN;
    }

    ctx->current_archive = af;

    // 写入每个文件
    int success_count = 0;
    for (int i = 0; i < count; i++) {
        report_progress(ctx, (i * 100) / count, files[i]);
        
        if (write_file_to_archive(af->fp, files[i], ctx->compression_level, ctx->password)) {
            success_count++;
            af->header.file_count++;
            
            // 更新文件大小统计
            struct stat st;
            if (stat(files[i], &st) == 0) {
                af->header.total_size += st.st_size;
            }
        } else {
            fprintf(stderr, "Failed to write file: %s\n", files[i]);
        }
    }
    
    // 计算归档文件大小
    af->header.archive_size = ftell(af->fp);
    af->is_modified = 1;
    
    // 关闭归档文件
    close_archive_file(af);
    ctx->current_archive = NULL;
    
    report_progress(ctx, 100, "Archive creation complete");
    
    if (success_count == 0) {
        return ARCHIVE_ERROR_WRITE;
    } else if (success_count < count) {
        fprintf(stderr, "Warning: Only %d of %d files were archived\n", success_count, count);
    }
    
    return ARCHIVE_OK;
}

// 实际的extract函数实现
  int archive_extract(ArchiveContext *ctx, const char *archive, const char *dest) {
    if (!archive) {
        report_error(ctx, "Archive filename is NULL");
        return ARCHIVE_ERROR_INVALID;
    }
    
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        report_error(ctx, "Failed to open archive file");
        return ARCHIVE_ERROR_OPEN;
    }
    
    ctx->current_archive = af;
    
    // 创建目标目录（如果不存在）
    if (dest && *dest) {
        #ifdef _WIN32
            _mkdir(dest);
        #else
            mkdir(dest, 0755);
        #endif
    }
    
    // 提取每个文件
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        report_progress(ctx, (i * 100) / af->header.file_count, af->entries[i].filename);
        
        if (!read_file_from_archive(af->fp, &af->entries[i], dest, ctx->password)) {
            fprintf(stderr, "Failed to extract file: %s\n", af->entries[i].filename);
        }
    }
    
    close_archive_file(af);
    ctx->current_archive = NULL;
    
    report_progress(ctx, 100, "Extraction complete");
    return ARCHIVE_OK;
}

// 实际的list函数实现
  int archive_list(ArchiveContext *ctx, const char *archive) {
    if (!archive) {
        report_error(ctx, "Archive filename is NULL");
        return ARCHIVE_ERROR_INVALID;
    }
    
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        report_error(ctx, "Failed to open archive file");
        return ARCHIVE_ERROR_OPEN;
    }
    
    printf("Archive: %s\n", archive);
    printf("Version: %d.%d\n", af->header.version >> 8, af->header.version & 0xFF);
    printf("Created: %s", ctime(&af->header.create_time));
    printf("Total files: %u\n", af->header.file_count);
    printf("Total size: %u bytes\n", af->header.total_size);
    printf("Archive size: %u bytes\n", af->header.archive_size);
    printf("Compression ratio: %.2f%%\n", 
           (float)af->header.archive_size / af->header.total_size * 100);
    
    printf("\nFiles:\n");
    printf("┌─────┬──────────────────────────────────────┬──────────────┬──────────────┬────────────────┐\n");
    printf("│ No. │ Filename                             │ Size (bytes) │ Stored Size  │ Flags          │\n");
    printf("├─────┼──────────────────────────────────────┼──────────────┼──────────────┼────────────────┤\n");
    
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        FileEntry *entry = &af->entries[i];
        
        // 构建标志字符串
        char flags_str[16] = {0};
        if (entry->flags & FLAG_COMPRESSED) strcat(flags_str, "C");
        if (entry->flags & FLAG_ENCRYPTED) strcat(flags_str, "E");
        if (entry->flags & FLAG_DIRECTORY) strcat(flags_str, "D");
        if (entry->flags & FLAG_SYMLINK) strcat(flags_str, "L");
        
        printf("│ %3u │ %-36s │ %12u │ %12u │ %-14s │\n",
               i + 1, entry->filename, entry->file_size, 
               entry->stored_size, flags_str);
    }
    
    printf("└─────┴──────────────────────────────────────┴──────────────┴──────────────┴────────────────┘\n");
    
    close_archive_file(af);
    return ARCHIVE_OK;
}

// 添加文件到现有归档
  int archive_add(ArchiveContext *ctx, const char *archive, char **files, int count) {
    // 1. 打开现有归档读取所有内容
    // 2. 创建临时文件
    // 3. 写入原有文件
    // 4. 写入新文件
    // 5. 替换原文件
    
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", archive);
    
    // 读取现有归档
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        return archive_create(ctx, archive, files, count);
    }
    
    // 创建临时归档
    ArchiveFile *temp_af = open_archive_file(temp_file, "wb");
    if (!temp_af) {
        close_archive_file(af);
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 复制原有文件
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        fseek(af->fp, af->entries[i].offset, SEEK_SET);
        uint8_t *data = malloc(af->entries[i].stored_size);
        fread(data, 1, af->entries[i].stored_size, af->fp);
        
        // 写入到临时文件
        fwrite(&af->entries[i], sizeof(FileEntry), 1, temp_af->fp);
        fwrite(data, 1, af->entries[i].stored_size, temp_af->fp);
        
        free(data);
    }
    
    // 添加新文件
    for (int i = 0; i < count; i++) {
        if (write_file_to_archive(temp_af->fp, files[i], ctx->compression_level, ctx->password)) {
            temp_af->header.file_count++;
        }
    }
    
    // 更新头信息
    temp_af->is_modified = 1;
    close_archive_file(temp_af);
    close_archive_file(af);
    
    // 替换文件
    remove(archive);
    rename(temp_file, archive);
    
    return ARCHIVE_OK;
}

// 验证归档完整性
  int archive_verify(ArchiveContext *ctx, const char *archive) {
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        return ARCHIVE_ERROR_OPEN;
    }
    
    printf("Verifying archive: %s\n", archive);
    printf("Checking %u files...\n", af->header.file_count);
    
    int errors = 0;
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        FileEntry *entry = &af->entries[i];
        
        // 跳转到文件数据
        fseek(af->fp, entry->offset, SEEK_SET);
        
        // 读取存储的数据
        uint8_t *stored_data = malloc(entry->stored_size);
        fread(stored_data, 1, entry->stored_size, af->fp);
        
        // 解密（如果需要）
        uint8_t *decrypted_data = stored_data;
        size_t decrypted_size = entry->stored_size;
        
        if (entry->flags & FLAG_ENCRYPTED) {
            if (ctx->password && *ctx->password) {
                decrypt_data(stored_data, entry->stored_size, 
                           &decrypted_data, &decrypted_size, ctx->password);
                free(stored_data);
            }
        }
        
        // 解压（如果需要）
        uint8_t *decompressed_data = decrypted_data;
        if (entry->flags & FLAG_COMPRESSED) {
            decompress_data(decrypted_data, decrypted_size,
                          &decompressed_data, entry->file_size);
            if (decrypted_data != stored_data) {
                free(decrypted_data);
            }
        }
        
        // 计算CRC32
        uint32_t calculated_crc = calculate_crc32(decompressed_data, entry->file_size);
        if (calculated_crc != entry->crc32) {
            printf("  [ERROR] File %s: CRC32 mismatch (expected: %08X, got: %08X)\n",
                   entry->filename, entry->crc32, calculated_crc);
            errors++;
        } else {
            printf("  [OK] File %s: CRC32 verified\n", entry->filename);
        }
        
        free(decompressed_data);
    }
    
    close_archive_file(af);
    
    if (errors == 0) {
        printf("All files verified successfully!\n");
        return ARCHIVE_OK;
    } else {
        printf("%d file(s) failed verification\n", errors);
        return ARCHIVE_ERROR_CORRUPTED;
    }
}




  int archive_remove(const char *archive, char **files, int count) {
    // 1. 打开现有归档读取所有内容
    // 2. 创建临时文件
    // 3. 写入未删除的文件
    // 4. 替换原文件
    
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", archive);
    
    // 读取现有归档
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 创建临时归档
    ArchiveFile *temp_af = open_archive_file(temp_file, "wb");
    if (!temp_af) {
        close_archive_file(af);
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 复制未删除的文件
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        int to_delete = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(af->entries[i].filename, files[j]) == 0) {
                to_delete = 1;
                break;
            }
        }
        
        if (!to_delete) {
            fseek(af->fp, af->entries[i].offset, SEEK_SET);
            uint8_t *data = malloc(af->entries[i].stored_size);
            fread(data, 1, af->entries[i].stored_size, af->fp);
            
            // 写入到临时文件
            fwrite(&af->entries[i], sizeof(FileEntry), 1, temp_af->fp);
            fwrite(data, 1, af->entries[i].stored_size, temp_af->fp);
            
            free(data);
        }
    }
    
    // 更新头信息
    temp_af->is_modified = 1;
    close_archive_file(temp_af);
    close_archive_file(af);
    
    // 替换文件
    remove(archive);
    rename(temp_file, archive);
    
    return ARCHIVE_OK;
}
  int archive_update(const char *archive, char **files, int count) {
    // 1. 打开现有归档读取所有内容
    // 2. 创建临时文件
    // 3. 写入原有文件（更新指定文件）
    // 4. 替换原文件
    
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", archive);
    
    // 读取现有归档
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 创建临时归档
    ArchiveFile *temp_af = open_archive_file(temp_file, "wb");
    if (!temp_af) {
        close_archive_file(af);
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 复制原有文件，更新指定文件
    for (uint32_t i = 0; i < af->header.file_count; i++) {
        int to_update = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(af->entries[i].filename, files[j]) == 0) {
                to_update = 1;
                break;
            }
        }
        
        if (to_update) {
            // 写入更新的文件
            write_file_to_archive(temp_af->fp, af->entries[i].filename, COMPRESSION_DEFAULT, NULL);
        } else {
            fseek(af->fp, af->entries[i].offset, SEEK_SET);
            uint8_t *data = malloc(af->entries[i].stored_size);
            fread(data, 1, af->entries[i].stored_size, af->fp);
            
            // 写入到临时文件
            fwrite(&af->entries[i], sizeof(FileEntry), 1, temp_af->fp);
            fwrite(data, 1, af->entries[i].stored_size, temp_af->fp);
            
            free(data);
        }
    }
    
    // 更新头信息
    temp_af->is_modified = 1;
    close_archive_file(temp_af);
    close_archive_file(af);
    
    // 替换文件
    remove(archive);
    rename(temp_file, archive);
    
    return ARCHIVE_OK;
}
  int archive_test(const char *archive) {
    // 简单测试归档能否打开和读取头信息
    ArchiveFile *af = open_archive_file(archive, "rb");
    if (!af) {
        return ARCHIVE_ERROR_OPEN;
    }
    
    // 仅验证头信息
    if (af->header.magic != 0x48435241) { // "ARCH"
        close_archive_file(af);
        return ARCHIVE_ERROR_INVALID;
    }
    
    close_archive_file(af);
    return ARCHIVE_OK;
}

// 进度回调函数
   void progress_callback(int percentage, const char *filename) {
    if (quiet || !progress) return;
    
      int last_percentage = -1;
    if (percentage != last_percentage) {
        if (filename && *filename) {
            printf("\rProgress: %3d%% - %-30s", percentage, filename);
        } else {
            printf("\rProgress: %3d%%", percentage);
        }
        fflush(stdout);
        
        if (percentage >= 100) {
            printf("\n");
        }
        last_percentage = percentage;
    }
}
// 错误回调函数
   void error_callback(const char *message) { 
    fprintf(stderr, "Error: %s\n", message);
}


// 计算CRC32校验和
 uint32_t calculate_crc32(const uint8_t *data, size_t length) {
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
ArchiveContext* archive_context_create(void){
    ArchiveContext *ctx = malloc(sizeof(ArchiveContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(ArchiveContext));
    ctx->compression_level = COMPRESSION_DEFAULT;
    ctx->password = NULL;
    ctx->log_file = NULL;
    ctx->current_archive = NULL;
    ctx->write_buffer = NULL;
    ctx->recursive = 0;
    ctx->exclude_patterns = NULL;
    ctx->exclude_count = 0;
    ctx->api = NULL;
    
    return ctx;
}
int archive_context_destroy(ArchiveContext *ctx){
    if (!ctx) return -1;
    
    if (ctx->log_file) {
        fclose(ctx->log_file);
    }
    
    if (ctx->write_buffer) {
        free(ctx->write_buffer);
    }
    
    if (ctx->exclude_patterns) {
        for (int i = 0; i < ctx->exclude_count; i++) {
            free(ctx->exclude_patterns[i]);
        }
        free(ctx->exclude_patterns);
    }
    
    free(ctx);
    return 0;
}
 int archive_append_files(ArchiveContext *ctx, const char **files, int file_count) {
    if (!ctx || !ctx->current_archive) {
        report_error(ctx, "No open archive to append files to");
        return ARCHIVE_ERROR_INVALID;
    }
    
    for (int i = 0; i < file_count; i++) {
        report_progress(ctx, (i * 100) / file_count, files[i]);
        
        if (!write_file_to_archive(ctx->current_archive->fp, files[i], 
                                   ctx->compression_level, ctx->password)) {
            fprintf(stderr, "Failed to append file: %s\n", files[i]);
        } else {
            ctx->current_archive->header.file_count++;
        }
    }
    
    ctx->current_archive->is_modified = 1;
    return ARCHIVE_OK;
}