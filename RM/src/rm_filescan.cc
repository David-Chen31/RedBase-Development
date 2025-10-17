#include "../include/rm.h"
#include "../internal/rm_internal.h"
#include <cstring>

//
// 构造函数
//
RM_FileScan::RM_FileScan() {
    pfFileHandle = NULL;
    bScanOpen = false;
    currentPage = 0;
    currentSlot = 0;
    value = NULL;
    recordSize = 0;
    recordsPerPage = 0;
    numPages = 0;
    pinHint = NO_HINT;
}

//
// 析构函数
//
RM_FileScan::~RM_FileScan() {
    if (bScanOpen) {
        // 如果扫描仍在进行中，应该关闭它
        CloseScan();
    }
}

//
// 开始扫描
//
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle,
                         AttrType attrType,
                         int attrLength, 
                         int attrOffset,
                         CompOp compOp,
                         void *value,
                         ClientHint pinHint) {
    
    // 检查文件句柄是否打开
    if (!fileHandle.bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 检查扫描是否已经打开
    if (bScanOpen) {
        return RM_SCANALREADYOPEN;
    }
    
    // 参数检查
    if (attrLength <= 0 || attrOffset < 0) {
        return RM_INVALIDRECORD;
    }
    
    // 初始化扫描参数
    this->pfFileHandle = fileHandle.pfFileHandle;
    this->attrType = attrType;
    this->attrLength = attrLength;
    this->attrOffset = attrOffset;
    this->compOp = compOp;
    this->value = value;
    this->pinHint = pinHint;
    
    // 从文件句柄复制信息
    this->recordSize = fileHandle.recordSize;
    this->recordsPerPage = fileHandle.recordsPerPage;
    this->numPages = fileHandle.numPages;
    
    // 初始化扫描位置
    this->currentPage = 1;  // 从第一个数据页开始（页0是文件头）
    this->currentSlot = 0;
    
    this->bScanOpen = true;
    return OK;
}

//
// 获取下一条匹配记录
//
RC RM_FileScan::GetNextRec(RM_Record &rec) {
    RC rc;
    
    // 检查扫描是否打开
    if (!bScanOpen) {
        return RM_SCANNOTOPEN;
    }
    
    // 扫描所有页面
    while (currentPage < numPages) {
        // 获取当前页面
        PF_PageHandle pageHandle;
        char* pageData;
        
        if ((rc = pfFileHandle->GetThisPage(currentPage, pageHandle)) ||
            (rc = pageHandle.GetData(pageData))) {
            return rc;
        }
        
        // 获取页头和位图
        RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;
        char* bitmap = RM_GetBitmap(pageData);
        
        // 扫描当前页面的所有槽位
        while (currentSlot < recordsPerPage) {
            // 检查槽位是否被使用
            if (RM_TestBit(bitmap, currentSlot)) {
                // 获取记录数据
                int recordOffset = RM_GetRecordOffset(currentSlot, recordSize);
                char* recordData = pageData + RM_PAGE_HDR_SIZE + 
                                   RM_CalcBitmapSize(recordsPerPage) + 
                                   recordOffset;
                
                // 检查条件匹配
                bool matches = true;
                if (value != NULL) {
                    char* attrData = recordData + attrOffset;
                    matches = RM_CompareAttr(attrData, value, attrType, attrLength, compOp);
                }
                
                if (matches) {
                    // 找到匹配记录，复制到结果中
                    if (rec.pData != NULL) {
                        delete[] rec.pData;
                    }
                    rec.pData = new char[recordSize];
                    memcpy(rec.pData, recordData, recordSize);
                    rec.rid = RID(currentPage, currentSlot);
                    rec.recordSize = recordSize;
                    rec.bValidRecord = true;
                    
                    // 移动到下一个位置
                    currentSlot++;
                    
                    // 解除页面固定
                    pfFileHandle->UnpinPage(currentPage);
                    
                    return OK;
                }
            }
            
            currentSlot++;
        }
        
        // 解除页面固定
        pfFileHandle->UnpinPage(currentPage);
        
        // 移动到下一页
        currentPage++;
        currentSlot = 0;
    }
    
    // 没有更多记录了
    return RM_EOF;
}

//
// 关闭扫描
//
RC RM_FileScan::CloseScan() {
    // 检查扫描是否打开
    if (!bScanOpen) {
        return RM_SCANNOTOPEN;
    }
    
    // 重置状态
    bScanOpen = false;
    pfFileHandle = NULL;
    currentPage = 0;
    currentSlot = 0;
    value = NULL;
    
    return OK;
}