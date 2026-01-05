#include "../include/archiver.h"
#include "../include/file_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// 版本信息
#define VERSION "1.0.0"
#define AUTHOR  "yasso-d Archive Tool"

// 全局变量
static int verbose = 0;

static int compression_level = 6;
static char *password = NULL;
ArchiveAPI *API = NULL;
static ArchiveContext ctx;



// 函数工具声明

static void print_usage_tool(void);
static void print_version_tool(void);
static void print_help_tool(void);
static int parse_arguments_tool(int argc, char *argv[]);
static int create_archive_tool(int argc, char *argv[]);
static int extract_archive_tool(int argc, char *argv[]);
static int list_archive_tool(int argc, char *argv[]);
static int add_files_tool(int argc, char *argv[]);
static int remove_files_tool(int argc, char *argv[]);
static int verify_archive_tool(int argc, char *argv[]);
static int update_archive_tool(int argc, char *argv[]);
static int test_archive_tool(int argc, char *argv[]);

//void progress_callback(int percentage, const char *filename);
//void error_callback(const char *message);
//int archive_cleanup(ArchiveAPI *api);

void close_archive_file(ArchiveFile *af);

//static char** expand_wildcards_tool(const char *pattern, int *count);
//static int is_directory_tool(const char *path);
//static int recursive_add_tool(const char *archive, const char *path);

const char* archive_strerror(int error_code) ;
// 自定义 strdup 实现
char* custom_strdup(const char* str) ;
// 命令行选项定义

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {"quiet", no_argument, 0, 'q'},
    {"compression", required_argument, 0, 'c'},
    {"password", required_argument, 0, 'p'},
    {"no-progress", no_argument, 0, 'n'},
    {"file", required_argument, 0, 'f'},
    {"directory", required_argument, 0, 'C'},
    {"exclude", required_argument, 0, 'x'},
    {"include", required_argument, 0, 'i'},
    {"list", no_argument, 0, 'l'},
    {"extract", no_argument, 0, 'x'},
    {"test", no_argument, 0, 't'},
    {"verify", no_argument, 0, 'y'},
    {"update", no_argument, 0, 'u'},
    {0, 0, 0, 0}
};

// 主函数
int main(int argc, char *argv[]) {
    int ret = 0;
    
    // 解析命令行参数
    if (parse_arguments_tool(argc, argv) != 0) {
        return 1;
    }
    
    // 如果没有指定子命令，显示帮助
    if (argc < 2) {
        print_usage_tool();
        return 0;
    }
    
    // 初始化归档API
    API = archive_init();
    if (!API) {
        fprintf(stderr, "Failed to initialize archive library\n");
        return 1;
    }
    
    // 设置回调函数
   // api->progress_callback = progress ? progress_callback : NULL;
   // api->error_callback = error_callback;
    
    // 设置压缩级别
    API->set_compression(compression_level);
    
    // 设置密码（如果有）
    if (password) {
        API->set_encryption(password);
    }
    
    // 根据子命令执行相应操作
    char *subcommand = argv[1];
    
    if (strcmp(subcommand, "create") == 0 || strcmp(subcommand, "c") == 0) {
        ret = create_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "extract") == 0 || strcmp(subcommand, "x") == 0) {
        ret = extract_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "list") == 0 || strcmp(subcommand, "l") == 0) {
        ret = list_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "add") == 0 || strcmp(subcommand, "a") == 0) {
        ret = add_files_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "remove") == 0 || strcmp(subcommand, "r") == 0) {
        ret = remove_files_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "verify") == 0 || strcmp(subcommand, "v") == 0) {
        ret = verify_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "update") == 0 || strcmp(subcommand, "u") == 0) {
        ret = update_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "test") == 0 || strcmp(subcommand, "t") == 0) {
        ret = test_archive_tool(argc - 2, argv + 2);
    } else if (strcmp(subcommand, "help") == 0 || strcmp(subcommand, "h") == 0) {
        print_help_tool();
    } else if (strcmp(subcommand, "version") == 0 || strcmp(subcommand, "V") == 0) {
        print_version_tool();
    } else {
        fprintf(stderr, "Unknown command: %s\n", subcommand);
        print_usage_tool();
        ret = 1;
    }
    
    // 清理资源
    if (API) {
        archive_cleanup(API);
    }
    
    if (password) {
        // 安全清除密码内存
        memset(password, 0, strlen(password));
    }
    
    return ret;
}

