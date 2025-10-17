#ifndef PF_HASH_TABLE_H
#define PF_HASH_TABLE_H

#include <vector>
#include <list>
#include "pf.h"

/**
 * @file hash_table.h
 * @brief PF 内部使用的哈希表，用于快速查找页面
 *
 * 该哈希表维护 fileID+pageNum → frameID 的映射关系，
 * 是 BufferManager 的核心辅助结构。
 */

class HashTable {
public:
    /**
     * @brief 构造函数
     * @param size 哈希表桶数（默认97是一个质数）
     */
    explicit HashTable(size_t size = 97);

    /**
     * @brief 插入映射记录
     * @param fileDesc 文件描述符
     * @param pageNum  页号
     * @param frameID  对应缓冲池的 frame 索引
     * @return RC 错误码
     */
    RC Insert(int fileDesc, PageNum pageNum, int frameID);

    /**
     * @brief 查找记录
     * @param fileDesc 文件描述符
     * @param pageNum  页号
     * @param frameID  返回 frame 索引
     * @return RC 错误码
     */
    RC Find(int fileDesc, PageNum pageNum, int &frameID) const;

    /**
     * @brief 删除映射记录
     */
    RC Remove(int fileDesc, PageNum pageNum);

private:
    struct Entry {
        int fileDesc;
        PageNum pageNum;
        int frameID;
    };

    size_t tableSize;                       // 哈希桶数量
    std::vector<std::list<Entry>> buckets;  // 每个桶是一个链表

    /**
     * @brief 计算哈希值
     */
    size_t Hash(int fileDesc, PageNum pageNum) const;
};

#endif // PF_HASH_TABLE_H
