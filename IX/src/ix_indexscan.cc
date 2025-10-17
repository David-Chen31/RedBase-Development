//
// ix_indexscan.cc: IX_IndexScan class implementation  
//
// IX_IndexScan提供对索引的扫描功能，支持范围查询和条件过滤
//

#include <iostream>
#include <cstring>
#include "ix_internal.h"

using namespace std;

//
// IX_IndexScan构造函数
//
IX_IndexScan::IX_IndexScan() {
    isOpenScan = FALSE;
    indexHandle = NULL;
    currentPageNum = IX_NO_PAGE;
    currentSlot = -1;
    pfPageHandle = NULL;
    pinned = FALSE;
}

//
// IX_IndexScan析构函数
//
IX_IndexScan::~IX_IndexScan() {
    // 如果扫描仍然打开，需要先关闭
    if (isOpenScan) {
        cerr << "IX_IndexScan::~IX_IndexScan() - 警告：析构时索引扫描仍然打开" << endl;
        if (pinned && pfPageHandle != NULL) {
            indexHandle->pfh->UnpinPage(currentPageNum);
        }
    }
}

//
// OpenScan: 打开索引扫描
// 输入: indexHandle - 索引句柄
//       compOp      - 比较操作符
//       value       - 比较值
//       clientHint  - 客户端提示（未使用）
// 返回: RC码
//
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle_,
                         CompOp compOp_,
                         void *value_,
                         ClientHint  pinHint) {
    RC rc;
    
    // 检查索引句柄是否打开
    if (!indexHandle_.isOpenHandle) {
        return IX_INDEXNOTOPEN;
    }
    
    // 检查扫描是否已经打开
    if (isOpenScan) {
        return IX_SCANOPEN;
    }
    
    // 保存参数
    indexHandle = const_cast<IX_IndexHandle*>(&indexHandle_);
    compOp = compOp_;
    
    // 复制比较值
    if (value_ != NULL) {
        if (indexHandle->indexHdr.attrType == STRING) {
            value = new char[indexHandle->indexHdr.attrLength];
            memcpy(value, value_, indexHandle->indexHdr.attrLength);
        } else if (indexHandle->indexHdr.attrType == INT) {
            value = new int;
            *(int*)value = *(int*)value_;
        } else if (indexHandle->indexHdr.attrType == FLOAT) {
            value = new float;
            *(float*)value = *(float*)value_;
        }
        hasValue = TRUE;
    } else {
        value = NULL;
        hasValue = FALSE;
    }
    
    // 初始化扫描状态
    currentPageNum = IX_NO_PAGE;
    currentSlot = -1;
    pfPageHandle = NULL;
    pinned = FALSE;
    scanEnded = FALSE;
    
    // 根据比较操作找到起始位置
    if (compOp == NO_OP) {
        // 无条件扫描：从最左边的叶子节点开始
        rc = FindFirstLeafPage();
    } else {
        // 有条件扫描：找到满足条件的第一个位置
        rc = FindStartPosition();
    }
    
    if (rc != 0) {
        // 清理已分配的内存
        if (value != NULL) {
            if (indexHandle->indexHdr.attrType == STRING) {
                delete[] (char*)value;
            } else {
                delete value;
            }
        }
        return rc;
    }
    
    isOpenScan = TRUE;
    return 0;
}

//
// GetNextEntry: 获取下一个满足条件的条目
// 输出: rid - 记录标识符
// 返回: RC码，如果到达扫描结束返回IX_EOF
//
RC IX_IndexScan::GetNextEntry(RID &rid) {
    RC rc;
    
    // 检查扫描是否打开
    if (!isOpenScan) {
        return IX_SCANNOTOPEN;
    }
    
    // 检查是否已经结束
    if (scanEnded) {
        return IX_EOF;
    }
    
    // 查找下一个满足条件的条目
    while (true) {
        // 如果没有当前页面，获取第一个页面
        if (currentPageNum == IX_NO_PAGE) {
            if (compOp == NO_OP) {
                rc = FindFirstLeafPage();
            } else {
                rc = FindStartPosition();
            }
            
            if (rc != 0) {
                scanEnded = TRUE;
                return (rc == IX_ENTRYNOTFOUND) ? IX_EOF : rc;
            }
        }
        
        // 在当前页面中查找条目
        rc = GetNextEntryInPage(rid);
        
        if (rc == 0) {
            // 找到条目，检查是否满足条件
            if (SatisfiesCondition(rid)) {
                return 0; // 成功找到
            }
            // 不满足条件，继续查找
        } else if (rc == IX_EOF) {
            // 当前页面结束，移动到下一页
            rc = MoveToNextPage();
            if (rc != 0) {
                scanEnded = TRUE;
                return IX_EOF;
            }
        } else {
            // 其他错误
            scanEnded = TRUE;
            return rc;
        }
    }
}

