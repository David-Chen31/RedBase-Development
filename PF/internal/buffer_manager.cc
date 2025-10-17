#include "buffer_manager.h"
#include "pf_internal.h"
#include "pf_statistics.h"
#include <unistd.h>
#include <algorithm>
#include <cstring>

/**
 * @brief 构造函数
 * @param poolSize 缓冲池大小（最多缓存多少页）
 */
BufferManager::BufferManager(size_t poolSize) 
    : poolSize(poolSize), frames(poolSize), pageTable(97) {
    
    // 初始化所有frame
    for (size_t i = 0; i < poolSize; ++i) {
        frames[i].fileDesc = -1;
        frames[i].pageNum = -1;
        frames[i].dirty = false;
        frames[i].pinCount = 0;
        frames[i].data = new char[sizeof(PF_PageHeader) + PF_PAGE_SIZE];
        
        // 将frame索引加入LRU列表
        lruList.push_back(static_cast<int>(i));
    }
}

/**
 * @brief 析构函数
 *        析构前会强制将所有脏页写回磁盘
 */
BufferManager::~BufferManager() {
    // 写回所有脏页
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].fileDesc != -1 && frames[i].dirty) {
            WriteFrameToDisk(static_cast<int>(i));
        }
        // 释放内存
        delete[] frames[i].data;
    }
}

/**
 * @brief 获取一个页面（可能命中缓冲池，也可能从磁盘加载到缓冲池再返回）
 * @param fileDesc  文件描述符
 * @param pageNum   页号
 * @param pageData  返回指向页面数据的指针
 * @return RC 错误码
 */
RC BufferManager::FetchPage(int fileDesc, PageNum pageNum, char **pageData) {
    int frameID;
    
    // 首先在哈希表中查找
    if (pageTable.Find(fileDesc, pageNum, frameID) == 0) {
        // 缓冲池命中
        PF_Statistics::AddHit();
        
        // 更新LRU：将该frame移到列表末尾
        auto it = std::find(lruList.begin(), lruList.end(), frameID);
        if (it != lruList.end()) {
            lruList.erase(it);
            lruList.push_back(frameID);
        }
        
        // 固定页面
        frames[frameID].pinCount++;
        
        // 返回数据指针
        *pageData = frames[frameID].data;
        return 0;
    }
    
    // 缓冲池未命中
    PF_Statistics::AddMiss();
    
    // 选择一个victim frame
    RC rc = SelectVictimFrame(frameID);
    if (rc != 0) {
        return rc;
    }
    
    // 如果victim frame是脏的，先写回磁盘
    if (frames[frameID].dirty) {
        rc = WriteFrameToDisk(frameID);
        if (rc != 0) {
            return rc;
        }
    }
    
    // 从哈希表中移除旧的映射
    if (frames[frameID].fileDesc != -1) {
        pageTable.Remove(frames[frameID].fileDesc, frames[frameID].pageNum);
    }
    
    // 从磁盘读取新页面
    rc = ReadPageFromDisk(fileDesc, pageNum, frameID);
    if (rc != 0) {
        return rc;
    }
    
    // 更新frame信息
    frames[frameID].fileDesc = fileDesc;
    frames[frameID].pageNum = pageNum;
    frames[frameID].dirty = false;
    frames[frameID].pinCount = 1;
    
    // 插入到哈希表
    rc = pageTable.Insert(fileDesc, pageNum, frameID);
    if (rc != 0) {
        return rc;
    }
    
    // 更新LRU：将该frame移到列表末尾
    auto it = std::find(lruList.begin(), lruList.end(), frameID);
    if (it != lruList.end()) {
        lruList.erase(it);
        lruList.push_back(frameID);
    }
    
    // 返回数据指针
    *pageData = frames[frameID].data;
    return 0;
}

/**
 * @brief 固定页面（增加 pinCount），防止被替换
 */
RC BufferManager::PinPage(int fileDesc, PageNum pageNum) {
    int frameID;
    
    // 在哈希表中查找
    RC rc = pageTable.Find(fileDesc, pageNum, frameID);
    if (rc != 0) {
        return PF_PAGENOTINBUF;  // 页面不在缓冲池中
    }
    
    // 增加pinCount
    frames[frameID].pinCount++;
    
    return 0;
}

