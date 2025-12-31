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
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

// 归档文件格式版本
#define ARCHIVE_FORMAT_VERSION 0x0100  // 1.0

// 文件头标志位
#define FLAG_COMPRESSED    0x01  // 文件已压缩
#define FLAG_ENCRYPTED     0x02  // 文件已加密
#define FLAG_DIRECTORY     0x04  // 目录
#define FLAG_SYMLINK       0x08  // 符号链接
#define FLAG_MODIFIED      0x10  // 文件已修改

static int quiet = 0;
static int progress = 1;

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

// 内部数据结构
typedef struct {
    ArchiveHeader header;
    FileEntry *entries;
    FILE *fp;
    char *filename;
    int is_modified;
} ArchiveFile;

// 扩展ArchiveContext
typedef struct {
    CompressionLevel compression_level;
    char *password;
    ProgressCallback progress_callback;
    ErrorCallback error_callback;
    FILE *log_file;
    ArchiveFile *current_archive;
    MemoryBuffer *write_buffer;

    int recursive;  // 是否递归添加目录
    char **exclude_patterns;  // 排除模式
    int exclude_count;
} ArchiveContext;


// 核心接口
typedef struct {
    // 归档操作
    int (*create)(const char *archive, char **files, int count);
    int (*extract)(ArchiveContext *ctx, const char *archive, const char *dest);
    int (*list)(ArchiveContext *ctx,const char *archive);
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


// 进度回调函数类型
typedef void (*ProgressCallback)(int percentage, const char *filename);
typedef void (*ErrorCallback)(const char *message);

// 打开归档文件（内部使用）
static ArchiveFile* open_archive_file(const char *filename, const char *mode);
// 关闭归档文件
static void close_archive_file(ArchiveFile *af);
// 实际的create函数实现
static int archive_create(ArchiveContext *ctx, const char *archive, char **files, int count);
// 实际的extract函数实现
static int archive_extract(ArchiveContext *ctx, const char *archive, const char *dest);
// 实际的list函数实现
static int archive_list(ArchiveContext *ctx, const char *archive);
// 添加文件到现有归档
static int archive_add(ArchiveContext *ctx, const char *archive, char **files, int count);
// 验证归档完整性
static int archive_verify(ArchiveContext *ctx, const char *archive);
static int archive_remove(const char *archive, char **files, int count);
static int archive_update(const char *archive, char **files, int count);
static int archive_test(const char *archive);
// 压缩函数
static int compress_data(const uint8_t *input, size_t input_size,
                         uint8_t **output, size_t *output_size,
                         int level);
// 解压函数
static int decompress_data(const uint8_t *input, size_t input_size,
                           uint8_t **output, size_t *output_size);
// 加密函数
static int encrypt_data(const uint8_t *input, size_t input_size,
                        uint8_t **output, size_t *output_size,
                        const char *password);
// 解密函数
static int decrypt_data(const uint8_t *input, size_t input_size,
                        uint8_t **output, size_t *output_size,
                        const char *password);

// 进度回调函数
static void progress_callback(int percentage, const char *filename);
// 错误回调函数
static void error_callback(const char *message);

#endif // ARCHIVER_H