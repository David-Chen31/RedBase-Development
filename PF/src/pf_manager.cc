#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../internal/pf_internal.h"
#include "pf_manager.h"
#include "../internal/buffer_manager.h"

//
// PF_Manager
//
// 描述: 构造函数
//
PF_Manager::PF_Manager() : diskSpaceLimit(0), usedDiskPages(0), databaseName("default") {
    // 初始化磁盘空间管理变量
    LoadDiskUsageMetadata();  // 从磁盘加载使用情况
}

//
// ~PF_Manager
//
// 描述: 析构函数
//
PF_Manager::~PF_Manager() {
    // 保存磁盘使用情况到文件
    if (diskSpaceLimit > 0) {
        SaveDiskUsageMetadata();
    }
}

//
// CreateFile
//
// 描述: 创建一个新的分页文件
// 输入参数:
//     fileName - 要创建的文件名
// 返回值:
//     PF return code
//
RC PF_Manager::CreateFile(const char *fileName) {
    int fd;     // 文件描述符
    
    // 检查文件名是否为空
    if (fileName == nullptr)
        return PF_INVALIDNAME;
    
    // 检查磁盘空间限制 - 创建文件至少需要1页（文件头）
    if (diskSpaceLimit > 0 && !CanAllocateDiskPages(1)) {
        printf("错误: 磁盘空间不足，无法创建文件 %s\n", fileName);
        PrintDiskUsage();
        return PF_NOMEM;
    }
    
    // 创建文件
#ifdef _WIN32
    fd = _open(fileName, O_CREAT | O_EXCL | O_WRONLY | O_BINARY,
               _S_IREAD | _S_IWRITE);
#else
    fd = open(fileName, O_CREAT | O_EXCL | O_WRONLY,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
    if (fd < 0)
        return PF_UNIX;        // 返回 UNIX 系统错误
    
    // 初始化文件头信息
    PF_FileHeader fileHeader;
    fileHeader.firstFree = PF_PAGE_LIST_END;    // 没有空闲页面
    fileHeader.numPages = 0;                     // 初始页数为0
    
    // 写入文件头
    int numBytes = write(fd, &fileHeader, sizeof(fileHeader));
    if (numBytes != sizeof(fileHeader)) {
        // 写入文件头失败，清理并返回错误
        unlink(fileName);    // 删除刚创建的文件
        close(fd);
        return PF_HDRWRITE;
    }
    
    // 关闭文件
    if (close(fd) < 0)
        return PF_UNIX;
    
    // 文件创建成功，分配磁盘空间（文件头算作1页）
    if (diskSpaceLimit > 0) {
        AllocateDiskPages(1);
    }
        
    return 0;   // 成功返回
}

//
// DestroyFile
//
// 描述: 删除分页文件
// 输入参数:
//     fileName - 要删除的文件名
// 返回值:
//     PF return code
//
RC PF_Manager::DestroyFile(const char *fileName) {
    // 检查文件名是否为空
    if (fileName == nullptr)
        return PF_INVALIDNAME;
    
    // 如果启用了磁盘空间管理，需要计算要释放的页面数
    size_t pagesToFree = 0;
    if (diskSpaceLimit > 0) {
        // 尝试打开文件读取页面数
        int fd = open(fileName, O_RDONLY);
        if (fd >= 0) {
            PF_FileHeader fileHeader;
            if (read(fd, &fileHeader, sizeof(fileHeader)) == sizeof(fileHeader)) {
                pagesToFree = fileHeader.numPages + 1; // +1 for file header
            }
            close(fd);
        }
    }
    
    // 删除文件
    if (unlink(fileName) < 0)
        return PF_UNIX;    // UNIX 系统错误
    
    // 释放磁盘空间
    if (diskSpaceLimit > 0 && pagesToFree > 0) {
        DeallocateDiskPages(pagesToFree);
    }
        
    return 0;    // 成功返回
}

//
// OpenFile
//
// 描述: 打开已存在的分页文件
// 输入参数:
//     fileName   - 要打开的文件名
// 输入/输出参数:
//     fileHandle - 返回与文件关联的句柄
// 返回值:
//     PF return code
//
RC PF_Manager::OpenFile(const char *fileName, PF_FileHandle &fileHandle) {
    int fd;    // UNIX 文件描述符
    
    // 检查文件名是否为空
    if (fileName == nullptr)
        return PF_INVALIDNAME;
    
    // 检查文件句柄是否已经打开文件
    if (fileHandle.GetFd() >= 0)
        return PF_FILEOPEN;

    // 打开文件
#ifdef _WIN32
    fd = _open(fileName, O_RDWR | O_BINARY);
#else
    fd = open(fileName, O_RDWR);
#endif
    if (fd < 0)
        return PF_UNIX;    // UNIX 系统错误

    // 读取文件头
    PF_FileHeader fileHeader;
    int numBytes = read(fd, &fileHeader, sizeof(fileHeader));
    if (numBytes != sizeof(fileHeader)) {
        close(fd);
        return PF_HDRREAD;
    }
    
    // 初始化文件句柄
    if ((fileHandle.Init(fd, fileHeader, this)) != 0) {
        close(fd);
        return PF_PAGEINBUF;
    }
    
    return 0;    // 成功返回
}

//
// CloseFile
//
// 描述: 关闭已打开的分页文件
// 输入参数:
//     fileHandle - 要关闭的文件的句柄
// 返回值:
//     PF return code
//
RC PF_Manager::CloseFile(PF_FileHandle &fileHandle) {
    // 获取文件描述符
    int fd = fileHandle.GetFd();
    
    // 检查文件是否已经关闭
    if (fd < 0)
        return PF_CLOSEDFILE;
    
    // 刷新所有脏页到磁盘
    RC rc = fileHandle.ForcePages();
    if (rc != 0)
        return rc;
    
    // 写回文件头（如果被修改）
    rc = fileHandle.WriteHeader();
    if (rc != 0)
        return rc;
    
    // 关闭文件
    
    // 清空该文件在缓冲区中的所有页面（防止文件描述符重用导致的缓存问题）
    rc = BufferManager::Instance().ClearFilePages(fd);
    if (rc != 0)
        return rc;
    if (close(fd) < 0)
        return PF_UNIX;
    
    // 重置文件句柄
    fileHandle.Reset();
    
    return 0;    // 成功返回
}

//
// AllocateBlock
//
// 描述: 分配一个新的临时页面（用于暂存数据）
// 输出参数:
//     buffer - 返回指向分配的内存的指针
// 返回值:
//     PF return code
//
RC PF_Manager::AllocateBlock(char *&buffer) {
    // 分配一个页面大小的内存块
    buffer = new char[PF_PAGE_SIZE];
    if (buffer == nullptr)
        return PF_NOMEM;
        
    return 0;    // 成功返回
}

//
// DisposeBlock
//
// 描述: 释放之前通过 AllocateBlock 分配的内存
// 输入参数:
//     buffer - 要释放的内存块的指针
// 返回值:
//     PF return code
//
RC PF_Manager::DisposeBlock(char *buffer) {
    // 检查参数是否为空
    if (buffer == nullptr)
        return PF_INVALIDNAME;
        
    // 释放内存
    delete[] buffer;
    
    return 0;    // 成功返回
}

// ============================================================================
// 新增：磁盘空间管理实现
// ============================================================================

void PF_Manager::SetDatabaseName(const std::string& dbName) {
    if (databaseName != dbName) {
        // 保存当前数据库的磁盘使用情况
        if (diskSpaceLimit > 0) {
            SaveDiskUsageMetadata();
        }
        
        // 切换到新数据库
        databaseName = dbName;
        
        // 静默加载新数据库的磁盘使用情况
        LoadDiskUsageMetadata();
    }
}

size_t PF_Manager::SetDiskSpaceLimit(size_t diskKB) {
    // 计算每个页面占用的磁盘空间（包括页面头部）
    size_t bytesPerPage = PF_PAGE_SIZE + sizeof(PF_PageHeader);
    
    // 转换 KB 到字节，然后计算可以容纳多少页面
    size_t diskBytes = diskKB * 1024;
    size_t newDiskSpaceLimit = diskBytes / bytesPerPage;
    
    // 检查是否已经有保存的磁盘限制
    if (diskSpaceLimit > 0 && diskSpaceLimit != newDiskSpaceLimit) {
        printf("警告: 数据库 '%s' 已有磁盘空间限制 %zu 页面\n", 
               databaseName.c_str(), diskSpaceLimit);
        printf("  当前设置: %zu KB (%zu 页面)\n", diskKB, newDiskSpaceLimit);
        printf("  已保存设置: %.1f KB (%zu 页面)\n", 
               (diskSpaceLimit * bytesPerPage) / 1024.0, diskSpaceLimit);
        
        printf("  自动覆盖旧设置\n");
        // 注释掉交互式输入，自动覆盖
        // char response;
        // printf("是否覆盖已保存的设置? (y/N): ");
        // scanf(" %c", &response);
        // if (response != 'y' && response != 'Y') {
        //     printf("保持原有磁盘空间限制: %zu 页面\n", diskSpaceLimit);
        //     return diskSpaceLimit;
        // }
    }
    
    diskSpaceLimit = newDiskSpaceLimit;
    
    // 只加载已使用页数，不覆盖刚设置的diskSpaceLimit
    if (usedDiskPages == 0) {
        // 保存当前设置的diskSpaceLimit
        size_t savedLimit = diskSpaceLimit;
        LoadDiskUsageMetadata();
        diskSpaceLimit = savedLimit;  // 恢复刚设置的值
    }
    
    printf("磁盘空间限制已设置 [数据库: %s]:\n", databaseName.c_str());
    printf("  设置空间: %zu KB (%zu 字节)\n", diskKB, diskBytes);
    printf("  每页空间: %zu 字节 (数据:%zu + 头部:%zu)\n", 
           bytesPerPage, (size_t)PF_PAGE_SIZE, sizeof(PF_PageHeader));
    printf("  可分配页面数: %zu\n", diskSpaceLimit);
    printf("  当前已使用: %zu 页面\n", usedDiskPages);
    printf("  预计可用空间: %zu KB\n", ((diskSpaceLimit - usedDiskPages) * bytesPerPage) / 1024);
    printf("  磁盘空间管理: 已启用并保存\n");
    
    // 保存更新后的元数据（包括新的磁盘空间限制）
    SaveDiskUsageMetadata();
    
    return diskSpaceLimit;
}

bool PF_Manager::CanAllocateDiskPages(size_t numPages) const {
    return (usedDiskPages + numPages) <= diskSpaceLimit;
}

bool PF_Manager::AllocateDiskPages(size_t numPages) {
    if (!CanAllocateDiskPages(numPages)) {
        printf("错误: 磁盘空间不足!\n");
        printf("  请求页面: %zu\n", numPages);
        printf("  已使用: %zu/%zu\n", usedDiskPages, diskSpaceLimit);
        printf("  剩余可用: %zu\n", GetAvailableDiskPages());
        return false;
    }
    
    usedDiskPages += numPages;
    printf("已分配 %zu 个磁盘页面 (总使用: %zu/%zu)\n", 
           numPages, usedDiskPages, diskSpaceLimit);
    
    // 立即保存到磁盘
    SaveDiskUsageMetadata();
    return true;
}

void PF_Manager::DeallocateDiskPages(size_t numPages) {
    if (numPages > usedDiskPages) {
        printf("警告: 尝试释放的页面数 (%zu) 超过已使用数 (%zu)\n", 
               numPages, usedDiskPages);
        usedDiskPages = 0;
    } else {
        usedDiskPages -= numPages;
    }
    printf("已释放 %zu 个磁盘页面 (总使用: %zu/%zu)\n", 
           numPages, usedDiskPages, diskSpaceLimit);
    
    // 立即保存到磁盘
    SaveDiskUsageMetadata();
}

void PF_Manager::GetDiskStats(size_t& used, size_t& total, float& usagePercent) const {
    used = usedDiskPages;
    total = diskSpaceLimit;
    usagePercent = total > 0 ? (float(used) / float(total) * 100.0f) : 0.0f;
}

void PF_Manager::PrintDiskUsage() const {
    size_t used, total;
    float usagePercent;
    GetDiskStats(used, total, usagePercent);
    
    size_t available = GetAvailableDiskPages();
    size_t usedKB = (used * (PF_PAGE_SIZE + sizeof(PF_PageHeader))) / 1024;
    size_t totalKB = (total * (PF_PAGE_SIZE + sizeof(PF_PageHeader))) / 1024;
    
    printf("\n========== 磁盘使用状况 [数据库: %s] ==========\n", databaseName.c_str());
    printf("已使用页面: %zu\n", used);
    printf("总页面数: %zu\n", total);
    printf("可用页面: %zu\n", available);
    printf("使用率: %.1f%%\n", usagePercent);
    printf("已使用空间: ~%zu KB\n", usedKB);
    printf("总分配空间: ~%zu KB\n", totalKB);
    printf("===============================================\n\n");
}

// ============================================================================
// 新增：磁盘使用情况持久化存储（包含磁盘空间限制）
// ============================================================================

void PF_Manager::LoadDiskUsageMetadata() {
    std::string metadataFile = GetMetadataFileName();
    int fd = open(metadataFile.c_str(), O_RDONLY);
    
    if (fd < 0) {
        // 文件不存在，使用默认值（静默）
        usedDiskPages = 0;
        diskSpaceLimit = 0;
        return;
    }
    
    struct DiskUsageMetadata {
        size_t diskSpaceLimit;
        size_t usedDiskPages;
        time_t lastUpdate;
        char databaseName[256];
        size_t originalDiskKB;
    } metadata;
    
    ssize_t bytesRead = read(fd, &metadata, sizeof(metadata));
    close(fd);
    
    if (bytesRead == sizeof(metadata)) {
        if (strcmp(metadata.databaseName, databaseName.c_str()) == 0) {
            // 静默恢复配置
            diskSpaceLimit = metadata.diskSpaceLimit;
            usedDiskPages = metadata.usedDiskPages;
        } else {
            usedDiskPages = 0;
            diskSpaceLimit = 0;
        }
    } else if (bytesRead > 0) {
        // 旧版本兼容（静默）
        struct OldDiskUsageMetadata {
            size_t diskSpaceLimit;
            size_t usedDiskPages;
            time_t lastUpdate;
            char databaseName[256];
        } oldMetadata;
        
        fd = open(metadataFile.c_str(), O_RDONLY);
        if (fd >= 0 && read(fd, &oldMetadata, sizeof(oldMetadata)) == sizeof(oldMetadata)) {
            if (strcmp(oldMetadata.databaseName, databaseName.c_str()) == 0) {
                diskSpaceLimit = oldMetadata.diskSpaceLimit;
                usedDiskPages = oldMetadata.usedDiskPages;
            }
        }
        if (fd >= 0) close(fd);
    } else {
        usedDiskPages = 0;
        diskSpaceLimit = 0;
    }
}

// 描述： 保存当前的磁盘使用情况到元数据文件
void PF_Manager::SaveDiskUsageMetadata() {
    std::string metadataFile = GetMetadataFileName();
    
    struct DiskUsageMetadata {
        size_t diskSpaceLimit;
        size_t usedDiskPages;
        time_t lastUpdate;
        char databaseName[256];  // 数据库名称验证
        size_t originalDiskKB;   // 新增：保存原始的磁盘空间设置（KB）
    } metadata;
    
    metadata.diskSpaceLimit = diskSpaceLimit;
    metadata.usedDiskPages = usedDiskPages;
    metadata.lastUpdate = time(nullptr);
    strncpy(metadata.databaseName, databaseName.c_str(), sizeof(metadata.databaseName) - 1);
    metadata.databaseName[sizeof(metadata.databaseName) - 1] = '\0';
    
    // 计算并保存原始的KB设置
    size_t bytesPerPage = PF_PAGE_SIZE + sizeof(PF_PageHeader);
    metadata.originalDiskKB = (diskSpaceLimit * bytesPerPage) / 1024;
    
    int fd = open(metadataFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    
    if (fd < 0) {
        printf("警告: 无法保存数据库 '%s' 的磁盘使用元数据\n", databaseName.c_str());
        return;
    }
    
    ssize_t bytesWritten = write(fd, &metadata, sizeof(metadata));
    close(fd);
    
    if (bytesWritten != sizeof(metadata)) {
        printf("警告: 数据库 '%s' 磁盘使用元数据保存不完整\n", databaseName.c_str());
    }
}

void PF_Manager::ResetDiskUsage() {
    usedDiskPages = 0;
    SaveDiskUsageMetadata();
    printf("数据库 '%s' 磁盘使用计数已重置\n", databaseName.c_str());
}


// ============================================================================
// 辅助函数：获取元数据文件名
// ============================================================================

std::string PF_Manager::GetMetadataFileName() const {
    // 元数据文件存储在当前目录，以数据库名命名
    return databaseName + ".pf_metadata";
}
