#include "pf_pagehandle.h"

//
// 私有类成员
//
struct PF_PageHandle_Private {
    char *pData;           // 指向页面数据的指针
    PageNum pageNum;       // 页号
    bool valid;           // 页面句柄是否有效

    // 构造函数
    PF_PageHandle_Private() : pData(nullptr), pageNum(-1), valid(false) {}
};

//
// PF_PageHandle
//
// 描述: 构造函数
//
PF_PageHandle::PF_PageHandle() {
    // 初始化私有成员
    this->p = new PF_PageHandle_Private();
}

//
// ~PF_PageHandle
//
// 描述: 析构函数
//
PF_PageHandle::~PF_PageHandle() {
    delete this->p;
}

//
// PF_PageHandle
//
// 描述: 拷贝构造函数
//
PF_PageHandle::PF_PageHandle(const PF_PageHandle &pageHandle) {
    // 创建新的私有成员并复制数据
    this->p = new PF_PageHandle_Private();
    this->p->pData = pageHandle.p->pData;
    this->p->pageNum = pageHandle.p->pageNum;
    this->p->valid = pageHandle.p->valid;
}

//
// operator=
//
// 描述: 赋值运算符
//
PF_PageHandle& PF_PageHandle::operator=(const PF_PageHandle &pageHandle) {
    // 检查自赋值
    if (this != &pageHandle) {
        // 复制数据
        this->p->pData = pageHandle.p->pData;
        this->p->pageNum = pageHandle.p->pageNum;
        this->p->valid = pageHandle.p->valid;
    }
    return *this;
}

//
// GetData
//
// 描述: 返回指向页面数据的指针
// 输出参数:
//     pData - 返回指向页面数据的指针
// 返回值:
//     PF return code
//
RC PF_PageHandle::GetData(char *&pData) const {
    // 检查页面句柄是否有效
    if (!this->p->valid)
        return PF_INVALIDPAGE;

    // 返回数据指针
    pData = this->p->pData;
    return 0;
}

//
// GetPageNum
//
// 描述: 返回页号
// 输出参数:
//     pageNum - 返回页号
// 返回值:
//     PF return code
//
RC PF_PageHandle::GetPageNum(PageNum &pageNum) const {
    // 检查页面句柄是否有效
    if (!this->p->valid)
        return PF_INVALIDPAGE;

    // 返回页号
    pageNum = this->p->pageNum;
    return 0;
}

//
// 以下是内部使用的方法，通常由 PF_FileHandle 调用
//

//
// Init
//
// 描述: 初始化页面句柄
// 输入参数:
//     pData   - 指向页面数据的指针
//     pageNum - 页号
//
void PF_PageHandle::Init(char *pData, PageNum pageNum) {
    this->p->pData = pData;
    this->p->pageNum = pageNum;
    this->p->valid = true;
}

//
// Clear
//
// 描述: 清除页面句柄
//
void PF_PageHandle::Clear() {
    this->p->pData = nullptr;
    this->p->pageNum = -1;
    this->p->valid = false;
}

//
// IsValid
//
// 描述: 检查页面句柄是否有效
//
bool PF_PageHandle::IsValid() const {
    return this->p->valid;
}
