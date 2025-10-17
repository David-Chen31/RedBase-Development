#include "hash_table.h"
#include <algorithm>

/**
 * @brief 构造函数
 * @param size 哈希表桶数（默认97是一个质数）
 */
HashTable::HashTable(size_t size) : tableSize(size) {
    buckets.resize(tableSize);
}

/**
 * @brief 插入映射记录
 * @param fileDesc 文件描述符
 * @param pageNum  页号
 * @param frameID  对应缓冲池的 frame 索引
 * @return RC 错误码
 */
RC HashTable::Insert(int fileDesc, PageNum pageNum, int frameID) {
    size_t bucket = Hash(fileDesc, pageNum);
    
    // 检查是否已经存在该页面
    for (const auto& entry : buckets[bucket]) {
        if (entry.fileDesc == fileDesc && entry.pageNum == pageNum) {
            return PF_HASHPAGEEXIST;  // 页面已经存在
        }
    }
    
    // 插入新的映射记录
    Entry newEntry = {fileDesc, pageNum, frameID};
    buckets[bucket].push_back(newEntry);
    
    return 0;  // 成功
}

/**
 * @brief 查找记录
 * @param fileDesc 文件描述符
 * @param pageNum  页号
 * @param frameID  返回 frame 索引
 * @return RC 错误码
 */
RC HashTable::Find(int fileDesc, PageNum pageNum, int &frameID) const {
    size_t bucket = Hash(fileDesc, pageNum);
    
    // 在对应的桶中查找
    for (const auto& entry : buckets[bucket]) {
        if (entry.fileDesc == fileDesc && entry.pageNum == pageNum) {
            frameID = entry.frameID;
            return 0;  // 找到了
        }
    }
    
    return PF_HASHNOTFOUND;  // 未找到
}

/**
 * @brief 删除映射记录
 */
RC HashTable::Remove(int fileDesc, PageNum pageNum) {
    size_t bucket = Hash(fileDesc, pageNum);
    
    // 查找并删除对应的条目
    auto& bucketList = buckets[bucket];
    auto it = std::find_if(bucketList.begin(), bucketList.end(),
        [fileDesc, pageNum](const Entry& entry) {
            return entry.fileDesc == fileDesc && entry.pageNum == pageNum;
        });
    
    if (it != bucketList.end()) {
        bucketList.erase(it);
        return 0;  // 成功删除
    }
    
    return PF_HASHNOTFOUND;  // 未找到要删除的记录
}

/**
 * @brief 计算哈希值
 */
size_t HashTable::Hash(int fileDesc, PageNum pageNum) const {
    // 使用简单的哈希函数：(fileDesc * 大质数 + pageNum) % tableSize
    // 这里使用1009作为大质数，避免冲突
    return ((static_cast<size_t>(fileDesc) * 1009) + static_cast<size_t>(pageNum)) % tableSize;
}
