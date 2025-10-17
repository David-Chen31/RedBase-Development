#ifndef QL_H
#define QL_H

#include "../../PF/include/pf.h"
#include "../../RM/include/redbase.h"
#include "../../RM/include/rm.h"
#include "../../IX/include/ix.h"
#include "../../SM/include/sm.h"
#include "sql_parser.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cstring>

// Forward declarations
class SM_Manager;
class IX_Manager;
class RM_Manager;

// 查询计划节点类型
enum NodeType {
    NODE_FILESCAN,      // 文件扫描
    NODE_INDEXSCAN,     // 索引扫描
    NODE_FILTER,        // 过滤条件
    NODE_PROJECTION,    // 投影
    NODE_NESTLOOP,      // 嵌套循环连接
    NODE_UPDATE,        // 更新
    NODE_DELETE         // 删除
};

// 比较操作符类型（扩展redbase.h中的CompOp）
// 注意：这里重用redbase.h中已定义的CompOp枚举

// 关系属性结构
struct RelAttr {
    char *relName;     // 关系名 (可能为NULL)
    char *attrName;    // 属性名
    
    RelAttr() : relName(nullptr), attrName(nullptr) {}
    RelAttr(const char *rel, const char *attr);
    ~RelAttr();
    RelAttr(const RelAttr &other);
    RelAttr& operator=(const RelAttr &other);
};

// 值结构
struct Value {
    AttrType type;     // 值类型
    void     *data;    // 值数据
    
    Value() : type(INT), data(nullptr) {}
    Value(AttrType t, void *d);
    ~Value();
    Value(const Value &other);
    Value& operator=(const Value &other);
};

// 条件结构
struct Condition {
    RelAttr lhsAttr;      // 左操作数属性
    CompOp  op;           // 比较操作符
    int     bRhsIsAttr;   // 右操作数是否为属性 (0/1)
    RelAttr rhsAttr;      // 右操作数属性 (如果bRhsIsAttr=1)
    Value   rhsValue;     // 右操作数值 (如果bRhsIsAttr=0)
    
    Condition() : op(NO_OP), bRhsIsAttr(0) {}
};

// 查询计划节点基类
class PlanNode {
public:
    NodeType nodeType;
    std::vector<DataAttrInfo> outputAttrs;  // 输出属性
    
    PlanNode(NodeType type) : nodeType(type) {}
    virtual ~PlanNode() {}
    virtual RC Open() = 0;
    virtual RC GetNext(char *data) = 0;
    virtual RC Close() = 0;
    virtual void Print(int indent = 0) = 0;
    virtual int GetTupleLength() = 0;
};

// 文件扫描节点
class FileScanNode : public PlanNode {
private:
    char *relName;
    RM_Manager *rmManager;
    RM_FileHandle fileHandle;
    RM_FileScan fileScan;
    bool isOpen;
    
public:
    FileScanNode(const char *relation, RM_Manager *rmm);
    ~FileScanNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
};

// 索引扫描节点
class IndexScanNode : public PlanNode {
private:
    char *relName;
    char *attrName;
    CompOp op;
    Value value;
    IX_Manager *ixManager;
    RM_Manager *rmManager;
    IX_IndexHandle indexHandle;
    IX_IndexScan indexScan;
    RM_FileHandle fileHandle;
    bool isOpen;
    
public:
    IndexScanNode(const char *relation, const char *attribute, 
                  CompOp operation, const Value &val,
                  IX_Manager *ixm, RM_Manager *rmm);
    ~IndexScanNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
};

// 过滤节点
class FilterNode : public PlanNode {
private:
    std::unique_ptr<PlanNode> child;
    std::vector<Condition> conditions;
    
public:
    FilterNode(std::unique_ptr<PlanNode> childNode, 
               const std::vector<Condition> &conds);
    ~FilterNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    
private:
    bool EvaluateConditions(const char *data);
    bool CompareValues(const void *lhs, const void *rhs, 
                      AttrType type, CompOp op);
};

// 投影节点
class ProjectionNode : public PlanNode {
private:
    std::unique_ptr<PlanNode> child;
    std::vector<RelAttr> selectAttrs;
    std::vector<int> projectionMap;  // 属性投影映射
    
public:
    ProjectionNode(std::unique_ptr<PlanNode> childNode,
                   const std::vector<RelAttr> &selAttrs);
    ~ProjectionNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    
private:
    RC SetupProjectionMap();
};

