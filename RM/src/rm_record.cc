#include "../include/rm.h"
#include "../internal/rm_internal.h"
#include <cstring>

//
// 构造函数
//
RM_Record::RM_Record() {
    pData = NULL;
    recordSize = 0;
    bValidRecord = false;
}

//
// 析构函数
//
RM_Record::~RM_Record() {
    if (pData != NULL) {
        delete[] pData;
        pData = NULL;
    }
}

//
// 获取记录数据
//
RC RM_Record::GetData(char *&pData) const {
    // 检查记录是否有效
    if (!bValidRecord || this->pData == NULL) {
        return RM_INVALIDRECORD;
    }
    
    // 返回数据指针
    pData = this->pData;
    return OK;
}

//
// 获取记录ID
//
RC RM_Record::GetRid(RID &rid) const {
    // 检查记录是否有效
    if (!bValidRecord) {
        return RM_INVALIDRECORD;
    }
    
    // 返回记录ID
    rid = this->rid;
    return OK;
}