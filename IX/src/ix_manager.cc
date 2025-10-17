#include "../include/ix.h"
#include "../internal/ix_internal.h"
#include <cstring>
#include <iostream>

// 添加缺失的错误码定义
#define IX_BADINDEXSPEC        (START_IX_ERR - 7)    // 无效索引规格
#define IX_INVALID_PAGE        IX_NO_PAGE            // 无效页号

//
// 构造函数
//
IX_Manager::IX_Manager(PF_Manager &pfm) : pfManager(&pfm) {
    // 无需额外初始化
}

//
// 析构函数
//
IX_Manager::~IX_Manager() {
    // 无需特殊清理
}

//
// 创建索引
//
// 流程：
// 1. 参数检查
// 2. 生成索引文件名
// 3. 调用PF管理器创建文件
// 4. 打开文件，分配并初始化IX_FileHdr文件头页面
// 5. 标记页面为脏页并解除固定
// 6. 关闭文件
//
RC IX_Manager::CreateIndex(const char *fileName, int indexNo, 
                          AttrType attrType, int attrLength) {
    RC rc;
    
    // 参数检查
    if (fileName == NULL || indexNo < 0) {
        return IX_BADINDEXSPEC;
    }
    
    // 检查属性类型和长度
    if ((attrType == INT || attrType == FLOAT) && attrLength != 4) {
        return IX_BADINDEXSPEC;
    }
    if (attrType == STRING && (attrLength < 1 || attrLength > MAXSTRINGLEN)) {
        return IX_BADINDEXSPEC;
    }
    
    // 生成索引文件名：fileName.indexNo
    char indexFileName[256];
    sprintf(indexFileName, "%s.%d", fileName, indexNo);
    
    // 调用PF管理器创建文件
    if ((rc = pfManager->CreateFile(indexFileName))) {
        return rc;
    }
    
    // 打开刚创建的文件以初始化PF_FileHeader文件头
    PF_FileHandle fileHandle;
    if ((rc = pfManager->OpenFile(indexFileName, fileHandle))) {
        return rc;
    }
    
    // 分配IX_FileHdr文件头页面
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = fileHandle.AllocatePage(pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        pfManager->CloseFile(fileHandle);
        return rc;
    }
    
    // 初始化IX_FileHdr文件头，一个索引文件有两个不同的文件头，一个是PF_FileHeader，一个是IX_FileHdr
    IX_FileHdr* fileHdr = (IX_FileHdr*)pageData;
    fileHdr->attrType = attrType;
    fileHdr->attrLength = attrLength;
    fileHdr->rootPage = IX_INVALID_PAGE;  // 空索引，暂无根节点
    fileHdr->numPages = 1;                // 只有头页面
    fileHdr->firstFreePage = IX_INVALID_PAGE;
    
    // 标记页面为脏页并解除固定
    PageNum pageNum;
    if ((rc = pageHandle.GetPageNum(pageNum)) ||
        (rc = fileHandle.MarkDirty(pageNum)) ||
        (rc = fileHandle.UnpinPage(pageNum))) {
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
// 删除索引
//
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {
    if (fileName == NULL || indexNo < 0) {
        return IX_BADINDEXSPEC;
    }
    
    // 生成索引文件名
    char indexFileName[256];
    sprintf(indexFileName, "%s.%d", fileName, indexNo);
    
    // 直接调用PF管理器删除文件
    return pfManager->DestroyFile(indexFileName);
}

//
// 打开索引
//
// 流程：
// 1. 参数检查
// 2. 检查索引句柄是否已经打开
// 3. 生成索引文件名
// 4. 分配PF文件句柄
// 5. 打开PF文件
// 6. 读取文件头信息到索引句柄 
// 7. 解除文件头页面的固定
// 8. 设置索引句柄状态
RC IX_Manager::OpenIndex(const char *fileName, int indexNo, 
                        IX_IndexHandle &indexHandle) {
    RC rc;
    
    // 参数检查
    if (fileName == NULL || indexNo < 0) {
        return IX_BADINDEXSPEC;
    }
    
    // 检查索引句柄是否已经打开
    if (indexHandle.isOpenHandle) {
        return IX_INDEXNOTOPEN;  // 索引句柄已在使用中
    }
    
    // 生成索引文件名
    char indexFileName[256];
    sprintf(indexFileName, "%s.%d", fileName, indexNo);
    
    // 分配PF文件句柄
    indexHandle.pfh = new PF_FileHandle();
    
    // 打开PF文件
    if ((rc = pfManager->OpenFile(indexFileName, *(indexHandle.pfh)))) {
        delete indexHandle.pfh;
        indexHandle.pfh = NULL;
        return rc;
    }
    
    // 读取文件头信息
    PF_PageHandle pageHandle;
    char* pageData;
    if ((rc = indexHandle.pfh->GetThisPage(0, pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        pfManager->CloseFile(*(indexHandle.pfh));
        delete indexHandle.pfh;
        indexHandle.pfh = NULL;
        return rc;
    }
    
    // 复制文件头信息到索引句柄
    IX_FileHdr* fileHdr = (IX_FileHdr*)pageData;
    indexHandle.indexHdr.attrType = fileHdr->attrType;
    indexHandle.indexHdr.attrLength = fileHdr->attrLength;
    indexHandle.indexHdr.rootPage = fileHdr->rootPage;
    indexHandle.indexHdr.numPages = fileHdr->numPages;
    indexHandle.indexHdr.firstFreePage = fileHdr->firstFreePage;
    
    // 解除文件头页面的固定
    PageNum pageNum;
    if ((rc = pageHandle.GetPageNum(pageNum)) ||
        (rc = indexHandle.pfh->UnpinPage(pageNum))) {
        pfManager->CloseFile(*(indexHandle.pfh));
        delete indexHandle.pfh;
        indexHandle.pfh = NULL;
        return rc;
    }
    
    // 设置索引句柄状态
    indexHandle.isOpenHandle = true;
    
    return OK;
}

//
// 关闭索引
//
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
    RC rc;
    
    // 检查索引是否打开
    if (!indexHandle.isOpenHandle || indexHandle.pfh == NULL) {
        return IX_INDEXNOTOPEN;
    }
    
    // 写回文件头信息
    PF_PageHandle pageHandle;
    char* pageData;
    
    // 获取文件头页面
    if ((rc = indexHandle.pfh->GetThisPage(0, pageHandle)) ||
        (rc = pageHandle.GetData(pageData))) {
        return rc;
    }
    
    // 更新文件头信息
    IX_FileHdr* fileHdr = (IX_FileHdr*)pageData;
    fileHdr->attrType = indexHandle.indexHdr.attrType;
    fileHdr->attrLength = indexHandle.indexHdr.attrLength;
    fileHdr->rootPage = indexHandle.indexHdr.rootPage;
    fileHdr->numPages = indexHandle.indexHdr.numPages;
    fileHdr->firstFreePage = indexHandle.indexHdr.firstFreePage;
    
    // 标记为脏页并解除固定
    PageNum pageNum;
    if ((rc = pageHandle.GetPageNum(pageNum)) ||
        (rc = indexHandle.pfh->MarkDirty(pageNum)) ||
        (rc = indexHandle.pfh->UnpinPage(pageNum))) {
        return rc;
    }
    
    // 关闭PF文件
    rc = pfManager->CloseFile(*(indexHandle.pfh));
    
    // 清理索引句柄
    delete indexHandle.pfh;
    indexHandle.pfh = NULL;
    indexHandle.isOpenHandle = false;
    
    return rc;
}