// 嵌套循环连接节点
class NestedLoopNode : public PlanNode {
private:
    std::unique_ptr<PlanNode> leftChild;
    std::unique_ptr<PlanNode> rightChild;
    std::vector<Condition> joinConditions;
    char *leftData;
    char *rightData;
    bool leftDataValid;
    
public:
    NestedLoopNode(std::unique_ptr<PlanNode> left,
                   std::unique_ptr<PlanNode> right,
                   const std::vector<Condition> &joinConds);
    ~NestedLoopNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    
private:
    bool EvaluateJoinConditions();
    RC CombineTuples(char *output);
};

//
// QL_Manager: 查询语言管理器类
//
class QL_Manager {
public:
    QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();
    
    // RQL命令接口
    RC Select(int nSelAttrs,                    // Select子句中属性数量
              const RelAttr selAttrs[],         // Select子句中的属性
              int nRelations,                   // From子句中关系数量
              const char * const relations[],  // From子句中的关系
              int nConditions,                  // Where子句中条件数量
              const Condition conditions[]);   // Where子句中的条件
              
    RC Insert(const char *relName,              // 要插入的关系
              int nValues,                      // 插入值的数量
              const Value values[]);            // 插入的值
              
    RC Delete(const char *relName,              // 要删除的关系
              int nConditions,                  // Where子句中条件数量
              const Condition conditions[]);   // Where子句中的条件
              
    RC Update(const char *relName,              // 要更新的关系
              const RelAttr &updAttr,           // 要更新的属性
              const int bIsValue,               // 右边是否为值(0/1)
              const RelAttr &rhsRelAttr,        // 右边的属性
              const Value &rhsValue,            // 右边的值
              int nConditions,                  // Where子句中条件数量
              const Condition conditions[]);   // Where子句中的条件


private:
    // 组件引用
    SM_Manager *smManager;
    IX_Manager *ixManager;
    RM_Manager *rmManager;
    SQLParser* sqlParser;
    
    // Select查询优化和执行
    RC ValidateSelectQuery(int nSelAttrs, const RelAttr selAttrs[],
                          int nRelations, const char * const relations[],
                          int nConditions, const Condition conditions[]);
                          
    std::unique_ptr<PlanNode> BuildQueryPlan(
        int nSelAttrs, const RelAttr selAttrs[],
        int nRelations, const char * const relations[],
        int nConditions, const Condition conditions[]);
        
    std::unique_ptr<PlanNode> OptimizePlan(std::unique_ptr<PlanNode> plan);
    
    RC ExecuteSelect(std::unique_ptr<PlanNode> plan,
                    int nSelAttrs, const RelAttr selAttrs[]);
    
    // Insert/Delete/Update辅助方法
    RC ValidateInsertValues(const char *relName, 
                           int nValues, const Value values[]);
                           
    RC ValidateConditions(const char *relName,
                         int nConditions, const Condition conditions[]);
                         
    RC GetRelationInfo(const char *relName,
                      DataAttrInfo *&attrs, int &nAttrs);
                      
    RC ResolveAttribute(const RelAttr &relAttr,
                       const char * const relations[], int nRelations,
                       DataAttrInfo &attrInfo);
                       
    // 工具方法
    bool HasIndex(const char *relName, const char *attrName, int &indexNo);
    RC ConvertValue(const Value &value, AttrType targetType, void *target);
    RC CompareAttrs(const void *value1, AttrType type1,
                   const void *value2, AttrType type2,
                   CompOp op, bool &result);
    void PrintIndent(int indent);
};

// 新增的查询计划节点类
class ScanNode : public PlanNode {
public:
    std::string relation;
    SM_Manager *smManager;
    RM_Manager *rmManager;
    RM_FileHandle fileHandle;
    RM_FileScan fileScan;
    bool isOpen;
    
    ScanNode(const std::string &relationName, SM_Manager *sm, RM_Manager *rm);
    ~ScanNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    int GetTupleLength() override;
};

class SelectNode : public PlanNode {
public:
    std::unique_ptr<PlanNode> childNode;
    Condition condition;
    
