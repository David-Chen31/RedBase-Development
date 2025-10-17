//
// ix_indexhandle.cc: IX_IndexHandle class implementation
//
// IX_IndexHandle提供对打开的索引文件的操作接口，包括插入、删除、查找等操作
//

#include <cstring>
#include <cstdio>
#include <iostream>
#include "ix_internal.h"

using namespace std;

//
// IX_IndexHandle构造函数
//
IX_IndexHandle::IX_IndexHandle() {
    isOpenHandle = FALSE;
    pfh = NULL;
}

//
// IX_IndexHandle析构函数  
//
IX_IndexHandle::~IX_IndexHandle() {
    // 如果句柄仍然打开，需要先关闭
    if (isOpenHandle) {
        cerr << "IX_IndexHandle::~IX_IndexHandle() - 警告：析构时索引句柄仍然打开" << endl;
    }
}

//
// InsertEntry: 向索引中插入一个条目
// 输入: pData - 指向属性值的指针
//       rid   - 记录标识符
// 返回: RC码
//
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {
    RC rc;
    
    // 检查句柄是否打开
    if (!isOpenHandle) {
        return IX_INDEXNOTOPEN;
    }
    
    // 参数检查
    if (pData == NULL) {
        return IX_NULLPOINTER;
    }
    
    // 从根节点开始插入
    bool wasSplit = false;
    void *newChildKey = NULL;
    PageNum newChildPage = IX_NO_PAGE;
    
    // 分配临时缓冲区用于可能的分裂操作
    if (indexHdr.attrType == STRING) {
        newChildKey = new char[indexHdr.attrLength];
    } else {
        newChildKey = new char[sizeof(int)]; // 假设最大为int大小
    }
    
    // 递归插入
    rc = InsertIntoNode(indexHdr.rootPage, pData, rid, wasSplit, newChildKey, newChildPage);
    
    // 如果根节点分裂，需要创建新的根节点
    if (rc == 0 && wasSplit) {
        rc = CreateNewRoot(newChildKey, indexHdr.rootPage, newChildPage);
    }
    
    // 清理临时内存
    delete[] (char*)newChildKey;
    
    // 更新头信息并写回磁盘
    if (rc == 0) {
        rc = WriteHeader();
    }
    
    return rc;
}

//
// DeleteEntry: 从索引中删除一个条目
// 输入: pData - 指向属性值的指针  
//       rid   - 记录标识符
// 返回: RC码
//
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {
    RC rc;
    
    // 检查句柄是否打开
    if (!isOpenHandle) {
        return IX_INDEXNOTOPEN;
    }
    
    // 参数检查
    if (pData == NULL) {
        return IX_NULLPOINTER;
    }
    
    // 从根节点开始删除
    rc = DeleteFromNode(indexHdr.rootPage, pData, rid);
    
    // 更新头信息
    if (rc == 0) {
        rc = WriteHeader();
    }
    
    return rc;
}

//
// ForcePages: 强制将缓冲页面写入磁盘
// 返回: RC码
//
RC IX_IndexHandle::ForcePages() {
    RC rc;
    
    // 检查句柄是否打开
    if (!isOpenHandle) {
        return IX_INDEXNOTOPEN;
    }
    
    // 调用PF的ForcePages
    if ((rc = pfh->ForcePages())) {
        return rc;
    }
    
    return 0;
}

//
// InsertIntoNode: 递归地向B+树节点插入条目
// 这是插入操作的核心递归函数
//
RC IX_IndexHandle::InsertIntoNode(PageNum pageNum, void *pData, const RID &rid,
                                 bool &wasSplit, void *&newChildKey, PageNum &newChildPage) {
    RC rc;
    PF_PageHandle ph;
    char *nodeData;
    IX_NodeHdr *nodeHdr;
    
    // 获取页面
    if ((rc = pfh->GetThisPage(pageNum, ph))) {
        return rc;
    }
    
    if ((rc = ph.GetData((char *&)nodeData))) {
        pfh->UnpinPage(pageNum);
        return rc;
    }
    
    nodeHdr = (IX_NodeHdr *)nodeData;
    
    if (nodeHdr->isLeaf) {
        // 叶子节点：直接插入
        rc = InsertIntoLeaf(pageNum, nodeData, pData, rid, wasSplit, newChildKey, newChildPage);
    } else {
        // 内部节点：找到子节点并递归插入
        PageNum childPage;
        rc = FindChildPage(nodeData, pData, childPage);
        
        if (rc == 0) {
            bool childSplit = false;
            void *childKey = NULL;
            PageNum childNewPage = IX_NO_PAGE;
            
            // 分配键值缓冲区
            if (indexHdr.attrType == STRING) {
                childKey = new char[indexHdr.attrLength];
            } else {
                childKey = new char[sizeof(int)];
            }
            
            // 递归插入到子节点
            rc = InsertIntoNode(childPage, pData, rid, childSplit, childKey, childNewPage);
            
            // 如果子节点分裂，需要在当前节点插入新的键值-页面对
            if (rc == 0 && childSplit) {
                rc = InsertIntoInternal(nodeData, childKey, childNewPage, wasSplit, newChildKey, newChildPage);
            }
            
            delete[] (char*)childKey;
        }
    }
    
    // 如果节点被修改，标记为脏页
    if (rc == 0 && (wasSplit || nodeHdr->isLeaf)) {
        pfh->MarkDirty(pageNum);
    }
    
    pfh->UnpinPage(pageNum);
    return rc;
}

