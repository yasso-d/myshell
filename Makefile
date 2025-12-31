# Archive Tool Makefile
# 支持多种构建目标

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_FILE_OFFSET_BITS=64
LDFLAGS = -lz -lcrypto -lm

# 目标文件
LIB_SRCS = archive.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

TOOL_SRC = archiver-tool.c
TOOL_OBJ = $(TOOL_SRC:.c=.o)

# 检测操作系统
UNAME_S := $(shell uname -s)

# 根据不同系统设置不同的目标和选项
ifeq ($(UNAME_S),Linux)
    LIB_TARGET = libarchive.so
    SHARED_FLAG = -shared
    INSTALL_LIB_DIR = /usr/local/lib
    INSTALL_INCLUDE_DIR = /usr/local/include
    INSTALL_BIN_DIR = /usr/local/bin
    INSTALL_MAN_DIR = /usr/local/share/man/man1
endif
ifeq ($(UNAME_S),Darwin)
    LIB_TARGET = libarchive.dylib
    SHARED_FLAG = -dynamiclib
    INSTALL_LIB_DIR = /usr/local/lib
    INSTALL_INCLUDE_DIR = /usr/local/include
    INSTALL_BIN_DIR = /usr/local/bin
    INSTALL_MAN_DIR = /usr/local/share/man/man1
endif
ifeq ($(UNAME_S),Windows_NT)
    LIB_TARGET = libarchive.dll
    SHARED_FLAG = -shared
    INSTALL_LIB_DIR = /usr/local/lib
    INSTALL_INCLUDE_DIR = /usr/local/include
    INSTALL_BIN_DIR = /usr/local/bin
    INSTALL_MAN_DIR = /usr/local/share/man/man1
endif

STATIC_LIB = libarchive.a
TOOL_TARGET = archive

# 默认目标：构建所有
all: $(LIB_TARGET) $(STATIC_LIB) $(TOOL_TARGET)

# 构建动态库
$(LIB_TARGET): $(LIB_OBJS)
	$(CC) $(SHARED_FLAG) -o $@ $(LIB_OBJS) $(LDFLAGS)

# 构建静态库
$(STATIC_LIB): $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

# 构建命令行工具
$(TOOL_TARGET): $(TOOL_OBJ) $(LIB_TARGET)
	$(CC) $(CFLAGS) -o $@ $(TOOL_OBJ) -L. -larchive $(LDFLAGS)

# 编译目标文件
%.o: %.c archive.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: clean all

# 发布版本
release: CFLAGS += -DNDEBUG
release: clean all

# 清理
clean:
	rm -f $(LIB_OBJS) $(TOOL_OBJ) $(LIB_TARGET) $(STATIC_LIB) $(TOOL_TARGET) *.a *.so *.dylib *.dll

# 安装到系统
install: all
	@echo "安装Archive Tool到系统..."
	@echo "需要root权限..."
	sudo mkdir -p $(INSTALL_INCLUDE_DIR) $(INSTALL_LIB_DIR) $(INSTALL_BIN_DIR) $(INSTALL_MAN_DIR)
	
	# 安装头文件
	sudo cp archive.h $(INSTALL_INCLUDE_DIR)/
	sudo chmod 644 $(INSTALL_INCLUDE_DIR)/archive.h
	
	# 安装静态库
	sudo cp $(STATIC_LIB) $(INSTALL_LIB_DIR)/
	sudo chmod 644 $(INSTALL_LIB_DIR)/$(STATIC_LIB)
	
	# 安装动态库
	@if [ -f $(LIB_TARGET) ]; then \
		if [ "$(UNAME_S)" = "Linux" ]; then \
			sudo cp $(LIB_TARGET) $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0; \
			sudo chmod 755 $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0; \
			sudo ln -sf $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0 $(INSTALL_LIB_DIR)/$(LIB_TARGET).1; \
			sudo ln -sf $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0 $(INSTALL_LIB_DIR)/$(LIB_TARGET); \
			sudo ldconfig; \
		elif [ "$(UNAME_S)" = "Darwin" ]; then \
			sudo cp $(LIB_TARGET) $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0; \
			sudo chmod 755 $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0; \
			sudo ln -sf $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0 $(INSTALL_LIB_DIR)/$(LIB_TARGET).1; \
			sudo ln -sf $(INSTALL_LIB_DIR)/$(LIB_TARGET).1.0.0 $(INSTALL_LIB_DIR)/$(LIB_TARGET); \
		fi \
	fi
	
	# 安装命令行工具
	sudo cp $(TOOL_TARGET) $(INSTALL_BIN_DIR)/
	sudo chmod 755 $(INSTALL_BIN_DIR)/$(TOOL_TARGET)
	
	# 创建man手册
	@if [ -f archive.1 ]; then \
		sudo cp archive.1 $(INSTALL_MAN_DIR)/; \
		sudo chmod 644 $(INSTALL_MAN_DIR)/archive.1; \
	fi
	
	@echo "安装完成!"
	@echo "使用: archive --help 查看帮助"

# 卸载
uninstall:
	@echo "卸载Archive Tool..."
	@echo "需要root权限..."
	sudo rm -f $(INSTALL_INCLUDE_DIR)/archive.h
	sudo rm -f $(INSTALL_LIB_DIR)/$(STATIC_LIB)
	sudo rm -f $(INSTALL_LIB_DIR)/$(LIB_TARGET)*
	sudo rm -f $(INSTALL_BIN_DIR)/$(TOOL_TARGET)
	sudo rm -f $(INSTALL_MAN_DIR)/archive.1
	@if [ "$(UNAME_S)" = "Linux" ]; then \
		sudo ldconfig; \
	fi
	@echo "卸载完成"

# 测试
test: $(TOOL_TARGET)
	@echo "运行测试..."
	@./$(TOOL_TARGET) version
	@echo ""
	@echo "测试创建归档..."
	@echo "Test content" > test_file.txt
	@./$(TOOL_TARGET) create test.arc test_file.txt
	@echo ""
	@echo "测试列出归档..."
	@./$(TOOL_TARGET) list test.arc
	@echo ""
	@echo "测试提取归档..."
	@mkdir -p test_output
	@./$(TOOL_TARGET) extract test.arc test_output
	@echo ""
	@echo "清理测试文件..."
	@rm -f test_file.txt test.arc
	@rm -rf test_output
	@echo "测试完成!"

# 创建压缩包
dist: clean
	@echo "创建发布包..."
	mkdir -p archive-tool-1.0.0
	cp *.c *.h Makefile README.md LICENSE archive-tool-1.0.0/
	tar czf archive-tool-1.0.0.tar.gz archive-tool-1.0.0/
	rm -rf archive-tool-1.0.0
	@echo "发布包创建完成: archive-tool-1.0.0.tar.gz"

# 查看帮助
help:
	@echo "Archive Tool 构建系统"
	@echo ""
	@echo "可用目标:"
	@echo "  all       - 构建所有目标（默认）"
	@echo "  debug     - 构建调试版本"
	@echo "  release   - 构建发布版本"
	@echo "  clean     - 清理构建文件"
	@echo "  install   - 安装到系统"
	@echo "  uninstall - 从系统卸载"
	@echo "  test      - 运行测试"
	@echo "  dist      - 创建发布包"
	@echo "  help      - 显示此帮助"

.PHONY: all debug release clean install uninstall test dist help