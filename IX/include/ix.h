#ifndef IX_H
#define IX_H

#include "../../RM/include/redbase.h"
#include "../../RM/include/rm_rid.h"
#include "../../PF/include/pf.h"

// Forward declarations
class IX_IndexHandle;
class IX_IndexScan;
class PF_Manager;
class PF_FileHandle;
class PF_PageHandle;

//
// IX_Manager: 索引管理器类
// 处理索引的创建、删除、打开和关闭
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);                       // 构造函数
    ~IX_Manager();                                     // 析构函数
    
    RC CreateIndex(const char *fileName,               // 创建索引
                   int indexNo,
                   AttrType attrType,
                   int attrLength);
    RC DestroyIndex(const char *fileName,              // 删除索引
                    int indexNo);
    RC OpenIndex(const char *fileName,                 // 打开索引
                 int indexNo,
                 IX_IndexHandle &indexHandle);
    RC CloseIndex(IX_IndexHandle &indexHandle);        // 关闭索引

private:
    PF_Manager *pfManager;                             // PF管理器指针
};

//
// IX_IndexHandle: 索引句柄类
// 用于操作打开索引中的条目
//
class IX_IndexHandle {
public:
    IX_IndexHandle();                                  // 构造函数
    ~IX_IndexHandle();                                 // 析构函数
    
    RC InsertEntry(void *pData, const RID &rid);      // 插入索引条目
    RC DeleteEntry(void *pData, const RID &rid);      // 删除索引条目
    RC ForcePages();                                   // 强制写入页面

private:
    friend class IX_Manager;
    friend class IX_IndexScan;
    
    // 基本信息
    bool isOpenHandle;                                 // 索引句柄是否打开
    PF_FileHandle *pfh;                               // PF文件句柄指针
    
    // 索引文件头信息（内部结构定义在ix_internal.h中）
    struct {
        AttrType attrType;                            // 属性类型
        int attrLength;                               // 属性长度
        PageNum rootPage;                             // 根节点页号
        int numPages;                                 // 文件中的总页数
        PageNum firstFreePage;                        // 第一个空闲页号
    } indexHdr;
    
    // B+树操作的私有方法（声明）
    RC InsertIntoNode(PageNum pageNum, void *pData, const RID &rid,
                     bool &wasSplit, void *&newChildKey, PageNum &newChildPage);
    RC InsertIntoLeaf(PageNum currentPageNum, char *nodeData, void *pData, const RID &rid,
                     bool &wasSplit, void *&newChildKey, PageNum &newChildPage);
    RC InsertEntryIntoLeaf(char *nodeData, void *pData, const RID &rid);
    RC SplitLeafNode(PageNum currentPageNum, char *nodeData, void *pData, const RID &rid,
                    bool &wasSplit, void *&newChildKey, PageNum &newChildPage);
    RC DeleteFromNode(PageNum pageNum, void *pData, const RID &rid);
    RC DeleteFromLeaf(char *nodeData, void *pData, const RID &rid);
    RC FindChildPage(char *nodeData, void *pData, PageNum &childPage);
    RC CreateNewRoot(void *pData, PageNum leftPage, PageNum rightPage);
    RC WriteHeader();
    int CompareKeys(void *key1, void *key2);
    
    // 辅助方法
    int GetMaxLeafEntries();
    int GetMaxInternalEntries();
    int GetLeafEntrySize();
    int GetInternalEntrySize();
    
    // B+树内部操作方法（在ix_btree.cc中实现）
    RC InsertIntoInternal(char *nodeData, void *pData, PageNum newPage,
                         bool &wasSplit, void *&newChildKey, PageNum &newChildPage);
    RC InsertEntryIntoInternal(char *nodeData, void *pData, PageNum newPage);
    RC SplitInternalNode(char *nodeData, void *pData, PageNum newPage,
                        bool &wasSplit, void *&newChildKey, PageNum &newChildPage);
    RC TraverseTree(PageNum pageNum, int level);
    RC ValidateTree(PageNum pageNum, void *minKey, void *maxKey, int &height);
    RC PrintTree();
};

//
// IX_IndexScan: 索引扫描类  
// 提供对索引的条件扫描功能
//
class IX_IndexScan {
public:
    IX_IndexScan();                                    // 构造函数
    ~IX_IndexScan();                                   // 析构函数
    
    RC OpenScan(const IX_IndexHandle &indexHandle,     // 开始扫描
                CompOp compOp,
                void *value,
                ClientHint pinHint = NO_HINT);
    RC GetNextEntry(RID &rid);                         // 获取下一条条目
    RC CloseScan();                                    // 结束扫描

private:
    // 基本状态
    bool isOpenScan;                                   // 扫描是否打开
    bool scanEnded;                                    // 扫描是否结束
    bool hasValue;                                     // 是否有比较值
    bool pinned;                                       // 当前页面是否被pin
    
    // 扫描参数
    IX_IndexHandle *indexHandle;                       // 索引句柄指针
    CompOp compOp;                                     // 比较操作
    void *value;                                       // 比较值
    
    // 当前位置
    PageNum currentPageNum;                            // 当前页面号
    int currentSlot;                                   // 当前槽位
    PF_PageHandle *pfPageHandle;                       // 当前页面句柄
    
    // 扫描相关的私有方法
    RC FindFirstLeafPage();
    RC FindStartPosition();
    RC SearchKey(void *searchKey, PageNum &leafPage, int &slotNum);
    RC FindKeyInLeaf(char *nodeData, void *searchKey, int &slotNum);
    RC GetNextEntryInPage(RID &rid);
    RC MoveToNextPage();
    bool SatisfiesCondition(const RID &rid);
    RC FindChildPageForKey(char *nodeData, void *searchKey, PageNum &childPage);
    RC FindInsertPosition(void *searchKey, PageNum &leafPage, int &slotNum);
};

//
// 错误处理函数
//
void IX_PrintError(RC rc);

// IX组件返回码
#define IX_ENTRYNOTFOUND       (START_IX_WARN + 1)   // 条目未找到
#define IX_EOF                 (START_IX_WARN + 2)   // 扫描结束
#define IX_LASTWARN            IX_EOF

#define IX_INDEXNOTOPEN        (START_IX_ERR - 1)    // 索引未打开
#define IX_SCANNOTOPEN         (START_IX_ERR - 2)    // 扫描未打开
#define IX_SCANOPEN            (START_IX_ERR - 3)    // 扫描已经打开
#define IX_BUCKETFULL          (START_IX_ERR - 4)    // Bucket页已满
#define IX_NULLPOINTER         (START_IX_ERR - 5)    // 空指针参数
#define IX_INVALIDTREE         (START_IX_ERR - 6)    // B+树结构无效
#define IX_LASTERROR           IX_INVALIDTREE

// IX组件返回码范围定义（添加到redbase.h中）
#define START_IX_WARN          200
#define END_IX_WARN            299
#define START_IX_ERR           -200
#define END_IX_ERR             -299

#endif // IX_H