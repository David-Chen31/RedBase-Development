#include "../include/ql.h"
#include <iostream>
#include <cstring>

using namespace std;

//
// QL组件错误信息表
//
static char *QL_WarnMsg[] = {
    (char*)"invalid attribute",
    (char*)"invalid relation",
    (char*)"relation specified twice in from clause", 
    (char*)"ambiguous attribute name",
    (char*)"no such table",
    (char*)"no such attribute",
    (char*)"attribute specified twice in select clause",
    (char*)"invalid condition operator",
    (char*)"query plan not open",
    (char*)"query plan already open",
    (char*)"null pointer in plan node",
    (char*)"invalid attribute for relation",
    (char*)"attribute not found in any relation"
};

static char *QL_ErrorMsg[] = {
    (char*)"incompatible attribute types",
    (char*)"invalid number of values for insert",
    (char*)"invalid condition in where clause",
    (char*)"system catalog modification not allowed",
    (char*)"end of file"
};

//
// QL_PrintError
//
// 描述: 打印QL组件的错误消息到stderr
// 输入:   rc - 返回码，由QL组件的方法返回
// 输出:   错误消息
//
void QL_PrintError(RC rc)
{
    // 检查返回码是否为QL错误
    if (rc >= START_QL_WARN && rc <= QL_LASTWARN) {
        // 计算警告消息索引
        int index = rc - START_QL_WARN;
        cerr << "QL warning: " << QL_WarnMsg[index] << endl;
    }
    else if (rc <= START_QL_ERR && rc >= QL_LASTERROR) {
        // 计算错误消息索引（注意错误码是负数）
        int index = START_QL_ERR - rc;
        cerr << "QL error: " << QL_ErrorMsg[index] << endl;
    }
    else if (rc == 0) {
        cerr << "QL: no error" << endl;
    }
    else {
        cerr << "QL: unknown error code " << rc << endl;
    }
}

//
// QL_GetErrorString
//
// 描述: 获取错误消息字符串
// 输入:   rc - 返回码
// 返回:   错误消息字符串指针
//
char* QL_GetErrorString(RC rc)
{
    if (rc >= START_QL_WARN && rc <= QL_LASTWARN) {
        int index = rc - START_QL_WARN;
        return QL_WarnMsg[index];
    }
    else if (rc <= START_QL_ERR && rc >= QL_LASTERROR) {
        int index = START_QL_ERR - rc;
        return QL_ErrorMsg[index];
    }
    else if (rc == 0) {
        return (char*)"no error";
    }
    else {
        return (char*)"unknown error";
    }
}

//
// QL_ValidateCondition
//
// 描述: 验证查询条件的有效性
// 输入:   condition - 要验证的条件
// 返回:   RC - OK 如果条件有效，否则返回错误码
//
RC QL_ValidateCondition(const Condition &condition)
{
    // 检查左操作数属性
    if (!condition.lhsAttr.attrName) {
        return QL_INVALIDCONDITION;
    }
    
    // 检查操作符
    if (condition.op < EQ_OP || condition.op > NE_OP) {
        return QL_INVALIDOPERATOR;
    }
    
    // 检查右操作数
    if (condition.bRhsIsAttr) {
        // 右操作数是属性
        if (!condition.rhsAttr.attrName) {
            return QL_INVALIDCONDITION;
        }
    } else {
        // 右操作数是值
        if (!condition.rhsValue.data) {
            return QL_INVALIDCONDITION;
        }
        
        // 验证值的类型
        if (condition.rhsValue.type != INT && 
            condition.rhsValue.type != FLOAT && 
            condition.rhsValue.type != STRING) {
            return QL_INCOMPATIBLETYPES;
        }
    }
    
    return OK;
}

//
// QL_ValidateRelAttr
//
// 描述: 验证关系属性的有效性
// 输入:   relAttr - 要验证的关系属性
// 返回:   RC - OK 如果属性有效，否则返回错误码
//
RC QL_ValidateRelAttr(const RelAttr &relAttr)
{
    // 检查属性名
    if (!relAttr.attrName) {
        return QL_NOSUCHATTR;
    }
    
    // 属性名不能为空字符串
    if (strlen(relAttr.attrName) == 0) {
        return QL_NOSUCHATTR;
    }
    
    // 如果指定了关系名，检查其有效性
    if (relAttr.relName) {
        if (strlen(relAttr.relName) == 0) {
            return QL_NOSUCHTABLE;
        }
        
        // 关系名长度限制
        if (strlen(relAttr.relName) > MAXNAME) {
            return QL_NOSUCHTABLE;
        }
    }
    
    // 属性名长度限制
    if (strlen(relAttr.attrName) > MAXNAME) {
        return QL_NOSUCHATTR;
    }
    
    return OK;
}