/**
 * @brief 释放页面（减少 pinCount），pinCount 为 0 时可被替换
 */
RC BufferManager::UnpinPage(int fileDesc, PageNum pageNum) {
    int frameID;
    
    // 在哈希表中查找
    RC rc = pageTable.Find(fileDesc, pageNum, frameID);
    if (rc != 0) {
        return PF_PAGENOTINBUF;  // 页面不在缓冲池中
    }
    
    // 检查pinCount
    if (frames[frameID].pinCount <= 0) {
        return PF_PAGEUNPINNED;  // 页面已经unpinned
    }
    
    // 减少pinCount
    frames[frameID].pinCount--;
    
    return 0;
}

/**
 * @brief 标记页面已修改，替换前需写回磁盘
 */
RC BufferManager::MarkDirty(int fileDesc, PageNum pageNum) {
    int frameID;
    
    // 在哈希表中查找
    RC rc = pageTable.Find(fileDesc, pageNum, frameID);
    if (rc != 0) {
        return PF_PAGENOTINBUF;  // 页面不在缓冲池中
    }
    
    // 标记为脏
    frames[frameID].dirty = true;
    
    return 0;
}

/**
 * @brief 将所有脏页写回磁盘
 */
RC BufferManager::FlushAllPages(int fileDesc) {
    RC rc = 0;
    
    // 遍历所有frame，写回指定文件的脏页
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].fileDesc == fileDesc && frames[i].dirty) {
            RC writeRC = WriteFrameToDisk(static_cast<int>(i));
            if (writeRC != 0) {
                rc = writeRC;  // 记录错误，但继续处理其他页面
            } else {
                frames[i].dirty = false;  // 成功写回后清除dirty标记
            }
        }
    }
    
    return rc;
}

//
// ClearFilePages - 清空指定文件的所有缓冲区页面
//
// 描述: 将指定文件的所有页面从缓冲区中移除，用于文件关闭时确保缓冲区一致性
// 输入:  fileDesc - 文件描述符
// 返回: RC 错误码
//
RC BufferManager::ClearFilePages(int fileDesc) {
    RC rc = 0;
    
    // 先刷新该文件的所有脏页
    rc = FlushAllPages(fileDesc);
    if (rc != 0) {
        return rc;
    }
    
    // 遍历所有frame，清空指定文件的页面
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].fileDesc == fileDesc) {
            // 检查页面是否还被pin
            if (frames[i].pinCount > 0) {
                // 页面仍被使用，不能清空
                // 但这在正常情况下不应该发生（文件关闭前应该unpin所有页面）
                continue;
            }
            
            // 从哈希表中移除该页面的映射
            pageTable.Remove(frames[i].fileDesc, frames[i].pageNum);

            // 清空frame
            frames[i].fileDesc = -1;
            frames[i].pageNum = -1;
            frames[i].dirty = false;
            frames[i].pinCount = 0;
            // Frame清空完成，fileDesc=-1表示该frame可用
        }
    }
    
    return 0;
}

/**
 * @brief 选择一个 frame 替换
 * @param victim 返回被替换的 frame 索引
 * @return RC 错误码
 */
RC BufferManager::SelectVictimFrame(int &victim) {
    // 使用LRU算法选择victim
    for (auto it = lruList.begin(); it != lruList.end(); ++it) {
        int frameID = *it;
        
        // 如果frame没有被pin，则可以作为victim
        if (frames[frameID].pinCount == 0) {
            victim = frameID;
            return 0;
        }
    }
    
    // 所有页面都被pin住了，无法替换
    return PF_NOBUF;
}

/**
 * @brief 将 frame 写回磁盘
 */
