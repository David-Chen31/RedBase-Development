#ifndef PF_INTERNAL_H
#define PF_INTERNAL_H

#include "pf.h"

//
// 文件头数据结构
//
struct PF_FileHeader {
    int firstFree;     // 第一个空闲页的页号（如果没有则为 PF_PAGE_LIST_END）
    int numPages;      // 文件中的页面总数
};

//
// 页头数据结构
//
struct PF_PageHeader {
    int nextFree;      // 下一个空闲页的页号（如果没有则为 PF_PAGE_LIST_END）
};

// 特殊页号定义
#define PF_PAGE_LIST_END  -1   // 标记空闲页面链表的结束

#endif // PF_INTERNAL_H