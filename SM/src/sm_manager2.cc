#include "../include/sm.h"
#include "../internal/sm_internal.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>

using namespace std;

//
// 数据加载
// 作用：将外部数据文件中的数据加载到指定关系中，并更新相应的索引
// 比如：Load("students", "students_data.txt"); 将students_data.txt中的数据加载到students关系中
//
RC SM_Manager::Load(const char *relName, const char *fileName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    if (fileName == NULL) {
        return SM_BADFILENAME;
    }
    
    if (IsSystemCatalog(relName)) {
        return SM_SYSTEMCATALOG;
    }
    
    // 获取关系信息
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = GetRelInfo(relName, attributes, attrCount))) {
        return rc;
    }
    
    // 打开关系文件
    RM_FileHandle fileHandle;
    if ((rc = rmManager->OpenFile(relName, fileHandle))) {
        delete[] attributes;
        return rc;
    }
    
    // 打开所有索引
    IX_IndexHandle *indexHandles = new IX_IndexHandle[attrCount];
    bool *indexOpen = new bool[attrCount];
    for (int i = 0; i < attrCount; i++) {
        indexOpen[i] = false;
        if (attributes[i].indexNo != -1) {
            if ((rc = ixManager->OpenIndex(relName, attributes[i].indexNo, indexHandles[i])) == OK) {
                indexOpen[i] = true;
            }
        }
    }
    
    // 打开数据文件
    ifstream dataFile(fileName);
    if (!dataFile.is_open()) {
        rmManager->CloseFile(fileHandle);
        for (int i = 0; i < attrCount; i++) {
            if (indexOpen[i]) {
                ixManager->CloseIndex(indexHandles[i]);
            }
        }
        delete[] indexHandles;
        delete[] indexOpen;
        delete[] attributes;
        return SM_BADFILENAME;
    }
    
    // 计算元组长度
    int tupleLength = 0;
    for (int i = 0; i < attrCount; i++) {
        tupleLength += attributes[i].attrLength;
    }
    
    string line;
    int lineNum = 0;
    
    // 逐行读取数据
    while (getline(dataFile, line)) {
        lineNum++;
        
        // 跳过空行
        if (line.empty()) {
            continue;
        }
        
        // 解析行数据
        char *tupleData = new char[tupleLength];
        memset(tupleData, 0, tupleLength);
        
        stringstream ss(line);
        string token;
        int attrIndex = 0;
        
        while (getline(ss, token, ',') && attrIndex < attrCount) {
            void *valuePtr = (void*)(tupleData + attributes[attrIndex].offset);
            
            if ((rc = ParseValue(token.c_str(), attributes[attrIndex].attrType, 
                               attributes[attrIndex].attrLength, valuePtr))) {
                cout << "Warning: Parse error at line " << lineNum << ", attribute " << attrIndex << endl;
            }
            
            attrIndex++;
        }
        
        if (attrIndex != attrCount) {
            cout << "Warning: Incomplete record at line " << lineNum << endl;
            delete[] tupleData;
            continue;
        }
        
        // 插入记录
        RID rid;
        if ((rc = fileHandle.InsertRec(tupleData, rid))) {
            cout << "Error inserting record at line " << lineNum << endl;
            delete[] tupleData;
            break;
        }
        
        // 更新索引
        for (int i = 0; i < attrCount; i++) {
            if (indexOpen[i]) {
                void *keyValue = (void*)(tupleData + attributes[i].offset);
                if ((rc = indexHandles[i].InsertEntry(keyValue, rid))) {
                    cout << "Error updating index for attribute " << i << " at line " << lineNum << endl;
                }
            }
        }
        
        delete[] tupleData;
    }
    
    // 关闭文件和索引
    dataFile.close();
    rmManager->CloseFile(fileHandle);
    
    for (int i = 0; i < attrCount; i++) {
        if (indexOpen[i]) {
            ixManager->CloseIndex(indexHandles[i]);
        }
    }
    
    delete[] indexHandles;
    delete[] indexOpen;
    delete[] attributes;
    
    return OK;
}

