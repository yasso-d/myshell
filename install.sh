#!/bin/bash
echo "测试归档工具修复..."

# 清理
make clean

# 编译
make debug

# 创建测试文件
echo "创建测试文件..."
echo "This is test file 1" > test1.txt
echo "This is test file 2" > test2.txt
echo "This is test file 3" > test3.txt

# 测试不同情况
echo -e "\n1. 测试正常情况:"
./build/bin/archive create test.arc test1.txt test2.txt test3.txt

echo -e "\n2. 测试不存在文件:"
./build/bin/archive create test2.arc test1.txt nonexistent.txt

echo -e "\n3. 测试参数不足:"
./build/bin/archive create

echo -e "\n4. 测试帮助信息:"
./build/bin/archive --help

echo -e "\n5. 列出归档内容:"
./build/bin/archive list test.arc

# 清理
rm -f test1.txt test2.txt test3.txt test.arc test2.arc
echo -e "\n测试完成"