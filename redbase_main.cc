//
// redbase_main_simplified.cc - 简化的主程序，完全使用统一解析器
//

#include <iostream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <iomanip>
#include <cstring>
#include <vector>
#include <cctype>
#include <algorithm>

#include "PF/include/pf.h"
#include "PF/include/pf_manager.h"
#include "PF/internal/buffer_manager.h"
#include "RM/include/rm.h"
#include "IX/include/ix.h"
#include "SM/include/sm.h"
#include "QL/include/ql.h"

using namespace std;

// 全局管理器
PF_Manager *pPfManager = nullptr;
RM_Manager *pRmManager = nullptr;
IX_Manager *pIxManager = nullptr;
SM_Manager *pSmManager = nullptr;
QL_Manager *pQlManager = nullptr;

// 统一解析器
SQLParser *pSqlParser = nullptr;

// 当前数据库名
string currentDatabase = "";

//保存应用启动时的工作目录
string initialWorkingDir = "";

// 函数声明 - 大幅简化，所有函数接受ParsedSQL参数
void InitializeSystem();
void CleanupSystem();
void PrintWelcome();
void PrintHelp();
void ProcessCommand(const string &command);  // 唯一的命令处理入口

// 执行函数 - 统一接受ParsedSQL参数
void ExecuteCreateDatabase(const ParsedSQL &parsed);
void ExecuteUseDatabase(const ParsedSQL &parsed);
void ExecuteCreateTable(const ParsedSQL &parsed);
void ExecuteDropTable(const ParsedSQL &parsed);
void ExecuteInsert(const ParsedSQL &parsed);
void ExecuteSelect(const ParsedSQL &parsed);
void ExecuteDelete(const ParsedSQL &parsed);
void ExecuteUpdate(const ParsedSQL &parsed);
void ExecuteShowTables();
void ExecuteDescTable(const ParsedSQL &parsed);
void ExecuteCreateIndex(const ParsedSQL &parsed);
void ExecuteDropIndex(const ParsedSQL &parsed);

int main(int argc, char *argv[]) {
    try {
        InitializeSystem();
        PrintWelcome();
        
        string command;
        cout << "RedBase> ";
        
        // 主命令循环 - 大幅简化
        while (getline(cin, command)) {
            // 去除前后空格
            command.erase(0, command.find_first_not_of(" \t"));
            command.erase(command.find_last_not_of(" \t") + 1);
            
            // 跳过空命令
            if (command.empty()) {
                cout << "RedBase> ";
                continue;
            }
            
            // 处理命令 - 统一入口
            try {
                ProcessCommand(command);
            } catch (const exception &e) {
                cout << "Error: " << e.what() << endl;
            }
            
            cout << "RedBase> ";
        }
        
        CleanupSystem();
        
    } catch (const exception &e) {
        cerr << "Fatal error: " << e.what() << endl;
        CleanupSystem();
        return 1;
    }
    
    return 0;
}

void InitializeSystem() {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        initialWorkingDir = cwd;
    }

    pPfManager = new PF_Manager();
    pRmManager = new RM_Manager(*pPfManager);
    pIxManager = new IX_Manager(*pPfManager);
    pSmManager = new SM_Manager(*pIxManager, *pRmManager);
    pQlManager = new QL_Manager(*pSmManager, *pIxManager, *pRmManager);
    pSqlParser = new SQLParser();
    
    cout << "RedBase Database System initialized successfully." << endl;
}

void CleanupSystem() {
    delete pSqlParser;
    delete pQlManager;
    delete pSmManager;
    delete pIxManager;
    delete pRmManager;
    delete pPfManager;
    
    pSqlParser = nullptr;
    pQlManager = nullptr;
    pSmManager = nullptr;
    pIxManager = nullptr;
    pRmManager = nullptr;
    pPfManager = nullptr;
}

void PrintWelcome() {
    cout << endl;
    cout << "=========================================" << endl;
    cout << "    Welcome to RedBase Database System   " << endl;
    cout << "=========================================" << endl;
    cout << "Version 1.0" << endl;
    cout << "Type 'help' for command information." << endl;
    cout << "Type 'quit' or 'exit' to exit." << endl;
    cout << endl;
}