//
// 帮助信息（显示所有关系）
//
RC SM_Manager::Help() {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    cout << "\nDatabase: " << dbName << endl;
    cout << "Relations:" << endl;
    cout << "----------" << endl;
    
    // 扫描relcat表
    RM_FileScan relScan;
    if ((rc = relScan.OpenScan(relcatFH, INT, sizeof(int), 0, NO_OP, NULL))) {
        return rc;
    }
    
    RM_Record record;
    int count = 0;
    
    while ((rc = relScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        char *data;
        record.GetData(data);
        RelcatRecord *relcatRec = (RelcatRecord*)data;
        
        cout << relcatRec->relName 
             << " (attributes: " << relcatRec->attrCount 
             << ", indexes: " << relcatRec->indexCount << ")" << endl;
        count++;
    }
    
    relScan.CloseScan();
    
    if (rc != RM_EOF && rc != OK) {
        return rc;
    }
    
    cout << "\nTotal relations: " << count << endl;
    return OK;
}

//
// 帮助信息（显示指定关系的属性）
//
RC SM_Manager::Help(const char *relName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    // 获取关系信息
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = GetRelInfo(relName, attributes, attrCount))) {
        return rc;
    }
    
    // 使用Printer类显示信息
    Printer printer(attributes, attrCount);
    
    cout << "\nRelation: " << relName << endl;
    printer.PrintHeader(cout);
    
    // 这里我们不打印实际数据，只打印属性信息
    cout << "Attributes:" << endl;
    for (int i = 0; i < attrCount; i++) {
        cout << "  " << attributes[i].attrName 
             << " (" << AttrTypeToString(attributes[i].attrType)
             << ", " << attributes[i].attrLength << " bytes"
             << ", offset " << attributes[i].offset;
        if (attributes[i].indexNo != -1) {
            cout << ", indexed";
        }
        cout << ")" << endl;
    }
    
    delete[] attributes;
    return OK;
}

