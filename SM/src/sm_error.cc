#include "../include/sm.h"
#include <iostream>
#include <cstdlib>

using namespace std;

//
// SM 错误信息打印函数
//
void SM_PrintError(RC rc) {
    switch (rc) {
        // 警告信息
        case SM_DUPLICATEREL:
            cerr << "SM Warning: Duplicate relation name" << endl;
            break;
        case SM_DUPLICATEATTR:
            cerr << "SM Warning: Duplicate attribute name" << endl;
            break;
        case SM_DUPLICATEINDEX:
            cerr << "SM Warning: Index already exists" << endl;
            break;
        case SM_RELNOTFOUND:
            cerr << "SM Warning: Relation not found" << endl;
            break;
        case SM_ATTRNOTFOUND:
            cerr << "SM Warning: Attribute not found" << endl;
            break;
        case SM_INDEXNOTFOUND:
            cerr << "SM Warning: Index not found" << endl;
            break;
            
        // 错误信息
        case SM_BADRELNAME:
            cerr << "SM Error: Invalid relation name" << endl;
            break;
        case SM_BADATTRNAME:
            cerr << "SM Error: Invalid attribute name" << endl;
            break;
        case SM_BADATTRTYPE:
            cerr << "SM Error: Invalid attribute type" << endl;
            break;
        case SM_BADATTRLENGTH:
            cerr << "SM Error: Invalid attribute length" << endl;
            break;
        case SM_TOOMANYATTRS:
            cerr << "SM Error: Too many attributes" << endl;
            break;
        case SM_DBNOTOPEN:
            cerr << "SM Error: Database not open" << endl;
            break;
        case SM_INVALIDDB:
            cerr << "SM Error: Invalid database" << endl;
            break;
        case SM_SYSTEMCATALOG:
            cerr << "SM Error: Cannot modify system catalog" << endl;
            break;
        case SM_BADFILENAME:
            cerr << "SM Error: Invalid file name" << endl;
            break;
            
        default:
            if (rc > 0) {
                cerr << "SM Warning: Unknown warning code " << rc << endl;
            } else {
                cerr << "SM Error: Unknown error code " << rc << endl;
            }
    }
}