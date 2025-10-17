#ifndef PF_FILEHANDLE_H
#define PF_FILEHANDLE_H

#include "pf.h"
#include "pf_pagehandle.h"
#include "../internal/pf_internal.h"

class PF_FileHandle {
public:
    PF_FileHandle  ();                                  // Default constructor
    ~PF_FileHandle ();                                  // Destructor
    PF_FileHandle  (const PF_FileHandle &fileHandle);   // Copy constructor
    PF_FileHandle& operator= (const PF_FileHandle &fileHandle);  // Overload =

    // 文件操作方法
    RC GetFirstPage   (PF_PageHandle &pageHandle) const;   // Get the first page
    RC GetLastPage    (PF_PageHandle &pageHandle) const;   // Get the last page
    RC GetNextPage    (PageNum current, PF_PageHandle &pageHandle) const;
                                                          // Get the next page
    RC GetPrevPage    (PageNum current, PF_PageHandle &pageHandle) const;
                                                          // Get the previous page
    RC GetThisPage    (PageNum pageNum, PF_PageHandle &pageHandle) const;
                                                          // Get a specific page
    RC AllocatePage   (PF_PageHandle &pageHandle);        // Allocate a new page
    RC DisposePage    (PageNum pageNum);                  // Dispose of a page
    RC MarkDirty      (PageNum pageNum) const;            // Mark a page as dirty
    RC UnpinPage      (PageNum pageNum) const;            // Unpin a page
    RC ForcePages     (PageNum pageNum = ALL_PAGES) const;// Write dirty page(s)
                                                         // to disk

    // 内部工具方法 - 由 PF_Manager 使用
    int GetFd() const { return fd; }                     // 返回文件描述符
    RC Init(int _fd, PF_FileHeader _hdr, class PF_Manager *pMgr);               // 初始化文件句柄
    RC Reset();                                         // 重置文件句柄状态
    RC WriteHeader();                                   // 写回文件头到磁盘

private:
    int fd;                                             // 文件描述符
    PF_FileHeader hdr;                                  // 文件头信息
    bool headerChanged;                                 // 文件头是否被修改
    bool open;                                         // 文件是否打开
    class PF_Manager *pManager;                         // 指向PF_Manager的指针，用于磁盘使用统计
};

#endif // PF_FILEHANDLE_H