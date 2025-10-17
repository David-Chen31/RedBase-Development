#include "../include/rm_rid.h"
#include "../include/redbase.h"

//
// 默认构造函数
//
RID::RID() {
    pageNum = -1;
    slotNum = -1;
    bValidRID = false;
}

//
// 带参数构造函数
//
RID::RID(PageNum pageNum, SlotNum slotNum) {
    this->pageNum = pageNum;
    this->slotNum = slotNum;
    
    // 检查参数有效性
    if (pageNum >= 0 && slotNum >= 0) {
        bValidRID = true;
    } else {
        bValidRID = false;
    }
}

//
// 析构函数
//
RID::~RID() {
    // 无需特殊清理
}

//
// 获取页号
//
RC RID::GetPageNum(PageNum &pageNum) const {
    if (!bValidRID) {
        return RM_INVALIDRID_PAGENUM;
    }
    
    pageNum = this->pageNum;
    return OK;
}

//
// 获取槽号
//
RC RID::GetSlotNum(SlotNum &slotNum) const {
    if (!bValidRID) {
        return RM_INVALIDRID_SLOTNUM;
    }
    
    slotNum = this->slotNum;
    return OK;
}

//
// 相等比较操作符
//
bool RID::operator==(const RID &other) const {
    if (!bValidRID || !other.bValidRID) {
        return false;
    }
    
    return (pageNum == other.pageNum && slotNum == other.slotNum);
}

//
// 不等比较操作符
//
bool RID::operator!=(const RID &other) const {
    return !(*this == other);
}

//
// 小于比较操作符（用于排序）
//
bool RID::operator<(const RID &other) const {
    if (!bValidRID || !other.bValidRID) {
        return false;
    }
    
    if (pageNum < other.pageNum) {
        return true;
    } else if (pageNum == other.pageNum) {
        return slotNum < other.slotNum;
    } else {
        return false;
    }
}