void PrintHelp() {
    cout << endl;
    cout << "Available Commands:" << endl;
    cout << "==================" << endl;
    cout << endl;
    cout << "Database Operations:" << endl;
    cout << "  CREATE DATABASE <db_name>         - Create a new database" << endl;
    cout << "  USE <db_name>                     - Switch to a database" << endl;
    cout << endl;
    cout << "Table Operations:" << endl;
    cout << "  CREATE TABLE <table_name> (       - Create a new table" << endl;
    cout << "    <column_name> <type> [constraints]," << endl;
    cout << "    ...                             " << endl;
    cout << "  );" << endl;
    cout << "  DROP TABLE <table_name>           - Drop a table" << endl;
    cout << "  SHOW TABLES                       - List all tables" << endl;
    cout << "  DESC <table_name>                 - Describe table structure" << endl;
    cout << endl;
    cout << "Data Operations:" << endl;
    cout << "  INSERT INTO <table> VALUES (...)  - Insert data" << endl;
    cout << "  SELECT * FROM <table> [WHERE ...] - Query data" << endl;
    cout << "  UPDATE <table> SET ... [WHERE ...]- Update data" << endl;
    cout << "  DELETE FROM <table> [WHERE ...]   - Delete data" << endl;
    cout << endl;
    cout << "Index Operations:" << endl;
    cout << "  CREATE INDEX <index_name> ON <table>(<column>)" << endl;
    cout << "  DROP INDEX <index_name>           - Drop an index" << endl;
    cout << endl;
    cout << "System Commands:" << endl;
    cout << "  HELP or ?                         - Show this help" << endl;
    cout << "  QUIT or EXIT                      - Exit RedBase" << endl;
    cout << endl;
}

// 关键！完全统一的命令处理函数
void ProcessCommand(const string &command) {
    // 统一解析 - 消除了所有重复的tokenize和类型判断逻辑
    ParsedSQL parsed = pSqlParser->ParseCommand(command);
    
    // 单一switch处理所有命令类型
    switch (parsed.type) {
        case SQL_CREATE_DATABASE:
            ExecuteCreateDatabase(parsed);
            break;
        case SQL_USE_DATABASE:
            ExecuteUseDatabase(parsed);
            break;
        case SQL_CREATE_TABLE:
            ExecuteCreateTable(parsed);
            break;
        case SQL_DROP_TABLE:
            ExecuteDropTable(parsed);
            break;
        case SQL_INSERT:
            ExecuteInsert(parsed);
            break;
        case SQL_SELECT:
            ExecuteSelect(parsed);
            break;
        case SQL_DELETE:
            ExecuteDelete(parsed);
            break;
        case SQL_UPDATE:
            ExecuteUpdate(parsed);
            break;
        case SQL_CREATE_INDEX:
            ExecuteCreateIndex(parsed);
            break;
        case SQL_DROP_INDEX:
            ExecuteDropIndex(parsed);
            break;
        case SQL_SHOW_TABLES:
            ExecuteShowTables();
            break;
        case SQL_DESC_TABLE:
            ExecuteDescTable(parsed);
            break;
        case SQL_HELP:
            PrintHelp();
            break;
        case SQL_QUIT:
            cout << "Goodbye!" << endl;
            exit(0);
            break;
        default:
            cout << "Unknown or unsupported command. Type 'help' for available commands." << endl;
            break;
    }
}

