#!/bin/bash

# 指定目录
directory="."

# 指定文件名
file_pattern="*.sh"  # 替换为你需要处理的文件名模式

# 查找所有符合条件的文件
find "$directory" -type f -name "$file_pattern" -exec sh -c '
    for file do
        # 使用dos2unix将CRLF转换为LF
        dos2unix "$file"
    done
' sh {} +

