//
// ix_btree.cc: B+树核心算法实现
//
// 这个文件包含B+树的核心算法，包括节点操作和树维护功能
//

#include <cstring>
#include <iostream>
#include "ix_internal.h"

using namespace std;

//
// InsertIntoInternal: 向内部节点插入键值-页面对
// 当子节点分裂时调用此函数
//
RC IX_IndexHandle::InsertIntoInternal(char *nodeData, void *pData, PageNum newPage,
                                     bool &wasSplit, void *&newChildKey, PageNum &newChildPage) {
    RC rc = 0;
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    // 计算内部节点的最大条目数
    int maxEntries = GetMaxInternalEntries();
    
    // 检查是否需要分裂
    if (nodeHdr->numKeys >= maxEntries) {
        // 需要分裂内部节点
        rc = SplitInternalNode(nodeData, pData, newPage, wasSplit, newChildKey, newChildPage);
    } else {
        // 直接插入到内部节点
        rc = InsertEntryIntoInternal(nodeData, pData, newPage);
        wasSplit = false;
    }
    
    return rc;
}

//
// InsertEntryIntoInternal: 在内部节点中插入条目（假设有足够空间）
//
RC IX_IndexHandle::InsertEntryIntoInternal(char *nodeData, void *pData, PageNum newPage) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = GetInternalEntrySize();
    
    // 跳过第一个页面指针
    entries += sizeof(PageNum);
    
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
    memcpy(newEntry + indexHdr.attrLength, &newPage, sizeof(PageNum));
    
    nodeHdr->numKeys++;
    
    return 0;
}

//
// SplitInternalNode: 分裂内部节点
//
RC IX_IndexHandle::SplitInternalNode(char *nodeData, void *pData, PageNum newPage,
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
    newNodeHdr->isLeaf = FALSE;
    newNodeHdr->numKeys = 0;
    newNodeHdr->parent = nodeHdr->parent;
    newNodeHdr->left = IX_NO_PAGE;  // 内部节点不使用兄弟指针
    newNodeHdr->right = IX_NO_PAGE;
    
    // 创建临时数组包含所有条目（原有的加上新的）
    int entrySize = GetInternalEntrySize();
    int totalEntries = nodeHdr->numKeys + 1;
    char *tempEntries = new char[totalEntries * entrySize];
    char *tempPages = new char[(totalEntries + 1) * sizeof(PageNum)];
    
    // 复制原有的页面指针和条目
    char *oldEntries = nodeData + sizeof(IX_NodeHdr);
    char *oldPages = oldEntries;
    oldEntries += sizeof(PageNum);
    
    // 复制第一个页面指针
    memcpy(tempPages, oldPages, sizeof(PageNum));
    
    // 复制原有条目和对应的页面指针，并插入新条目到正确位置
    int insertPos = 0;
    bool inserted = false;
    
    for (int i = 0, j = 0; j < totalEntries; j++) {
        if (!inserted && (i >= nodeHdr->numKeys || 
            CompareKeys(pData, oldEntries + i * entrySize) <= 0)) {
            // 插入新条目
            memcpy(tempEntries + j * entrySize, pData, indexHdr.attrLength);
            memcpy(tempEntries + j * entrySize + indexHdr.attrLength, &newPage, sizeof(PageNum));
            memcpy(tempPages + (j + 1) * sizeof(PageNum), &newPage, sizeof(PageNum));
            inserted = true;
        } else {
            // 复制原有条目
            memcpy(tempEntries + j * entrySize, oldEntries + i * entrySize, entrySize);
            memcpy(tempPages + (j + 1) * sizeof(PageNum), 
                   oldEntries + i * entrySize + indexHdr.attrLength, sizeof(PageNum));
            i++;
        }
    }
    
    // 计算分裂点和中间键
    int splitPoint = totalEntries / 2;
    
    // 提取中间键作为向上传播的键
    memcpy(newChildKey, tempEntries + splitPoint * entrySize, indexHdr.attrLength);
    
    // 更新原节点（包含前splitPoint个条目）
    nodeHdr->numKeys = splitPoint;
    char *originalEntries = nodeData + sizeof(IX_NodeHdr);
    
    // 复制页面指针（splitPoint + 1个）
    memcpy(originalEntries, tempPages, (splitPoint + 1) * sizeof(PageNum));
    originalEntries += sizeof(PageNum);
    
    // 复制条目（splitPoint个）
    memcpy(originalEntries, tempEntries, splitPoint * entrySize);
    
    // 填充新节点（包含剩余条目，不包括中间键）
    int rightEntries = totalEntries - splitPoint - 1;
    newNodeHdr->numKeys = rightEntries;
    char *newEntries = newNodeData + sizeof(IX_NodeHdr);
    
    // 复制页面指针（从中间键的右指针开始）
    memcpy(newEntries, tempPages + (splitPoint + 1) * sizeof(PageNum), 
           (rightEntries + 1) * sizeof(PageNum));
    newEntries += sizeof(PageNum);
    
    // 复制条目（跳过中间键）
    memcpy(newEntries, tempEntries + (splitPoint + 1) * entrySize, rightEntries * entrySize);
    
    delete[] tempEntries;
    delete[] tempPages;
    wasSplit = true;
    
    pfh->MarkDirty(newChildPage);
    pfh->UnpinPage(newChildPage);
    
    return 0;
}

