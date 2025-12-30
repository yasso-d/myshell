#include<stdio.h>
#include<stdlib.h>
#include<string.h>


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

int main() {
    ArchiveAPI *api = malloc(sizeof(ArchiveAPI));
    if (!api) {
        return 0;
    }
    // 打印结构体的大小
    printf("Size of ArchiveAPI structure: %zu bytes\n", sizeof(ArchiveAPI));
    // 打印指针的大小
    printf("Size of api pointer: %zu bytes\n", sizeof(api));
    // 注意：这里使用 %zu 来打印 size_t 类型

    // 初始化函数指针（例如，设置为NULL）
    api->create = NULL;
    api->extract = NULL;
    api->list = NULL;
    api->add = NULL;
    api->remove = NULL;
    api->verify = NULL;
    api->update = NULL;
    api->test = NULL;
    api->set_compression = NULL;
    api->set_encryption = NULL;
    api->progress_callback = NULL;
    api->error_callback = NULL;
    api->context = NULL;

    // 使用 api 结构体...

    free(api);
    return 0;
}