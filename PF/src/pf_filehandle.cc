#include <unistd.h>
#include <sys/types.h>
#include "pf_internal.h"
#include "pf_filehandle.h"
#include "pf_pagehandle.h"
#include "../internal/buffer_manager.h"
#include "pf_manager.h"
#include "pf_manager.h"

//
// PF_FileHandle
//
// 描述: 构造函数
//
PF_FileHandle::PF_FileHandle() {
    this->fd = -1;                // 初始化为无效的文件描述符
    this->open = false;          // 文件未打开
    this->headerChanged = false;  // 文件头未修改
    this->pManager = nullptr;     // 初始化为空指针
}

//
// ~PF_FileHandle
//
// 描述: 析构函数
//
PF_FileHandle::~PF_FileHandle() {
    // 如果文件头被修改且文件打开，则需要写回文件头
    if (this->headerChanged && this->open) {
        // 定位到文件开始
        if (lseek(this->fd, 0, SEEK_SET) < 0)
            PF_PrintError(PF_UNIX);
        else {
            // 写入文件头
            int numBytes = write(this->fd, &this->hdr, sizeof(PF_FileHeader));
            if (numBytes != sizeof(PF_FileHeader))
                PF_PrintError(PF_HDRWRITE);
        }
    }
}

//
// PF_FileHandle
//
// 描述: 拷贝构造函数
//
PF_FileHandle::PF_FileHandle(const PF_FileHandle &fileHandle) {
    this->fd = fileHandle.fd;
    this->hdr = fileHandle.hdr;
    this->headerChanged = fileHandle.headerChanged;
    this->open = fileHandle.open;
    this->pManager = fileHandle.pManager;  // 复制指针
}

//
// operator=
//
// 描述: 赋值运算符
//
PF_FileHandle& PF_FileHandle::operator=(const PF_FileHandle &fileHandle) {
    if (this != &fileHandle) {
        this->fd = fileHandle.fd;
        this->hdr = fileHandle.hdr;
        this->headerChanged = fileHandle.headerChanged;
        this->open = fileHandle.open;
        this->pManager = fileHandle.pManager;  // 复制指针
    }
    return *this;
}

//
// GetFirstPage
//
// 描述: 获取文件的第一个页面
// 输出参数:
//     pageHandle - 返回页面句柄
// 返回值:
//     PF return code
//
RC PF_FileHandle::GetFirstPage(PF_PageHandle &pageHandle) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查是否有页面
    if (this->hdr.numPages == 0)
        return PF_EOF;

    // 获取第一个页面（页号从0开始）
    return GetThisPage(0, pageHandle);
}

//
// GetLastPage
//
// 描述: 获取文件的最后一个页面
// 输出参数:
//     pageHandle - 返回页面句柄
// 返回值:
//     PF return code
//
RC PF_FileHandle::GetLastPage(PF_PageHandle &pageHandle) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查是否有页面
    if (this->hdr.numPages == 0)
        return PF_EOF;

    // 获取最后一个页面
    return GetThisPage(this->hdr.numPages - 1, pageHandle);
}

//
// GetNextPage
//
// 描述: 获取指定页面的下一个页面
// 输入参数:
//     current - 当前页号
// 输出参数:
//     pageHandle - 返回页面句柄
// 返回值:
//     PF return code
//
RC PF_FileHandle::GetNextPage(PageNum current, PF_PageHandle &pageHandle) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (current < 0 || current >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    // 检查是否存在下一页
    if (current + 1 >= this->hdr.numPages)
        return PF_EOF;

    // 获取下一页
    return GetThisPage(current + 1, pageHandle);
}

//
// GetPrevPage
//
// 描述: 获取指定页面的上一个页面
// 输入参数:
//     current - 当前页号
// 输出参数:
//     pageHandle - 返回页面句柄
// 返回值:
//     PF return code
//
RC PF_FileHandle::GetPrevPage(PageNum current, PF_PageHandle &pageHandle) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (current < 0 || current >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    // 检查是否存在上一页
    if (current - 1 < 0)
        return PF_EOF;

    // 获取上一页
    return GetThisPage(current - 1, pageHandle);
}

//
// GetThisPage
//
// 描述: 获取指定页号的页面
// 输入参数:
//     pageNum - 页号
// 输出参数:
//     pageHandle - 返回页面句柄
// 返回值:
//     PF return code
//
RC PF_FileHandle::GetThisPage(PageNum pageNum, PF_PageHandle &pageHandle) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (pageNum < 0 || pageNum >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    // 计算页面在文件中的偏移位置
    long offset = pageNum * (sizeof(PF_PageHeader) + PF_PAGE_SIZE) + sizeof(PF_FileHeader);

    // 从文件读取页面
    char *pageData;
    BufferManager& bufMgr = BufferManager::Instance();
    RC rc = bufMgr.FetchPage(this->fd, pageNum, &pageData);
    if (rc != 0)
        return rc;

    // 初始化页面句柄
    pageHandle.Init(pageData + sizeof(PF_PageHeader), pageNum);

    return 0;
}