//
// CloseScan: 关闭索引扫描
// 返回: RC码
//
RC IX_IndexScan::CloseScan() {
    RC rc = 0;
    
    // 检查扫描是否打开
    if (!isOpenScan) {
        return IX_SCANNOTOPEN;
    }
    
    // 释放当前pin的页面
    if (pinned && pfPageHandle != NULL) {
        rc = indexHandle->pfh->UnpinPage(currentPageNum);
        if (rc != 0) {
            // 即使unpin失败，也要继续清理
        }
    }
    
    // 清理分配的内存
    if (value != NULL) {
        if (indexHandle->indexHdr.attrType == STRING) {
            delete[] (char*)value;
        } else {
            delete value;
        }
        value = NULL;
    }
    
    // 重置状态
    isOpenScan = FALSE;
    indexHandle = NULL;
    currentPageNum = IX_NO_PAGE;
    currentSlot = -1;
    pfPageHandle = NULL;
    pinned = FALSE;
    
    return rc;
}

//
// FindFirstLeafPage: 找到最左边的叶子页面（用于无条件扫描）
//
RC IX_IndexScan::FindFirstLeafPage() {
    RC rc;
    PageNum pageNum = indexHandle->indexHdr.rootPage;
    
    // 如果索引为空
    if (pageNum == IX_NO_PAGE) {
        return IX_EOF;
    }
    
    // 从根节点开始，沿着最左边的路径下降到叶子节点
    while (true) {
        PF_PageHandle ph;
        char *nodeData;
        
        if ((rc = indexHandle->pfh->GetThisPage(pageNum, ph))) {
            return rc;
        }
        
        if ((rc = ph.GetData(nodeData))) {
            indexHandle->pfh->UnpinPage(pageNum);
            return rc;
        }
        
        IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
        
        if (nodeHdr->isLeaf) {
            // 到达叶子节点
            currentPageNum = pageNum;
            currentSlot = -1; // 从第一个条目之前开始
            pfPageHandle = new PF_PageHandle(ph);
            pinned = TRUE;
            
            return 0;
        } else {
            // 内部节点，取第一个子指针
            char *entries = nodeData + sizeof(IX_NodeHdr);
            PageNum childPage = *(PageNum *)entries;
            
            indexHandle->pfh->UnpinPage(pageNum);
            pageNum = childPage;
        }
    }
}

//
// FindStartPosition: 根据比较条件找到扫描起始位置
//
RC IX_IndexScan::FindStartPosition() {
    RC rc;
    
    if (!hasValue) {
        // 没有比较值，根据操作类型决定起始位置
        if (compOp == LT_OP || compOp == LE_OP || compOp == NE_OP) {
            return FindFirstLeafPage();
        } else {
            return IX_EOF; // GT, GE, EQ 操作需要比较值
        }
    }
    
    // 使用B+树搜索找到键值位置
    PageNum leafPage;
    int slotNum;
    
    rc = SearchKey(value, leafPage, slotNum);
    
    if (rc == IX_ENTRYNOTFOUND) {
        // 没有找到确切匹配，根据比较操作调整位置
        rc = FindInsertPosition(value, leafPage, slotNum);
    }
    
    if (rc != 0 && rc != IX_ENTRYNOTFOUND) {
        return rc;
    }
    
    // 设置扫描起始位置
    currentPageNum = leafPage;
    currentSlot = slotNum - 1; // GetNextEntry会递增slot
    
    // Pin页面
    pfPageHandle = new PF_PageHandle();
    if ((rc = indexHandle->pfh->GetThisPage(currentPageNum, *pfPageHandle))) {
        delete pfPageHandle;
        pfPageHandle = NULL;
        return rc;
    }
    pinned = TRUE;
    
    return 0;
}

//
// SearchKey: 在B+树中搜索键值
//
RC IX_IndexScan::SearchKey(void *searchKey, PageNum &leafPage, int &slotNum) {
    RC rc;
    PageNum pageNum = indexHandle->indexHdr.rootPage;
    
    if (pageNum == IX_NO_PAGE) {
        return IX_ENTRYNOTFOUND;
    }
    
    // 从根节点开始搜索
    while (true) {
        PF_PageHandle ph;
        char *nodeData;
        
        if ((rc = indexHandle->pfh->GetThisPage(pageNum, ph))) {
            return rc;
        }
        
        if ((rc = ph.GetData(nodeData))) {
            indexHandle->pfh->UnpinPage(pageNum);
            return rc;
        }
        
        IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
        
        if (nodeHdr->isLeaf) {
            // 到达叶子节点，查找键值
            rc = FindKeyInLeaf(nodeData, searchKey, slotNum);
            leafPage = pageNum;
            
            indexHandle->pfh->UnpinPage(pageNum);
            return rc;
        } else {
            // 内部节点，找到子节点
            PageNum childPage;
            rc = FindChildPageForKey(nodeData, searchKey, childPage);
            
            indexHandle->pfh->UnpinPage(pageNum);
            
            if (rc != 0) {
                return rc;
            }
            
            pageNum = childPage;
        }
    }
}

