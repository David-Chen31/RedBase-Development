#include "pf_statistics.h"

// 静态成员变量初始化
size_t PF_Statistics::diskReads = 0;
size_t PF_Statistics::diskWrites = 0;
size_t PF_Statistics::bufferHits = 0;
size_t PF_Statistics::bufferMisses = 0;