//
// AllocatePage
//
// 描述: 分配一个新页面到Frame中，还未写回磁盘
// 输出参数:
//     pageHandle - 返回新页面的句柄
// 返回值:
//     PF return code
//
// 流程：
// 1. 检查文件是否打开
// 2. 使用BufferManager分配新页面，页号为当前文件的页面总数：调用FetchPage从缓冲区获取页面，FetchPage调用ReadPageFromDisk在缓冲区中选一个Frame来存放新页面的数据
// 3. 初始化页面头
// 4. 更新文件头的页面总数和空闲链表
// 5. 初始化页面句柄
// 6. 标记页面为脏
// 7. 更新磁盘使用统计
//
RC PF_FileHandle::AllocatePage(PF_PageHandle &pageHandle) {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 分配新页面
    char *pageData;
    BufferManager& bufMgr = BufferManager::Instance();
    PageNum pageNum = this->hdr.numPages;  // 页号是文件的页号，而不是缓冲区的页框号
    RC rc = bufMgr.FetchPage(this->fd, pageNum, &pageData);  //this->hdr.numPages 是当前文件的页面总数，新页面的页号就是当前页面数（从0开始计数）
    if (rc != 0)
        return rc;

    // 初始化页头
    PF_PageHeader *pageHeader = reinterpret_cast<PF_PageHeader*>(pageData);
    pageHeader->nextFree = PF_PAGE_LIST_END;

    // 更新文件头
    this->hdr.numPages++;
    this->headerChanged = true;

    // 初始化页面句柄
    pageHandle.Init(pageData + sizeof(PF_PageHeader), pageNum);

    // 标记页面为脏
    rc = bufMgr.MarkDirty(this->fd, pageNum);
    if (rc != 0)
        return rc;


    // 更新磁盘使用统计
    if (pManager != nullptr && pManager->GetDiskSpaceLimit() > 0) {
        pManager->AllocateDiskPages(1);
    }

    return 0;
}

//
// DisposePage
//
// 描述: 释放指定的页面
// 输入参数:
//     pageNum - 要释放的页号
// 返回值:
//     PF return code
//
RC PF_FileHandle::DisposePage(PageNum pageNum) {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (pageNum < 0 || pageNum >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    // 将页面添加到空闲链表
    char *pageData;
    BufferManager& bufMgr = BufferManager::Instance();
    RC rc = bufMgr.FetchPage(this->fd, pageNum, &pageData);
    if (rc != 0)
        return rc;

    PF_PageHeader *pageHeader = reinterpret_cast<PF_PageHeader*>(pageData);
    pageHeader->nextFree = this->hdr.firstFree;
    this->hdr.firstFree = pageNum;
    this->headerChanged = true;

    // 标记页面为脏
    rc = bufMgr.MarkDirty(this->fd, pageNum);
    if (rc != 0)
        return rc;

    // 解除页面固定
    rc = bufMgr.UnpinPage(this->fd, pageNum);
    if (rc != 0)
        return rc;

    // 更新磁盘使用统计
    if (pManager != nullptr && pManager->GetDiskSpaceLimit() > 0) {
        pManager->DeallocateDiskPages(1);
    }

    return 0;
}

//
// MarkDirty
//
// 描述: 标记页面为脏页
// 输入参数:
//     pageNum - 页号
// 返回值:
//     PF return code
//
RC PF_FileHandle::MarkDirty(PageNum pageNum) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (pageNum < 0 || pageNum >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    return BufferManager::Instance().MarkDirty(this->fd, pageNum);
}

//
// UnpinPage
//
// 描述: 解除页面固定
// 输入参数:
//     pageNum - 页号
// 返回值:
//     PF return code
//
RC PF_FileHandle::UnpinPage(PageNum pageNum) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效
    if (pageNum < 0 || pageNum >= this->hdr.numPages)
        return PF_INVALIDPAGE;

    return BufferManager::Instance().UnpinPage(this->fd, pageNum);
}

//
// ForcePages
//
// 描述: 将指定页面写回磁盘
// 输入参数:
//     pageNum - 页号（如果为ALL_PAGES则写回所有页面）
// 返回值:
//     PF return code
//
RC PF_FileHandle::ForcePages(PageNum pageNum) const {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;

    // 检查页号是否有效（除非是ALL_PAGES）
    if (pageNum != ALL_PAGES && (pageNum < 0 || pageNum >= this->hdr.numPages))
        return PF_INVALIDPAGE;

    return BufferManager::Instance().FlushAllPages(this->fd);
}

//
// Init
//
// 描述: 初始化文件句柄
// 输入参数:
//     fd  - 文件描述符
//     hdr - 文件头信息
// 返回值:
//     PF return code
//
RC PF_FileHandle::Init(int fd, PF_FileHeader hdr, class PF_Manager *pMgr) {
    this->fd = fd;
    this->hdr = hdr;
    this->headerChanged = false;
    this->open = true;
    this->pManager = pMgr;  // 保存PF_Manager指针
    return 0;
}

//
// Reset
//
// 描述: 重置文件句柄状态
// 返回值:
//     PF return code
//
RC PF_FileHandle::Reset() {
    this->fd = -1;
    this->headerChanged = false;
    this->open = false;
    return 0;
}

//
// WriteHeader
//
// 描述: 写回文件头到磁盘
// 返回值:
//     PF return code
//
RC PF_FileHandle::WriteHeader() {
    // 检查文件是否打开
    if (!this->open)
        return PF_CLOSEDFILE;
    
    // 如果文件头被修改，写回文件头
    if (this->headerChanged) {
        // 定位到文件开始
        if (lseek(this->fd, 0, SEEK_SET) < 0)
            return PF_UNIX;
        
        // 写入文件头
        int numBytes = write(this->fd, &this->hdr, sizeof(PF_FileHeader));
        if (numBytes != sizeof(PF_FileHeader))
            return PF_HDRWRITE;
        
        // 重置标志
        this->headerChanged = false;
    }
    
    return 0;
}