// 解析命令行参数
static int parse_arguments_tool(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hVvqc:p:nf:C:x:i:latyur", 
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help_tool();
                exit(0);
            case 'V':
                print_version_tool();
                exit(0);
            case 'v':
                verbose = 1;
                break;
            case 'q':
                quiet = 1;
                progress = 0;
                break;
            case 'c':
                compression_level = atoi(optarg);
                if (compression_level < 0 || compression_level > 9) {
                    fprintf(stderr, "Compression level must be between 0 and 9\n");
                    return 1;
                }
                break;
            case 'p':
                password = custom_strdup(optarg);
                break;
            case 'n':
                progress = 0;
                break;
            case 'f':
                // 归档文件名，在子命令中处理
                break;
            default:
                // 其他选项在子命令中处理
                break;
        }
    }
    
    return 0;
}



// 自定义 strdup 实现
char* custom_strdup(const char* str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str) + 1;
    char* new_str = (char*)malloc(len);
    if (new_str == NULL) return NULL;
    strcpy(new_str, str);
    return new_str;
}

// 创建归档文件
// 创建归档文件的工具函数
static int create_archive_tool(int argc, char *argv[]) {
    // 最少需要2个参数：归档文件名和至少一个文件
    if (argc < 2) {
        fprintf(stderr, "Usage: archive create [options] <archive> <files...>\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -f, --file <archive>    Archive filename\n");
        fprintf(stderr, "  -r, --recursive         Add directories recursively\n");
        fprintf(stderr, "  -v, --verbose           Verbose output\n");
        fprintf(stderr, "  -q, --quiet             Quiet mode\n");
        fprintf(stderr, "  -c, --compress <level>  Compression level (0-9)\n");
        fprintf(stderr, "  -p, --password <pass>   Encryption password\n");
        return 1;
    }
    
    // 解析选项和参数
    char *archive_name = NULL;
    char **files = NULL;
    int file_count = 0;
    int recursive = 0;
    int compress_level = 5; // 默认压缩级别
    char *password = NULL;
    
    // 解析参数
    int i = 0;
    while (i < argc) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 < argc) {
                archive_name = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            recursive = 1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0) {
            if (i + 1 < argc) {
                compress_level = atoi(argv[++i]);
                if (compress_level < 0 || compress_level > 9) {
                    fprintf(stderr, "Warning: Compression level should be 0-9, using default 5\n");
                    compress_level = 5;
                }
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--password") == 0) {
            if (i + 1 < argc) {
                password = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
        else {
            // 第一个非选项参数是归档文件名
            if (!archive_name) {
                archive_name = argv[i];
            } else {
                // 后续的非选项参数是文件
                // 计算文件数量
                file_count = argc - i;
                files = &argv[i];
                break; // 剩下的都是文件
            }
        }
        i++;
    }
    
    // 检查必要参数
    if (!archive_name) {
        fprintf(stderr, "Error: Archive filename is required\n");
        return 1;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "Error: At least one file is required\n");
        return 1;
    }
    
    if (!quiet) {
        printf("Creating archive: %s\n", archive_name);
        printf("Files to archive: %d\n", file_count);
        if (verbose) {
            for (int i = 0; i < file_count; i++) {
                printf("  %s\n", files[i]);
            }
        }
    }
    
    // 创建归档上下文
    ArchiveContext *ctx = archive_context_create();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create archive context\n");
        return 1;
    }
    
    // 设置上下文参数
    ctx->compression_level = compress_level;
    if (password) {
        strncpy(ctx->password, password, sizeof(ctx->password) - 1);
        ctx->password[sizeof(ctx->password) - 1] = '\0';
        //ctx->encryption_enabled = 1;
    }
    //ctx->verbose = verbose;
   // ctx->quiet = quiet;
    
    // 调用API创建归档
    int result = API->create(ctx, archive_name, files, file_count);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to create archive: %s\n", archive_strerror(result));
        archive_context_destroy(ctx);
        return 1;
    }
    
    if (!quiet) {
        printf("Archive created successfully: %s\n", archive_name);
    }
    
    // 清理
    archive_context_destroy(ctx);
    
    return 0;
}