RC BufferManager::WriteFrameToDisk(int frameID) {
    if (frameID < 0 || frameID >= static_cast<int>(poolSize)) {
        return PF_INVALIDPAGE;
    }
    
    Frame& frame = frames[frameID];
    if (frame.fileDesc < 0) {
        return PF_INVALIDPAGE;
    }
    
    // 计算页面在文件中的偏移位置
    long offset = static_cast<long>(frame.pageNum) * (sizeof(PF_PageHeader) + PF_PAGE_SIZE) 
                  + sizeof(PF_FileHeader);
    
    // 定位到文件位置
    if (lseek(frame.fileDesc, offset, SEEK_SET) < 0) {
        return PF_UNIX;
    }
    
    // 写入页面数据（包括页头和页面内容）
    ssize_t bytesWritten = write(frame.fileDesc, frame.data, 
                                sizeof(PF_PageHeader) + PF_PAGE_SIZE);
    if (bytesWritten != static_cast<ssize_t>(sizeof(PF_PageHeader) + PF_PAGE_SIZE)) {
        return PF_INCOMPLETEWRITE;
    }
    
    // 更新统计信息
    PF_Statistics::AddDiskWrite();
    
    return 0;
}

/**
 * @brief 将页面从磁盘读取到 frame
 */
RC BufferManager::ReadPageFromDisk(int fileDesc, PageNum pageNum, int frameID) {
    if (frameID < 0 || frameID >= static_cast<int>(poolSize)) {
        return PF_INVALIDPAGE;
    }
    
    // 计算页面在文件中的偏移位置
    long offset = static_cast<long>(pageNum) * (sizeof(PF_PageHeader) + PF_PAGE_SIZE) 
                  + sizeof(PF_FileHeader);
    
    // 定位到文件位置
    if (lseek(fileDesc, offset, SEEK_SET) < 0) {
        return PF_UNIX;
    }
    
    // 读取页面数据（包括页头和页面内容）
    ssize_t bytesRead = read(fileDesc, frames[frameID].data, 
                            sizeof(PF_PageHeader) + PF_PAGE_SIZE);
    if (bytesRead != static_cast<ssize_t>(sizeof(PF_PageHeader) + PF_PAGE_SIZE)) {
        // 如果是新页面（读取失败），则初始化为空页面
        if (bytesRead < 0) {
            return PF_UNIX;
        }
        
        // 初始化页面头和数据
        memset(frames[frameID].data, 0, sizeof(PF_PageHeader) + PF_PAGE_SIZE);
        PF_PageHeader* pageHeader = reinterpret_cast<PF_PageHeader*>(frames[frameID].data);
        pageHeader->nextFree = PF_PAGE_LIST_END;
    }
    
    // 更新统计信息
    PF_Statistics::AddDiskRead();
    
    return 0;
}

// ======================================================================
// 新增功能：动态缓冲区管理
// ======================================================================

/**
 * @brief 非线程安全的单例获取（修改为支持动态大小）
 */
BufferManager& BufferManager::Instance(size_t poolSize) {
    static BufferManager* instance = nullptr;
    static size_t currentPoolSize = 0;
    
    if (instance == nullptr || poolSize != currentPoolSize) {
        if (instance != nullptr) {
            delete instance;
        }
        instance = new BufferManager(poolSize);
        currentPoolSize = poolSize;
    }
    return *instance;
}

/**
 * @brief 根据可用内存动态设置缓冲池大小
 */
size_t BufferManager::SetBufferSizeFromMemory(size_t memoryKB) {
    // 计算每页实际需要的内存（页面数据 + 页面头 + Frame结构开销）
    size_t bytesPerPage = sizeof(PF_PageHeader) + PF_PAGE_SIZE + sizeof(Frame) + 64;
    size_t totalBytes = memoryKB * 1024;
    size_t newPoolSize = totalBytes / bytesPerPage;
    
    if (newPoolSize < 1) newPoolSize = 1;
    
    std::cout << "\n=== 动态配置主存缓冲区 ===" << std::endl;
    std::cout << "输入主存空间: " << memoryKB << " KB" << std::endl;
    std::cout << "每页内存开销: " << bytesPerPage << " 字节" << std::endl;
    std::cout << "当前缓冲池大小: " << poolSize << " 页" << std::endl;
    std::cout << "计算新大小: " << newPoolSize << " 页" << std::endl;
    
    if (newPoolSize != poolSize) {
        std::cout << "正在重新配置缓冲池..." << std::endl;
        RC rc = ReinitializeBuffer(newPoolSize);
        if (rc == 0) {
            std::cout << "✓ 缓冲池重新配置成功" << std::endl;
            std::cout << "✓ 新缓冲池大小: " << poolSize << " 页" << std::endl;
            std::cout << "✓ 实际内存使用: " << (poolSize * bytesPerPage / 1024) << " KB" << std::endl;
        } else {
            std::cout << "✗ 缓冲池重新配置失败" << std::endl;
        }
    } else {
        std::cout << "✓ 缓冲池大小无需调整" << std::endl;
    }
    std::cout << std::endl;
    
    return poolSize;
}

