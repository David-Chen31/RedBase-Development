#ifndef SM_INTERNAL_H
#define SM_INTERNAL_H

#include "../include/sm.h"

// 系统目录表名
#define RELCAT_RELNAME    "relcat"
#define ATTRCAT_RELNAME   "attrcat"

// 使用packed属性确保结构体没有填充
#pragma pack(push, 1)

// relcat 表的结构
struct RelcatRecord {
    char relName[MAXNAME+1];        // 关系名
    int tupleLength;                // 元组长度
    int attrCount;                  // 属性数量
    int indexCount;                 // 索引数量
};

// attrcat 表的结构  
struct AttrcatRecord {
    char relName[MAXNAME+1];        // 关系名
    char attrName[MAXNAME+1];       // 属性名
    int offset;                     // 偏移量
    AttrType attrType;              // 属性类型
    int attrLength;                 // 属性长度
    int indexNo;                    // 索引号 (-1表示无索引)
};

#pragma pack(pop)

// relcat 表的属性偏移量
#define RELCAT_RELNAME_OFFSET    0
#define RELCAT_TUPLELENGTH_OFFSET (RELCAT_RELNAME_OFFSET + MAXNAME + 1)
#define RELCAT_ATTRCOUNT_OFFSET   (RELCAT_TUPLELENGTH_OFFSET + sizeof(int))
#define RELCAT_INDEXCOUNT_OFFSET  (RELCAT_ATTRCOUNT_OFFSET + sizeof(int))
#define RELCAT_RECORD_SIZE       (RELCAT_INDEXCOUNT_OFFSET + sizeof(int))

// attrcat 表的属性偏移量
#define ATTRCAT_RELNAME_OFFSET   0
#define ATTRCAT_ATTRNAME_OFFSET  (ATTRCAT_RELNAME_OFFSET + MAXNAME + 1)
#define ATTRCAT_OFFSET_OFFSET    (ATTRCAT_ATTRNAME_OFFSET + MAXNAME + 1)
#define ATTRCAT_ATTRTYPE_OFFSET  (ATTRCAT_OFFSET_OFFSET + sizeof(int))
#define ATTRCAT_ATTRLENGTH_OFFSET (ATTRCAT_ATTRTYPE_OFFSET + sizeof(AttrType))
#define ATTRCAT_INDEXNO_OFFSET   (ATTRCAT_ATTRLENGTH_OFFSET + sizeof(int))
#define ATTRCAT_RECORD_SIZE      (ATTRCAT_INDEXNO_OFFSET + sizeof(int))

// 工具函数声明
bool IsValidName(const char *name);
bool IsSystemCatalog(const char *relName);
const char *AttrTypeToString(AttrType type);
RC ParseValue(const char *valueStr, AttrType type, int length, void *value);

#endif // SM_INTERNAL_H
