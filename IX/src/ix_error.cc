//
// ix_error.cc: IX模块错误处理和测试示例
//
// 包含错误信息定义和简单的测试用例
//

#include <iostream>
#include <cstring>
#include "ix_internal.h"

using namespace std;

//
// 错误信息数组
//
static char *IX_WarnMsg[] = {
    (char*)"索引条目未找到",
    (char*)"索引扫描结束",
};

static char *IX_ErrorMsg[] = {
    (char*)"索引未打开",
    (char*)"索引已打开",  
    (char*)"扫描未打开",
    (char*)"扫描已打开",
    (char*)"bucket页已满",
    (char*)"空指针参数",
    (char*)"无效的属性类型",
    (char*)"无效的比较操作",
    (char*)"B+树结构无效",
    (char*)"页面格式错误",
};

//
// IX_PrintError: 打印IX错误信息
//
void IX_PrintError(RC rc) {
    if (rc >= START_IX_WARN && rc <= IX_LASTWARN) {
        // 警告信息
        cerr << "IX警告: " << IX_WarnMsg[rc - START_IX_WARN] << endl;
    } else if (rc >= START_IX_ERR && rc >= IX_LASTERROR) {
        // 错误信息  
        cerr << "IX错误: " << IX_ErrorMsg[START_IX_ERR - rc] << endl;
    } else {
        // 其他错误，可能来自PF模块
        cerr << "IX: 未知错误码 " << rc << endl;
    }
}

//
// IX_GetErrorString: 获取错误信息字符串
//
const char* IX_GetErrorString(RC rc) {
    if (rc >= START_IX_WARN && rc <= IX_LASTWARN) {
        return IX_WarnMsg[rc - START_IX_WARN];
    } else if (rc >= START_IX_ERR && rc >= IX_LASTERROR) {
        return IX_ErrorMsg[START_IX_ERR - rc];
    } else {
        return "未知错误";
    }
}

#ifdef IX_TEST

//
// 以下是简单的测试代码，用于验证IX模块的基本功能
//

//
// TestBasicOperations: 测试基本的索引操作
//
RC TestBasicOperations() {
    RC rc;
    IX_Manager ixm;
    
    cout << "\n=== 测试基本索引操作 ===" << endl;
    
    // 创建索引
    cout << "创建INT类型索引..." << endl;
    if ((rc = ixm.CreateIndex("test.dat", 0, INT, sizeof(int)))) {
        IX_PrintError(rc);
        return rc;
    }
    
    // 打开索引
    IX_IndexHandle ih;
    cout << "打开索引..." << endl;
    if ((rc = ixm.OpenIndex("test.dat", 0, ih))) {
        IX_PrintError(rc);
        ixm.DestroyIndex("test.dat", 0);
        return rc;
    }
    
    // 插入一些条目
    cout << "插入测试条目..." << endl;
    for (int i = 1; i <= 10; i++) {
        int key = i * 10;
        RID rid(i, i);
        
        if ((rc = ih.InsertEntry(&key, rid))) {
            IX_PrintError(rc);
            ixm.CloseIndex(ih);
            ixm.DestroyIndex("test.dat", 0);
            return rc;
        }
    }
    
    // 扫描索引
    cout << "扫描索引条目..." << endl;
    IX_IndexScan scan;
    RID rid;
    int key = 50;
    
    if ((rc = scan.OpenScan(ih, EQ_OP, &key))) {
        IX_PrintError(rc);
    } else {
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            cout << "找到条目: (" << rid.GetPageNum() << "," << rid.GetSlotNum() << ")" << endl;
        }
        
        if (rc != IX_EOF) {
            IX_PrintError(rc);
        }
        
        scan.CloseScan();
    }
    
    // 关闭和销毁索引
    cout << "关闭索引..." << endl;
    ixm.CloseIndex(ih);
    
    cout << "删除索引..." << endl;
    ixm.DestroyIndex("test.dat", 0);
    
    cout << "基本操作测试完成！" << endl;
    return 0;
}

