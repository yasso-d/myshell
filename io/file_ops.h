#ifndef FILE_OPS_H
#define FILE_OPS_H


#include "../include/archiver.h"
#include "../io/buffer.h"
#include "../compress/compress.h"
#include "../encrypt/encrypt.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>


// 实际写入文件到归档
int write_file_to_archive(FILE *archive_fp, const char *filename,
                         CompressionLevel compression_level,
                         const char *password);
// 从归档读取文件

int read_file_from_archive(FILE *archive_fp, const FileEntry *entry,
                          const char *dest_path, const char *password);
#endif