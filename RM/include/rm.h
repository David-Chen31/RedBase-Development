#ifndef RM_H
#define RM_H

#include "redbase.h"
#include "rm_rid.h"

// Forward declarations
class RM_Record;
class RM_FileHandle;
class RM_FileScan;
class PF_Manager;
class PF_FileHandle;

//
// RM_Manager: 记录管理器类
// 处理记录文件的创建、删除、打开和关闭
//
class RM_Manager {
public:
    RM_Manager(PF_Manager &pfm);           // 构造函数
    ~RM_Manager();                         // 析构函数
    
    RC CreateFile(const char *fileName, int recordSize);  // 创建文件
    RC DestroyFile(const char *fileName);                 // 删除文件
    RC OpenFile(const char *fileName, RM_FileHandle &fileHandle);  // 打开文件
    RC CloseFile(RM_FileHandle &fileHandle);              // 关闭文件

private:
    PF_Manager *pfManager;                 // PF管理器指针
};

//
// RM_FileHandle: 记录文件句柄类
// 用于操作打开文件中的记录
//
class RM_FileHandle {
public:
    RM_FileHandle();                       // 构造函数
    ~RM_FileHandle();                      // 析构函数
    
    RC GetRec(const RID &rid, RM_Record &rec) const;      // 获取记录
    RC InsertRec(const char *pData, RID &rid);             // 插入记录
    RC DeleteRec(const RID &rid);                          // 删除记录
    RC UpdateRec(const RM_Record &rec);                    // 更新记录
    RC ForcePages(PageNum pageNum = ALL_PAGES) const;      // 强制写入页面

private:
    friend class RM_Manager;
    friend class RM_FileScan;
    
    PF_FileHandle *pfFileHandle;           // PF文件句柄
    int recordSize;                        // 记录大小
    int recordsPerPage;                    // 每页记录数
    PageNum numPages;                      // 总页数
    PageNum firstFree;                     // 第一个空闲页
    bool bFileOpen;                        // 文件是否打开
    bool bHdrChanged;                      // 头部是否被修改
};

//
// RM_FileScan: 文件扫描类
// 提供对记录文件的扫描功能
//
class RM_FileScan {
public:
    RM_FileScan();                         // 构造函数
    ~RM_FileScan();                        // 析构函数
    
    RC OpenScan(const RM_FileHandle &fileHandle,
                AttrType attrType,
                int attrLength, 
                int attrOffset,
                CompOp compOp,
                void *value,
                ClientHint pinHint = NO_HINT);             // 开始扫描
    RC GetNextRec(RM_Record &rec);                         // 获取下一条记录
    RC CloseScan();                                        // 结束扫描

private:
    PF_FileHandle *pfFileHandle;           // PF文件句柄
    AttrType attrType;                     // 属性类型
    int attrLength;                        // 属性长度
    int attrOffset;                        // 属性偏移
    CompOp compOp;                         // 比较操作
    void *value;                           // 比较值
    ClientHint pinHint;                    // 页面固定提示
    
    PageNum currentPage;                   // 当前页面
    SlotNum currentSlot;                   // 当前槽位
    bool bScanOpen;                        // 扫描是否开启
    int recordSize;                        // 记录大小
    int recordsPerPage;                    // 每页记录数
    PageNum numPages;                      // 总页数
};

//
// RM_Record: 记录类
// 表示从文件中读取的记录
//
class RM_Record {
public:
    RM_Record();                           // 构造函数
    ~RM_Record();                          // 析构函数
    
    RC GetData(char *&pData) const;        // 获取记录数据
    RC GetRid(RID &rid) const;             // 获取记录ID

private:
    friend class RM_FileHandle;
    friend class RM_FileScan;
    
    char *pData;                           // 记录数据
    RID rid;                               // 记录ID
    int recordSize;                        // 记录大小
    bool bValidRecord;                     // 记录是否有效
};

//
// 错误处理函数
//
void RM_PrintError(RC rc);

// RM组件返回码
#define RM_INVALIDRID      (START_RM_WARN + 0)  // 无效的RID
#define RM_RECORDNOTFOUND  (START_RM_WARN + 1)  // 记录未找到
#define RM_EOF             (START_RM_WARN + 2)  // 文件结束
#define RM_INVALIDRECORD   (START_RM_WARN + 3)  // 无效记录
#define RM_LASTWARN        RM_INVALIDRECORD

#define RM_RECORDSIZETOOBIG (START_RM_ERR - 0)  // 记录太大
#define RM_FILENOTOPEN      (START_RM_ERR - 1)  // 文件未打开
#define RM_SCANALREADYOPEN  (START_RM_ERR - 2)  // 扫描已经打开
#define RM_SCANNOTOPEN      (START_RM_ERR - 3)  // 扫描未打开
#define RM_INVALIDFILE      (START_RM_ERR - 4)  // 无效文件
#define RM_LASTERROR        RM_INVALIDFILE

#endif // RM_H