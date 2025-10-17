#include "../include/sm.h"
#include "../internal/sm_internal.h"
#include "../../PF/include/pf.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

using namespace std;

//
// SM_Manager 构造函数
//
SM_Manager::SM_Manager(IX_Manager &ixm, RM_Manager &rmm) {
    ixManager = &ixm;
    rmManager = &rmm;
    bDbOpen = false;
    dbName[0] = '\0';
}

//
// SM_Manager 析构函数
//
SM_Manager::~SM_Manager() {
    if (bDbOpen) {
        CloseDb();
    }
}

//
// 打开数据库
//
RC SM_Manager::OpenDb(const char *dbName) {
    RC rc;
    
    // 检查参数
    if (dbName == NULL) {
        return SM_BADFILENAME;
    }
    
    // 检查数据库是否已打开
    if (bDbOpen) {
        return SM_INVALIDDB;
    }
    
    // 切换到数据库目录
    if (chdir(dbName) < 0) {
        return SM_INVALIDDB;
    }
    
    // 打开系统目录文件
    if ((rc = rmManager->OpenFile(RELCAT_RELNAME, relcatFH))) {
        return rc;
    }
    
    if ((rc = rmManager->OpenFile(ATTRCAT_RELNAME, attrcatFH))) {
        rmManager->CloseFile(relcatFH);
        return rc;
    }
    
    // 记录状态
    strcpy(this->dbName, dbName);
    bDbOpen = true;
    
    return OK;
}

//
// 关闭数据库
//
RC SM_Manager::CloseDb() {
    RC rc = OK, tmp;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    // 关闭系统目录文件
    if ((tmp = rmManager->CloseFile(relcatFH))) {
        rc = tmp;
    }
    
    if ((tmp = rmManager->CloseFile(attrcatFH))) {
        rc = tmp;
    }
    
    bDbOpen = false;
    dbName[0] = '\0';
    
    return rc;
}

//
// 创建表
//
// 流程：
// 1. 参数检查
// 2. 检查关系是否已存在
// 3. 检查属性名是否重复
// 4. 计算元组长度和偏移量
// 5. 创建关系文件
// 6. 插入relcat记录
// 7. 插入attrcat记录
// 8. 强制写入目录文件
// 
RC SM_Manager::CreateTable(const char *relName, int attrCount, AttrInfo *attributes) {
    RC rc;
    
    // 参数检查
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    if (IsSystemCatalog(relName)) {
        return SM_SYSTEMCATALOG;
    }
    
    if (attrCount <= 0 || attrCount > MAXATTRS) {
        return SM_TOOMANYATTRS;
    }
    
    if (attributes == NULL) {
        return SM_BADATTRNAME;
    }
    
    // 检查属性信息
    for (int i = 0; i < attrCount; i++) {
        if (!IsValidName(attributes[i].attrName)) {
            return SM_BADATTRNAME;
        }
        
        if (attributes[i].attrType != INT && 
            attributes[i].attrType != FLOAT && 
            attributes[i].attrType != STRING) {
            return SM_BADATTRTYPE;
        }
        
        if ((attributes[i].attrType == INT || attributes[i].attrType == FLOAT) && 
            attributes[i].attrLength != 4) {
            return SM_BADATTRLENGTH;
        }
        
        if (attributes[i].attrType == STRING && 
            (attributes[i].attrLength <= 0 || attributes[i].attrLength > MAXSTRINGLEN)) {
            return SM_BADATTRLENGTH;
        }
    }
    
    // 检查关系是否已存在
    RM_FileScan relScan;
    RM_Record rec;
    if ((rc = relScan.OpenScan(relcatFH, STRING, MAXNAME+1, RELCAT_RELNAME_OFFSET, 
                               EQ_OP, (void*)relName))) {
        return rc;
    }
    
    if ((rc = relScan.GetNextRec(rec)) != RM_EOF) {
        relScan.CloseScan();
        return (rc == OK) ? SM_DUPLICATEREL : rc;
    }
    
    if ((rc = relScan.CloseScan())) {
        return rc;
    }
    
    // 检查属性名是否重复
    for (int i = 0; i < attrCount; i++) {
        for (int j = i + 1; j < attrCount; j++) {
            if (strcmp(attributes[i].attrName, attributes[j].attrName) == 0) {
                return SM_DUPLICATEATTR;
            }
        }
    }
    
    // 计算元组长度和偏移量
    int tupleLength = CalculateTupleLength(attributes, attrCount);
    
    // 创建关系文件
    if ((rc = rmManager->CreateFile(relName, tupleLength))) {
        return rc;
    }
    
    // 插入relcat记录
    if ((rc = InsertIntoRelcat(relName, tupleLength, attrCount, 0))) {
        rmManager->DestroyFile(relName);
        return rc;
    }
    
    // 插入attrcat记录
    for (int i = 0; i < attrCount; i++) {
        int offset = CalculateOffset(attributes, i);
        if ((rc = InsertIntoAttrcat(relName, attributes[i].attrName, offset,
                                   attributes[i].attrType, attributes[i].attrLength, -1))) {
            // 回滚
            DeleteFromRelcat(relName);
            DeleteFromAttrcat(relName);
            rmManager->DestroyFile(relName);
            return rc;
        }
    }
    
    // 强制写入目录文件
    relcatFH.ForcePages();
    attrcatFH.ForcePages();
    
    return OK;
}

