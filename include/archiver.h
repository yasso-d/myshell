#ifndef ARCHIVER_H
#define ARCHIVER_H
//公共头文件



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

// 归档文件格式版本
#define ARCHIVE_FORMAT_VERSION 0x0100  // 1.0

// 文件头标志位
#define FLAG_COMPRESSED    0x01  // 文件已压缩
#define FLAG_ENCRYPTED     0x02  // 文件已加密
#define FLAG_DIRECTORY     0x04  // 目录
#define FLAG_SYMLINK       0x08  // 符号链接
#define FLAG_MODIFIED      0x10  // 文件已修改


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
    uint32_t magic;        // 魔数 "ARCH"
    uint16_t version;      // 格式版本
    uint16_t header_size;  // 头大小
    uint32_t file_count;   // 文件总数
    uint32_t total_size;   // 总大小（未压缩）
    uint32_t archive_size; // 归档文件大小
    time_t create_time;    // 创建时间
    uint32_t flags;        // 归档标志
    uint8_t reserved[64];  // 保留字段
} ArchiveHeader;

// 文件条目设计
typedef struct {
    char filename[256];    // 文件名
    uint32_t file_size;    // 原始文件大小
    uint32_t stored_size;  // 存储大小
    uint32_t offset;       // 在归档中的偏移量
    time_t mtime;          // 修改时间
    time_t atime;          // 访问时间
    uint16_t mode;         // 文件权限
    uint16_t flags;        // 文件标志
    uint32_t crc32;        // CRC32校验和
    uint8_t reserved[32];  // 保留字段
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
        // 上下文指针 - 添加这一行
    void *context;
} ArchiveAPI;

// 初始化函数
ArchiveAPI* archive_init(void);
void archive_cleanup(ArchiveAPI *api);

// 工具函数
const char* archive_strerror(int error_code);
int archive_get_file_count(const char *archive);
#endif // ARCHIVER_H