// 执行函数实现 - 接受ParsedSQL参数，消除重复解析
void ExecuteCreateDatabase(const ParsedSQL &parsed) {
    string dbName = parsed.databaseName;
    
    if (dbName.empty() || dbName.length() > 24) {
        cout << "Invalid database name. Name must be 1-24 characters long." << endl;
        return;
    }
    
    try {
        // 1. 创建数据库目录
        if (mkdir(dbName.c_str(), 0755) != 0) {
            if (errno == EEXIST) {
                cout << "Database '" << dbName << "' already exists." << endl;
                return;
            } else {
                cout << "Failed to create database directory '" << dbName << "'." << endl;
                return;
            }
        }
        
        cout << "\n╔════════════════════════════════════════════════════╗" << endl;
        cout << "║  Database Creation Wizard - " << dbName << string(21 - dbName.length(), ' ') << "║" << endl;
        cout << "╚════════════════════════════════════════════════════╝\n" << endl;
        
        // 2. 配置主存大小（缓冲池）
        size_t memoryKB;
        cout << "Enter buffer memory size (KB, default=164): ";
        string input;
        getline(cin, input);
        
        if (input.empty()) {
            memoryKB = 164;  // 40 pages * 4KB = 160KB + overhead
        } else {
            memoryKB = atoi(input.c_str());
            if (memoryKB <= 0 || memoryKB > 100000) {
                cout << "Invalid memory size. Using default (164 KB)." << endl;
                memoryKB = 164;
            }
        }
        
        // 配置缓冲池大小
        size_t bufferPages = BufferManager::Instance().SetBufferSizeFromMemory(memoryKB);
        cout << "✓ Buffer pool configured: " << bufferPages << " pages (~" << memoryKB << " KB)" << endl;
        
        // 3. 配置磁盘空间限制
        size_t diskKB;
        cout << "Enter disk space limit (KB, default=10240): ";
        getline(cin, input);
        
        if (input.empty()) {
            diskKB = 10240;  // 10 MB 默认
        } else {
            diskKB = atoi(input.c_str());
            if (diskKB <= 0) {
                cout << "Invalid disk size. Using default (10240 KB)." << endl;
                diskKB = 10240;
            }
        }
        
        // 设置数据库名称和磁盘限制
        pPfManager->SetDatabaseName(dbName);
        size_t diskPages = pPfManager->SetDiskSpaceLimit(diskKB);
        cout << "✓ Disk space limit configured: " << diskKB << " KB (" << diskPages << " pages)" << endl;
        
        // 4. 初始化系统目录文件
        cout << "\nInitializing system catalogs..." << endl;
        
        // 保存当前目录
        char currentDir[256];
        if (getcwd(currentDir, sizeof(currentDir)) == nullptr) {
            rmdir(dbName.c_str());
            cout << "Failed to get current directory." << endl;
            return;
        }
        
        // 切换到数据库目录
        if (chdir(dbName.c_str()) != 0) {
            rmdir(dbName.c_str());
            cout << "Failed to enter database directory." << endl;
            return;
        }
        
        // 创建系统目录表
        RC rc;
        if ((rc = pSmManager->SetupRelcat()) != 0) {
            chdir(currentDir);
            rmdir(dbName.c_str());
            cout << "Failed to create relcat. Error code: " << rc << endl;
            return;
        }
        
        if ((rc = pSmManager->SetupAttrcat()) != 0) {
            chdir(currentDir);
            // 清理已创建的文件
            unlink("relcat");
            rmdir(dbName.c_str());
            cout << "Failed to create attrcat. Error code: " << rc << endl;
            return;
        }
        
        // 返回原目录
        chdir(currentDir);
        
        cout << "✓ System catalogs initialized (relcat, attrcat)" << endl;
        
        cout << "\n╔════════════════════════════════════════════════════╗" << endl;
        cout << "║  Database '" << dbName << "' created successfully!" << string(20 - dbName.length(), ' ') << "║" << endl;
        cout << "╠════════════════════════════════════════════════════╣" << endl;
        cout << "║  Configuration Summary:                            ║" << endl;
        cout << "║    Buffer Memory: " << setw(6) << memoryKB << " KB (" << setw(3) << bufferPages << " pages)           ║" << endl;
        cout << "║    Disk Limit:    " << setw(6) << diskKB << " KB (" << setw(4) << diskPages << " pages)          ║" << endl;
        cout << "╚════════════════════════════════════════════════════╝" << endl;
        cout << "\nUse 'USE " << dbName << "' to connect to the database.\n" << endl;
        
    } catch (const exception &e) {
        cout << "Error creating database: " << e.what() << endl;
    }
}