//
// InsertIntoLeaf: 向叶子节点插入条目
//
RC IX_IndexHandle::InsertIntoLeaf(PageNum currentPageNum, char *nodeData, void *pData, const RID &rid,
                                 bool &wasSplit, void *&newChildKey, PageNum &newChildPage) {
    RC rc = 0;
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    // 计算叶子节点的最大条目数
    int maxEntries = GetMaxLeafEntries();
    
    // 检查是否需要分裂
    if (nodeHdr->numKeys >= maxEntries) {
        // 需要分裂叶子节点
        rc = SplitLeafNode(currentPageNum, nodeData, pData, rid, wasSplit, newChildKey, newChildPage);
    } else {
        // 直接插入到叶子节点
        rc = InsertEntryIntoLeaf(nodeData, pData, rid);
        wasSplit = false;
    }
    
    return rc;
}

//
// InsertEntryIntoLeaf: 在叶子节点中插入条目（假设有足够空间）
//
// 流程：
// 1. 找到插入位置（保持排序）
// 2. 移动后面的条目为新条目腾出空间
// 3. 插入新条目
//
RC IX_IndexHandle::InsertEntryIntoLeaf(char *nodeData, void *pData, const RID &rid) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    
    int entrySize = GetLeafEntrySize();
    int insertPos = 0;
    
    // 找到插入位置（保持排序）
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *currentEntry = entries + i * entrySize;
        if (CompareKeys(pData, currentEntry) <= 0) {
            insertPos = i;
            break;
        }
        insertPos = i + 1;
    }
    
    // 移动后面的条目为新条目腾出空间
    if (insertPos < nodeHdr->numKeys) {
        char *insertPoint = entries + insertPos * entrySize;
        char *nextPoint = entries + (insertPos + 1) * entrySize;
        int moveSize = (nodeHdr->numKeys - insertPos) * entrySize;
        memmove(nextPoint, insertPoint, moveSize);
    }
    
    // 插入新条目
    char *newEntry = entries + insertPos * entrySize;
    memcpy(newEntry, pData, indexHdr.attrLength);
    memcpy(newEntry + indexHdr.attrLength, &rid, sizeof(RID));
    
    nodeHdr->numKeys++;
    
    return 0;
}

