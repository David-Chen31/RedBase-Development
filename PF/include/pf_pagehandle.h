#ifndef PF_PAGEHANDLE_H
#define PF_PAGEHANDLE_H

#include "pf.h"

// 前向声明
struct PF_PageHandle_Private;

class PF_PageHandle {
public:
    PF_PageHandle  ();                          // Default constructor
    ~PF_PageHandle ();                          // Destructor
    PF_PageHandle  (const PF_PageHandle &pageHandle); 
                                               // Copy constructor
    PF_PageHandle& operator= (const PF_PageHandle &pageHandle);
                                               // Overload =
    RC GetData     (char *&pData) const;       // Set pData to point to
                                               //   the page contents
    RC GetPageNum  (PageNum &pageNum) const;   // Return the page number

    // 内部方法 - 由 PF_FileHandle 使用
    void Init(char *pData, PageNum pageNum);   // 初始化页面句柄
    void Clear();                              // 清除页面句柄
    bool IsValid() const;                      // 检查页面句柄是否有效

private:
    PF_PageHandle_Private *p;                  // 指向私有实现的指针
};

#endif // PF_PAGEHANDLE_H