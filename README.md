# myshell


archiver/
├── src/                    # 源代码
│   ├── core/              # 核心模块
│   │   ├── archive.c
│   │   ├── archive.h
│   │   ├── format.c
│   │   └── format.h
│   ├── io/                # IO模块
│   │   ├── file_ops.c
│   │   ├── file_ops.h
│   │   ├── buffer.c
│   │   └── buffer.h
│   ├── compress/          # 压缩模块
│   │   ├── compress.c
│   │   └── compress.h
│   ├── encrypt/           # 加密模块
│   │   ├── encrypt.c
│   │   └── encrypt.h
│   └── utils/             # 工具模块
│       ├── error.c
│       ├── error.h
│       ├── log.c
│       └── log.h
├── include/               # 公共头文件
│   └── archiver.h
├── tests/                 # 测试代码
│   ├── unit_tests.c
│   └── integration_tests.c
├── examples/              # 示例代码
│   └── basic_usage.c
├── docs/                  # 文档
│   ├── api.md
│   └── format_spec.md
├── CMakeLists.txt         # CMake构建文件
├── Makefile               # Make构建文件
└── README.md              # 项目说明