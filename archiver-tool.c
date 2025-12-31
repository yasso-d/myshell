#include "archive.h"
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
static ArchiveAPI *api = NULL;
static ArchiveContext ctx;


// 函数声明
static void print_usage_tool(void);
static void print_version_tool(void);
static void print_help_tool(void);
static int parse_arguments_tool(int argc, char *argv[]);
static int create_archive_tool(int argc, char *argv[]);
static int extract_archive_tool(int argc, char *argv[]);
static int list_archive_tool(int argc, char *argv[]);
static int add_files_tool(int argc, char *argv[]);
static int remove_files_tool(int argc, char *argv[]);
static int verify_archive(int argc, char *argv[]);
static int update_archive_tool(int argc, char *argv[]);
static int test_archive_tool(int argc, char *argv[]);
static void progress_callback_tool(int percentage, const char *filename);
static void error_callback_tool(const char *message);
static char** expand_wildcards_tool(const char *pattern, int *count);
static int is_directory_tool(const char *path);
static int recursive_add_tool(const char *archive, const char *path);

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
    if (parse_arguments(argc, argv) != 0) {
        return 1;
    }
    
    // 如果没有指定子命令，显示帮助
    if (argc < 2) {
        print_usage();
        return 0;
    }
    
    // 初始化归档API
    api = archive_init();
    if (!api) {
        fprintf(stderr, "Failed to initialize archive library\n");
        return 1;
    }
    
    // 设置回调函数
    api->progress_callback = progress ? progress_callback : NULL;
    api->error_callback = error_callback;
    
    // 设置压缩级别
    api->set_compression(compression_level);
    
    // 设置密码（如果有）
    if (password) {
        api->set_encryption(password);
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
    if (api) {
        archive_cleanup(api);
    }
    
    if (password) {
        // 安全清除密码内存
        memset(password, 0, strlen(password));
    }
    
    return ret;
}

// 解析命令行参数
static int parse_arguments(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hVvqc:p:nf:C:x:i:latyur", 
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help();
                exit(0);
            case 'V':
                print_version();
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
                password = strdup(optarg);
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

// 创建归档文件
static int create_archive_tool(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: archive create [options] <archive> <files...>\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -f, --file <archive>    Archive filename\n");
        fprintf(stderr, "  -r, --recursive         Add directories recursively\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    int file_count = argc - 1;
    char **files = argv + 1;
    
    if (!quiet) {
        printf("Creating archive: %s\n", archive_name);
        printf("Files to archive: %d\n", file_count);
        if (verbose) {
            for (int i = 0; i < file_count; i++) {
                printf("  %s\n", files[i]);
            }
        }
    }
    
    int result = api->create(archive_name, files, file_count);
    if (result != ARCHIVE_ERROR_OPEN) {
        fprintf(stderr, "Failed to create archive: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Archive created successfully: %s\n", archive_name);
    }
    
    return 0;
}

// 提取归档文件
static int extract_archive_tool(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: archive extract [options] <archive> [dest]\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -C, --directory <dir>   Extract to directory\n");
        fprintf(stderr, "  -p, --password <pass>   Password for encrypted archive\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    char *dest_dir = argc > 1 ? argv[1] : ".";
    
    if (!quiet) {
        printf("Extracting archive: %s\n", archive_name);
        printf("Destination: %s\n", dest_dir);
    }
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    
    // 创建目标目录（如果不存在）
    struct stat st;
    if (stat(dest_dir, &st) != 0) {
        if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Failed to create directory: %s\n", dest_dir);
            return 1;
        }
    }
    int result = api->extract(&ctx,archive_name, dest_dir);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to extract archive: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Extraction completed: %s\n", archive_name);
    }
    
    return 0;
}

// 列出归档内容
static int list_archive_tool(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: archive list <archive>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    
    // 检查归档文件是否存在
    if (access(archive_name, F_OK) != 0) {
        fprintf(stderr, "Archive not found: %s\n", archive_name);
        return 1;
    }
    int result = api->list(&ctx, archive_name);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to list archive: %s\n", archive_strerror(result));
        return 1;
    }
    
    return 0;
}

// 添加文件到归档
static int add_files_tool(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: archive add <archive> <files...>\n");
        return 1;
    }
    
    char *archive_name = argv[0];
    int file_count = argc - 1;
    char **files = argv + 1;
    
    if (!quiet) {
        printf("Adding files to archive: %s\n", archive_name);
        printf("Files to add: %d\n", file_count);
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
    
    int result = api->add(archive_name, files, file_count);
    if (result != ARCHIVE_OK) {
        fprintf(stderr, "Failed to add files: %s\n", archive_strerror(result));
        return 1;
    }
    
    if (!quiet) {
        printf("Files added successfully\n");
    }
    
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
    
    int result = api->remove(archive_name, files, file_count);
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
    
    int result = api->verify(archive_name);
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
    
    int result = api->update(archive_name, files, file_count);
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
    
    int result = api->test(archive_name);
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
static void print_version_tool(void) {
    printf("Archive Tool v%s\n", VERSION);
    printf("Copyright (c) %s\n", AUTHOR);
    printf("A powerful file archiving utility with compression and encryption support\n");
}