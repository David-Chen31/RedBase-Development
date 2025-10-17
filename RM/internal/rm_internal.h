#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include "../include/rm.h"
#include "../include/redbase.h"
#include "../../PF/include/pf.h"
#include "../../PF/include/pf_manager.h"
#include "../../PF/include/pf_filehandle.h"
#include "../../PF/include/pf_pagehandle.h"

//
// 内部常量定义
//
#define RM_FILE_HDR_SIZE      sizeof(RM_FileHdr)      // 文件头大小
#define RM_PAGE_HDR_SIZE      sizeof(RM_PageHdr)      // 页头大小
#define RM_INVALID_PAGE       -1                      // 无效页号
#define RM_INVALID_SLOT       -1                      // 无效槽号

//
// 文件头结构
// 存储在文件的第一页（页号0）
//
struct RM_FileHdr {
    int recordSize;           // 记录大小（字节）
    int recordsPerPage;       // 每页最大记录数
    PageNum numPages;         // 文件中的总页数
    PageNum firstFree;        // 第一个有空闲空间的页号（-1表示没有）
};

//
// 页头结构
// 存储在每个数据页的开始位置
//
struct RM_PageHdr {
    int numRecords;           // 当前页面中的记录数
    PageNum nextFree;         // 下一个有空闲空间的页号（-1表示没有）
    // 注意：位图紧跟在页头后面
};

//
// 内部辅助函数声明
//

// 计算给定记录大小下每页能存储的记录数
int RM_CalcRecordsPerPage(int recordSize);

// 计算位图大小（字节数）
int RM_CalcBitmapSize(int recordsPerPage);

// 获取页面数据中的位图指针
char* RM_GetBitmap(char* pageData);

// 位图操作函数
void RM_SetBit(char* bitmap, int bitNum);      // 设置位
void RM_ClearBit(char* bitmap, int bitNum);    // 清除位
bool RM_TestBit(char* bitmap, int bitNum);     // 测试位

// 在位图中查找第一个空闲槽位
int RM_FindFreeSlot(char* bitmap, int recordsPerPage);

// 比较两个属性值
bool RM_CompareAttr(void* attr1, void* attr2, AttrType attrType, int attrLength, CompOp compOp);

// 获取记录在页面中的偏移量
int RM_GetRecordOffset(int slotNum, int recordSize);

#endif // RM_INTERNAL_H