#ifndef RM_RID_H
#define RM_RID_H

#include "redbase.h"

//
// RID: 记录标识符类
// 用于唯一标识文件中的一条记录（页号 + 槽号）
//
class RID {
public:
    RID();                                         // 默认构造函数
    RID(PageNum pageNum, SlotNum slotNum);         // 带参数构造函数
    ~RID();                                        // 析构函数
    
    RC GetPageNum(PageNum &pageNum) const;         // 获取页号
    RC GetSlotNum(SlotNum &slotNum) const;         // 获取槽号

    // 比较操作符（用于排序和比较）
    bool operator==(const RID &other) const;
    bool operator!=(const RID &other) const;
    bool operator<(const RID &other) const;

private:
    PageNum pageNum;                               // 页号
    SlotNum slotNum;                               // 槽号
    bool bValidRID;                                // RID是否有效
};

// RID相关的返回码
#define RM_INVALIDRID_PAGENUM  (START_RM_WARN + 10)  // 无效页号
#define RM_INVALIDRID_SLOTNUM  (START_RM_WARN + 11)  // 无效槽号

#endif // RM_RID_H