//
// 删除表
//
RC SM_Manager::DropTable(const char *relName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    if (IsSystemCatalog(relName)) {
        return SM_SYSTEMCATALOG;
    }
    
    // 检查关系是否存在
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = GetRelInfo(relName, attributes, attrCount))) {
        return rc;
    }
    
    // 删除所有索引
    for (int i = 0; i < attrCount; i++) {
        if (attributes[i].indexNo != -1) {
            ixManager->DestroyIndex(relName, attributes[i].indexNo);
        }
    }
    
    // 删除关系文件
    if ((rc = rmManager->DestroyFile(relName))) {
        delete[] attributes;
        return rc;
    }
    
    // 从目录中删除记录
    DeleteFromRelcat(relName);
    DeleteFromAttrcat(relName);
    
    // 强制写入目录文件
    relcatFH.ForcePages();
    attrcatFH.ForcePages();
    
    delete[] attributes;
    return OK;
}

//
// 创建索引
//
RC SM_Manager::CreateIndex(const char *relName, const char *attrName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    if (attrName == NULL || !IsValidName(attrName)) {
        return SM_BADATTRNAME;
    }
    
    if (IsSystemCatalog(relName)) {
        return SM_SYSTEMCATALOG;
    }
    
    // 获取属性信息
    DataAttrInfo attr;
    if ((rc = GetAttrInfo(relName, attrName, attr))) {
        return rc;
    }
    
    // 检查索引是否已存在
    if (attr.indexNo != -1) {
        return SM_DUPLICATEINDEX;
    }
    
    // 分配索引号（简单递增）
    int indexNo = 0;
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = GetRelInfo(relName, attributes, attrCount))) {
        return rc;
    }
    
    for (int i = 0; i < attrCount; i++) {
        if (attributes[i].indexNo >= indexNo) {
            indexNo = attributes[i].indexNo + 1;
        }
    }
    
    // 创建索引文件
    if ((rc = ixManager->CreateIndex(relName, indexNo, attr.attrType, attr.attrLength))) {
        delete[] attributes;
        return rc;
    }
    
    // 打开索引和关系文件
    IX_IndexHandle indexHandle;
    RM_FileHandle fileHandle;
    
    if ((rc = ixManager->OpenIndex(relName, indexNo, indexHandle))) {
        ixManager->DestroyIndex(relName, indexNo);
        delete[] attributes;
        return rc;
    }
    
    if ((rc = rmManager->OpenFile(relName, fileHandle))) {
        ixManager->CloseIndex(indexHandle);
        ixManager->DestroyIndex(relName, indexNo);
        delete[] attributes;
        return rc;
    }
    
    // 扫描关系，为现有记录建立索引
    RM_FileScan fileScan;
    if ((rc = fileScan.OpenScan(fileHandle, INT, sizeof(int), 0, NO_OP, NULL))) {
        rmManager->CloseFile(fileHandle);
        ixManager->CloseIndex(indexHandle);
        ixManager->DestroyIndex(relName, indexNo);
        delete[] attributes;
        return rc;
    }
    
    RM_Record record;
    while ((rc = fileScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        char *data;
        RID rid;
        record.GetData(data);
        record.GetRid(rid);
        
        // 插入索引条目
        void *keyValue = (void*)(data + attr.offset);
        if ((rc = indexHandle.InsertEntry(keyValue, rid))) {
            break;
        }
    }
    
    fileScan.CloseScan();
    rmManager->CloseFile(fileHandle);
    ixManager->CloseIndex(indexHandle);
    
    if (rc != RM_EOF && rc != OK) {
        ixManager->DestroyIndex(relName, indexNo);
        delete[] attributes;
        return rc;
    }
    
    // 更新目录中的索引信息
    if ((rc = UpdateAttrIndexNo(relName, attrName, indexNo))) {
        ixManager->DestroyIndex(relName, indexNo);
        delete[] attributes;
        return rc;
    }
    
    // 更新关系的索引计数
    RM_FileScan relScan;
    RM_Record relRec;
    if ((rc = relScan.OpenScan(relcatFH, STRING, MAXNAME+1, RELCAT_RELNAME_OFFSET,
                              EQ_OP, (void*)relName))) {
        delete[] attributes;
        return rc;
    }
    
    if ((rc = relScan.GetNextRec(relRec)) == OK) {
        char *relData;
        relRec.GetData(relData);
        RelcatRecord *relcatRec = (RelcatRecord*)relData;
        relcatRec->indexCount++;
        
        if ((rc = relcatFH.UpdateRec(relRec))) {
            relScan.CloseScan();
            delete[] attributes;
            return rc;
        }
    }
    
    relScan.CloseScan();
    
    // 强制写入
    relcatFH.ForcePages();
    attrcatFH.ForcePages();
    
    delete[] attributes;
    return OK;
}

