#include "../include/sm.h"
#include "../internal/sm_internal.h"
#include <cstring>

//
// 设置relcat表
//
RC SM_Manager::SetupRelcat() {
    RC rc;
    
    // 创建relcat文件
    if ((rc = rmManager->CreateFile(RELCAT_RELNAME, RELCAT_RECORD_SIZE))) {
        return rc;
    }
    
    // 使用成员变量文件句柄，因为InsertIntoRelcat使用成员变量
    if ((rc = rmManager->OpenFile(RELCAT_RELNAME, relcatFH))) {
        rmManager->DestroyFile(RELCAT_RELNAME);
        return rc;
    }
    
    // 为relcat本身插入记录
    if ((rc = InsertIntoRelcat(RELCAT_RELNAME, RELCAT_RECORD_SIZE, 4, 0))) {
        rmManager->CloseFile(relcatFH);
        rmManager->DestroyFile(RELCAT_RELNAME);
        return rc;
    }
    
    
    // 强制刷新到磁盘，确保数据持久化
    relcatFH.ForcePages();
    // 关闭relcat文件
    rmManager->CloseFile(relcatFH);
    
    return OK;
}

//
// 设置attrcat表
//
RC SM_Manager::SetupAttrcat() {
    RC rc;
    
    // 创建attrcat文件
    if ((rc = rmManager->CreateFile(ATTRCAT_RELNAME, ATTRCAT_RECORD_SIZE))) {
        return rc;
    }
    
    // 使用成员变量文件句柄，因为InsertIntoAttrcat使用成员变量
    if ((rc = rmManager->OpenFile(ATTRCAT_RELNAME, attrcatFH))) {
        rmManager->DestroyFile(ATTRCAT_RELNAME);
        return rc;
    }
    
    // 需要重新打开relcat以便调用InsertIntoRelcat
    if ((rc = rmManager->OpenFile(RELCAT_RELNAME, relcatFH))) {
        rmManager->CloseFile(attrcatFH);
        rmManager->DestroyFile(ATTRCAT_RELNAME);
        return rc;
    }
    
    // 为attrcat本身插入记录
    if ((rc = InsertIntoRelcat(ATTRCAT_RELNAME, ATTRCAT_RECORD_SIZE, 6, 0))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        rmManager->DestroyFile(ATTRCAT_RELNAME);
        return rc;
    }
    
    // 为relcat的属性插入记录到attrcat
    if ((rc = InsertIntoAttrcat(RELCAT_RELNAME, "relName", 
                               RELCAT_RELNAME_OFFSET, STRING, MAXNAME+1, -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(RELCAT_RELNAME, "tupleLength", 
                               RELCAT_TUPLELENGTH_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(RELCAT_RELNAME, "attrCount", 
                               RELCAT_ATTRCOUNT_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(RELCAT_RELNAME, "indexCount", 
                               RELCAT_INDEXCOUNT_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    // 为attrcat的属性插入记录到attrcat
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "relName", 
                               ATTRCAT_RELNAME_OFFSET, STRING, MAXNAME+1, -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "attrName", 
                               ATTRCAT_ATTRNAME_OFFSET, STRING, MAXNAME+1, -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "offset", 
                               ATTRCAT_OFFSET_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "attrType", 
                               ATTRCAT_ATTRTYPE_OFFSET, INT, sizeof(AttrType), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "attrLength", 
                               ATTRCAT_ATTRLENGTH_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    if ((rc = InsertIntoAttrcat(ATTRCAT_RELNAME, "indexNo", 
                               ATTRCAT_INDEXNO_OFFSET, INT, sizeof(int), -1))) {
        rmManager->CloseFile(relcatFH);
        rmManager->CloseFile(attrcatFH);
        return rc;
    }
    
    
    // 强制刷新到磁盘，确保数据持久化
    relcatFH.ForcePages();
    attrcatFH.ForcePages();
    // 关闭两个文件
    rmManager->CloseFile(relcatFH);
    rmManager->CloseFile(attrcatFH);
    
    return OK;
}
