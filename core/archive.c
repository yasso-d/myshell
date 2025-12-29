#include "archive.h"

// 内部数据结构
typedef struct {
    char **files;
    int count;
    int capacity;
} FileList;

typedef struct {
    CompressionLevel compression_level;
    char *password;
    ProgressCallback progress_callback;
    ErrorCallback error_callback;
    FILE *log_file;
} ArchiveContext;


// 内部工具函数声明
static FileList* create_file_list(int initial_capacity);
static void free_file_list(FileList *list);
static int add_file_to_list(FileList *list, const char *filename);
static int validate_archive(const char *archive);
static int process_directory(const char *dirpath, FileList *list);
static void report_progress(ArchiveContext *ctx, int percentage, const char *filename);
static void report_error(ArchiveContext *ctx, const char *message);

// 实际的操作函数
static int archive_create(ArchiveContext *ctx, const char *archive, char **files, int count);
static int archive_extract(ArchiveContext *ctx, const char *archive, const char *dest);
static int archive_list(ArchiveContext *ctx, const char *archive);
static int archive_add(ArchiveContext *ctx, const char *archive, char **files, int count);
static int archive_remove(ArchiveContext *ctx, const char *archive, char **files, int count);
static int archive_verify(ArchiveContext *ctx, const char *archive);
static int archive_update(ArchiveContext *ctx, const char *archive, char **files, int count);
static int archive_test(ArchiveContext *ctx, const char *archive);
static void archive_set_compression(ArchiveContext *ctx, int level);
static void archive_set_encryption(ArchiveContext *ctx, const char *password);
static void archive_set_progress_callback(ArchiveContext *ctx, ProgressCallback callback);


// 初始化函数
ArchiveAPI* archive_init(void) {
    ArchiveAPI *api = malloc(sizeof(ArchiveAPI));
    if (!api) {
        return NULL;
    }
    
    // 分配上下文（修改！）
    ArchiveContext *ctx = calloc(1, sizeof(ArchiveContext));
    if (!ctx) {
        free(api);
        return NULL;
    }
    
    // 设置默认值
    ctx->compression_level = COMPRESSION_DEFAULT;
    ctx->password = NULL;
    ctx->progress_callback = NULL;
    ctx->error_callback = NULL;
    ctx->log_file = stderr;
    
    // 将上下文指针存储在API结构体的末尾（技巧）
    // 我们需要确保API结构体足够大
    memcpy(api + 1, &ctx, sizeof(ArchiveContext*));
    
    // 设置函数指针
    api->create = (int (*)(const char*, char**, int))&archive_create;
    api->extract = (int (*)(const char*, const char*))&archive_extract;
    api->list = (int (*)(const char*))&archive_list;
    api->add = (int (*)(const char*, char**, int))&archive_add;
    api->remove = (int (*)(const char*, char**, int))&archive_remove;
    api->verify = (int (*)(const char*))&archive_verify;
    api->update = (int (*)(const char*, char**, int))&archive_update;
    api->test = (int (*)(const char*))&archive_test;
    api->set_compression = (void (*)(int))&archive_set_compression;
    api->set_encryption = (void (*)(const char*))&archive_set_encryption;
    api->progress_callback = NULL;
    api->error_callback = NULL;
    
    return api;
}


// 清理函数
void archive_cleanup(ArchiveAPI *api) {
    if (!api) return;
    
    // 获取上下文
    ArchiveContext **ctx_ptr = (ArchiveContext**)(api + 1);
    ArchiveContext *ctx = *ctx_ptr;
    
    if (ctx) {
        if (ctx->password) {
            free(ctx->password);
        }
        if (ctx->log_file && ctx->log_file != stderr && ctx->log_file != stdout) {
            fclose(ctx->log_file);
        }
        free(ctx);
    }
    
    free(api);
}
