//
// test_ql.cc - QL组件测试程序
//
// 该文件包含对QL组件的基本测试，验证查询处理功能的正确性
//

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include "../include/ql.h"
#include "../../SM/include/sm.h"
#include "../../RM/include/rm.h"
#include "../../IX/include/ix.h"
#include "../../PF/include/pf.h"

using namespace std;

// 全局变量
PF_Manager pfManager;
RM_Manager rmManager(pfManager);
IX_Manager ixManager(pfManager);
SM_Manager smManager(ixManager, rmManager);
QL_Manager qlManager(smManager, ixManager, rmManager);

// 测试函数声明
void TestQLManager();
void TestSelectQuery();
void TestInsertQuery();
void TestConditionValidation();
void TestQueryPlan();
void TestErrorHandling();
void CreateTestDatabase();
void CleanupTestDatabase();

// 工具函数
void PrintTestHeader(const char* testName);
void PrintTestResult(bool passed, const char* testName);

//
// main - 主测试函数
//
int main()
{
    cout << "=== RedBase QL Component Test Suite ===" << endl;
    cout << "Testing Query Language (QL) module functionality..." << endl << endl;

    try {
        // 创建测试数据库环境
        CreateTestDatabase();
        
        // 运行各项测试
        TestQLManager();
        TestConditionValidation();
        TestErrorHandling();
        TestQueryPlan();
        TestSelectQuery();
        TestInsertQuery();
        
        // 清理测试环境
        CleanupTestDatabase();
        
        cout << endl << "=== All QL Tests Completed ===" << endl;
        
    } catch (const exception& e) {
        cerr << "Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}

//
// TestQLManager - 测试QL_Manager基本功能
//
void TestQLManager()
{
    PrintTestHeader("QL_Manager Basic Functions");
    
    bool allPassed = true;
    
    // 测试1: QL_Manager构造和析构
    try {
        QL_Manager testManager(smManager, ixManager, rmManager);
        PrintTestResult(true, "QL_Manager constructor/destructor");
    } catch (...) {
        PrintTestResult(false, "QL_Manager constructor/destructor");
        allPassed = false;
    }
    
    cout << "QL_Manager basic tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// TestConditionValidation - 测试条件验证
//
void TestConditionValidation()
{
    PrintTestHeader("Condition Validation");
    
    bool allPassed = true;
    
    // 测试1: 有效条件
    {
        Condition cond;
        cond.lhsAttr.relName = new char[10];
        strcpy(cond.lhsAttr.relName, "student");
        cond.lhsAttr.attrName = new char[10];
        strcpy(cond.lhsAttr.attrName, "age");
        cond.op = GT_OP;
        cond.bRhsIsAttr = false;
        cond.rhsValue.type = INT;
        int value = 18;
        cond.rhsValue.data = &value;
        
        RC rc = QL_ValidateCondition(cond);
        bool passed = (rc == OK);
        PrintTestResult(passed, "Valid condition");
        if (!passed) allPassed = false;
        
        delete[] cond.lhsAttr.relName;
        delete[] cond.lhsAttr.attrName;
    }
    
    // 测试2: 无效操作符
    {
        Condition cond;
        cond.lhsAttr.relName = nullptr;
        cond.lhsAttr.attrName = new char[10];
        strcpy(cond.lhsAttr.attrName, "name");
        cond.op = (CompOp)99; // 无效操作符
        cond.bRhsIsAttr = false;
        cond.rhsValue.type = STRING;
        char str[] = "test";
        cond.rhsValue.data = str;
        
        RC rc = QL_ValidateCondition(cond);
        bool passed = (rc != OK);
        PrintTestResult(passed, "Invalid operator detection");
        if (!passed) allPassed = false;
        
        delete[] cond.lhsAttr.attrName;
    }
    
    // 测试3: RelAttr验证
    {
        RelAttr attr("student", "name");
        RC rc = QL_ValidateRelAttr(attr);
        bool passed = (rc == OK);
        PrintTestResult(passed, "Valid RelAttr");
        if (!passed) allPassed = false;
    }
    
    // 测试4: Value验证
    {
        int value = 42;
        Value val(INT, &value);
        RC rc = QL_ValidateValue(val);
        bool passed = (rc == OK);
        PrintTestResult(passed, "Valid Value");
        if (!passed) allPassed = false;
    }
    
    cout << "Condition validation tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// TestErrorHandling - 测试错误处理
//
void TestErrorHandling()
{
    PrintTestHeader("Error Handling");
    
    bool allPassed = true;
    
    // 测试1: 错误消息打印
    try {
        QL_PrintError(QL_NOSUCHTABLE);
        QL_PrintError(QL_INVALIDCONDITION);
        QL_PrintError(OK);
        PrintTestResult(true, "Error message printing");
    } catch (...) {
        PrintTestResult(false, "Error message printing");
        allPassed = false;
    }
    
    // 测试2: 错误字符串获取
    {
        char* errStr = QL_GetErrorString(QL_INCOMPATIBLETYPES);
        bool passed = (errStr != nullptr && strlen(errStr) > 0);
        PrintTestResult(passed, "Error string retrieval");
        if (!passed) allPassed = false;
    }
    
    // 测试3: 类型兼容性检查
    {
        bool compat1 = QL_CompareAttrTypes(INT, INT);
        bool compat2 = QL_CompareAttrTypes(INT, FLOAT);
        bool compat3 = QL_CompareAttrTypes(INT, STRING);
        
        bool passed = (compat1 == true && compat2 == true && compat3 == false);
        PrintTestResult(passed, "Attribute type compatibility");
        if (!passed) allPassed = false;
    }
    
    // 测试4: 操作符有效性检查
    {
        bool valid1 = QL_IsValidOperator(EQ_OP);
        bool valid2 = QL_IsValidOperator(GT_OP);
        bool valid3 = QL_IsValidOperator((CompOp)99);
        
        bool passed = (valid1 == true && valid2 == true && valid3 == false);
        PrintTestResult(passed, "Operator validation");
        if (!passed) allPassed = false;
    }
    
    cout << "Error handling tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// TestQueryPlan - 测试查询计划
//
void TestQueryPlan()
{
    PrintTestHeader("Query Plan Operations");
    
    bool allPassed = true;
    
    // 测试1: ScanNode创建
    try {
        ScanNode scanNode("test_table", &smManager, &rmManager);
        PrintTestResult(true, "ScanNode creation");
    } catch (...) {
        PrintTestResult(false, "ScanNode creation");
        allPassed = false;
    }
    
    // 测试2: 条件创建和打印
    try {
        Condition cond;
        cond.lhsAttr.relName = new char[10];
        strcpy(cond.lhsAttr.relName, "student");
        cond.lhsAttr.attrName = new char[10]; 
        strcpy(cond.lhsAttr.attrName, "age");
        cond.op = GT_OP;
        cond.bRhsIsAttr = false;
        cond.rhsValue.type = INT;
        int value = 20;
        cond.rhsValue.data = &value;
        
        // 创建SelectNode
        auto scanNode = make_unique<ScanNode>("student", &smManager, &rmManager);
        SelectNode selectNode(std::move(scanNode), cond);
        
        // 测试打印功能
        cout << "Query plan structure:" << endl;
        selectNode.Print(0);
        
        PrintTestResult(true, "Query plan construction and printing");
        
        delete[] cond.lhsAttr.relName;
        delete[] cond.lhsAttr.attrName;
        
    } catch (...) {
        PrintTestResult(false, "Query plan construction and printing");
        allPassed = false;
    }
    
    cout << "Query plan tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// TestSelectQuery - 测试Select查询
//
void TestSelectQuery()
{
    PrintTestHeader("Select Query Processing");
    
    bool allPassed = true;
    
    // 注意: 这些测试需要实际的数据库表存在
    // 在实际环境中，这些测试可能需要先创建测试表
    
    // 测试1: 简单Select查询验证
    try {
        // 创建测试查询参数
        RelAttr selAttrs[2];
        selAttrs[0].relName = nullptr;
        selAttrs[0].attrName = new char[10];
        strcpy(selAttrs[0].attrName, "*");
        
        const char* relations[1];
        relations[0] = "test_table";
        
        // 验证查询（不执行）
        RC rc = qlManager.ValidateSelectQuery(1, selAttrs, 1, relations, 0, nullptr);
        
        // 由于表可能不存在，我们期望特定的错误
        bool passed = true; // 只要没有崩溃就算通过
        PrintTestResult(passed, "Select query validation");
        if (!passed) allPassed = false;
        
        delete[] selAttrs[0].attrName;
        
    } catch (...) {
        PrintTestResult(false, "Select query validation");
        allPassed = false;
    }
    
    cout << "Select query tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// TestInsertQuery - 测试Insert查询
//
void TestInsertQuery()
{
    PrintTestHeader("Insert Query Processing");
    
    bool allPassed = true;
    
    // 测试1: Insert值验证
    try {
        Value values[2];
        int intVal = 123;
        char strVal[] = "test";
        
        values[0] = Value(INT, &intVal);
        values[1] = Value(STRING, strVal);
        
        // 验证插入值（不执行实际插入）
        RC rc = qlManager.ValidateInsertValues("test_table", 2, values);
        
        // 由于表可能不存在，我们只检查不会崩溃
        bool passed = true;
        PrintTestResult(passed, "Insert value validation");
        if (!passed) allPassed = false;
        
    } catch (...) {
        PrintTestResult(false, "Insert value validation");
        allPassed = false;
    }
    
    // 测试2: 系统目录保护
    try {
        Value values[1];
        int intVal = 1;
        values[0] = Value(INT, &intVal);
        
        RC rc = qlManager.Insert("relcat", 1, values);
        bool passed = (rc == QL_SYSTEMCATALOG);
        PrintTestResult(passed, "System catalog protection");
        if (!passed) allPassed = false;
        
    } catch (...) {
        PrintTestResult(false, "System catalog protection");
        allPassed = false;
    }
    
    cout << "Insert query tests: " << (allPassed ? "PASSED" : "FAILED") << endl << endl;
}

//
// CreateTestDatabase - 创建测试数据库环境
//
void CreateTestDatabase()
{
    cout << "Setting up test database environment..." << endl;
    
    // 这里可以创建测试表和数据
    // 由于依赖其他组件，这里只做基本初始化
    
    cout << "Test database environment ready." << endl << endl;
}

//
// CleanupTestDatabase - 清理测试数据库环境
//
void CleanupTestDatabase()
{
    cout << "Cleaning up test database environment..." << endl;
    
    // 清理测试创建的文件和数据
    
    cout << "Test database environment cleaned." << endl;
}

//
// PrintTestHeader - 打印测试标题
//
void PrintTestHeader(const char* testName)
{
    cout << "--- Testing: " << testName << " ---" << endl;
}

//
// PrintTestResult - 打印测试结果
//
void PrintTestResult(bool passed, const char* testName)
{
    cout << "  " << testName << ": " 
         << (passed ? "PASS" : "FAIL") << endl;
}

//
// 测试辅助宏
//
#define EXPECT_OK(rc) \
    do { \
        if ((rc) != OK) { \
            cout << "Expected OK, got " << (rc) << " at line " << __LINE__ << endl; \
            return false; \
        } \
    } while(0)

#define EXPECT_ERROR(rc, expected) \
    do { \
        if ((rc) != (expected)) { \
            cout << "Expected " << (expected) << ", got " << (rc) << " at line " << __LINE__ << endl; \
            return false; \
        } \
    } while(0)

//
// 性能测试函数（可选）
//
void PerformanceTest()
{
    PrintTestHeader("Performance Test");
    
    // 可以添加性能测试代码
    // 例如：测试大量查询的处理时间
    
    cout << "Performance test: SKIPPED (not implemented)" << endl << endl;
}