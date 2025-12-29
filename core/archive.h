#ifndef ARCHIVE_H
#define ARCHIVE_H

#include"../include/archiver.h" //公共头文件


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdint.h>
#include<time.h>
#include<errno.h>
#include<stddef.h>

// 错误代码定义
typedef enum {
    ARCHIVE_OK = 0,
    ARCHIVE_ERROR_OPEN = -1,
    ARCHIVE_ERROR_READ = -2,
    ARCHIVE_ERROR_WRITE = -3,
    ARCHIVE_ERROR_MEMORY = -4,
    ARCHIVE_ERROR_INVALID = -5,
    ARCHIVE_ERROR_NOT_FOUND = -6,
    ARCHIVE_ERROR_COMPRESSION = -7,
    ARCHIVE_ERROR_ENCRYPTION = -8,
    ARCHIVE_ERROR_CORRUPTED = -9
} ArchiveError;

// 压缩级别
typedef enum {
    COMPRESSION_NONE = 0,
    COMPRESSION_FAST = 1,
    COMPRESSION_DEFAULT = 6,
    COMPRESSION_MAX = 9
} CompressionLevel;

// 进度回调函数类型
typedef void (*ProgressCallback)(int percentage, const char *filename);
typedef void (*ErrorCallback)(const char *message);


// 归档格式设计
typedef struct {
    uint32_t magic_number;     // 魔数：标识文件类型
    uint16_t version;          // 版本号
    uint16_t flags;            // 标志位（压缩、加密等）
    uint64_t index_offset;     // 索引表偏移量
    uint32_t file_count;       // 文件数量
    uint32_t reserved;         // 保留字段
    uint8_t  signature[64];    // 数字签名
} ArchiveHeader;

// 文件条目设计
typedef struct {
    char     filename[256];    // 文件名
    uint64_t offset;           // 文件数据偏移
    uint64_t size;            // 原始大小
    uint64_t compressed_size;  // 压缩后大小
    uint32_t crc32;           // CRC校验值
    uint32_t mode;            // 文件权限
    uint32_t uid;             // 用户ID
    uint32_t gid;             // 组ID
    uint64_t mtime;           // 修改时间
    uint32_t flags;           // 文件标志
} FileEntry;

// 归档上下文
typedef struct {
    FILE *fp;                  // 文件指针
    ArchiveHeader header;      // 归档头
    FileEntry *index;         // 索引表
    size_t index_size;        // 索引大小
    char *password;           // 密码（可选）
    uint8_t *buffer;          // 缓冲区
    size_t buffer_size;       // 缓冲区大小
} ArchiveContext;


// 核心接口
typedef struct {
    // 归档操作
    int (*create)(const char *archive, char **files, int count);
    int (*extract)(const char *archive, const char *dest);
    int (*list)(const char *archive);
    int (*add)(const char *archive, char **files, int count);
    int (*remove)(const char *archive, char **files, int count);
    
    // 高级功能
    int (*verify)(const char *archive);
    int (*update)(const char *archive, char **files, int count);
    int (*test)(const char *archive);
    
    // 压缩/加密
    void (*set_compression)(int level);
    void (*set_encryption)(const char *password);
    
    // 回调函数
    void (*progress_callback)(int percentage, const char *filename);
    void (*error_callback)(const char *message);
} ArchiveAPI;

// 初始化函数
ArchiveAPI* archive_init(void);
void archive_cleanup(ArchiveAPI *api);

// 工具函数
const char* archive_strerror(int error_code);
int archive_get_file_count(const char *archive);
#endif