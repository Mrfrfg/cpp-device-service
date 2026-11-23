# Multithreaded TCP Device Service

一个使用 C++17 和 Linux Socket 实现的多线程设备数据服务。

## 功能

- TCP 文本协议
- 多客户端并发连接
- 固定大小线程池
- 有界客户端任务队列
- 过载快速拒绝
- 共享设备数据同步
- SIGINT 优雅停止
- RAII 管理文件描述符
- 单元测试与网络集成测试

## 环境要求

- Linux
- GCC/G++ 13 或兼容版本
- CMake 3.16 或更高版本
- pthread

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
