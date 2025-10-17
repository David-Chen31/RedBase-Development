#include "../include/sm.h"
#include "../internal/sm_internal.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>

using namespace std;

//
// Printer构造函数
//
Printer::Printer(const DataAttrInfo *attributes, const int attrCount) {
    this->attrCount = attrCount;
    this->tupleCount = 0;
    
    // 分配并复制属性信息
    this->attributes = new DataAttrInfo[attrCount];
    for (int i = 0; i < attrCount; i++) {
        this->attributes[i] = attributes[i];
    }
    
    // 设置打印宽度
    SetupPrintLengths();
}

//
// Printer析构函数
//
Printer::~Printer() {
    delete[] attributes;
    delete[] printLengths;
}

//
// 设置打印宽度
//
void Printer::SetupPrintLengths() {
    printLengths = new int[attrCount];
    
    for (int i = 0; i < attrCount; i++) {
        // 计算属性名长度
        int nameLen = strlen(attributes[i].attrName);
        
        // 根据属性类型设置最小宽度
        int minWidth;
        switch (attributes[i].attrType) {
            case INT:
                minWidth = 12;  // 足够显示整数
                break;
            case FLOAT:
                minWidth = 12;  // 足够显示浮点数
                break;
            case STRING:
                minWidth = (attributes[i].attrLength < 30) ? attributes[i].attrLength : 30;  // 待改进
                break;
            default:
                minWidth = 10;
        }
        
        // 取最大值
        printLengths[i] = (nameLen > minWidth) ? nameLen : minWidth;
    }
}

//
// 打印表头
//
void Printer::PrintHeader(ostream &c) const {
    // 打印属性名
    c << endl;
    for (int i = 0; i < attrCount; i++) {
        c << left << setw(printLengths[i]) << attributes[i].attrName;
        if (i < attrCount - 1) {
            c << " | ";
        }
    }
    c << endl;
    
    // 打印分隔线
    for (int i = 0; i < attrCount; i++) {
        for (int j = 0; j < printLengths[i]; j++) {
            c << "-";
        }
        if (i < attrCount - 1) {
            c << "-+-";
        }
    }
    c << endl;
}

//
// 打印数据行
//
void Printer::Print(ostream &c, const char * const data) {
    for (int i = 0; i < attrCount; i++) {
        const char *valuePtr = data + attributes[i].offset;
        
        switch (attributes[i].attrType) {
            case INT: {
                int value;
                memcpy(&value, valuePtr, sizeof(int));
                c << left << setw(printLengths[i]) << value;
                break;
            }
            
            case FLOAT: {
                float value;
                memcpy(&value, valuePtr, sizeof(float));
                c << left << setw(printLengths[i]) << fixed << setprecision(2) << value;
                break;
            }
            
            case STRING: {
                // 创建以null结尾的字符串
                char *strValue = new char[attributes[i].attrLength + 1];
                memcpy(strValue, valuePtr, attributes[i].attrLength);
                strValue[attributes[i].attrLength] = '\0';
                
                c << left << setw(printLengths[i]) << strValue;
                delete[] strValue;
                break;
            }
        }
        
        if (i < attrCount - 1) {
            c << " | ";
        }
    }
    c << endl;
    
    tupleCount++;
}

//
// 打印数据行（从指针数组）
//
void Printer::Print(ostream &c, const void * const data[]) {
    for (int i = 0; i < attrCount; i++) {
        const void *valuePtr = data[i];
        
        switch (attributes[i].attrType) {
            case INT: {
                int value = *((int*)valuePtr);
                c << left << setw(printLengths[i]) << value;
                break;
            }
            
            case FLOAT: {
                float value = *((float*)valuePtr);
                c << left << setw(printLengths[i]) << fixed << setprecision(2) << value;
                break;
            }
            
            case STRING: {
                const char *strValue = (const char*)valuePtr;
                c << left << setw(printLengths[i]) << strValue;
                break;
            }
        }
        
        if (i < attrCount - 1) {
            c << " | ";
        }
    }
    c << endl;
    
    tupleCount++;
}

//
// 打印表尾
//
void Printer::PrintFooter(ostream &c) const {
    c << "\n" << tupleCount << " tuple(s) selected." << endl;
}