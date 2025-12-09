//
// ix_visualizer.cc - B+树索引可视化工具
//
// 用法: ix_visualizer <index_file>
// 示例: ix_visualizer world/students.0
//

#include <iostream>
#include <iomanip>
#include <cstring>
#include "../IX/internal/ix_internal.h"
#include "../PF/include/pf.h"

using namespace std;

// 打印页面头信息
void PrintPageHeader(const IX_NodeHdr *hdr, PageNum pageNum) {
    cout << "\n╔════════════════════════════════════════════════════╗" << endl;
    cout << "║  Page " << setw(5) << pageNum << " - ";
    if (hdr->isLeaf) {
        cout << "LEAF NODE";
    } else {
        cout << "INTERNAL NODE";
    }
    cout << setw(28) << " ║" << endl;
    cout << "╠════════════════════════════════════════════════════╣" << endl;
    cout << "║  Keys: " << setw(5) << hdr->numKeys;
    cout << "    Parent: " << setw(5) << hdr->parent;
    cout << "    Next: " << setw(5) << hdr->right << "         ║" << endl;
    cout << "╚════════════════════════════════════════════════════╝" << endl;
}

// 打印整数键
void PrintIntKeys(const char *nodeData, const IX_FileHdr *fileHdr) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = (char *)(nodeData + sizeof(IX_NodeHdr));
    
    cout << "Keys: ";
    if (nodeHdr->isLeaf) {
        int entrySize = fileHdr->attrLength + sizeof(RID);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            int *key = (int *)(entries + i * entrySize);
            cout << *key;
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    } else {
        int entrySize = fileHdr->attrLength + sizeof(PageNum);
        // 跳过第一个指针
        entries += sizeof(PageNum);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            int *key = (int *)(entries + i * entrySize);
            cout << *key;
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    }
    cout << endl;
}

// 打印浮点数键
void PrintFloatKeys(const char *nodeData, const IX_FileHdr *fileHdr) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = (char *)(nodeData + sizeof(IX_NodeHdr));
    
    cout << "Keys: ";
    if (nodeHdr->isLeaf) {
        int entrySize = fileHdr->attrLength + sizeof(RID);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            float *key = (float *)(entries + i * entrySize);
            cout << *key;
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    } else {
        int entrySize = fileHdr->attrLength + sizeof(PageNum);
        entries += sizeof(PageNum);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            float *key = (float *)(entries + i * entrySize);
            cout << *key;
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    }
    cout << endl;
}

// 打印字符串键
void PrintStringKeys(const char *nodeData, const IX_FileHdr *fileHdr) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    char *entries = (char *)(nodeData + sizeof(IX_NodeHdr));
    
    cout << "Keys: ";
    if (nodeHdr->isLeaf) {
        int entrySize = fileHdr->attrLength + sizeof(RID);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            char *key = entries + i * entrySize;
            cout << "\"" << key << "\"";
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    } else {
        int entrySize = fileHdr->attrLength + sizeof(PageNum);
        entries += sizeof(PageNum);
        for (int i = 0; i < nodeHdr->numKeys; i++) {
            char *key = entries + i * entrySize;
            cout << "\"" << key << "\"";
            if (i < nodeHdr->numKeys - 1) cout << ", ";
        }
    }
    cout << endl;
}

// 打印子页面指针（内部节点）
void PrintChildren(const char *nodeData, const IX_FileHdr *fileHdr) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    if (nodeHdr->isLeaf) return;
    
    char *entries = (char *)(nodeData + sizeof(IX_NodeHdr));
    int entrySize = fileHdr->attrLength + sizeof(PageNum);
    
    cout << "Children: ";
    // 第一个指针
    PageNum *firstPtr = (PageNum *)entries;
    cout << *firstPtr;
    
    // 后续指针
    entries += sizeof(PageNum);
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        PageNum *childPtr = (PageNum *)(entries + i * entrySize + fileHdr->attrLength);
        cout << ", " << *childPtr;
    }
    cout << endl;
}

