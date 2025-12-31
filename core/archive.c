#include "archive.h"

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


// 打开归档文件（内部使用）
static ArchiveFile* open_archive_file(const char *filename, const char *mode) {
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
static void close_archive_file(ArchiveFile *af) {
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

// 实际的create函数实现
static int archive_create(ArchiveContext *ctx, const char *archive, char **files, int count) {
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
static int archive_extract(ArchiveContext *ctx, const char *archive, const char *dest) {
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
static int archive_list(ArchiveContext *ctx, const char *archive) {
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
static int archive_add(ArchiveContext *ctx, const char *archive, char **files, int count) {
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
static int archive_verify(ArchiveContext *ctx, const char *archive) {
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
// 进度回调函数
static void progress_callback(int percentage, const char *filename) {
    if (quiet || !progress) return;
    
    static int last_percentage = -1;
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
static void error_callback(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
}

static int archive_remove(const char *archive, char **files, int count) {
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
static int archive_update(const char *archive, char **files, int count) {
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
static int archive_test(const char *archive) {
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