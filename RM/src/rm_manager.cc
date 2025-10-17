#include "../include/rm.h"
#include "../internal/rm_internal.h"
#include "../../PF/include/pf.h"
#include "../../PF/include/pf_manager.h"
#include <cstring>
#include <iostream>

//
// 构造函数
//
RM_Manager::RM_Manager(PF_Manager &pfm) : pfManager(&pfm) {
    // 无需额外初始化
}

//
// 析构函数
//
RM_Manager::~RM_Manager() {
    // 无需特殊清理
}

//
// 创建记录文件
//
RC RM_Manager::CreateFile(const char *fileName, int recordSize) {
    RC rc;
    
    // 参数检查
    if (fileName == NULL || recordSize <= 0) {
        return RM_INVALIDFILE;
    }
    
    // 检查记录大小是否过大
    if (recordSize > PF_PAGE_SIZE - RM_PAGE_HDR_SIZE - 10) {  // 预留一些空间给位图
        return RM_RECORDSIZETOOBIG;
    }
    
    // 调用PF管理器创建文件
    if ((rc = pfManager->CreateFile(fileName))) {
        return rc;  // 传递PF错误码
    }
    
    // 打开刚创建的文件以写入文件头信息
    PF_FileHandle fileHandle;
    if ((rc = pfManager->OpenFile(fileName, fileHandle))) {
        return rc;
    }
    
    // 分配文件头页面
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = fileHandle.AllocatePage(pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        pfManager->CloseFile(fileHandle);
        return rc;
    }
    
    // 初始化文件头
    RM_FileHdr* fileHdr = (RM_FileHdr*)pageData;
    fileHdr->recordSize = recordSize;
    fileHdr->recordsPerPage = RM_CalcRecordsPerPage(recordSize);
    fileHdr->numPages = 1;  // 只有头页面
    fileHdr->firstFree = RM_INVALID_PAGE;  // 暂时没有数据页
    
    // 标记页面为脏页并解除固定
    PageNum pageNum;
    if ((rc = pageHandle.GetPageNum(pageNum)) ||
        (rc = fileHandle.MarkDirty(pageNum)) ||
        (rc = fileHandle.UnpinPage(pageNum))) {
        pfManager->CloseFile(fileHandle);
        return rc;
    }
    
    
    // 强制写入所有页面到磁盘（包括文件头）
    if ((rc = fileHandle.ForcePages())) {
        pfManager->CloseFile(fileHandle);
        return rc;
    }
    // 关闭文件
    if ((rc = pfManager->CloseFile(fileHandle))) {
        return rc;
    }
    
    return OK;
}

//
// 删除记录文件
//
RC RM_Manager::DestroyFile(const char *fileName) {
    if (fileName == NULL) {
        return RM_INVALIDFILE;
    }
    
    // 直接调用PF管理器删除文件
    return pfManager->DestroyFile(fileName);
}

//
// 打开记录文件
//
RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle) {
    RC rc;
    
    // 参数检查
    if (fileName == NULL) {
        return RM_INVALIDFILE;
    }
    
    // 检查文件句柄是否已经打开
    if (fileHandle.bFileOpen) {
        return RM_FILENOTOPEN;  // 文件句柄已在使用中
    }
    
    // 分配PF文件句柄
    fileHandle.pfFileHandle = new PF_FileHandle();
    
    // 打开PF文件
    if ((rc = pfManager->OpenFile(fileName, *(fileHandle.pfFileHandle)))) {
        delete fileHandle.pfFileHandle;
        fileHandle.pfFileHandle = NULL;
        return rc;
    }
    
    // 读取文件头信息
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = fileHandle.pfFileHandle->GetFirstPage(pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        pfManager->CloseFile(*(fileHandle.pfFileHandle));
        delete fileHandle.pfFileHandle;
        fileHandle.pfFileHandle = NULL;
        return rc;
    }
    
    // 复制文件头信息到文件句柄
    RM_FileHdr* fileHdr = (RM_FileHdr*)pageData;
    fileHandle.recordSize = fileHdr->recordSize;
    fileHandle.recordsPerPage = fileHdr->recordsPerPage;
    fileHandle.numPages = fileHdr->numPages;
    fileHandle.firstFree = fileHdr->firstFree;
    
    // 解除文件头页面的固定
    PageNum pageNum;
    if ((rc = pageHandle.GetPageNum(pageNum)) ||
        (rc = fileHandle.pfFileHandle->UnpinPage(pageNum))) {
        pfManager->CloseFile(*(fileHandle.pfFileHandle));
        delete fileHandle.pfFileHandle;
        fileHandle.pfFileHandle = NULL;
        return rc;
    }
    
    // 设置文件句柄状态
    fileHandle.bFileOpen = true;
    fileHandle.bHdrChanged = false;
    
    return OK;
}

//
// 关闭记录文件
//
RC RM_Manager::CloseFile(RM_FileHandle &fileHandle) {
    RC rc;
    
    // 检查文件是否打开
    if (!fileHandle.bFileOpen || fileHandle.pfFileHandle == NULL) {
        return RM_FILENOTOPEN;
    }
    
    // 如果文件头被修改，需要写回
    if (fileHandle.bHdrChanged) {
        PF_PageHandle pageHandle;
        char* pageData;
        
        // 获取文件头页面
        if ((rc = fileHandle.pfFileHandle->GetFirstPage(pageHandle)) ||
            (rc = pageHandle.GetData(pageData))) {
            return rc;
        }
        
        // 更新文件头信息
        RM_FileHdr* fileHdr = (RM_FileHdr*)pageData;
        fileHdr->recordSize = fileHandle.recordSize;
        fileHdr->recordsPerPage = fileHandle.recordsPerPage;
        fileHdr->numPages = fileHandle.numPages;
        fileHdr->firstFree = fileHandle.firstFree;
        
        // 标记为脏页并解除固定
        PageNum pageNum;
        if ((rc = pageHandle.GetPageNum(pageNum)) ||
            (rc = fileHandle.pfFileHandle->MarkDirty(pageNum)) ||
            (rc = fileHandle.pfFileHandle->UnpinPage(pageNum))) {
            return rc;
        }
    }
    
    // 关闭PF文件
    rc = pfManager->CloseFile(*(fileHandle.pfFileHandle));
    
    // 清理文件句柄
    delete fileHandle.pfFileHandle;
    fileHandle.pfFileHandle = NULL;
    fileHandle.bFileOpen = false;
    fileHandle.bHdrChanged = false;
    
    return rc;
}