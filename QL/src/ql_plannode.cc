#include "../include/ql.h"
#include "../internal/ql_internal.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>

using namespace std;

//
// PlanNode 基类实现 - 移除，因为已在头文件中内联定义
//

//
// ScanNode 实现
//
ScanNode::ScanNode(const string &relationName, SM_Manager *sm, RM_Manager *rm) 
    : PlanNode(NODE_FILESCAN), relation(relationName), smManager(sm), rmManager(rm), isOpen(false) {
    // 获取关系的属性信息
    DataAttrInfo *attrs = nullptr;
    int nAttrs = 0;
    
    // 从 SM_Manager 获取表的属性信息
    RC rc = smManager->GetRelInfo(relation.c_str(), attrs, nAttrs);
    if (rc == OK && attrs != nullptr && nAttrs > 0) {
        // 复制属性信息到 outputAttrs
        for (int i = 0; i < nAttrs; i++) {
            outputAttrs.push_back(attrs[i]);
        }
        delete[] attrs;
    }
}


ScanNode::~ScanNode() {
    if (isOpen) {
        Close();
    }
}

RC ScanNode::Open() {
    RC rc;
    
    if (isOpen) {
        return QL_PLANOPEN;
    }
    
    // 打开关系文件
    if ((rc = rmManager->OpenFile(relation.c_str(), fileHandle))) {
        return rc;
    }
    
    // 初始化文件扫描
    if ((rc = fileScan.OpenScan(fileHandle, INT, 4, 0, NO_OP, nullptr))) {
        rmManager->CloseFile(fileHandle);
        return rc;
    }
    
    isOpen = true;
    return OK;
}

RC ScanNode::GetNext(char *data) {
    RC rc;
    
    if (!isOpen) {
        return QL_PLANNOTOPEN;
    }
    
    RM_Record record;
    if ((rc = fileScan.GetNextRec(record))) {
        return rc;
    }
    
    char *recordData;
    if ((rc = record.GetData(recordData))) {
        return rc;
    }
    
    // 复制记录数据
    memcpy(data, recordData, GetTupleLength());
    
    return OK;
}

RC ScanNode::Close() {
    RC rc = OK;
    
    if (!isOpen) {
        return QL_PLANNOTOPEN;
    }
    
    // 关闭扫描
    RC rc1 = fileScan.CloseScan();
    RC rc2 = rmManager->CloseFile(fileHandle);
    
    isOpen = false;
    
    return (rc1 != OK) ? rc1 : rc2;
}

void ScanNode::Print(int indent) {
    PrintIndent(indent);
    cout << "Scan(" << relation << ")" << endl;
}

int ScanNode::GetTupleLength() {
    // 计算元组长度：使用 outputAttrs 中的最后一个属性的 offset + length
    if (outputAttrs.empty()) {
        return 0;
    }
    
    int maxEnd = 0;
    for (const auto &attr : outputAttrs) {
        int attrEnd = attr.offset + attr.attrLength;
        if (attrEnd > maxEnd) {
            maxEnd = attrEnd;
        }
    }
    
    return maxEnd;
}


//
// SelectNode 实现
//
SelectNode::SelectNode(unique_ptr<PlanNode> child, const Condition &cond)
    : PlanNode(NODE_FILTER), childNode(std::move(child)), condition(cond) {
    // 继承子节点的输出属性
    if (childNode) {
        outputAttrs = childNode->outputAttrs;
    }
}

SelectNode::~SelectNode() {
    // unique_ptr会自动清理childNode
}

RC SelectNode::Open() {
    return childNode ? childNode->Open() : QL_NULLPOINTER;
}

RC SelectNode::GetNext(char *data) {
    RC rc;
    
    if (!childNode) {
        return QL_NULLPOINTER;
    }
    
    // 循环直到找到满足条件的记录
    while ((rc = childNode->GetNext(data)) == OK) {
        bool satisfies = false;
        
        // 评估条件
        if ((rc = EvaluateCondition(data, condition, satisfies)) != OK) {
            return rc;
        }
        
        if (satisfies) {
            return OK;
        }
    }
    
    return rc; // QL_EOF 或其他错误
}

RC SelectNode::Close() {
    return childNode ? childNode->Close() : OK;
}

void SelectNode::Print(int indent) {
    PrintIndent(indent);
    cout << "Select(";
    PrintCondition(condition);
    cout << ")" << endl;
    
    if (childNode) {
        childNode->Print(indent + 1);
    }
}

int SelectNode::GetTupleLength() {
    return childNode ? childNode->GetTupleLength() : 0;
}