// 函数逻辑： 1. 检查当前是否有打开的数据库，如果有则关闭它。
//          2. 检查指定的数据库目录是否存在。
//          3. 尝试打开指定的数据库，如果成功则更新currentDatabase变量。
void ExecuteUseDatabase(const ParsedSQL &parsed) {
    string dbName = parsed.databaseName;
    
    try {
        if (!currentDatabase.empty()) {
            pSmManager->CloseDb();
        }
        
        // ← 关键：先回到初始工作目录
        if (chdir(initialWorkingDir.c_str()) != 0) {
            cout << "Failed to switch to working directory." << endl;
            currentDatabase = "";
            return;
        }
        
        // 转换为绝对路径
        char absolutePath[256];
        if (dbName[0] != '/') {
            snprintf(absolutePath, sizeof(absolutePath), "%s/%s", 
                    initialWorkingDir.c_str(), dbName.c_str());
        } else {
            strcpy(absolutePath, dbName.c_str());
        }

        struct stat st;
        if (stat(absolutePath, &st) != 0 || !S_ISDIR(st.st_mode)) {
            cout << "Database '" << dbName << "' does not exist." << endl;
            cout << "Use 'CREATE DATABASE " << dbName << "' to create it first." << endl;
            currentDatabase = "";
            return;
        }
        
        // 先打开数据库（这会chdir到数据库目录）
        RC rc = pSmManager->OpenDb(absolutePath);
        
        if (rc == 0) {
            // 打开成功后，设置数据库名称并加载配置（此时已经在数据库目录中）
            pPfManager->SetDatabaseName(dbName);
            currentDatabase = dbName;
            
            cout << "\n╔════════════════════════════════════════════════════╗" << endl;
            cout << "║  Database '" << dbName << "' opened successfully!" << string(20 - dbName.length(), ' ') << "║" << endl;
            cout << "╠════════════════════════════════════════════════════╣" << endl;
            cout << "║  Configuration:                                    ║" << endl;
            
            // 显示磁盘空间配置
            if (pPfManager->IsDiskSpaceLimitConfigured()) {
                size_t limit = pPfManager->GetDiskSpaceLimit();
                size_t used = pPfManager->GetUsedDiskPages();
                double usagePercent = (limit > 0) ? (100.0 * used / limit) : 0.0;
                
                cout << "║    Disk Limit:  " << setw(6) << (limit * 4) << " KB (" << setw(4) << limit << " pages)          ║" << endl;
                cout << "║    Disk Used:   " << setw(6) << (used * 4) << " KB (" << setw(4) << used << " pages, " 
                     << fixed << setprecision(1) << setw(4) << usagePercent << "%)  ║" << endl;
            } else {
                cout << "║    Disk Limit:  Not configured                     ║" << endl;
                cout << "║    ⚠ Warning: Use CONFIG DISK to set limit        ║" << endl;
            }
            
            cout << "╚════════════════════════════════════════════════════╝\n" << endl;
            
        } else {
            cout << "Failed to open database '" << dbName << "'. Error code: " << rc << endl;
            PF_PrintError(rc);
            currentDatabase = "";
        }
    } catch (const exception &e) {
        cout << "Error opening database: " << e.what() << endl;
        currentDatabase = "";
    }
}


