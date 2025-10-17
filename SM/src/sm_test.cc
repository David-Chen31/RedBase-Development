#include "SM/include/sm.h"
#include <iostream>
#include <cstring>

using namespace std;

int main() {
    RC rc;
    
    // 初始化组件
    PF_Manager pfm;
    RM_Manager rmm(pfm);
    IX_Manager ixm(pfm);
    SM_Manager smm(ixm, rmm);
    
    cout << "=== SM Component Test ===" << endl;
    
    // 测试数据库操作
    cout << "\n1. Testing database operations..." << endl;
    
    // 这里测试需要先创建数据库目录
    system("mkdir testdb");
    system("cd testdb");
    
    // 设置系统目录
    if ((rc = smm.SetupRelcat())) {
        SM_PrintError(rc);
        return 1;
    }
    
    if ((rc = smm.SetupAttrcat())) {
        SM_PrintError(rc);
        return 1;
    }
    
    if ((rc = smm.OpenDb("testdb"))) {
        SM_PrintError(rc);
        return 1;
    }
    
    cout << "Database operations successful." << endl;
    
    // 测试表创建
    cout << "\n2. Testing table creation..." << endl;
    
    AttrInfo attrs[3];
    
    // 定义属性
    char attr1[] = "id";
    char attr2[] = "name";
    char attr3[] = "age";
    
    attrs[0].attrName = attr1;
    attrs[0].attrType = INT;
    attrs[0].attrLength = 4;
    
    attrs[1].attrName = attr2;
    attrs[1].attrType = STRING;
    attrs[1].attrLength = 20;
    
    attrs[2].attrName = attr3;
    attrs[2].attrType = INT;
    attrs[2].attrLength = 4;
    
    if ((rc = smm.CreateTable("student", 3, attrs))) {
        SM_PrintError(rc);
    } else {
        cout << "Table creation successful." << endl;
    }
    
    // 测试帮助命令
    cout << "\n3. Testing help commands..." << endl;
    
    cout << "\nAll relations:" << endl;
    if ((rc = smm.Help())) {
        SM_PrintError(rc);
    }
    
    cout << "\nStudent relation:" << endl;
    if ((rc = smm.Help("student"))) {
        SM_PrintError(rc);
    }
    
    // 测试索引创建
    cout << "\n4. Testing index creation..." << endl;
    
    if ((rc = smm.CreateIndex("student", "id"))) {
        SM_PrintError(rc);
    } else {
        cout << "Index creation successful." << endl;
    }
    
    // 关闭数据库
    if ((rc = smm.CloseDb())) {
        SM_PrintError(rc);
        return 1;
    }
    
    cout << "\n=== SM Component Test Complete ===" << endl;
    
    return 0;
}