//
// FindKeyInLeaf: 在叶子节点中查找键值
//
RC IX_IndexScan::FindKeyInLeaf(char *nodeData, void *searchKey, int &slotNum) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = indexHandle->GetLeafEntrySize();
    
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *entry = entries + i * entrySize;
        int cmp = indexHandle->CompareKeys(searchKey, entry);
        
        if (cmp == 0) {
            slotNum = i;
            return 0; // 找到匹配键值
        } else if (cmp < 0) {
            slotNum = i;
            return IX_ENTRYNOTFOUND; // 没有完全匹配，但找到插入位置
        }
    }
    
    slotNum = nodeHdr->numKeys;
    return IX_ENTRYNOTFOUND; // 键值比所有现有键都大
}

//
// GetNextEntryInPage: 在当前页面中获取下一个条目
//
RC IX_IndexScan::GetNextEntryInPage(RID &rid) {
    RC rc;
    char *nodeData;
    
    if (!pinned || pfPageHandle == NULL) {
        return IX_SCANNOTOPEN;
    }
    
    if ((rc = pfPageHandle->GetData(nodeData))) {
        return rc;
    }
    
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    // 移动到下一个slot
    currentSlot++;
    
    if (currentSlot >= nodeHdr->numKeys) {
        return IX_EOF; // 当前页面结束
    }
    
    // 获取当前条目的RID
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = indexHandle->GetLeafEntrySize();
    char *currentEntry = entries + currentSlot * entrySize;
    
    // RID存储在键值之后
    RID *entryRID = (RID *)(currentEntry + indexHandle->indexHdr.attrLength);
    rid = *entryRID;
    
    return 0;
}

//
// MoveToNextPage: 移动到下一个叶子页面
//
RC IX_IndexScan::MoveToNextPage() {
    RC rc;
    char *nodeData;
    
    if (!pinned || pfPageHandle == NULL) {
        return IX_EOF;
    }
    
    // 获取当前页面数据
    if ((rc = pfPageHandle->GetData(nodeData))) {
        return rc;
    }
    
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    PageNum nextPage = nodeHdr->right;
    
    // 释放当前页面
    indexHandle->pfh->UnpinPage(currentPageNum);
    delete pfPageHandle;
    pinned = FALSE;
    
    if (nextPage == IX_NO_PAGE) {
        return IX_EOF; // 没有更多页面
    }
    
    // 获取下一个页面
    pfPageHandle = new PF_PageHandle();
    if ((rc = indexHandle->pfh->GetThisPage(nextPage, *pfPageHandle))) {
        delete pfPageHandle;
        pfPageHandle = NULL;
        return rc;
    }
    
    currentPageNum = nextPage;
    currentSlot = -1; // 重新开始
    pinned = TRUE;
    
    return 0;
}

//
// SatisfiesCondition: 检查当前条目是否满足扫描条件
//
bool IX_IndexScan::SatisfiesCondition(const RID &rid) {
    RC rc;
    char *nodeData;
    
    if (!hasValue || compOp == NO_OP) {
        return true; // 无条件或无比较值
    }
    
    // 获取当前条目的键值
    if ((rc = pfPageHandle->GetData(nodeData))) {
        return false;
    }
    
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = indexHandle->GetLeafEntrySize();
    char *currentEntry = entries + currentSlot * entrySize;
    
    // 比较键值
    int cmp = indexHandle->CompareKeys(currentEntry, value);
    
    switch (compOp) {
        case EQ_OP:
            return cmp == 0;
        case LT_OP:
            return cmp < 0;
        case GT_OP:
            return cmp > 0;
        case LE_OP:
            return cmp <= 0;
        case GE_OP:
            return cmp >= 0;
        case NE_OP:
            return cmp != 0;
        case NO_OP:
            return true;
        default:
            return false;
    }
}

//
// FindChildPageForKey: 在内部节点中找到包含键值的子页面
//
RC IX_IndexScan::FindChildPageForKey(char *nodeData, void *searchKey, PageNum &childPage) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = indexHandle->GetInternalEntrySize();
    
    // 第一个子页面指针
    childPage = *(PageNum *)entries;
    entries += sizeof(PageNum);
    
    // 查找正确的子页面
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *keyPtr = entries + i * entrySize;
        
        if (indexHandle->CompareKeys(searchKey, keyPtr) < 0) {
            // 键值小于当前键，应该在当前子页面中
            return 0;
        }
        
        // 移动到下一个子页面
        childPage = *(PageNum *)(keyPtr + indexHandle->indexHdr.attrLength);
    }
    
    // 键值大于所有键，在最右边的子页面中
    return 0;
}

//
// FindInsertPosition: 找到键值的插入位置（用于范围查询的起始位置）
//
RC IX_IndexScan::FindInsertPosition(void *searchKey, PageNum &leafPage, int &slotNum) {
    // 这个函数与SearchKey类似，但专门用于找到插入位置
    // 当没有找到确切匹配时，返回应该插入的位置
    
    RC rc = SearchKey(searchKey, leafPage, slotNum);
    
    // SearchKey如果返回IX_ENTRYNOTFOUND，slotNum已经设置为插入位置
    if (rc == IX_ENTRYNOTFOUND) {
        return 0; // 转换为成功，因为我们需要的是位置信息
    }
    
    return rc;
}