//
// 打印关系内容
//
RC SM_Manager::Print(const char *relName) {
    RC rc;
    
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (relName == NULL || !IsValidName(relName)) {
        return SM_BADRELNAME;
    }
    
    // 获取关系信息
    DataAttrInfo *attributes;
    int attrCount;
    if ((rc = GetRelInfo(relName, attributes, attrCount))) {
        return rc;
    }
    
    // 打开关系文件
    RM_FileHandle fileHandle;
    if ((rc = rmManager->OpenFile(relName, fileHandle))) {
        delete[] attributes;
        return rc;
    }
    
    // 使用Printer类
    Printer printer(attributes, attrCount);
    printer.PrintHeader(cout);
    
    // 扫描所有记录
    RM_FileScan fileScan;
    if ((rc = fileScan.OpenScan(fileHandle, INT, sizeof(int), 0, NO_OP, NULL))) {
        rmManager->CloseFile(fileHandle);
        delete[] attributes;
        return rc;
    }
    
    RM_Record record;
    while ((rc = fileScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        char *data;
        record.GetData(data);
        printer.Print(cout, data);
    }
    
    fileScan.CloseScan();
    rmManager->CloseFile(fileHandle);
    
    if (rc != RM_EOF && rc != OK) {
        delete[] attributes;
        return rc;
    }
    
    printer.PrintFooter(cout);
    delete[] attributes;
    
    return OK;
}

//
// 设置系统参数
//
RC SM_Manager::Set(const char *paramName, const char *value) {
    if (!bDbOpen) {
        return SM_DBNOTOPEN;
    }
    
    if (paramName == NULL || value == NULL) {
        return SM_BADFILENAME;
    }
    
    cout << "Set parameter '" << paramName << "' to '" << value << "'" << endl;
    
    // 这里可以添加实际的参数设置逻辑
    // 例如调试级别、缓冲区大小等
    
    return OK;
}

//
// 获取关系信息
//
RC SM_Manager::GetRelInfo(const char *relName, DataAttrInfo *&attributes, int &attrCount) {
    RC rc;
    
    // 首先检查关系是否存在
    RM_FileScan relScan;
    RM_Record relRecord;
    
    if ((rc = relScan.OpenScan(relcatFH, STRING, MAXNAME+1, RELCAT_RELNAME_OFFSET,
                              EQ_OP, (void*)relName))) {
        return rc;
    }
    
    if ((rc = relScan.GetNextRec(relRecord)) != OK) {
        relScan.CloseScan();
        return (rc == RM_EOF) ? SM_RELNOTFOUND : rc;
    }
    
    char *relData;
    relRecord.GetData(relData);
    RelcatRecord *relcatRec = (RelcatRecord*)relData;
    attrCount = relcatRec->attrCount;
    
    relScan.CloseScan();
    
    // 分配属性信息数组
    attributes = new DataAttrInfo[attrCount];
    
    // 扫描attrcat获取属性信息
    RM_FileScan attrScan;
    if ((rc = attrScan.OpenScan(attrcatFH, STRING, MAXNAME+1, ATTRCAT_RELNAME_OFFSET,
                               EQ_OP, (void*)relName))) {
        delete[] attributes;
        return rc;
    }
    
    RM_Record attrRecord;
    int index = 0;
    
    while ((rc = attrScan.GetNextRec(attrRecord)) != RM_EOF && index < attrCount) {
        if (rc) {
            break;
        }
        
        char *attrData;
        attrRecord.GetData(attrData);
        AttrcatRecord *attrcatRec = (AttrcatRecord*)attrData;
        
        strcpy(attributes[index].relName, attrcatRec->relName);
        strcpy(attributes[index].attrName, attrcatRec->attrName);
        attributes[index].offset = attrcatRec->offset;
        attributes[index].attrType = attrcatRec->attrType;
        attributes[index].attrLength = attrcatRec->attrLength;
        attributes[index].indexNo = attrcatRec->indexNo;
        
        index++;
    }
    
    attrScan.CloseScan();
    
    if (rc != RM_EOF && rc != OK) {
        delete[] attributes;
        return rc;
    }
    
    if (index != attrCount) {
        delete[] attributes;
        return SM_RELNOTFOUND;
    }
    
    return OK;
}

//
// 获取属性信息
//
RC SM_Manager::GetAttrInfo(const char *relName, const char *attrName, DataAttrInfo &attr) {
    RC rc;
    
    // 扫描attrcat查找特定属性
    RM_FileScan attrScan;
    if ((rc = attrScan.OpenScan(attrcatFH, STRING, MAXNAME+1, ATTRCAT_RELNAME_OFFSET,
                               EQ_OP, (void*)relName))) {
        return rc;
    }
    
    RM_Record record;
    bool found = false;
    
    while ((rc = attrScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        char *data;
        record.GetData(data);
        AttrcatRecord *attrcatRec = (AttrcatRecord*)data;
        
        if (strcmp(attrcatRec->attrName, attrName) == 0) {
            strcpy(attr.relName, attrcatRec->relName);
            strcpy(attr.attrName, attrcatRec->attrName);
            attr.offset = attrcatRec->offset;
            attr.attrType = attrcatRec->attrType;
            attr.attrLength = attrcatRec->attrLength;
            attr.indexNo = attrcatRec->indexNo;
            found = true;
            break;
        }
    }
    
    attrScan.CloseScan();
    
    if (rc != RM_EOF && rc != OK) {
        return rc;
    }
    
    return found ? OK : SM_ATTRNOTFOUND;
}

//
// 从relcat删除记录
//
RC SM_Manager::DeleteFromRelcat(const char *relName) {
    RC rc;
    
    RM_FileScan relScan;
    if ((rc = relScan.OpenScan(relcatFH, STRING, MAXNAME+1, RELCAT_RELNAME_OFFSET,
                              EQ_OP, (void*)relName))) {
        return rc;
    }
    
    RM_Record record;
    if ((rc = relScan.GetNextRec(record)) == OK) {
        RID rid;
        record.GetRid(rid);
        relcatFH.DeleteRec(rid);
    }
    
    relScan.CloseScan();
    return (rc == RM_EOF) ? OK : rc;
}

//
// 从attrcat删除记录
//
RC SM_Manager::DeleteFromAttrcat(const char *relName) {
    RC rc;
    
    RM_FileScan attrScan;
    if ((rc = attrScan.OpenScan(attrcatFH, STRING, MAXNAME+1, ATTRCAT_RELNAME_OFFSET,
                               EQ_OP, (void*)relName))) {
        return rc;
    }
    
    RM_Record record;
    while ((rc = attrScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        RID rid;
        record.GetRid(rid);
        attrcatFH.DeleteRec(rid);
    }
    
    attrScan.CloseScan();
    return (rc == RM_EOF) ? OK : rc;
}

//
// 更新属性的索引号
//
RC SM_Manager::UpdateAttrIndexNo(const char *relName, const char *attrName, int indexNo) {
    RC rc;
    
    RM_FileScan attrScan;
    if ((rc = attrScan.OpenScan(attrcatFH, STRING, MAXNAME+1, ATTRCAT_RELNAME_OFFSET,
                               EQ_OP, (void*)relName))) {
        return rc;
    }
    
    RM_Record record;
    while ((rc = attrScan.GetNextRec(record)) != RM_EOF) {
        if (rc) {
            break;
        }
        
        char *data;
        record.GetData(data);
        AttrcatRecord *attrcatRec = (AttrcatRecord*)data;
        
        if (strcmp(attrcatRec->attrName, attrName) == 0) {
            attrcatRec->indexNo = indexNo;
            if ((rc = attrcatFH.UpdateRec(record))) {
                attrScan.CloseScan();
                return rc;
            }
            break;
        }
    }
    
    attrScan.CloseScan();
    return (rc == RM_EOF) ? OK : rc;
}