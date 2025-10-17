#ifndef REDBASE_H
#define REDBASE_H

#include "../../PF/include/pf.h"

//
// RM模块专有类型定义
//
typedef int SlotNum;               // 槽号类型

//
// 全局常量
//
#define OK                    0    // 成功返回码
#define MAXSTRINGLEN        255    // 最大字符串长度

//
// 属性类型枚举
//
enum AttrType {
    INT,                           // 整数类型（4字节）
    FLOAT,                         // 浮点类型（4字节）
    STRING                         // 字符串类型（1-255字节）
};

//
// 比较操作枚举
//
enum CompOp {
    NO_OP,                         // 无比较操作
    EQ_OP,                         // 等于 =
    LT_OP,                         // 小于 <
    GT_OP,                         // 大于 >
    LE_OP,                         // 小于等于 <=
    GE_OP,                         // 大于等于 >=
    NE_OP                          // 不等于 <>
};

//
// 客户端提示枚举（用于页面固定策略）
//
enum ClientHint {
    NO_HINT                        // 无提示
};

//
// 返回码范围定义
//

// RM 组件返回码范围
#define START_RM_WARN            100
#define END_RM_WARN              199
#define START_RM_ERR            -100
#define END_RM_ERR              -199

#endif // REDBASE_H