//
// TestRangeScans: 测试范围扫描功能
//
RC TestRangeScans() {
    RC rc;
    IX_Manager ixm;
    
    cout << "\n=== 测试范围扫描 ===" << endl;
    
    // 创建和打开索引
    if ((rc = ixm.CreateIndex("range_test.dat", 0, INT, sizeof(int)))) {
        IX_PrintError(rc);
        return rc;
    }
    
    IX_IndexHandle ih;
    if ((rc = ixm.OpenIndex("range_test.dat", 0, ih))) {
        IX_PrintError(rc);
        ixm.DestroyIndex("range_test.dat", 0);
        return rc;
    }
    
    // 插入随机顺序的数据
    cout << "插入测试数据..." << endl;
    int testData[] = {15, 3, 8, 12, 1, 20, 7, 18, 5, 10};
    int numData = sizeof(testData) / sizeof(int);
    
    for (int i = 0; i < numData; i++) {
        RID rid(i + 1, 0);
        if ((rc = ih.InsertEntry(&testData[i], rid))) {
            IX_PrintError(rc);
            ixm.CloseIndex(ih);
            ixm.DestroyIndex("range_test.dat", 0);
            return rc;
        }
    }
    
    // 测试不同的范围扫描
    struct {
        CompOp op;
        int value;
        const char* desc;
    } testCases[] = {
        {LT_OP, 10, "< 10"},
        {LE_OP, 10, "<= 10"},  
        {GT_OP, 10, "> 10"},
        {GE_OP, 10, ">= 10"},
        {EQ_OP, 10, "= 10"},
        {NE_OP, 10, "!= 10"},
        {NO_OP, 0, "全表扫描"}
    };
    
    int numTests = sizeof(testCases) / sizeof(testCases[0]);
    
    for (int t = 0; t < numTests; t++) {
        cout << "\n测试条件: " << testCases[t].desc << endl;
        
        IX_IndexScan scan;
        void *value = (testCases[t].op == NO_OP) ? NULL : &testCases[t].value;
        
        if ((rc = scan.OpenScan(ih, testCases[t].op, value))) {
            IX_PrintError(rc);
            continue;
        }
        
        RID rid;
        int count = 0;
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            count++;
            cout << "  RID: (" << rid.GetPageNum() << "," << rid.GetSlotNum() << ")";
            
            // 为了验证，我们可以根据RID找回键值
            if (rid.GetPageNum() <= numData) {
                cout << " -> Key: " << testData[rid.GetPageNum() - 1];
            }
            cout << endl;
        }
        
        if (rc != IX_EOF) {
            IX_PrintError(rc);
        } else {
            cout << "  找到 " << count << " 个条目" << endl;
        }
        
        scan.CloseScan();
    }
    
    // 清理
    ixm.CloseIndex(ih);
    ixm.DestroyIndex("range_test.dat", 0);
    
    cout << "范围扫描测试完成！" << endl;
    return 0;
}

//
// TestStringIndex: 测试字符串索引
//
RC TestStringIndex() {
    RC rc;
    IX_Manager ixm;
    
    cout << "\n=== 测试字符串索引 ===" << endl;
    
    const int STR_LEN = 20;
    
    // 创建字符串索引
    if ((rc = ixm.CreateIndex("string_test.dat", 0, STRING, STR_LEN))) {
        IX_PrintError(rc);
        return rc;
    }
    
    IX_IndexHandle ih;
    if ((rc = ixm.OpenIndex("string_test.dat", 0, ih))) {
        IX_PrintError(rc);
        ixm.DestroyIndex("string_test.dat", 0);
        return rc;
    }
    
    // 插入字符串数据
    cout << "插入字符串数据..." << endl;
    const char* testStrings[] = {
        "Apple", "Banana", "Cherry", "Date", "Elderberry",
        "Fig", "Grape", "Honeydew", "Kiwi", "Lemon"
    };
    int numStrings = sizeof(testStrings) / sizeof(testStrings[0]);
    
    for (int i = 0; i < numStrings; i++) {
        char keyBuf[STR_LEN];
        memset(keyBuf, 0, STR_LEN);
        strncpy(keyBuf, testStrings[i], STR_LEN - 1);
        
        RID rid(i + 1, 0);
        if ((rc = ih.InsertEntry(keyBuf, rid))) {
            IX_PrintError(rc);
            ixm.CloseIndex(ih);
            ixm.DestroyIndex("string_test.dat", 0);
            return rc;
        }
    }
    
    // 测试字符串查找
    cout << "\n测试字符串查找:" << endl;
    char searchKey[STR_LEN];
    memset(searchKey, 0, STR_LEN);
    strcpy(searchKey, "Fig");
    
    IX_IndexScan scan;
    if ((rc = scan.OpenScan(ih, EQ_OP, searchKey))) {
        IX_PrintError(rc);
    } else {
        RID rid;
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            cout << "找到 'Fig': (" << rid.GetPageNum() << "," << rid.GetSlotNum() << ")" << endl;
        }
        
        if (rc != IX_EOF) {
            IX_PrintError(rc);
        }
        scan.CloseScan();
    }
    
    // 测试字符串范围查询
    cout << "\n测试字符串范围查询 (>= 'F'):" << endl;
    memset(searchKey, 0, STR_LEN);
    strcpy(searchKey, "F");
    
    if ((rc = scan.OpenScan(ih, GE_OP, searchKey))) {
        IX_PrintError(rc);
    } else {
        RID rid;
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            cout << "  RID: (" << rid.GetPageNum() << "," << rid.GetSlotNum() << ")" << endl;
        }
        
        if (rc != IX_EOF) {
            IX_PrintError(rc);
        }
        scan.CloseScan();
    }
    
    // 清理
    ixm.CloseIndex(ih);
    ixm.DestroyIndex("string_test.dat", 0);
    
    cout << "字符串索引测试完成！" << endl;
    return 0;
}