// 提取归档文件
static int extract_archive_tool(int argc, char *argv[]) {
    // 参数检查
    if (argc < 1) {
        fprintf(stderr, "Usage: archive extract [options] <archive> [dest]\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -C, --directory <dir>   Extract to directory\n");
        fprintf(stderr, "  -p, --password <pass>   Password for encrypted archive\n");
        fprintf(stderr, "  -v, --verbose           Verbose output\n");
        fprintf(stderr, "  -q, --quiet             Quiet mode\n");
        fprintf(stderr, "  -f, --force             Overwrite existing files\n");
        fprintf(stderr, "  -k, --keep              Keep directory structure\n");
        return 1;
    }
    
    // 解析选项和参数
    char *archive_name = NULL;
    char *dest_dir = ".";  // 默认当前目录
    char *password = NULL;
    int force = 0;
    int keep_structure = 0;
    int overwrite = 0;
    
    // 解析参数
    int i = 0;
    while (i < argc) {
        if (strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--directory") == 0) {
            if (i + 1 < argc) {
                dest_dir = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--password") == 0) {
            if (i + 1 < argc) {
                password = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = 1;
            overwrite = 1;
        }
        else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) {
            keep_structure = 1;
        }
        else if (strcmp(argv[i], "--overwrite") == 0) {
            overwrite = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
        else {
            // 第一个非选项参数是归档文件名
            if (!archive_name) {
                archive_name = argv[i];
            } else if (!dest_dir || strcmp(dest_dir, ".") == 0) {
                // 第二个非选项参数是目标目录（如果还没有通过 -C 设置）
                dest_dir = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                return 1;
            }
        }
        i++;
    }
    
    // 检查必要参数
    if (!archive_name) {
        fprintf(stderr, "Error: Archive filename is required\n");
        return 1;
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Error: Archive not found: %s\n", archive_name);
        return 1;
    }
    
    if (!quiet) {
        printf("Extracting archive: %s\n", archive_name);
        printf("Destination: %s\n", dest_dir);
    }
    
    // 创建目标目录（如果不存在）
    struct stat st;
    if (stat(dest_dir, &st) != 0) {
        printf("Creating directory: %s\n", dest_dir);
        if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: Failed to create directory: %s\n", dest_dir);
            return 1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Destination is not a directory: %s\n", dest_dir);
        return 1;
    }
    
    // 创建归档上下文
    ArchiveContext *ctx = archive_context_create();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create archive context\n");
        return 1;
    }
    
    // 设置上下文参数
    if (password) {
        strncpy(ctx->password, password, sizeof(ctx->password) - 1);
        ctx->password[sizeof(ctx->password) - 1] = '\0';
       // ctx->encryption_enabled = 1;
    }
    //ctx->verbose = verbose;
    //ctx->quiet = quiet;
    //ctx->overwrite = overwrite;
    
    // 调用提取函数 - 根据你的API结构选择正确的方式
    
    // 方式1：如果API是一个全局结构体指针
    int result;
    if (API && API->extract) {
        result = API->extract(ctx, archive_name, dest_dir);
    } else {
        // 方式2：直接调用函数（如果没有API结构）
        result = archive_extract(ctx, archive_name, dest_dir);
    }
    
    // 检查结果
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to extract archive: %s\n", archive_strerror(result));
        archive_context_destroy(ctx);
        return 1;
    }
    
    if (!quiet) {
        printf("Extraction completed: %s\n", archive_name);
    }
    
    // 清理
    archive_context_destroy(ctx);
    
    return 0;
}