// 打印RID列表（叶子节点）
void PrintRIDs(const char *nodeData, const IX_FileHdr *fileHdr) {
    IX_NodeHdr *nodeHdr = (IX_NodeHdr *)nodeData;
    if (!nodeHdr->isLeaf) return;
    
    char *entries = (char *)(nodeData + sizeof(IX_NodeHdr));
    int entrySize = fileHdr->attrLength + sizeof(RID);
    
    cout << "RIDs:" << endl;
    for (int i = 0; i < nodeHdr->numKeys; i++) {
        RID *rid = (RID *)(entries + i * entrySize + fileHdr->attrLength);
        PageNum pageNum;
        SlotNum slotNum;
        rid->GetPageNum(pageNum);
        rid->GetSlotNum(slotNum);
        cout << "  [" << i << "] -> (page:" << pageNum << ", slot:" << slotNum << ")" << endl;
    }
}

// 主函数
int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <index_file>" << endl;
        cout << "Example: " << argv[0] << " world/students.0" << endl;
        return 1;
    }
    
    const char *indexFile = argv[1];
    
    try {
        // 打开索引文件
        PF_Manager pfm;
        PF_FileHandle fh;
        
        RC rc = pfm.OpenFile(indexFile, fh);
        if (rc != 0) {
            cout << "Error: Cannot open index file '" << indexFile << "'" << endl;
            return 1;
        }
        
        // 读取文件头
        PF_PageHandle ph;
        rc = fh.GetThisPage(0, ph);
        if (rc != 0) {
            cout << "Error: Cannot read file header" << endl;
            pfm.CloseFile(fh);
            return 1;
        }
        
        char *headerData;
        ph.GetData(headerData);
        IX_FileHdr *fileHdr = (IX_FileHdr *)headerData;
        
        // 打印索引元信息
        cout << "\n╔════════════════════════════════════════════════════╗" << endl;
        cout << "║          B+ Tree Index Visualization              ║" << endl;
        cout << "╠════════════════════════════════════════════════════╣" << endl;
        cout << "║  File: " << left << setw(43) << indexFile << " ║" << endl;
        cout << "║  Attribute Type: ";
        switch (fileHdr->attrType) {
            case INT: cout << "INT"; break;
            case FLOAT: cout << "FLOAT"; break;
            case STRING: cout << "STRING"; break;
        }
        cout << setw(33) << " " << "║" << endl;
        cout << "║  Attribute Length: " << setw(29) << fileHdr->attrLength << " ║" << endl;
        cout << "║  Root Page: " << setw(36) << fileHdr->rootPage << " ║" << endl;
        cout << "╚════════════════════════════════════════════════════╝" << endl;
        
        fh.UnpinPage(0);
        
        // 遍历所有页面
        int pageCount = 0;
        for (PageNum pageNum = 1; ; pageNum++) {
            rc = fh.GetThisPage(pageNum, ph);
            if (rc != 0) break;
            
            char *pageData;
            ph.GetData(pageData);
            IX_NodeHdr *nodeHdr = (IX_NodeHdr *)pageData;
            
            PrintPageHeader(nodeHdr, pageNum);
            
            // 根据类型打印键
            switch (fileHdr->attrType) {
                case INT:
                    PrintIntKeys(pageData, fileHdr);
                    break;
                case FLOAT:
                    PrintFloatKeys(pageData, fileHdr);
                    break;
                case STRING:
                    PrintStringKeys(pageData, fileHdr);
                    break;
            }
            
            // 打印指针信息
            if (nodeHdr->isLeaf) {
                PrintRIDs(pageData, fileHdr);
            } else {
                PrintChildren(pageData, fileHdr);
            }
            
            fh.UnpinPage(pageNum);
            pageCount++;
        }
        
        cout << "\n总计 " << pageCount << " 个页面" << endl;
        
        pfm.CloseFile(fh);
        
    } catch (const exception &e) {
        cout << "Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