/**
 * @brief 获取当前缓冲池统计信息
 */
void BufferManager::GetBufferStats(size_t& totalFrames, size_t& usedFrames, size_t& memoryUsageKB) const {
    totalFrames = poolSize;
    usedFrames = 0;
    
    // 统计已使用的frames
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].fileDesc != -1) {
            usedFrames++;
        }
    }
    
    // 计算内存使用量
    size_t bytesPerPage = sizeof(PF_PageHeader) + PF_PAGE_SIZE + sizeof(Frame);
    memoryUsageKB = (totalFrames * bytesPerPage) / 1024;
}

/**
 * @brief 打印缓冲池状态
 */
void BufferManager::PrintBufferStatus() const {
    size_t totalFrames, usedFrames, memoryUsageKB;
    GetBufferStats(totalFrames, usedFrames, memoryUsageKB);
    
    std::cout << "\n================= 缓冲池状态 =================" << std::endl;
    std::cout << "总Frame数: " << totalFrames << std::endl;
    std::cout << "已使用Frame数: " << usedFrames << std::endl;
    std::cout << "空闲Frame数: " << (totalFrames - usedFrames) << std::endl;
    std::cout << "使用率: " << (totalFrames > 0 ? (usedFrames * 100.0 / totalFrames) : 0) << "%" << std::endl;
    std::cout << "内存使用: " << memoryUsageKB << " KB" << std::endl;
    
    // 简单的使用率可视化
    int barWidth = 40;
    float usagePercent = totalFrames > 0 ? (float)usedFrames * 100.0f / (float)totalFrames : 0.0f;
    int pos = (int)(usagePercent * barWidth / 100.0f);
    std::cout << "使用率可视化: [";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "█";
        else std::cout << "░";
    }
    std::cout << "] " << usagePercent << "%" << std::endl;
    std::cout << "===========================================" << std::endl;
}

/**
 * @brief 重新初始化缓冲池（用于动态调整大小）
 */
RC BufferManager::ReinitializeBuffer(size_t newPoolSize) {
    if (newPoolSize == poolSize) {
        return 0;  // 大小没有变化
    }
    
    // 1. 将所有脏页写回磁盘
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].fileDesc != -1 && frames[i].dirty) {
            RC rc = WriteFrameToDisk(static_cast<int>(i));
            if (rc != 0) {
                std::cerr << "写回脏页失败，frame " << i << std::endl;
                return rc;
            }
        }
    }
    
    // 2. 清理现有frames
    CleanupFrames();
    
    // 3. 重新设置大小并初始化
    poolSize = newPoolSize;
    frames.resize(poolSize);
    lruList.clear();
    pageTable = HashTable(97);  // 重新初始化哈希表
    
    // 4. 初始化新的frames
    InitializeFrames();
    
    return 0;
}

/**
 * @brief 清理所有frames
 */
void BufferManager::CleanupFrames() {
    for (size_t i = 0; i < poolSize; ++i) {
        if (frames[i].data) {
            delete[] frames[i].data;
            frames[i].data = nullptr;
        }
    }
}

/**
 * @brief 初始化frames
 */
void BufferManager::InitializeFrames() {
    for (size_t i = 0; i < poolSize; ++i) {
        frames[i].fileDesc = -1;
        frames[i].pageNum = -1;
        frames[i].dirty = false;
        frames[i].pinCount = 0;
        frames[i].data = new char[sizeof(PF_PageHeader) + PF_PAGE_SIZE];
        
        // 将frame索引加入LRU列表
        lruList.push_back(static_cast<int>(i));
    }
}
