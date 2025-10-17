#ifndef SM_H
#define SM_H

#include "../../PF/include/pf.h"
#include "../../RM/include/redbase.h"
#include "../../RM/include/rm.h"
#include "../../IX/include/ix.h"
#include <iostream>
#include <fstream>

// Forward declarations
class PF_Manager;
class RM_Manager;
class IX_Manager;

// 常量定义
#define MAXNAME         24      // 最大关系名/属性名长度
#define MAXATTRS        40      // 最大属性数量
#define MAXSTRINGLEN    255     // 最大字符串长度

// 属性信息结构 (用于创建表)
struct AttrInfo {
    char     *attrName;         // 属性名
    AttrType attrType;          // 属性类型
    int      attrLength;        // 属性长度
};

// 数据属性信息结构 (用于打印)
struct DataAttrInfo {
    char     relName[MAXNAME+1];    // 关系名
    char     attrName[MAXNAME+1];   // 属性名
    int      offset;                // 属性偏移量
    AttrType attrType;              // 属性类型
    int      attrLength;            // 属性长度
    int      indexNo;               // 索引号 (-1表示无索引)
};

//
// SM_Manager: 系统管理器类
//
class SM_Manager {
public:
    SM_Manager(IX_Manager &ixm, RM_Manager &rmm);      // 构造函数
    ~SM_Manager();                                      // 析构函数
    
    RC OpenDb(const char *dbName);                      // 打开数据库
    RC CloseDb();                                       // 关闭数据库
    
    // DDL 命令
    RC CreateTable(const char *relName,                 // 创建表
                   int attrCount,
                   AttrInfo *attributes);
    RC DropTable(const char *relName);                  // 删除表
    RC CreateIndex(const char *relName,                 // 创建索引
                   const char *attrName);
    RC DropIndex(const char *relName,                   // 删除索引
                 const char *attrName);
    
    // 系统工具命令
    RC Load(const char *relName,                        // 加载数据
            const char *fileName);
    RC Help();                                          // 显示所有关系
    RC Help(const char *relName);                       // 显示关系信息
    RC Print(const char *relName);                      // 打印关系内容
    RC Set(const char *paramName,                       // 设置系统参数
           const char *value);
    
    // 系统目录初始化（公共方法，用于dbcreate）
    RC SetupRelcat();                                   // 设置relcat表
    RC SetupAttrcat();                                  // 设置attrcat表


    // 查询接口（供QL层使用）
    RC GetRelInfo(const char *relName, DataAttrInfo *&attributes, int &attrCount);
    RC GetAttrInfo(const char *relName, const char *attrName, DataAttrInfo &attr);

private:
    // 私有成员变量
    IX_Manager *ixManager;                              // IX管理器指针

    // 查询接口（供QL层使用）
    RM_Manager *rmManager;                              // RM管理器指针
    
    // 系统目录文件句柄
    RM_FileHandle relcatFH;                             // relcat文件句柄
    RM_FileHandle attrcatFH;                            // attrcat文件句柄
    
    bool bDbOpen;                                       // 数据库是否打开
    char dbName[MAXNAME+1];                            // 当前数据库名
    
    // 私有辅助方法
    RC InsertIntoRelcat(const char *relName, int tupleLength, 
                       int attrCount, int indexCount);
    RC InsertIntoAttrcat(const char *relName, const char *attrName,
                        int offset, AttrType attrType, int attrLength, int indexNo);
    RC DeleteFromRelcat(const char *relName);
    RC DeleteFromAttrcat(const char *relName);
    RC UpdateAttrIndexNo(const char *relName, const char *attrName, int indexNo);
    
    // 辅助计算方法
    int CalculateOffset(AttrInfo *attributes, int attrNum);
    int CalculateTupleLength(AttrInfo *attributes, int attrCount);
    
    // 文件处理
    RC ParseLoadFile(const char *fileName, const char *relName);
};

// Printer类 - 用于格式化输出
class Printer {
public:
    Printer(const DataAttrInfo *attributes, const int attrCount);
    ~Printer();
    
    void PrintHeader(std::ostream &c) const;
    void Print(std::ostream &c, const char * const data);
    void Print(std::ostream &c, const void * const data[]);
    void PrintFooter(std::ostream &c) const;
    

    // 查询接口（供QL层使用）
    RC GetRelInfo(const char *relName, DataAttrInfo *&attributes, int &attrCount);
    RC GetAttrInfo(const char *relName, const char *attrName, DataAttrInfo &attr);

private:
    DataAttrInfo *attributes;       // 属性信息数组
    int attrCount;                  // 属性数量
    int tupleCount;                 // 已打印的元组数量
    int *printLengths;              // 每列的打印宽度
    
    void SetupPrintLengths();       // 设置打印宽度
};

// 错误处理函数
void SM_PrintError(RC rc);

// SM 组件返回码
#define START_SM_WARN           200
#define END_SM_WARN             299
#define START_SM_ERR           -200
#define END_SM_ERR             -299

// 警告码
#define SM_DUPLICATEREL        (START_SM_WARN + 0)   // 重复的关系名
#define SM_DUPLICATEATTR       (START_SM_WARN + 1)   // 重复的属性名
#define SM_DUPLICATEINDEX      (START_SM_WARN + 2)   // 重复的索引
#define SM_RELNOTFOUND         (START_SM_WARN + 3)   // 关系未找到
#define SM_ATTRNOTFOUND        (START_SM_WARN + 4)   // 属性未找到
#define SM_INDEXNOTFOUND       (START_SM_WARN + 5)   // 索引未找到
#define SM_LASTWARN            SM_INDEXNOTFOUND

// 错误码
#define SM_BADRELNAME          (START_SM_ERR - 0)    // 无效关系名
#define SM_BADATTRNAME         (START_SM_ERR - 1)    // 无效属性名
#define SM_BADATTRTYPE         (START_SM_ERR - 2)    // 无效属性类型
#define SM_BADATTRLENGTH       (START_SM_ERR - 3)    // 无效属性长度
#define SM_TOOMANYATTRS        (START_SM_ERR - 4)    // 属性数量过多
#define SM_DBNOTOPEN           (START_SM_ERR - 5)    // 数据库未打开
#define SM_INVALIDDB           (START_SM_ERR - 6)    // 无效数据库
#define SM_SYSTEMCATALOG       (START_SM_ERR - 7)    // 不能操作系统目录
#define SM_BADFILENAME         (START_SM_ERR - 8)    // 无效文件名
#define SM_LASTERROR           SM_BADFILENAME

#endif // SM_H