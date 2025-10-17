#include "../include/rm.h"
#include <iostream>
#include <cstdio>

//
// RM组件错误信息打印函数
//
void RM_PrintError(RC rc) {
    switch (rc) {
        case OK:
            std::cerr << "RM: Success" << std::endl;
            break;
            
        // 警告信息（正数）
        case RM_INVALIDRID:
            std::cerr << "RM: Invalid RID" << std::endl;
            break;
        case RM_RECORDNOTFOUND:
            std::cerr << "RM: Record not found" << std::endl;
            break;
        case RM_EOF:
            std::cerr << "RM: End of file" << std::endl;
            break;
        case RM_INVALIDRECORD:
            std::cerr << "RM: Invalid record" << std::endl;
            break;
            
        // 错误信息（负数）
        case RM_RECORDSIZETOOBIG:
            std::cerr << "RM: Record size too big" << std::endl;
            break;
        case RM_FILENOTOPEN:
            std::cerr << "RM: File not open" << std::endl;
            break;
        case RM_SCANALREADYOPEN:
            std::cerr << "RM: Scan already open" << std::endl;
            break;
        case RM_SCANNOTOPEN:
            std::cerr << "RM: Scan not open" << std::endl;
            break;
        case RM_INVALIDFILE:
            std::cerr << "RM: Invalid file" << std::endl;
            break;
            
        case RM_INVALIDRID_PAGENUM:
            std::cerr << "RM: Invalid RID page number" << std::endl;
            break;
        case RM_INVALIDRID_SLOTNUM:
            std::cerr << "RM: Invalid RID slot number" << std::endl;
            break;
            
        default:
            // 可能是PF组件的错误码，调用PF的错误打印函数
            if (rc >= START_RM_WARN && rc <= END_RM_WARN) {
                std::cerr << "RM: Unknown warning code " << rc << std::endl;
            } else if (rc >= START_RM_ERR && rc <= END_RM_ERR) {
                std::cerr << "RM: Unknown error code " << rc << std::endl;
            } else {
                // 调用PF错误打印函数
                PF_PrintError(rc);
            }
            break;
    }
}