//
// test_ix_compile.cc: 简单的编译测试文件
//
// 这个文件用来测试IX模块的编译是否正确
//

#include "../include/ix.h"
#include "../internal/ix_internal.h"
#include "../../PF/include/pf.h"

// 简单的编译测试
int main() {
    // 测试基本类的实例化
    PF_Manager pfm;
    IX_Manager ixm(pfm);
    
    // 测试创建索引
    RC rc = ixm.CreateIndex("test.dat", 0, INT, sizeof(int));
    
    if (rc == 0) {
        // 测试打开索引
        IX_IndexHandle ih;
        rc = ixm.OpenIndex("test.dat", 0, ih);
        
        if (rc == 0) {
            // 测试插入
            int key = 10;
            RID rid(1, 1);
            rc = ih.InsertEntry(&key, rid);
            
            // 测试扫描
            IX_IndexScan scan;
            rc = scan.OpenScan(ih, EQ_OP, &key);
            
            if (rc == 0) {
                RID foundRid;
                rc = scan.GetNextEntry(foundRid);
                scan.CloseScan();
            }
            
            ixm.CloseIndex(ih);
        }
        
        ixm.DestroyIndex("test.dat", 0);
    }
    
    return 0;
}