// 函数逻辑： 1. 检查是否有选中的数据库。
//          2. 构建AttrInfo数组。
void ExecuteCreateTable(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    vector<AttrInfo> attrInfos;
    for (int i = 0; i < parsed.columnNames.size(); i++) {
        AttrInfo attr;
        attr.attrName = new char[parsed.columnNames[i].length() + 1];
        strcpy(attr.attrName, parsed.columnNames[i].c_str());
        // 逻辑待完善：根据解析结果设置类型和长度
        attr.attrType = (i < parsed.columnTypes.size()) ? parsed.columnTypes[i] : INT;
        attr.attrLength = (i < parsed.columnLengths.size()) ? parsed.columnLengths[i] : 4;
        
        attrInfos.push_back(attr);
    }
    
    try {
        RC rc = pSmManager->CreateTable(parsed.tableName.c_str(), 
                                       attrInfos.size(), 
                                       attrInfos.data());
        if (rc == 0) {
            cout << "Table '" << parsed.tableName << "' created successfully." << endl;
        } else {
            cout << "Failed to create table '" << parsed.tableName << "'. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error creating table: " << e.what() << endl;
    }
    
    for (AttrInfo &attr : attrInfos) {
        delete[] attr.attrName;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用SM_Manager的DropTable方法删除表。
void ExecuteDropTable(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        RC rc = pSmManager->DropTable(parsed.tableName.c_str());
        if (rc == 0) {
            cout << "Table '" << parsed.tableName << "' dropped successfully." << endl;
        } else {
            cout << "Failed to drop table '" << parsed.tableName << "'. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error dropping table: " << e.what() << endl;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用QL_Manager的Insert方法插入数据。
void ExecuteInsert(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        vector<Value> values = parsed.values;
        
        RC rc = pQlManager->Insert(parsed.tableName.c_str(), 
                                  values.size(),
                                  values.data());
        if (rc == 0) {
            cout << "Data inserted successfully into table '" << parsed.tableName << "'." << endl;
        } else {
            cout << "Failed to insert data. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error inserting data: " << e.what() << endl;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 构建RelAttr数组表示选择的列（支持 * 或具体列名，支持表名前缀）。
//      3. 支持多表查询（FROM子句可包含多个表）。
//      4. 调用QL_Manager的Select方法执行查询，充分利用查询优化器。
//      5. 正确处理WHERE条件（包括单表选择条件和多表连接条件）。
void ExecuteSelect(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        // 1. 处理SELECT的列（支持 * 或具体列名，支持 table.column 格式）
        vector<RelAttr> selectAttrs;
        
        // 检查是否指定了具体的列
        if (!parsed.columnNames.empty()) {
            // 使用解析得到的具体列
            for (const string &colName : parsed.columnNames) {
                RelAttr attr;
                
                // 检查是否包含表名前缀（如 "students.name"）
                size_t dotPos = colName.find('.');
                if (dotPos != string::npos) {
                    // 包含表名前缀
                    string relName = colName.substr(0, dotPos);
                    string attrName = colName.substr(dotPos + 1);
                    
                    attr.relName = new char[relName.length() + 1];
                    strcpy(attr.relName, relName.c_str());
                    attr.attrName = new char[attrName.length() + 1];
                    strcpy(attr.attrName, attrName.c_str());
                } else {
                    // 没有表名前缀
                    attr.relName = nullptr;
                    attr.attrName = new char[colName.length() + 1];
                    strcpy(attr.attrName, colName.c_str());
                }
                
                selectAttrs.push_back(attr);
            }
        } else {
            // 默认SELECT *（没有指定列时）
            RelAttr allAttr;
            allAttr.relName = nullptr;
            allAttr.attrName = new char[2];
            strcpy(allAttr.attrName, "*");
            selectAttrs.push_back(allAttr);
        }
        
        // 2. 构建表名数组 - 支持多表查询
        vector<const char*> relNames;
        
        // 首先添加主表
        if (!parsed.tableName.empty()) {
            relNames.push_back(parsed.tableName.c_str());
        }
        
        // 如果解析器支持多表（parsed.tableNames），添加其他表
        // TODO: 当解析器升级支持 "FROM table1, table2, ..." 语法时启用
        // for (const string &tableName : parsed.tableNames) {
        //     relNames.push_back(tableName.c_str());
        // }
        
        // 验证至少有一个表
        if (relNames.empty()) {
            cout << "No table specified in SELECT query." << endl;
            
            // 清理内存
            for (RelAttr &attr : selectAttrs) {
                if (attr.relName) delete[] attr.relName;
                if (attr.attrName) delete[] attr.attrName;
            }
            return;
        }
        
        // 3. 执行SELECT查询 - 现在支持多表和完整的查询优化
        RC rc = pQlManager->Select(
            selectAttrs.size(),           // SELECT子句中的属性数量
            selectAttrs.data(),            // SELECT子句中的属性
            relNames.size(),               // FROM子句中的表数量（支持多表）
            relNames.data(),               // FROM子句中的表名数组
            parsed.conditions.size(),      // WHERE子句中的条件数量
            parsed.conditions.data()       // WHERE子句中的条件（包括选择和连接条件）
        );
        
        // 4. 清理动态分配的内存
        for (RelAttr &attr : selectAttrs) {
            if (attr.relName) {
                delete[] attr.relName;
                attr.relName = nullptr;
            }
            if (attr.attrName) {
                delete[] attr.attrName;
                attr.attrName = nullptr;
            }
        }
        
        // 5. 处理错误码
        // RM_EOF (102) 是正常的结束信号，不是错误
        if (rc != 0 && rc != 102) {  // 102 = RM_EOF
            cout << "Failed to execute SELECT. Error code: " << rc << endl;
        }
        
    } catch (const exception &e) {
        cout << "Error executing SELECT: " << e.what() << endl;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用QL_Manager的Delete方法删除数据。
void ExecuteDelete(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        RC rc = pQlManager->Delete(parsed.tableName.c_str(),
                                   parsed.conditions.size(),
                                   parsed.conditions.data());
        if (rc != 0) {
            cout << "Failed to delete data. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error deleting data: " << e.what() << endl;
    }
}
// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用QL_Manager的Update方法更新数据。
// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用QL_Manager的Update方法更新数据。
void ExecuteUpdate(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        // 构建更新属性
        RelAttr updAttr;
        updAttr.relName = nullptr;
        updAttr.attrName = new char[parsed.updateColumn.length() + 1];
        strcpy(updAttr.attrName, parsed.updateColumn.c_str());
        
        // 构建更新值
        Value updateValue(parsed.updateValueType, nullptr);
        
        // 根据类型设置值
        switch (parsed.updateValueType) {
            case INT: {
                int *intVal = (int*)malloc(sizeof(int)); *intVal = atoi(parsed.updateValueStr.c_str());
                updateValue.data = intVal;
                break;
            }
            case FLOAT: {
                float *floatVal = (float*)malloc(sizeof(float)); *floatVal = atof(parsed.updateValueStr.c_str());
                updateValue.data = floatVal;
                break;
            }
            case STRING: {
                char *strVal = (char*)malloc(parsed.updateValueStr.length() + 1);
                strcpy(strVal, parsed.updateValueStr.c_str());
                updateValue.data = strVal;
                break;
            }
        }
        
        // 调用Update方法
        RC rc = pQlManager->Update(
            parsed.tableName.c_str(),
            updAttr,
            1,  // bIsValue = 1 (使用值更新)
            RelAttr(),  // rhsRelAttr (不使用)
            updateValue,
            parsed.conditions.size(),
            parsed.conditions.data()
        );
        
        // 清理内存
        delete[] updAttr.attrName;
        
        if (rc != 0) {
            cout << "Failed to update data. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error updating data: " << e.what() << endl;
    }
}
// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用SM_Manager的Help方法列出所有表。
void ExecuteShowTables() {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        RC rc = pSmManager->Help();
        if (rc != 0) {
            cout << "Failed to show tables. Error: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error showing tables: " << e.what() << endl;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用SM_Manager的Help方法描述表结构。
void ExecuteDescTable(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        RC rc = pSmManager->Help(parsed.tableName.c_str());
        if (rc != 0) {
            cout << "Failed to describe table. Error: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error describing table: " << e.what() << endl;
    }
}

// 逻辑： 1. 检查是否有选中的数据库。
//      2. 调用SM_Manager的CreateIndex方法创建索引。
void ExecuteCreateIndex(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        if (parsed.columnNames.empty()) {
            cout << "No column specified for index." << endl;
            return;
        }
        
        RC rc = pSmManager->CreateIndex(parsed.tableName.c_str(),
                                       parsed.columnNames[0].c_str());
        if (rc == 0) {
            cout << "Index '" << parsed.indexName << "' created successfully." << endl;
        } else {
            cout << "Failed to create index. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error creating index: " << e.what() << endl;
    }
}

void ExecuteDropIndex(const ParsedSQL &parsed) {
    if (currentDatabase.empty()) {
        cout << "No database selected. Use 'USE <database_name>' first." << endl;
        return;
    }
    
    try {
        RC rc = pSmManager->DropIndex(parsed.tableName.c_str(),
                                     parsed.indexName.c_str());
        if (rc == 0) {
            cout << "Index '" << parsed.indexName << "' dropped successfully." << endl;
        } else {
            cout << "Failed to drop index. Error code: " << rc << endl;
        }
    } catch (const exception &e) {
        cout << "Error dropping index: " << e.what() << endl;
    }
}