RC SelectNode::EvaluateCondition(const char *data, const Condition &cond, bool &result) {
    // 获取左操作数的值
    void *lhsValue = nullptr;
    AttrType lhsType;
    if ((GetAttributeValue(data, cond.lhsAttr, lhsValue, lhsType)) != OK) {
        return QL_INVALIDCONDITION;
    }
    
    // 获取右操作数的值
    void *rhsValue = nullptr;
    AttrType rhsType;
    
    if (cond.bRhsIsAttr) {
        if ((GetAttributeValue(data, cond.rhsAttr, rhsValue, rhsType)) != OK) {
            return QL_INVALIDCONDITION;
        }
    } else {
        rhsValue = cond.rhsValue.data;
        rhsType = cond.rhsValue.type;
    }
    
    // 比较值
    return CompareValues(lhsValue, lhsType, rhsValue, rhsType, cond.op, result);
}

RC SelectNode::GetAttributeValue(const char *data, const RelAttr &attr, 
                                void *&value, AttrType &type) {
    
    // 在 outputAttrs 中查找匹配的属性
    for (const auto &attrInfo : outputAttrs) {
        // 匹配属性名（忽略表名，或者匹配表名）
        bool nameMatches = (strcmp(attrInfo.attrName, attr.attrName) == 0);
        bool relMatches = (!attr.relName || strlen(attr.relName) == 0 || 
                          strcmp(attrInfo.relName, attr.relName) == 0);
        
        
        if (nameMatches && relMatches) {
            // 找到了！从数据中提取值
            value = (void*)(data + attrInfo.offset);
            type = attrInfo.attrType;
            return OK;
        }
    }
    
    // 未找到属性
    return QL_INVALIDCONDITION;
}



