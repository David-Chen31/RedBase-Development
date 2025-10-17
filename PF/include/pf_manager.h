#ifndef PF_MANAGER_H
#define PF_MANAGER_H

#include "pf.h"
#include "pf_filehandle.h"
#include <cstddef>
#include <iostream>
#include <string.h>

/**
 * @brief PF_Manager 分页文件管理器
 * 
 * 增强功能：
 * - 磁盘空间管理和限制
 * - 动态分配磁盘块
 * - 磁盘使用统计
 */
class PF_Manager
{
  public:
    PF_Manager();                              // Constructor
    ~PF_Manager();                             // Destructor
    
    // 基本文件操作
    RC CreateFile(const char *fileName);       // Create a new file
    RC DestroyFile(const char *fileName);      // Destroy a file
    RC OpenFile(const char *fileName, PF_FileHandle &fileHandle);  
                                               // Open a file
    RC CloseFile(PF_FileHandle &fileHandle);   // Close a file
    RC AllocateBlock(char *&buffer);           // Allocate a new scratch page in buffer
    RC DisposeBlock(char *buffer);             // Dispose of a scratch page
    
    // ====================================================================== 
    // 新增：磁盘空间管理功能
    // ======================================================================
    
    /**
     * @brief 设置磁盘空间限制
     * @param diskKB 可用磁盘空间（KB）
     * @return 可用的磁盘页面数
     */
    size_t SetDiskSpaceLimit(size_t diskKB);
    
    /**
     * @brief 检查是否可以分配指定数量的磁盘页面
     */
    bool CanAllocateDiskPages(size_t numPages) const;
    
    /**
     * @brief 分配磁盘页面（记录使用量）
     */
    bool AllocateDiskPages(size_t numPages);
    
    /**
     * @brief 释放磁盘页面
     */
    void DeallocateDiskPages(size_t numPages);
    
    /**
     * @brief 获取磁盘空间统计
     */
    void GetDiskStats(size_t& used, size_t& total, float& usagePercent) const;
    
    /**
     * @brief 打印磁盘使用状况
     */
    void PrintDiskUsage() const;
    
    /**
     * @brief 获取当前磁盘空间限制
     */
    size_t GetDiskSpaceLimit() const { return diskSpaceLimit; }
    
    /**
     * @brief 获取已使用的磁盘页面数
     */
    size_t GetUsedDiskPages() const { return usedDiskPages; }
    
    /**
     * @brief 获取可用的磁盘页面数
     */
    size_t GetAvailableDiskPages() const { 
        return diskSpaceLimit > usedDiskPages ? diskSpaceLimit - usedDiskPages : 0; 
    }

  private:
    // 磁盘空间管理
    size_t diskSpaceLimit;      // 磁盘空间限制（页面数）
    size_t usedDiskPages;       // 已使用的磁盘页面数
    std::string databaseName;   // 当前数据库名称
    
    // 磁盘使用情况持久化存储（按数据库分别存储）
    void LoadDiskUsageMetadata();   // 从文件加载磁盘使用情况
    void SaveDiskUsageMetadata();   // 保存磁盘使用情况到文件
    std::string GetMetadataFileName() const;  // 获取元数据文件名

  public:
    // 设置当前数据库名称（用于区分不同数据库的磁盘使用）
    void SetDatabaseName(const std::string& dbName);
    
    // 获取当前数据库名称
    std::string GetDatabaseName() const { return databaseName; }
    
    // 重置磁盘使用计数（用于调试或管理）
    void ResetDiskUsage();
    
    /**
     * @brief 检查当前数据库是否已配置磁盘空间限制
     */
    bool IsDiskSpaceLimitConfigured() const { return diskSpaceLimit > 0; }
    
    /**
     * @brief 获取当前数据库的磁盘空间配置信息
     */
    void GetDiskSpaceConfig(size_t& limitKB, size_t& limitPages, bool& isConfigured) const {
        isConfigured = (diskSpaceLimit > 0);
        limitPages = diskSpaceLimit;
        size_t bytesPerPage = PF_PAGE_SIZE + sizeof(PF_PageHeader);
        limitKB = (diskSpaceLimit * bytesPerPage) / 1024;
    }
};

#endif