//
// SplitLeafNode: 分裂叶子节点
//
RC IX_IndexHandle::SplitLeafNode(PageNum currentPageNum, char *nodeData, void *pData, const RID &rid,
                                bool &wasSplit, void *&newChildKey, PageNum &newChildPage) {
    RC rc;
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    // 分配新页面
    PF_PageHandle newPh;
    if ((rc = pfh->AllocatePage(newPh))) {
        return rc;
    }
    
    if ((rc = newPh.GetPageNum(newChildPage))) {
        pfh->UnpinPage(newChildPage);
        return rc;
    }
    
    char *newNodeData;
    if ((rc = newPh.GetData(newNodeData))) {
        pfh->UnpinPage(newChildPage);
        return rc;
    }
    
    // 初始化新节点头
    IX_NodeHdr *newNodeHdr = (IX_NodeHdr *)newNodeData;
    newNodeHdr->isLeaf = TRUE;
    newNodeHdr->numKeys = 0;
    newNodeHdr->parent = nodeHdr->parent;
    newNodeHdr->left = IX_NO_PAGE;
    newNodeHdr->right = nodeHdr->right;
    
    // 更新链表指针
    if (nodeHdr->right != IX_NO_PAGE) {
        // 更新原右兄弟的left指针
        PF_PageHandle rightPh;
        char *rightData;
        if (pfh->GetThisPage(nodeHdr->right, rightPh) == 0) {
            if (rightPh.GetData(rightData) == 0) {
                IX_NodeHdr *rightHdr = (IX_NodeHdr *)rightData;
                rightHdr->left = newChildPage;
                pfh->MarkDirty(nodeHdr->right);
            }
            pfh->UnpinPage(nodeHdr->right);
        }
    }
    
    // 设置新的链表关系
    nodeHdr->right = newChildPage;
    newNodeHdr->left = currentPageNum;
    
    // 创建临时数组包含所有条目（原有的加上新的）
    int entrySize = GetLeafEntrySize();
    int totalEntries = nodeHdr->numKeys + 1;
    char *tempEntries = new char[totalEntries * entrySize];
    
    // 复制原有条目并插入新条目到正确位置
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int insertPos = 0;
    bool inserted = false;
    
    for (int i = 0, j = 0; j < totalEntries; j++) {
        if (!inserted && (i >= nodeHdr->numKeys || CompareKeys(pData, entries + i * entrySize) <= 0)) {
            // 插入新条目
            memcpy(tempEntries + j * entrySize, pData, indexHdr.attrLength);
            memcpy(tempEntries + j * entrySize + indexHdr.attrLength, &rid, sizeof(RID));
            inserted = true;
        } else {
            // 复制原有条目
            memcpy(tempEntries + j * entrySize, entries + i * entrySize, entrySize);
            i++;
        }
    }
    
    // 计算分裂点
    int splitPoint = totalEntries / 2;
    
    // 更新原节点
    nodeHdr->numKeys = splitPoint;
    memcpy(entries, tempEntries, splitPoint * entrySize);
    
    // 填充新节点
    newNodeHdr->numKeys = totalEntries - splitPoint;
    char *newEntries = newNodeData + sizeof(IX_NodeHdr);
    memcpy(newEntries, tempEntries + splitPoint * entrySize, newNodeHdr->numKeys * entrySize);
    
    // 设置分裂键值（新节点的第一个键）
    memcpy(newChildKey, tempEntries + splitPoint * entrySize, indexHdr.attrLength);
    
    delete[] tempEntries;
    wasSplit = true;
    
    pfh->MarkDirty(newChildPage);
    pfh->UnpinPage(newChildPage);
    
    return 0;
}

//
// DeleteFromNode: 从B+树节点删除条目（递归）
//
RC IX_IndexHandle::DeleteFromNode(PageNum pageNum, void *pData, const RID &rid) {
    RC rc;
    PF_PageHandle ph;
    char *nodeData;
    IX_NodeHdr *nodeHdr;
    
    // 获取页面
    if ((rc = pfh->GetThisPage(pageNum, ph))) {
        return rc;
    }
    
    if ((rc = ph.GetData(nodeData))) {
        pfh->UnpinPage(pageNum);
        return rc;
    }
    
    nodeHdr = (IX_NodeHdr *)nodeData;
    
    if (nodeHdr->isLeaf) {
        // 叶子节点：直接删除
        rc = DeleteFromLeaf(nodeData, pData, rid);
        if (rc == 0) {
            pfh->MarkDirty(pageNum);
        }
    } else {
        // 内部节点：找到子节点并递归删除
        PageNum childPage;
        rc = FindChildPage(nodeData, pData, childPage);
        if (rc == 0) {
            rc = DeleteFromNode(childPage, pData, rid);
        }
    }
    
    pfh->UnpinPage(pageNum);
    return rc;
}

//
// DeleteFromLeaf: 从叶子节点删除条目
//
RC IX_IndexHandle::DeleteFromLeaf(char *nodeData, void *pData, const RID &rid) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = GetLeafEntrySize();
    
    // 查找要删除的条目
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *currentEntry = entries + i * entrySize;
        
        if (CompareKeys(pData, currentEntry) == 0) {
            // 找到匹配的键，检查RID
            RID *currentRID = (RID *)(currentEntry + indexHdr.attrLength);
            PageNum currentPageNum, ridPageNum;
            SlotNum currentSlotNum, ridSlotNum;
            
            if (currentRID->GetPageNum(currentPageNum) == 0 && 
                currentRID->GetSlotNum(currentSlotNum) == 0 &&
                rid.GetPageNum(ridPageNum) == 0 &&
                rid.GetSlotNum(ridSlotNum) == 0 &&
                currentPageNum == ridPageNum && 
                currentSlotNum == ridSlotNum) {
                
                // 找到要删除的条目，移动后面的条目
                int moveSize = (nodeHdr->numKeys - i - 1) * entrySize;
                if (moveSize > 0) {
                    char *deletePoint = currentEntry;
                    char *nextEntry = currentEntry + entrySize;
                    memmove(deletePoint, nextEntry, moveSize);
                }
                
                nodeHdr->numKeys--;
                return 0;
            }
        } else if (CompareKeys(pData, currentEntry) < 0) {
            // 键值已经超过，不会找到匹配项
            break;
        }
    }
    
    return IX_ENTRYNOTFOUND;
}