RC SelectNode::CompareValues(const void *lhs, AttrType lhsType,
                            const void *rhs, AttrType rhsType,
                            CompOp op, bool &result) {
    if (lhsType != rhsType) {
        return QL_INCOMPATIBLETYPES;
    }
    
    int cmp = 0;
    
    switch (lhsType) {
        case INT: {
            int v1 = *(int*)lhs;
            int v2 = *(int*)rhs;
            cmp = (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
            break;
        }
        case FLOAT: {
            float v1 = *(float*)lhs;
            float v2 = *(float*)rhs;
            cmp = (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
            break;
        }
        case STRING: {
            cmp = strcmp((char*)lhs, (char*)rhs);
            break;
        }
    }
    
    switch (op) {
        case EQ_OP: result = (cmp == 0); break;
        case LT_OP: result = (cmp < 0); break;
        case GT_OP: result = (cmp > 0); break;
        case LE_OP: result = (cmp <= 0); break;
        case GE_OP: result = (cmp >= 0); break;
        case NE_OP: result = (cmp != 0); break;
        default: return QL_INVALIDCONDITION;
    }
    
    return OK;
}

void SelectNode::PrintCondition(const Condition &cond) {
    if (cond.lhsAttr.relName) {
        cout << cond.lhsAttr.relName << ".";
    }
    cout << cond.lhsAttr.attrName;
    
    switch (cond.op) {
        case EQ_OP: cout << " = "; break;
        case LT_OP: cout << " < "; break;
        case GT_OP: cout << " > "; break;
        case LE_OP: cout << " <= "; break;
        case GE_OP: cout << " >= "; break;
        case NE_OP: cout << " <> "; break;
    }
    
    if (cond.bRhsIsAttr) {
        if (cond.rhsAttr.relName) {
            cout << cond.rhsAttr.relName << ".";
        }
        cout << cond.rhsAttr.attrName;
    } else {
        switch (cond.rhsValue.type) {
            case INT: cout << *(int*)cond.rhsValue.data; break;
            case FLOAT: cout << *(float*)cond.rhsValue.data; break;
            case STRING: cout << "'" << (char*)cond.rhsValue.data << "'"; break;
        }
    }
}

//
// ProjectNode 实现
//
ProjectNode::ProjectNode(unique_ptr<PlanNode> child, const vector<RelAttr> &attrs)
    : PlanNode(NODE_PROJECTION), childNode(std::move(child)), projectAttrs(attrs) {
    // 设置输出属性
    outputAttrs.clear();
    for (const auto &attr : attrs) {
        // 这里需要将RelAttr转换为DataAttrInfo
        // 临时实现
        DataAttrInfo attrInfo;
        strcpy(attrInfo.relName, attr.relName ? attr.relName : "");
        strcpy(attrInfo.attrName, attr.attrName);
        outputAttrs.push_back(attrInfo);
    }
}

ProjectNode::~ProjectNode() {
    // unique_ptr会自动清理childNode
}

RC ProjectNode::Open() {
    return childNode ? childNode->Open() : QL_NULLPOINTER;
}

RC ProjectNode::GetNext(char *data) {
    RC rc;
    
    if (!childNode) {
        return QL_NULLPOINTER;
    }
    
    // 获取子节点的下一个元组
    char *childData = new char[4096]; // 临时缓冲区
    
    if ((rc = childNode->GetNext(childData)) != OK) {
        delete[] childData;
        return rc;
    }
    
    // 投影指定的属性
    rc = ProjectTuple(childData, data);
    
    delete[] childData;
    return rc;
}

RC ProjectNode::Close() {
    return childNode ? childNode->Close() : OK;
}

void ProjectNode::Print(int indent) {
    PrintIndent(indent);
    cout << "Project(";
    
    for (int i = 0; i < projectAttrs.size(); i++) {
        if (i > 0) cout << ", ";
        if (projectAttrs[i].relName) {
            cout << projectAttrs[i].relName << ".";
        }
        cout << projectAttrs[i].attrName;
    }
    
    cout << ")" << endl;
    
    if (childNode) {
        childNode->Print(indent + 1);
    }
}

int ProjectNode::GetTupleLength() {
    // 计算投影后的元组长度
    int totalLength = 0;
    for (const auto &attr : projectAttrs) {
        // 简化实现，假设每个属性4字节
        totalLength += 4; 
    }
    return totalLength;
}

RC ProjectNode::ProjectTuple(const char *inputData, char *outputData) {
    // 根据投影属性从输入元组中提取相应的属性值
    // 这需要schema信息来确定每个属性的偏移量和长度
    
    int outputOffset = 0;
    
    for (const auto &attr : projectAttrs) {
        // 找到属性在输入元组中的位置
        void *attrValue;
        AttrType attrType;
        int attrLength;
        
        if (GetAttributeFromTuple(inputData, attr, attrValue, attrType, attrLength) != OK) {
            return QL_INVALIDATTR;
        }
        
        // 复制属性值到输出元组
        memcpy(outputData + outputOffset, attrValue, attrLength);
        outputOffset += attrLength;
    }
    
    return OK;
}

RC ProjectNode::GetAttributeFromTuple(const char *tupleData, const RelAttr &attr,
                                     void *&value, AttrType &type, int &length) {
    // 根据属性名从元组中提取值
    // 这需要访问schema信息
    
    // 临时实现
    value = (void*)tupleData;
    type = INT;
    length = sizeof(int);
    
    return OK;
}

//
// JoinNode 实现
//
JoinNode::JoinNode(unique_ptr<PlanNode> left, unique_ptr<PlanNode> right,
                  const Condition &cond)
    : PlanNode(NODE_NESTLOOP), leftChild(std::move(left)), rightChild(std::move(right)), 
      joinCondition(cond), leftData(nullptr), rightData(nullptr) {
    
    // 合并左右子节点的输出属性
    if (leftChild) {
        outputAttrs.insert(outputAttrs.end(), 
                          leftChild->outputAttrs.begin(), 
                          leftChild->outputAttrs.end());
    }
    if (rightChild) {
        outputAttrs.insert(outputAttrs.end(),
                          rightChild->outputAttrs.begin(),
                          rightChild->outputAttrs.end());
    }
}

JoinNode::~JoinNode() {
    delete[] leftData;
    delete[] rightData;
}

RC JoinNode::Open() {
    RC rc;
    
    // 打开左子节点
    if (leftChild && (rc = leftChild->Open()) != OK) {
        return rc;
    }
    
    // 打开右子节点
    if (rightChild && (rc = rightChild->Open()) != OK) {
        if (leftChild) leftChild->Close();
        return rc;
    }
    
    // 分配数据缓冲区
    leftData = new char[4096];
    rightData = new char[4096];
    
    // 获取第一个左元组
    leftEOF = (leftChild->GetNext(leftData) != OK);
    
    return OK;
}

RC JoinNode::GetNext(char *data) {
    RC rc;
    
    if (!leftChild || !rightChild) {
        return QL_NULLPOINTER;
    }
    
    // 嵌套循环连接算法
    while (!leftEOF) {
        // 扫描右关系寻找匹配的元组
        while ((rc = rightChild->GetNext(rightData)) == OK) {
            bool matches = false;
            
            // 评估连接条件
            if ((rc = EvaluateJoinCondition(leftData, rightData, matches)) != OK) {
                return rc;
            }
            
            if (matches) {
                // 构造连接后的元组
                return ConcatenateTuples(leftData, rightData, data);
            }
        }
        
        // 重置右关系扫描
        rightChild->Close();
        rightChild->Open();
        
        // 获取下一个左元组
        leftEOF = (leftChild->GetNext(leftData) != OK);
    }
    
    return QL_EOF;
}

RC JoinNode::Close() {
    RC rc1 = leftChild ? leftChild->Close() : OK;
    RC rc2 = rightChild ? rightChild->Close() : OK;
    
    delete[] leftData;
    delete[] rightData;
    leftData = rightData = nullptr;
    
    return (rc1 != OK) ? rc1 : rc2;
}

void JoinNode::Print(int indent) {
    PrintIndent(indent);
    cout << "Join(";
    PrintJoinCondition(joinCondition);
    cout << ")" << endl;
    
    if (leftChild) {
        leftChild->Print(indent + 1);
    }
    if (rightChild) {
        rightChild->Print(indent + 1);
    }
}

RC JoinNode::EvaluateJoinCondition(const char *leftData, const char *rightData, bool &result) {
    // 从左元组获取左属性值
    void *lhsValue;
    AttrType lhsType;
    if (GetAttributeFromSide(leftData, true, joinCondition.lhsAttr, lhsValue, lhsType) != OK) {
        return QL_INVALIDCONDITION;
    }
    
    // 从右元组获取右属性值
    void *rhsValue;
    AttrType rhsType;
    if (joinCondition.bRhsIsAttr) {
        if (GetAttributeFromSide(rightData, false, joinCondition.rhsAttr, rhsValue, rhsType) != OK) {
            return QL_INVALIDCONDITION;
        }
    } else {
        rhsValue = joinCondition.rhsValue.data;
        rhsType = joinCondition.rhsValue.type;
    }
    
    // 比较值
    return CompareJoinValues(lhsValue, lhsType, rhsValue, rhsType, joinCondition.op, result);
}

RC JoinNode::GetAttributeFromSide(const char *tupleData, bool isLeft, const RelAttr &attr,
                                 void *&value, AttrType &type) {
    // 根据属性名和是左侧还是右侧来提取值
    // 这需要schema信息
    
    // 临时实现
    value = (void*)tupleData;
    type = INT;
    
    return OK;
}

RC JoinNode::CompareJoinValues(const void *lhs, AttrType lhsType,
                              const void *rhs, AttrType rhsType,
                              CompOp op, bool &result) {
    // 与SelectNode中的比较逻辑相同
    if (lhsType != rhsType) {
        return QL_INCOMPATIBLETYPES;
    }
    
    int cmp = 0;
    
    switch (lhsType) {
        case INT: {
            int v1 = *(int*)lhs;
            int v2 = *(int*)rhs;
            cmp = (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
            break;
        }
        case FLOAT: {
            float v1 = *(float*)lhs;
            float v2 = *(float*)rhs;
            cmp = (v1 < v2) ? -1 : (v1 > v2) ? 1 : 0;
            break;
        }
        case STRING: {
            cmp = strcmp((char*)lhs, (char*)rhs);
            break;
        }
    }
    
    switch (op) {
        case EQ_OP: result = (cmp == 0); break;
        case LT_OP: result = (cmp < 0); break;
        case GT_OP: result = (cmp > 0); break;
        case LE_OP: result = (cmp <= 0); break;
        case GE_OP: result = (cmp >= 0); break;
        case NE_OP: result = (cmp != 0); break;
        default: return QL_INVALIDCONDITION;
    }
    
    return OK;
}

RC JoinNode::ConcatenateTuples(const char *leftData, const char *rightData, char *outputData) {
    // 连接左右元组的数据
    int leftLength = GetLeftTupleLength();
    int rightLength = GetRightTupleLength();
    
    memcpy(outputData, leftData, leftLength);
    memcpy(outputData + leftLength, rightData, rightLength);
    
    return OK;
}

int JoinNode::GetTupleLength() {
    return GetLeftTupleLength() + GetRightTupleLength();
}

int JoinNode::GetLeftTupleLength() {
    // 计算左元组长度
    return leftChild ? leftChild->GetTupleLength() : 0;
}

int JoinNode::GetRightTupleLength() {
    // 计算右元组长度
    return rightChild ? rightChild->GetTupleLength() : 0;
}

void JoinNode::PrintJoinCondition(const Condition &cond) {
    if (cond.lhsAttr.relName) {
        cout << cond.lhsAttr.relName << ".";
    }
    cout << cond.lhsAttr.attrName;
    
    switch (cond.op) {
        case EQ_OP: cout << " = "; break;
        case LT_OP: cout << " < "; break;
        case GT_OP: cout << " > "; break;
        case LE_OP: cout << " <= "; break;
        case GE_OP: cout << " >= "; break;
        case NE_OP: cout << " <> "; break;
    }
    
    if (cond.bRhsIsAttr) {
        if (cond.rhsAttr.relName) {
            cout << cond.rhsAttr.relName << ".";
        }
        cout << cond.rhsAttr.attrName;
    } else {
        switch (cond.rhsValue.type) {
            case INT: cout << *(int*)cond.rhsValue.data; break;
            case FLOAT: cout << *(float*)cond.rhsValue.data; break;
            case STRING: cout << "'" << (char*)cond.rhsValue.data << "'"; break;
        }
    }
}

//
// 工具函数实现
//
void PrintIndent(int indent) {
    for (int i = 0; i < indent; i++) {
        cout << "  ";
    }
}