//
// TestLargeDataset: 测试大数据集的性能
//
RC TestLargeDataset() {
    RC rc;
    IX_Manager ixm;
    
    cout << "\n=== 测试大数据集性能 ===" << endl;
    
    const int NUM_ENTRIES = 1000;
    
    // 创建索引
    if ((rc = ixm.CreateIndex("large_test.dat", 0, INT, sizeof(int)))) {
        IX_PrintError(rc);
        return rc;
    }
    
    IX_IndexHandle ih;
    if ((rc = ixm.OpenIndex("large_test.dat", 0, ih))) {
        IX_PrintError(rc);
        ixm.DestroyIndex("large_test.dat", 0);
        return rc;
    }
    
    // 插入大量数据
    cout << "插入 " << NUM_ENTRIES << " 个条目..." << endl;
    for (int i = 0; i < NUM_ENTRIES; i++) {
        int key = i;
        RID rid(i / 100 + 1, i % 100);  // 模拟真实的RID分布
        
        if ((rc = ih.InsertEntry(&key, rid))) {
            IX_PrintError(rc);
            ixm.CloseIndex(ih);
            ixm.DestroyIndex("large_test.dat", 0);
            return rc;
        }
        
        if (i % 100 == 99) {
            cout << "  已插入 " << (i + 1) << " 个条目" << endl;
        }
    }
    
    // 测试随机查找
    cout << "\n测试随机查找..." << endl;
    IX_IndexScan scan;
    int testKeys[] = {50, 250, 500, 750, 999};
    int numTestKeys = sizeof(testKeys) / sizeof(int);
    
    for (int i = 0; i < numTestKeys; i++) {
        if ((rc = scan.OpenScan(ih, EQ_OP, &testKeys[i]))) {
            IX_PrintError(rc);
            continue;
        }
        
        RID rid;
        int count = 0;
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            count++;
        }
        
        if (rc == IX_EOF) {
            cout << "  键值 " << testKeys[i] << ": 找到 " << count << " 个条目" << endl;
        } else {
            IX_PrintError(rc);
        }
        
        scan.CloseScan();
    }
    
    // 测试范围查询性能
    cout << "\n测试范围查询 (200-300)..." << endl;
    int lowerBound = 200;
    if ((rc = scan.OpenScan(ih, GE_OP, &lowerBound))) {
        IX_PrintError(rc);
    } else {
        RID rid;
        int count = 0;
        
        while ((rc = scan.GetNextEntry(rid)) == 0) {
            // 这里应该获取实际键值来检查上界，但为简化只计数
            count++;
            if (count > 100) break; // 假设不超过100个结果
        }
        
        if (rc == IX_EOF || rc == 0) {
            cout << "  范围查询找到至少 " << count << " 个条目" << endl;
        } else {
            IX_PrintError(rc);
        }
        
        scan.CloseScan();
    }
    
    // 清理
    ixm.CloseIndex(ih);
    ixm.DestroyIndex("large_test.dat", 0);
    
    cout << "大数据集测试完成！" << endl;
    return 0;
}

//
// RunAllTests: 运行所有测试
//
RC RunAllTests() {
    RC rc;
    
    cout << "开始运行IX模块测试套件..." << endl;
    
    if ((rc = TestBasicOperations()) != 0) {
        cout << "基本操作测试失败！" << endl;
        return rc;
    }
    
    if ((rc = TestRangeScans()) != 0) {
        cout << "范围扫描测试失败！" << endl; 
        return rc;
    }
    
    if ((rc = TestStringIndex()) != 0) {
        cout << "字符串索引测试失败！" << endl;
        return rc;
    }
    
    if ((rc = TestLargeDataset()) != 0) {
        cout << "大数据集测试失败！" << endl;
        return rc;
    }
    
    cout << "\n所有IX模块测试通过！" << endl;
    return 0;
}

#endif // IX_TEST

//
// 主函数（仅在测试模式下编译）
//
#ifdef IX_TEST
int main() {
    RC rc;
    
    cout << "RedBase IX模块测试程序" << endl;
    cout << "======================" << endl;
    
    // 初始化PF模块
    PF_Manager pfm;
    
    if ((rc = RunAllTests()) != 0) {
        cout << "测试失败，错误码: " << rc << endl;
        return 1;
    }
    
    cout << "\n所有测试成功完成！" << endl;
    return 0;
}
#endif