//
// 删除索引
//
RC SM_Manager::DropIndex(const char *relName, const char *attrName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    if (attrName == NULL || !IsValidName(attrName)) {
        return SM_BADATTRNAME;
    }
    
    if (IsSystemCatalog(relName)) {
        return SM_SYSTEMCATALOG;
    }
    
    // 获取属性信息
    DataAttrInfo attr;
    if ((rc = GetAttrInfo(relName, attrName, attr))) {
        return rc;
    }
    
    // 检查索引是否存在
    if (attr.indexNo == -1) {
        return SM_INDEXNOTFOUND;
    }
    
    // 删除索引文件
    if ((rc = ixManager->DestroyIndex(relName, attr.indexNo))) {
        return rc;
    }
    
    // 更新目录中的索引信息
    if ((rc = UpdateAttrIndexNo(relName, attrName, -1))) {
        return rc;
    }
    
    // 更新关系的索引计数
    RM_FileScan relScan;
    RM_Record relRec;
    if ((rc = relScan.OpenScan(relcatFH, STRING, MAXNAME+1, RELCAT_RELNAME_OFFSET,
                              EQ_OP, (void*)relName))) {
        return rc;
    }
    
    if ((rc = relScan.GetNextRec(relRec)) == OK) {
        char *relData;
        relRec.GetData(relData);
        RelcatRecord *relcatRec = (RelcatRecord*)relData;
        if (relcatRec->indexCount > 0) {
            relcatRec->indexCount--;
        }
        
        if ((rc = relcatFH.UpdateRec(relRec))) {
            relScan.CloseScan();
            return rc;
        }
    }
    
    relScan.CloseScan();
    
    // 强制写入
    relcatFH.ForcePages();
    attrcatFH.ForcePages();
    
    return OK;
}

//
// 辅助方法实现
//

int SM_Manager::CalculateOffset(AttrInfo *attributes, int attrNum) {
    int offset = 0;
    for (int i = 0; i < attrNum; i++) {
        offset += attributes[i].attrLength;
    }
    return offset;
}

int SM_Manager::CalculateTupleLength(AttrInfo *attributes, int attrCount) {
    int length = 0;
    for (int i = 0; i < attrCount; i++) {
        length += attributes[i].attrLength;
    }
    return length;
}

RC SM_Manager::InsertIntoRelcat(const char *relName, int tupleLength, 
                               int attrCount, int indexCount) {
    RelcatRecord record;
    strcpy(record.relName, relName);
    record.tupleLength = tupleLength;
    record.attrCount = attrCount;
    record.indexCount = indexCount;
    
    RID rid;
    return relcatFH.InsertRec((char*)&record, rid);
}

RC SM_Manager::InsertIntoAttrcat(const char *relName, const char *attrName,
                                int offset, AttrType attrType, int attrLength, int indexNo) {
    AttrcatRecord record;
    strcpy(record.relName, relName);
    strcpy(record.attrName, attrName);
    record.offset = offset;
    record.attrType = attrType;
    record.attrLength = attrLength;
    record.indexNo = indexNo;
    
    RID rid;
    return attrcatFH.InsertRec((char*)&record, rid);
}