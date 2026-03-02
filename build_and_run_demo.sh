#!/bin/bash
# 在 libeio-1.0.2 目录下编译并运行 demo
set -e

# 进入源码目录
cd "$(dirname "$0")"

# 配置和编译
./configure
make

# 运行官方 demo
./demo