//
// FindChildPage: 在内部节点中找到包含指定键值的子页面
//
RC IX_IndexHandle::FindChildPage(char *nodeData, void *pData, PageNum &childPage) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = nodeData + sizeof(IX_NodeHdr);
    int entrySize = GetInternalEntrySize();
    
    // 第一个子页面指针
    childPage = *(PageNum *)entries;
    entries += sizeof(PageNum);
    
    // 查找正确的子页面
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        char *keyPtr = entries + i * entrySize;
        
        if (CompareKeys(pData, keyPtr) < 0) {
            // 键值小于当前键，应该在当前子页面中
            return 0;
        }
        
        // 移动到下一个子页面
        childPage = *(PageNum *)(keyPtr + indexHdr.attrLength);
    }
    
    // 键值大于等于所有键，在最右边的子页面中
    return 0;
}

//
// TraverseTree: 遍历B+树（调试用）
//
RC IX_IndexHandle::TraverseTree(PageNum pageNum, int level) {
    RC rc;
    PF_PageHandle ph;
    char *nodeData;
    
    if (pageNum == IX_NO_PAGE) {
        return 0;
    }
    
    // 获取页面
    if ((rc = pfh->GetThisPage(pageNum, ph))) {
        return rc;
    }
    
    if ((rc = ph.GetData(nodeData))) {
        pfh->UnpinPage(pageNum);
        return rc;
    }
    
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    // 打印节点信息
    for (int i = 0; i < level; i++) cout << "  ";
    cout << "Page " << pageNum << " (level " << level << "): ";
    cout << (nodeHdr->isLeaf ? "LEAF" : "INTERNAL") << " with " << nodeHdr->numKeys << " keys" << endl;
    
    if (nodeHdr->isLeaf) {
        // 叶子节点：打印所有键值
        char *entries = nodeData + sizeof(IX_NodeHdr);
        int entrySize = GetLeafEntrySize();
        
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            char *entry = entries + i * entrySize;
            
            for (int j = 0; j < level + 1; j++) cout << "  ";
            cout << "Entry " << i << ": ";
            
            // 根据类型打印键值
            switch (indexHdr.attrType) {
                case INT:
                    cout << *(int *)entry;
                    break;
                case FLOAT:
                    cout << *(float *)entry;
                    break;
                case STRING:
                    {
                        char temp[256];
                        strncpy(temp, (char *)entry, indexHdr.attrLength);
                        temp[indexHdr.attrLength] = '\0';
                        cout << temp;
                    }
                    break;
            }
            
            RID *rid = (RID *)(entry + indexHdr.attrLength);
            PageNum pageNum;
            SlotNum slotNum;
            if (rid->GetPageNum(pageNum) == 0 && rid->GetSlotNum(slotNum) == 0) {
                cout << " -> (" << pageNum << "," << slotNum << ")" << endl;
            } else {
                cout << " -> (invalid RID)" << endl;
            }
        }
    } else {
        // 内部节点：递归遍历子节点
        char *entries = nodeData + sizeof(IX_NodeHdr);
        int entrySize = GetInternalEntrySize();
        
        // 第一个子指针
        PageNum firstChild = *(PageNum *)entries;
        entries += sizeof(PageNum);
        
        // 递归遍历第一个子树
        TraverseTree(firstChild, level + 1);
        
        // 遍历其余子树
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            char *entry = entries + i * entrySize;
            
            // 打印键值
            for (int j = 0; j < level + 1; j++) cout << "  ";
            cout << "Key " << i << ": ";
            
            switch (indexHdr.attrType) {
                case INT:
                    cout << *(int *)entry;
                    break;
                case FLOAT:
                    cout << *(float *)entry;
                    break;
                case STRING:
                    {
                        char temp[256];
                        strncpy(temp, (char *)entry, indexHdr.attrLength);
                        temp[indexHdr.attrLength] = '\0';
                        cout << temp;
                    }
                    break;
            }
            cout << endl;
            
            // 递归遍历右子树
            PageNum rightChild = *(PageNum *)(entry + indexHdr.attrLength);
            TraverseTree(rightChild, level + 1);
        }
    }
    
    pfh->UnpinPage(pageNum);
    return 0;
}