//
// QL_ValidateValue
//
// 描述: 验证值的有效性
// 输入:   value - 要验证的值
// 返回:   RC - OK 如果值有效，否则返回错误码
//
RC QL_ValidateValue(const Value &value)
{
    // 检查数据指针
    if (!value.data) {
        return QL_INVALIDCONDITION;
    }
    
    // 根据类型验证值
    switch (value.type) {
        case INT:
            // 整数值验证
            break;
            
        case FLOAT:
            // 浮点数值验证
            break;
            
        case STRING:
            // 字符串值验证
            {
                char *str = (char*)value.data;
                int len = strlen(str);
                
                // 字符串长度限制
                if (len > MAXSTRINGLEN) {
                    return QL_INCOMPATIBLETYPES;
                }
            }
            break;
            
        default:
            return QL_INCOMPATIBLETYPES;
    }
    
    return OK;
}

//
// QL_CheckSystemCatalog
//
// 描述: 检查是否试图修改系统目录
// 输入:   relName - 关系名
// 返回:   RC - OK 如果不是系统目录，否则返回错误码
//
RC QL_CheckSystemCatalog(const char *relName)
{
    if (!relName) {
        return QL_NOSUCHTABLE;
    }
    
    // 检查是否为系统目录表
    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return QL_SYSTEMCATALOG;
    }
    
    return OK;
}

//
// QL_CompareAttrTypes
//
// 描述: 比较两个属性类型是否兼容
// 输入:   type1, type2 - 要比较的属性类型
// 返回:   true 如果类型兼容，否则false
//
bool QL_CompareAttrTypes(AttrType type1, AttrType type2)
{
    // 相同类型总是兼容的
    if (type1 == type2) {
        return true;
    }
    
    // 数值类型之间可能兼容
    if ((type1 == INT && type2 == FLOAT) || (type1 == FLOAT && type2 == INT)) {
        return true;
    }
    
    // 其他情况不兼容
    return false;
}

//
// QL_IsValidOperator
//
// 描述: 检查操作符是否有效
// 输入:   op - 要检查的操作符
// 返回:   true 如果操作符有效，否则false
//
bool QL_IsValidOperator(CompOp op)
{
    return (op >= EQ_OP && op <= NE_OP);
}

//
// QL_IsStringAttribute
//
// 描述: 检查属性是否为字符串类型
// 输入:   type - 属性类型
// 返回:   true 如果是字符串类型，否则false
//
bool QL_IsStringAttribute(AttrType type)
{
    return (type == STRING);
}

//
// QL_IsNumericAttribute
//
// 描述: 检查属性是否为数值类型
// 输入:   type - 属性类型
// 返回:   true 如果是数值类型，否则false
//
bool QL_IsNumericAttribute(AttrType type)
{
    return (type == INT || type == FLOAT);
}

//
// QL_ConvertCompOpToString
//
// 描述: 将比较操作符转换为字符串表示
// 输入:   op - 比较操作符
// 返回:   操作符的字符串表示
//
const char* QL_ConvertCompOpToString(CompOp op)
{
    switch (op) {
        case EQ_OP: return "=";
        case LT_OP: return "<";
        case GT_OP: return ">";
        case LE_OP: return "<=";
        case GE_OP: return ">=";
        case NE_OP: return "<>";
        default: return "unknown";
    }
}

//
// QL_ConvertAttrTypeToString
//
// 描述: 将属性类型转换为字符串表示
// 输入:   type - 属性类型
// 返回:   类型的字符串表示
//
const char* QL_ConvertAttrTypeToString(AttrType type)
{
    switch (type) {
        case INT: return "INT";
        case FLOAT: return "FLOAT";
        case STRING: return "STRING";
        default: return "unknown";
    }
}