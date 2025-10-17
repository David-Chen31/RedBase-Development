#ifndef IX_INTERNAL_H
#define IX_INTERNAL_H

#include "../include/ix.h"
#include "../../PF/include/pf.h"
#include "../../PF/include/pf_manager.h"
#include "../../PF/include/pf_filehandle.h"
#include "../../PF/include/pf_pagehandle.h"

//
// 布尔常量定义（如果没有在其他地方定义）
//
#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

//
// 内部常量定义
//
#define IX_FILE_HDR_SIZE       sizeof(IX_FileHdr)       // 文件头大小
#define IX_NODE_HDR_SIZE       sizeof(IX_NodeHdr)       // 节点头大小
#define IX_NO_PAGE             -1                       // 无效页号
#define IX_ROOT_PAGE           1                        // 根节点页号（页0是文件头）

// B+树相关常量
#define IX_MIN_DEGREE          2                        // 最小度数
#define IX_MAX_KEYS(keySize)   ((PF_PAGE_SIZE - IX_NODE_HDR_SIZE) / (keySize + sizeof(PageNum))) - 1
#define IX_MAX_RIDS_PER_BUCKET ((PF_PAGE_SIZE - sizeof(int)) / sizeof(RID))

//
// 索引文件头结构
// 存储在文件的第一页（页号0）
//
struct IX_FileHdr {
    AttrType attrType;              // 属性类型
    int attrLength;                 // 属性长度  
    PageNum rootPage;               // 根节点页号
    int numPages;                   // 文件中的总页数
    PageNum firstFreePage;          // 第一个空闲页号
};

//
// B+树节点头结构
// 存储在每个B+树节点页面的开始位置
//
struct IX_NodeHdr {
    bool isLeaf;                    // 是否为叶节点
    int numKeys;                    // 当前键的数量
    PageNum parent;                 // 父节点页号
    PageNum left;                   // 左兄弟节点页号
    PageNum right;                  // 右兄弟节点页号
    // 键值和指针数据紧跟在节点头后面
};

//
// Bucket页头结构（用于存储相同键值的多个RID）
//
struct IX_BucketHdr {
    int numRIDs;                    // 当前RID数量
    PageNum nextBucket;             // 下一个bucket页号（链表）
    // RID数据紧跟在bucket头后面
};

//
// 错误处理函数声明
//
void IX_PrintError(RC rc);
const char* IX_GetErrorString(RC rc);

#endif // IX_INTERNAL_H