#include "../include/rm.h"
#include "../internal/rm_internal.h"
#include <cstring>

//
// 构造函数
//
RM_FileHandle::RM_FileHandle() {
    pfFileHandle = NULL;
    recordSize = 0;
    recordsPerPage = 0;
    numPages = 0;
    firstFree = RM_INVALID_PAGE;
    bFileOpen = false;
    bHdrChanged = false;
}

//
// 析构函数
//
RM_FileHandle::~RM_FileHandle() {
    // 如果文件仍然打开，这是一个错误，但我们不能在析构函数中返回错误码
    if (bFileOpen && pfFileHandle != NULL) {
        // 可以记录日志或输出警告
        delete pfFileHandle;
        pfFileHandle = NULL;
    }
}

//
// 获取记录
//
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const {
    RC rc;
    PageNum pageNum;
    SlotNum slotNum;
    
    // 检查文件是否打开
    if (!bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 获取RID的页号和槽号
    if ((rc = rid.GetPageNum(pageNum)) ||
        (rc = rid.GetSlotNum(slotNum))) {
        return rc;
    }
    
    // 检查页号是否有效
    if (pageNum < 1 || pageNum >= numPages) {
        return RM_INVALIDRID;
    }
    
    // 检查槽号是否有效
    if (slotNum < 0 || slotNum >= recordsPerPage) {
        return RM_INVALIDRID;
    }
    
    // 获取页面
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = pfFileHandle->GetThisPage(pageNum, pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        return rc;
    }
    
    // 获取页头和位图
    RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;
    char* bitmap = RM_GetBitmap(pageData);
    
    // 检查槽位是否被使用
    if (!RM_TestBit(bitmap, slotNum)) {
        pfFileHandle->UnpinPage(pageNum);
        return RM_RECORDNOTFOUND;
    }
    
    // 计算记录在页面中的位置
    int recordOffset = RM_GetRecordOffset(slotNum, recordSize);
    char* recordData = pageData + RM_PAGE_HDR_SIZE + 
                       RM_CalcBitmapSize(recordsPerPage) + 
                       recordOffset;
    
    // 复制记录数据到RM_Record对象
    if (rec.pData != NULL) {
        delete[] rec.pData;
    }
    rec.pData = new char[recordSize];
    memcpy(rec.pData, recordData, recordSize);
    rec.rid = rid;
    rec.recordSize = recordSize;
    rec.bValidRecord = true;
    
    // 解除页面固定
    if ((rc = pfFileHandle->UnpinPage(pageNum))) {
        return rc;
    }
    
    return OK;
}

//
// 插入记录
//
RC RM_FileHandle::InsertRec(const char *pData, RID &rid) {
    RC rc;
    
    // 检查文件是否打开
    if (!bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 检查数据指针
    if (pData == NULL) {
        return RM_INVALIDRECORD;
    }
    
    PageNum pageNum;
    PF_PageHandle pageHandle;
    char* pageData;
    
    // 查找有空闲空间的页面
    if (firstFree != RM_INVALID_PAGE) {
        pageNum = firstFree;
        if ((rc = pfFileHandle->GetThisPage(pageNum, pageHandle)) ||
            (rc = pageHandle.GetData(pageData))) {
            return rc;
        }
    } else {
        // 没有空闲页面，创建新页面
        if ((rc = pfFileHandle->AllocatePage(pageHandle)) ||
            (rc = pageHandle.GetData(pageData)) ||
            (rc = pageHandle.GetPageNum(pageNum))) {
            return rc;
        }
        
        // 初始化新页面的页头
        RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;
        pageHdr->numRecords = 0;
        pageHdr->nextFree = RM_INVALID_PAGE;
        
        // 清空位图
        char* bitmap = RM_GetBitmap(pageData);
        memset(bitmap, 0, RM_CalcBitmapSize(recordsPerPage));
        
        // 更新文件信息
        numPages++;
        firstFree = pageNum;
        bHdrChanged = true;
    }
    
    // 获取页头和位图
    RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;
    char* bitmap = RM_GetBitmap(pageData);
    
    // 查找空闲槽位
    int slotNum = RM_FindFreeSlot(bitmap, recordsPerPage);
    if (slotNum == -1) {
        // 页面已满，从空闲链表中移除
        firstFree = pageHdr->nextFree;
        bHdrChanged = true;
        
        pfFileHandle->UnpinPage(pageNum);
        
        // 递归调用自己查找其他页面
        return InsertRec(pData, rid);
    }
    
    // 设置位图中对应位
    RM_SetBit(bitmap, slotNum);
    
    // 计算记录插入位置： 页头 + 位图 + 记录偏移
    int recordOffset = RM_GetRecordOffset(slotNum, recordSize);
    char* recordData = pageData + RM_PAGE_HDR_SIZE + 
                       RM_CalcBitmapSize(recordsPerPage) + 
                       recordOffset;
    
    // 复制记录数据
    memcpy(recordData, pData, recordSize);
    
    // 更新页头信息
    pageHdr->numRecords++;
    
    // 如果页面满了，从空闲链表中移除
    if (pageHdr->numRecords >= recordsPerPage) {
        firstFree = pageHdr->nextFree;
        pageHdr->nextFree = RM_INVALID_PAGE;
        bHdrChanged = true;
    }
    
    // 设置返回的RID
    rid = RID(pageNum, slotNum);
    
    // 标记页面为脏页并解除固定
    if ((rc = pfFileHandle->MarkDirty(pageNum)) ||
        (rc = pfFileHandle->UnpinPage(pageNum))) {
        return rc;
    }
    
    return OK;
}

//
// 删除记录
//
RC RM_FileHandle::DeleteRec(const RID &rid) {
    RC rc;
    PageNum pageNum;
    SlotNum slotNum;
    
    // 检查文件是否打开
    if (!bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 获取RID的页号和槽号
    if ((rc = rid.GetPageNum(pageNum)) ||
        (rc = rid.GetSlotNum(slotNum))) {
        return rc;
    }
    
    // 检查页号是否有效
    if (pageNum < 1 || pageNum >= numPages) {
        return RM_INVALIDRID;
    }
    
    // 检查槽号是否有效
    if (slotNum < 0 || slotNum >= recordsPerPage) {
        return RM_INVALIDRID;
    }
    
    // 获取页面
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = pfFileHandle->GetThisPage(pageNum, pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        return rc;
    }
    
    // 获取页头和位图
    RM_PageHdr* pageHdr = (RM_PageHdr*)pageData;
    char* bitmap = RM_GetBitmap(pageData);
    
    // 检查记录是否存在
    if (!RM_TestBit(bitmap, slotNum)) {
        pfFileHandle->UnpinPage(pageNum);
        return RM_RECORDNOTFOUND;
    }
    
    // 清除位图中对应位
    // 只有位图是1才读取数据，所以旧数据永远不会被误读
    RM_ClearBit(bitmap, slotNum);
    
    // 更新页头信息
    pageHdr->numRecords--;
    
    // 如果页面之前是满的，现在需要加入空闲链表
    if (pageHdr->numRecords == recordsPerPage - 1) {
        pageHdr->nextFree = firstFree;
        firstFree = pageNum;
        bHdrChanged = true;
    }
    
    // 如果页面变空，可以选择保留或释放页面
    // 这里选择保留页面以便将来使用
    
    // 标记页面为脏页并解除固定
    if ((rc = pfFileHandle->MarkDirty(pageNum)) ||
        (rc = pfFileHandle->UnpinPage(pageNum))) {
        return rc;
    }
    
    return OK;
}

//
// 更新记录
//
RC RM_FileHandle::UpdateRec(const RM_Record &rec) {
    RC rc;
    RID rid;
    
    // 检查文件是否打开
    if (!bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 检查记录是否有效
    if (!rec.bValidRecord || rec.pData == NULL) {
        return RM_INVALIDRECORD;
    }
    
    // 获取记录的RID
    if ((rc = rec.GetRid(rid))) {
        return rc;
    }
    
    // 实际上就是删除旧记录并插入新记录在同一位置
    // 但为了保持RID不变，我们直接覆盖数据
    
    PageNum pageNum;
    SlotNum slotNum;
    
    // 获取RID的页号和槽号
    if ((rc = rid.GetPageNum(pageNum)) ||
        (rc = rid.GetSlotNum(slotNum))) {
        return rc;
    }
    
    // 检查页号和槽号是否有效
    if (pageNum < 1 || pageNum >= numPages || 
        slotNum < 0 || slotNum >= recordsPerPage) {
        return RM_INVALIDRID;
    }
    
    // 获取页面
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = pfFileHandle->GetThisPage(pageNum, pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        return rc;
    }
    
    // 获取位图
    char* bitmap = RM_GetBitmap(pageData);
    
    // 检查记录是否存在
    if (!RM_TestBit(bitmap, slotNum)) {
        pfFileHandle->UnpinPage(pageNum);
        return RM_RECORDNOTFOUND;
    }
    
    // 计算记录位置并更新数据
    int recordOffset = RM_GetRecordOffset(slotNum, recordSize);
    char* recordData = pageData + RM_PAGE_HDR_SIZE + 
                       RM_CalcBitmapSize(recordsPerPage) + 
                       recordOffset;
    
    // 覆盖原记录数据
    memcpy(recordData, rec.pData, recordSize);
    
    // 标记页面为脏页并解除固定
    if ((rc = pfFileHandle->MarkDirty(pageNum)) ||
        (rc = pfFileHandle->UnpinPage(pageNum))) {
        return rc;
    }
    
    return OK;
}

//
// 强制写入页面到磁盘
//
RC RM_FileHandle::ForcePages(PageNum pageNum) const {
    // 检查文件是否打开
    if (!bFileOpen) {
        return RM_FILENOTOPEN;
    }
    
    // 调用PF文件句柄的ForcePages方法
    return pfFileHandle->ForcePages(pageNum);
}