// 列出归档内容
// 添加文件到归档
static int add_files_tool(int argc, char *argv[]) {
    // 参数检查
    if (argc < 2) {
        fprintf(stderr, "Usage: archive add [options] <archive> <files...>\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -r, --recursive     Add directories recursively\n");
        fprintf(stderr, "  -c, --compress      Compression level (0-9)\n");
        fprintf(stderr, "  -p, --password      Password for encryption\n");
        fprintf(stderr, "  -v, --verbose       Verbose output\n");
        fprintf(stderr, "  -q, --quiet         Quiet mode\n");
        fprintf(stderr, "  --update            Only add newer files\n");
        return 1;
    }
    
    // 解析选项和参数
    char *archive_name = NULL;
    char **files = NULL;
    int file_count = 0;
    int recursive = 0;
    int compress_level = 5; // 默认压缩级别
    char *password = NULL;
    int update_only = 0;
    
    // 解析参数
    int i = 0;
    while (i < argc) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--recursive") == 0) {
            recursive = 1;
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0) {
            if (i + 1 < argc) {
                compress_level = atoi(argv[++i]);
                if (compress_level < 0 || compress_level > 9) {
                    fprintf(stderr, "Warning: Compression level should be 0-9, using default 5\n");
                    compress_level = 5;
                }
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--password") == 0) {
            if (i + 1 < argc) {
                password = argv[++i];
            } else {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        }
        else if (strcmp(argv[i], "--update") == 0) {
            update_only = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
        else {
            // 第一个非选项参数是归档文件名
            if (!archive_name) {
                archive_name = argv[i];
            } else {
                // 剩下的都是要添加的文件
                file_count = argc - i;
                files = &argv[i];
                break;
            }
        }
        i++;
    }
    
    // 检查必要参数
    if (!archive_name) {
        fprintf(stderr, "Error: Archive filename is required\n");
        return 1;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "Error: At least one file is required\n");
        return 1;
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Error: Archive not found: %s\n", archive_name);
        return 1;
    }
    
    if (!quiet) {
        printf("Adding files to archive: %s\n", archive_name);
        printf("Files to add: %d\n", file_count);
        if (verbose) {
            for (int i = 0; i < file_count; i++) {
                printf("  %s\n", files[i]);
            }
        }
    }
    
    // 创建归档上下文
    ArchiveContext *ctx = archive_context_create();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create archive context\n");
        return 1;
    }
    
    // 设置上下文参数
    ctx->compression_level = compress_level;
    if (password) {
        strncpy(ctx->password, password, sizeof(ctx->password) - 1);
        ctx->password[sizeof(ctx->password) - 1] = '\0';
       // ctx->encryption_enabled = 1;
    }
   // ctx->verbose = verbose;
    //ctx->quiet = quiet;
    
    // 调用添加文件函数
    int result;
    if (API && API->add) {
        result = API->add(ctx, archive_name, files, file_count);
    } else {
        // 如果没有add函数，我们可以使用追加模式
        result = archive_append_files(ctx, files, file_count);
    }
    
    // 检查结果
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to add files: %s\n", archive_strerror(result));
        archive_context_destroy(ctx);
        return 1;
    }
    
    if (!quiet) {
        printf("Files added successfully\n");
    }
    
    // 清理
    archive_context_destroy(ctx);
    
    return 0;
}