//
// ValidateTree: 验证B+树的完整性（调试和测试用）
//
RC IX_IndexHandle::ValidateTree(PageNum pageNum, void *minKey, void *maxKey, int &height) {
    RC rc;
    PF_PageHandle ph;
    char *nodeData;
    
    if (pageNum == IX_NO_PAGE) {
        height = 0;
        return 0;
    }
    
    // 获取页面
    if ((rc = pfh->GetThisPage(pageNum, ph))) {
        return rc;
    }
    
    if ((rc = ph.GetData(nodeData))) {
        pfh->UnpinPage(pageNum);
        return rc;
    }
    
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    
    if (nodeHdr->isLeaf) {
        // 叶子节点验证
        height = 1;
        
        // 检查键值是否有序
        char *entries = nodeData + sizeof(IX_NodeHdr);
        int entrySize = GetLeafEntrySize();
        
        for (int i = 1; i < nodeHdr->numKeys; i++) {
            char *prevEntry = entries + (i - 1) * entrySize;
            char *currEntry = entries + i * entrySize;
            
            if (CompareKeys(prevEntry, currEntry) > 0) {
                pfh->UnpinPage(pageNum);
                cout << "错误：叶子节点键值无序！" << endl;
                return IX_INVALIDTREE;
            }
        }
        
        // 检查范围约束
        if (minKey && nodeHdr->numKeys > 0) {
            char *firstEntry = entries;
            if (CompareKeys(firstEntry, minKey) < 0) {
                pfh->UnpinPage(pageNum);
                cout << "错误：叶子节点键值超出下界！" << endl;
                return IX_INVALIDTREE;
            }
        }
        
        if (maxKey && nodeHdr->numKeys > 0) {
            char *lastEntry = entries + (nodeHdr->numKeys - 1) * entrySize;
            if (CompareKeys(lastEntry, maxKey) > 0) {
                pfh->UnpinPage(pageNum);
                cout << "错误：叶子节点键值超出上界！" << endl;
                return IX_INVALIDTREE;
            }
        }
        
    } else {
        // 内部节点验证
        char *entries = nodeData + sizeof(IX_NodeHdr);
        int entrySize = GetInternalEntrySize();
        
        // 验证所有子树
        PageNum firstChild = *(PageNum *)entries;
        entries += sizeof(PageNum);
        
        int childHeight;
        rc = ValidateTree(firstChild, minKey, 
                         nodeHdr->numKeys > 0 ? entries : maxKey, childHeight);
        if (rc != 0) {
            pfh->UnpinPage(pageNum);
            return rc;
        }
        
        height = childHeight + 1;
        
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            char *keyPtr = entries + i * entrySize;
            PageNum rightChild = *(PageNum *)(keyPtr + indexHdr.attrLength);
            
            void *leftBound = keyPtr;
            void *rightBound = (i + 1 < nodeHdr->numKeys) ? 
                              entries + (i + 1) * entrySize : maxKey;
            
            int rightHeight;
            rc = ValidateTree(rightChild, leftBound, rightBound, rightHeight);
            if (rc != 0 || rightHeight != childHeight) {
                pfh->UnpinPage(pageNum);
                if (rightHeight != childHeight) {
                    cout << "错误：B+树高度不一致！" << endl;
                    return IX_INVALIDTREE;
                }
                return rc;
            }
        }
        
        // 检查键值有序性
        for (int i = 1; i < nodeHdr->numKeys; i++) {
            char *prevKey = entries + (i - 1) * entrySize;
            char *currKey = entries + i * entrySize;
            
            if (CompareKeys(prevKey, currKey) >= 0) {
                pfh->UnpinPage(pageNum);
                cout << "错误：内部节点键值无序！" << endl;
                return IX_INVALIDTREE;
            }
        }
    }
    
    pfh->UnpinPage(pageNum);
    return 0;
}

//
// PrintTree: 以可读格式打印整个B+树
//
RC IX_IndexHandle::PrintTree() {
    cout << "=== B+树结构 ===" << endl;
    cout << "属性类型: " << indexHdr.attrType << ", 长度: " << indexHdr.attrLength << endl;
    cout << "根页面: " << indexHdr.rootPage << endl;
    cout << endl;
    
    if (indexHdr.rootPage != IX_NO_PAGE) {
        TraverseTree(indexHdr.rootPage, 0);
    } else {
        cout << "空树" << endl;
    }
    
    cout << "================" << endl;
    return 0;
}