#ifndef PF_STATISTICS_H
#define PF_STATISTICS_H

#include <cstddef>
#include <iostream>

/**
 * @file pf_statistics.h
 * @brief PF 统计模块
 *
 * 该模块用于记录缓冲池的使用情况，比如页面访问次数、
 * 缓冲池命中率、磁盘 I/O 统计等。
 */

class PF_Statistics {
public:
    // 增加一次磁盘读取
    static void AddDiskRead() { diskReads++; }

    // 增加一次磁盘写入
    static void AddDiskWrite() { diskWrites++; }

    // 增加一次缓冲池命中
    static void AddHit() { bufferHits++; }

    // 增加一次缓冲池未命中
    static void AddMiss() { bufferMisses++; }

    // 打印统计信息
    static void PrintStats(std::ostream &os = std::cout) {
        size_t totalAccesses = bufferHits + bufferMisses;
        double hitRate = (totalAccesses == 0) ? 0.0 :
                         (static_cast<double>(bufferHits) / totalAccesses) * 100.0;

        os << "==== PF Statistics ====" << std::endl;
        os << "Disk Reads     : " << diskReads << std::endl;
        os << "Disk Writes    : " << diskWrites << std::endl;
        os << "Buffer Hits    : " << bufferHits << std::endl;
        os << "Buffer Misses  : " << bufferMisses << std::endl;
        os << "Hit Rate       : " << hitRate << "%" << std::endl;
        os << "========================" << std::endl;
    }

    // 重置统计信息
    static void Reset() {
        diskReads = 0;
        diskWrites = 0;
        bufferHits = 0;
        bufferMisses = 0;
    }

private:
    static size_t diskReads;
    static size_t diskWrites;
    static size_t bufferHits;
    static size_t bufferMisses;
};

#endif // PF_STATISTICS_H