// 从归档中删除文件
static int remove_files_tool(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: archive remove <archive> <files...>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    int file_count = argc - 1;
    char **files = argv + 1;
    
    if (!quiet) {
        printf("Removing files from archive: %s\n", archive_name);
        printf("Files to remove: %d\n", file_count);
        if (verbose) {
            for (int i = 0; i < file_count; i++) {
                printf("  %s\n", files[i]);
            }
        }
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    
    int result =  API->remove(archive_name, files, file_count);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to remove files: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Files removed successfully\n");
    }
    
    return 0;
}

// 验证归档完整性
static int verify_archive_tool(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: archive verify <archive>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    
    if (!quiet) {
        printf("Verifying archive: %s\n", archive_name);
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    
    int result =  API->verify(archive_name);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Archive verification failed: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Archive verification passed: %s\n", archive_name);
    }
    
    return 0;
}

// 更新归档中的文件
static int update_archive_tool(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: archive update <archive> <files...>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    int file_count = argc - 1;
    char **files = argv + 1;
    
    if (!quiet) {
        printf("Updating files in archive: %s\n", archive_name);
        printf("Files to update: %d\n", file_count);
        if (verbose) {
            for (int i = 0; i < file_count; i++) {
                printf("  %s\n", files[i]);
            }
        }
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    
    int result =  API->update(archive_name, files, file_count);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to update files: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Files updated successfully\n");
    }
    
    return 0;
}

// 测试归档文件
static int test_archive_tool(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: archive test <archive>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    
    if (!quiet) {
        printf("Testing archive: %s\n", archive_name);
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    
    int result =  API->test(archive_name);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Archive test failed: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Archive test passed: %s\n", archive_name);
    }
    
    return 0;
}



// 打印使用说明
static void print_usage_tool(void) {
    printf("Archive Tool v%s - A powerful file archiving utility\n", VERSION);
    printf("Usage: archive <command> [options] [arguments]\n\n");
    printf("Commands:\n");
    printf("  create, c    Create a new archive\n");
    printf("  extract, x   Extract files from an archive\n");
    printf("  list, l      List contents of an archive\n");
    printf("  add, a       Add files to an existing archive\n");
    printf("  remove, r    Remove files from an archive\n");
    printf("  verify, v    Verify integrity of an archive\n");
    printf("  update, u    Update files in an archive\n");
    printf("  test, t      Test archive file integrity\n");
    printf("  help, h      Show this help message\n");
    printf("  version, V   Show version information\n\n");
    printf("Global options:\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -q, --quiet            Quiet mode (no output)\n");
    printf("  -c, --compression N    Compression level (0-9, default: 6)\n");
    printf("  -p, --password PASS    Password for encryption\n");
    printf("  -n, --no-progress      Disable progress display\n\n");
    printf("Examples:\n");
    printf("  archive create backup.arc file1.txt file2.txt\n");
    printf("  archive extract backup.arc\n");
    printf("  archive list backup.arc\n");
    printf("  archive add backup.arc newfile.txt\n");
}

// 打印详细帮助信息
static void print_help_tool(void) {
    print_usage_tool();
    printf("\nDetailed command usage:\n");
    printf("CREATE:\n");
    printf("  archive create [options] <archive> <files...>\n");
    printf("  Options:\n");
    printf("    -r, --recursive      Add directories recursively\n");
    printf("    -f, --file NAME      Specify archive filename\n\n");
    
    printf("EXTRACT:\n");
    printf("  archive extract [options] <archive> [dest]\n");
    printf("  Options:\n");
    printf("    -C, --directory DIR  Extract to specific directory\n");
    printf("    -p, --password PASS  Password for encrypted archive\n\n");
    
    printf("LIST:\n");
    printf("  archive list [options] <archive>\n");
    printf("  Options:\n");
    printf("    -v, --verbose        Show detailed information\n\n");
    
    printf("For more information, see the man page: man archive\n");
}

// 打印版本信息
 void print_version_tool(void){
    printf("Archive Tool v%s\n", VERSION);
    printf("Copyright (c) %s\n", AUTHOR);
    printf("A powerful file archiving utility with compression and encryption support\n");
}