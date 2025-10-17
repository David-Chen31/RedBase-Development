#include "../internal/rm_internal.h"
#include <cstring>
#include <cstdlib>

//
// 计算给定记录大小下每页能存储的记录数
//
int RM_CalcRecordsPerPage(int recordSize) {
    // 可用空间 = 页面大小 - 页头大小
    int availableSpace = PF_PAGE_SIZE - RM_PAGE_HDR_SIZE;
    
    // 需要考虑位图的空间占用
    // 设每页有n条记录，则位图需要 ceil(n/8) 字节
    // 可用空间 = n * recordSize + ceil(n/8)
    // 解这个不等式得到最大的n
    
    int maxRecords = 0;
    while (true) {
        int bitmapSize = (maxRecords + 7) / 8;  // ceil(maxRecords/8)
        int totalNeeded = maxRecords * recordSize + bitmapSize;
        
        if (totalNeeded > availableSpace) {
            break;
        }
        maxRecords++;
    }
    
    return maxRecords - 1;  // 减去超出的那一条
}

//
// 计算位图大小（字节数）
//
int RM_CalcBitmapSize(int recordsPerPage) {
    return (recordsPerPage + 7) / 8;  // ceil(recordsPerPage / 8)
}

//
// 获取页面数据中的位图指针
//
char* RM_GetBitmap(char* pageData) {
    return pageData + RM_PAGE_HDR_SIZE;
}

//
// 设置位图中的指定位
//
void RM_SetBit(char* bitmap, int bitNum) {
    int byteIndex = bitNum / 8;
    int bitIndex = bitNum % 8;
    bitmap[byteIndex] |= (1 << bitIndex);
}

//
// 清除位图中的指定位
//
void RM_ClearBit(char* bitmap, int bitNum) {
    int byteIndex = bitNum / 8;
    int bitIndex = bitNum % 8;
    bitmap[byteIndex] &= ~(1 << bitIndex);
}

//
// 测试位图中的指定位
//
bool RM_TestBit(char* bitmap, int bitNum) {
    int byteIndex = bitNum / 8;
    int bitIndex = bitNum % 8;
    return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
}

//
// 在位图中查找第一个空闲槽位
//
int RM_FindFreeSlot(char* bitmap, int recordsPerPage) {
    for (int i = 0; i < recordsPerPage; i++) {
        if (!RM_TestBit(bitmap, i)) {
            return i;
        }
    }
    return -1;  // 没有找到空闲槽位
}

//
// 比较两个属性值
//
bool RM_CompareAttr(void* attr1, void* attr2, AttrType attrType, int attrLength, CompOp compOp) {
    int intVal1, intVal2;
    float floatVal1, floatVal2;
    int cmpResult;
    
    switch (attrType) {
        case INT:
            intVal1 = *(int*)attr1;
            intVal2 = *(int*)attr2;
            
            switch (compOp) {
                case EQ_OP: return intVal1 == intVal2;
                case LT_OP: return intVal1 < intVal2;
                case GT_OP: return intVal1 > intVal2;
                case LE_OP: return intVal1 <= intVal2;
                case GE_OP: return intVal1 >= intVal2;
                case NE_OP: return intVal1 != intVal2;
                case NO_OP: return true;
                default: return false;
            }
            
        case FLOAT:
            floatVal1 = *(float*)attr1;
            floatVal2 = *(float*)attr2;
            
            switch (compOp) {
                case EQ_OP: return floatVal1 == floatVal2;
                case LT_OP: return floatVal1 < floatVal2;
                case GT_OP: return floatVal1 > floatVal2;
                case LE_OP: return floatVal1 <= floatVal2;
                case GE_OP: return floatVal1 >= floatVal2;
                case NE_OP: return floatVal1 != floatVal2;
                case NO_OP: return true;
                default: return false;
            }
            
        case STRING:
            cmpResult = strncmp((char*)attr1, (char*)attr2, attrLength);
            
            switch (compOp) {
                case EQ_OP: return cmpResult == 0;
                case LT_OP: return cmpResult < 0;
                case GT_OP: return cmpResult > 0;
                case LE_OP: return cmpResult <= 0;
                case GE_OP: return cmpResult >= 0;
                case NE_OP: return cmpResult != 0;
                case NO_OP: return true;
                default: return false;
            }
            
        default:
            return false;
    }
}

//
// 获取记录在页面中的偏移量
//
int RM_GetRecordOffset(int slotNum, int recordSize) {
    return slotNum * recordSize;
}