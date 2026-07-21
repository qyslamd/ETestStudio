#!/bin/bash
# 极简 core dump 设置脚本

echo "=== 当前 ulimit -c ==="
ulimit -c

echo "=== 设置 core 生成路径为 /tmp/cores/core.%e.%p ==="
sudo mkdir -p /tmp/cores
echo "/tmp/cores/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern

echo "=== 设置后的 core_pattern ==="
cat /proc/sys/kernel/core_pattern

echo "=== 开启当前会话 core dump (unlimited) ==="
ulimit -c unlimited
ulimit -c