//
// CompareKeys: 比较两个键值
// 返回: <0 如果key1 < key2, 0 如果相等, >0 如果key1 > key2
//
int IX_IndexHandle::CompareKeys(void *key1, void *key2) {
    switch (indexHdr.attrType) {
        case INT:
            return (*(int *)key1 - *(int *)key2);
            
        case FLOAT:
            {
                float f1 = *(float *)key1;
                float f2 = *(float *)key2;
                if (f1 < f2) return -1;
                if (f1 > f2) return 1;
                return 0;
            }
            
        case STRING:
            return strncmp((char *)key1, (char *)key2, indexHdr.attrLength);
            
        default:
            return 0; // 不应该到达这里
    }
}

//
// 辅助函数实现
//

// 获取叶子节点的最大条目数
int IX_IndexHandle::GetMaxLeafEntries() {
    int entrySize = indexHdr.attrLength + sizeof(RID);
    int availableSpace = PF_PAGE_SIZE - sizeof(IX_NodeHdr);
    return availableSpace / entrySize;
}

int IX_IndexHandle::GetMaxInternalEntries() {
    int entrySize = indexHdr.attrLength + sizeof(PageNum);
    int availableSpace = PF_PAGE_SIZE - sizeof(IX_NodeHdr) - sizeof(PageNum); // 减去第一个指针
    return availableSpace / entrySize;
}

int IX_IndexHandle::GetLeafEntrySize() {
    return indexHdr.attrLength + sizeof(RID);
}

// 获取内部节点条目大小(键值 + 页号)
int IX_IndexHandle::GetInternalEntrySize() {
    return indexHdr.attrLength + sizeof(PageNum);
}

//
// WriteHeader: 将索引头写入磁盘
//
RC IX_IndexHandle::WriteHeader() {
    RC rc;
    PF_PageHandle ph;
    char *data;
    
    // 获取第0页（头页）
    if ((rc = pfh->GetThisPage(0, ph))) {
        return rc;
    }
    
    if ((rc = ph.GetData(data))) {
        pfh->UnpinPage(0);
        return rc;
    }
    
    // 复制头信息
    memcpy(data, &indexHdr, sizeof(IX_FileHdr));
    
    // 标记为脏页并unpin
    pfh->MarkDirty(0);
    pfh->UnpinPage(0);
    
    return 0;
}

//
// CreateNewRoot: 创建新的根节点（当原根分裂时）
//
RC IX_IndexHandle::CreateNewRoot(void *pData, PageNum leftPage, PageNum rightPage) {
    RC rc;
    PF_PageHandle ph;
    char *nodeData;
    
    // 分配新页面作为新根
    if ((rc = pfh->AllocatePage(ph))) {
        return rc;
    }
    
    PageNum newRootPage;
    if ((rc = ph.GetPageNum(newRootPage))) {
        pfh->UnpinPage(newRootPage);
        return rc;
    }
    
    if ((rc = ph.GetData(nodeData))) {
        pfh->UnpinPage(newRootPage);
        return rc;
    }
    
    // 初始化新根节点
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    nodeHdr->isLeaf = FALSE;
    nodeHdr->numKeys = 1;
    nodeHdr->parent = IX_NO_PAGE;
    nodeHdr->left = IX_NO_PAGE;
    nodeHdr->right = IX_NO_PAGE;
    
    // 设置键值和指针
    char *entries = nodeData + sizeof(IX_NodeHdr);
    
    // 第一个指针指向左子树
    *(PageNum *)entries = leftPage;
    entries += sizeof(PageNum);
    
    // 键值
    memcpy(entries, pData, indexHdr.attrLength);
    entries += indexHdr.attrLength;
    
    // 第二个指针指向右子树
    *(PageNum *)entries = rightPage;
    
    // 更新索引头中的根页面号
    indexHdr.rootPage = newRootPage;
    
    pfh->MarkDirty(newRootPage);
    pfh->UnpinPage(newRootPage);
    
    return 0;
}