    SelectNode(std::unique_ptr<PlanNode> child, const Condition &cond);
    ~SelectNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    int GetTupleLength() override;
    
private:
    RC EvaluateCondition(const char *data, const Condition &cond, bool &result);
    RC GetAttributeValue(const char *data, const RelAttr &attr, void *&value, AttrType &type);
    RC CompareValues(const void *lhs, AttrType lhsType, const void *rhs, AttrType rhsType, CompOp op, bool &result);
    void PrintCondition(const Condition &cond);
};

class ProjectNode : public PlanNode {
public:
    std::unique_ptr<PlanNode> childNode;
    std::vector<RelAttr> projectAttrs;
    
    ProjectNode(std::unique_ptr<PlanNode> child, const std::vector<RelAttr> &attrs);
    ~ProjectNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    int GetTupleLength() override;
    
private:
    RC ProjectTuple(const char *inputData, char *outputData);
    RC GetAttributeFromTuple(const char *tupleData, const RelAttr &attr, void *&value, AttrType &type, int &length);
};

class JoinNode : public PlanNode {
public:
    std::unique_ptr<PlanNode> leftChild;
    std::unique_ptr<PlanNode> rightChild;
    Condition joinCondition;
    char *leftData;
    char *rightData;
    bool leftEOF;
    
    JoinNode(std::unique_ptr<PlanNode> left, std::unique_ptr<PlanNode> right, const Condition &cond);
    ~JoinNode();
    RC Open() override;
    RC GetNext(char *data) override;
    RC Close() override;
    void Print(int indent = 0) override;
    int GetTupleLength() override;
    
private:
    RC EvaluateJoinCondition(const char *leftData, const char *rightData, bool &result);
    RC GetAttributeFromSide(const char *tupleData, bool isLeft, const RelAttr &attr, void *&value, AttrType &type);
    RC CompareJoinValues(const void *lhs, AttrType lhsType, const void *rhs, AttrType rhsType, CompOp op, bool &result);
    RC ConcatenateTuples(const char *leftData, const char *rightData, char *outputData);
    int GetLeftTupleLength();
    int GetRightTupleLength();
    void PrintJoinCondition(const Condition &cond);
};

// 工具函数声明
void PrintIndent(int indent);

// 错误处理函数
void QL_PrintError(RC rc);

// QL组件返回码
#define START_QL_WARN           300
#define END_QL_WARN             399
#define START_QL_ERR           -300
#define END_QL_ERR             -399

// 警告码
#define QL_INVALIDATTR         (START_QL_WARN + 0)   // 无效属性
#define QL_INVALIDREL          (START_QL_WARN + 1)   // 无效关系
#define QL_DUPLICATEREL        (START_QL_WARN + 2)   // 重复关系
#define QL_AMBIGUOUSATTR       (START_QL_WARN + 3)   // 模糊属性
#define QL_NOSUCHTABLE         (START_QL_WARN + 4)   // 表不存在
#define QL_NOSUCHATTR          (START_QL_WARN + 5)   // 属性不存在
#define QL_DUPLICATEATTR       (START_QL_WARN + 6)   // 重复属性
#define QL_INVALIDOPERATOR     (START_QL_WARN + 7)   // 无效操作符
#define QL_PLANNOTOPEN         (START_QL_WARN + 8)   // 查询计划未打开
#define QL_PLANOPEN            (START_QL_WARN + 9)   // 查询计划已打开
#define QL_NULLPOINTER         (START_QL_WARN + 10)  // 空指针
#define QL_INVALIDATTRFORREL   (START_QL_WARN + 11)  // 关系的无效属性
#define QL_ATTRNOTFOUND        (START_QL_WARN + 12)  // 未找到属性
#define QL_LASTWARN            QL_ATTRNOTFOUND

// 错误码  
#define QL_INCOMPATIBLETYPES   (START_QL_ERR - 0)    // 类型不兼容
#define QL_INVALIDVALUECOUNT   (START_QL_ERR - 1)    // 值数量无效
#define QL_INVALIDCONDITION    (START_QL_ERR - 2)    // 无效条件
#define QL_SYSTEMCATALOG       (START_QL_ERR - 3)    // 不能修改系统目录
#define QL_EOF                 (START_QL_ERR - 4)    // 查询结束
#define QL_LASTERROR           QL_EOF

#endif // QL_H