#ifndef PF_BUFFER_MANAGER_H
#define PF_BUFFER_MANAGER_H

#include <cstddef>
#include <vector>
#include <list>
#include "pf.h"
#include "hash_table.h"

/**
 * @file buffer_manager.h
 * @brief 管理 PF 模块的缓冲池（Buffer Pool）
 *
 * BufferManager 负责内存与磁盘之间的数据交换，
 * 采用 LRU 算法实现页面替换，并与 HashTable 配合，
 * 提供高效的页面定位功能。
 * 
 * 增强功能：支持根据用户输入的主存大小动态分配缓冲区
 */

class BufferManager {
public:
    /**
     * @brief 获取实例（单例模式）
     * @param poolSize 缓冲池大小（最多缓存多少页）
     * @return BufferManager& 实例引用
     */
    static BufferManager& Instance(size_t poolSize = PF_BUFFER_SIZE);

    /**
     * @brief 根据可用内存动态设置缓冲池大小
     * @param memoryKB 可用主存空间（KB）
     * @return 实际设置的缓冲池页面数
     */
    size_t SetBufferSizeFromMemory(size_t memoryKB);

    /**
     * @brief 获取当前缓冲池统计信息
     */
    void GetBufferStats(size_t& totalFrames, size_t& usedFrames, size_t& memoryUsageKB) const;

    /**
     * @brief 打印缓冲池状态
     */
    void PrintBufferStatus() const;

    /**
     * @brief 重新初始化缓冲池（用于动态调整大小）
     */
    RC ReinitializeBuffer(size_t newPoolSize);

    /**
     * @brief 析构函数
     *        析构前会强制将所有脏页写回磁盘
     */
    ~BufferManager();

    // 禁止拷贝构造和赋值
    BufferManager(const BufferManager&) = delete;
    BufferManager& operator=(const BufferManager&) = delete;

    // ======================================================================
    // 关键操作接口
    // ======================================================================

    /**
     * @brief 获取一个页面（可能命中缓冲池，也可能从磁盘加载）
     * @param fileDesc  文件描述符
     * @param pageNum   页号
     * @param pageData  返回指向页面数据的指针
     * @return RC 错误码
     */
    RC FetchPage(int fileDesc, PageNum pageNum, char **pageData);

    /**
     * @brief 固定页面（增加 pinCount），防止被替换
     */
    RC PinPage(int fileDesc, PageNum pageNum);

    /**
     * @brief 释放页面（减少 pinCount），pinCount 为 0 时可被替换
     */
    RC UnpinPage(int fileDesc, PageNum pageNum);

    /**
     * @brief 标记页面已修改，替换前需写回磁盘
     */
    RC MarkDirty(int fileDesc, PageNum pageNum);

    /**
     * @brief 将所有脏页写回磁盘
     */
    RC FlushAllPages(int fileDesc);
    RC ClearFilePages(int fileDesc);  // 清空指定文件的所有缓冲区页面

    /**
     * @brief 获取当前缓冲池大小
     */
    size_t GetPoolSize() const { return poolSize; }

private:
    // ======================================================================
    // 内部结构
    // ======================================================================

    struct Frame {
        int fileDesc;       // 文件描述符
        PageNum pageNum;    // 页号
        bool dirty;         // 是否被修改过
        int pinCount;       // 是否被固定，固定则不可替换
        char *data;         // 指向页面数据的内存
    };

    size_t poolSize;                         // 缓冲池大小
    std::vector<Frame> frames;               // 所有 Frame
    std::list<int> lruList;                  // LRU 队列，存 frame 索引
    HashTable pageTable;                      // Hash 映射：(fileDesc,pageNum)->frame

    /**
     * @brief 选择一个 frame 替换
     * @param victim 返回被替换的 frame 索引
     * @return RC 错误码
     */
    RC SelectVictimFrame(int &victim);

    /**
     * @brief 将 frame 写回磁盘
     */
    RC WriteFrameToDisk(int frameID);

    /**
     * @brief 将页面从磁盘读取到 frame
     */
    RC ReadPageFromDisk(int fileDesc, PageNum pageNum, int frameID);

    /**
     * @brief 构造函数
     * @param poolSize 缓冲池大小（最多缓存多少页）
     */
    explicit BufferManager(size_t poolSize = PF_BUFFER_SIZE);

    /**
     * @brief 清理所有frames
     */
    void CleanupFrames();

    /**
     * @brief 初始化frames
     */
    void InitializeFrames();
};

#endif // PF_BUFFER_MANAGER_H
