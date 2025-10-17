#include "../internal/sm_internal.h"
#include <cstring>
#include <cstdlib>
#include <cctype>

//
// 检查名称是否有效
// 有效名称的规则：
// 1. 非空且长度不超过 MAXNAME
// 2. 以字母开头
// 3. 其余字符只能是字母、数字或下划线
//
bool IsValidName(const char *name) {
    if (name == NULL) {
        return false;
    }
    
    int len = strlen(name);
    if (len == 0 || len > MAXNAME) {
        return false;
    }
    
    // 必须以字母开头
    if (!isalpha(name[0])) {
        return false;
    }
    
    // 检查其余字符
    for (int i = 1; i < len; i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            return false;
        }
    }
    
    return true;
}

//
// 检查是否为系统目录表
//
bool IsSystemCatalog(const char *relName) {
    return (strcmp(relName, RELCAT_RELNAME) == 0 || 
            strcmp(relName, ATTRCAT_RELNAME) == 0);
}

//
// 属性类型转换为字符串
//
const char *AttrTypeToString(AttrType type) {
    switch (type) {
        case INT:    return "int";
        case FLOAT:  return "float";  
        case STRING: return "string";
        default:     return "unknown";
    }
}

//
// 解析值字符串
//
RC ParseValue(const char *valueStr, AttrType type, int length, void *value) {
    if (valueStr == NULL || value == NULL) {
        return SM_BADFILENAME;
    }
    
    switch (type) {
        case INT: {
            int intValue = atoi(valueStr);
            memcpy(value, &intValue, sizeof(int));
            break;
        }
        
        case FLOAT: {
            float floatValue = (float)atof(valueStr);
            memcpy(value, &floatValue, sizeof(float));
            break;
        }
        
        case STRING: {
            // 清零字符串缓冲区
            memset(value, 0, length);
            
            // 复制字符串，截断过长的部分
            int copyLen = strlen(valueStr);
            if (copyLen > length) {
                copyLen = length;
            }
            
            memcpy(value, valueStr, copyLen);
            break;
        }
        
        default:
            return SM_BADATTRTYPE;
    }
    
    return OK;
}