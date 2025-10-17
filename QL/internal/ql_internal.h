#ifndef QL_INTERNAL_H
#define QL_INTERNAL_H

#include "../include/ql.h"
#include <string>
#include <vector>

// 查询计划打印控制（在parser.h中定义）
extern int bQueryPlans;

// 内部常量
#define MAX_QUERY_ATTRS    100    // 查询中最大属性数
#define MAX_JOIN_RELATIONS 10     // 最大连接关系数
#define MAX_CONDITIONS     50     // 最大条件数

// 属性解析结果
struct AttrDesc {
    char relName[MAXNAME + 1];     // 关系名
    char attrName[MAXNAME + 1];    // 属性名
    AttrType attrType;             // 属性类型
    int attrLength;                // 属性长度
    int offset;                    // 在元组中的偏移
    int indexNo;                   // 索引号 (-1表示无索引)
    
    AttrDesc() : attrType(INT), attrLength(0), 
                 offset(0), indexNo(-1) {
        relName[0] = '\0';
        attrName[0] = '\0';
    }
};

// 查询上下文
struct QueryContext {
    std::vector<std::string> relations;        // 查询中的关系列表
    std::vector<AttrDesc> allAttrs;           // 所有涉及的属性
    std::vector<Condition> conditions;        // 查询条件
    std::vector<RelAttr> selectAttrs;         // 选择的属性
    
    QueryContext() {}
};

// 条件分类
struct ConditionGroup {
    std::vector<Condition> localConditions;   // 局部选择条件 (R.A = value)
    std::vector<Condition> joinConditions;    // 连接条件 (R.A = S.B)
    std::vector<Condition> otherConditions;   // 其他条件
    
    ConditionGroup() {}
};

// 索引使用信息
struct IndexInfo {
    char relName[MAXNAME + 1];
    char attrName[MAXNAME + 1]; 
    int indexNo;
    CompOp op;
    Value value;
    bool applicable;
    
    IndexInfo() : indexNo(-1), op(NO_OP), applicable(false) {
        relName[0] = '\0';
        attrName[0] = '\0';
    }
};

// 查询优化器
class QueryOptimizer {
public:
    QueryOptimizer(SM_Manager *smm, IX_Manager *ixm, RM_Manager *rmm);
    ~QueryOptimizer();
    
    // 优化查询计划
    std::unique_ptr<PlanNode> OptimizeQuery(const QueryContext &context);
    
private:
    SM_Manager *smManager;
    IX_Manager *ixManager; 
    RM_Manager *rmManager;
    
    // 条件分析和分组
    ConditionGroup AnalyzeConditions(const std::vector<Condition> &conditions,
                                     const std::vector<std::string> &relations);
    
    // 索引使用分析
    std::vector<IndexInfo> AnalyzeIndexes(
        const std::vector<Condition> &localConditions,
        const std::vector<std::string> &relations);
    
    // 构建扫描节点
    std::unique_ptr<PlanNode> BuildScanNode(
        const std::string &relation,
        const std::vector<Condition> &localConds,
        const std::vector<IndexInfo> &indexes);
    
    // 构建连接计划
    std::unique_ptr<PlanNode> BuildJoinPlan(
        std::vector<std::unique_ptr<PlanNode>> scanNodes,
        const std::vector<Condition> &joinConditions);
    
    // 关系排序（简单的启发式）
    void OrderRelations(std::vector<std::string> &relations);
    
    // 代价估计（简化版本）
    double EstimateCost(const PlanNode *node);
    
    // 添加缺失的方法声明
    std::unique_ptr<PlanNode> BuildInitialPlan(const QueryContext &context);
    std::unique_ptr<PlanNode> ApplyOptimizations(std::unique_ptr<PlanNode> plan, const QueryContext &context);
    std::unique_ptr<PlanNode> SelectAccessPaths(std::unique_ptr<PlanNode> plan, const QueryContext &context);
    
    std::vector<std::unique_ptr<PlanNode>> ApplySelections(
        std::vector<std::unique_ptr<PlanNode>> scanNodes,
        const std::vector<Condition> &conditions);
    
    bool IsSelectionCondition(const Condition &cond, const PlanNode *node);
    std::string GetRelationName(const RelAttr &attr, const PlanNode *node);
    
    std::unique_ptr<PlanNode> ApplyJoins(
        std::vector<std::unique_ptr<PlanNode>> nodes,
        const std::vector<Condition> &conditions);
    
    bool IsJoinCondition(const Condition &cond, const std::vector<std::unique_ptr<PlanNode>> &nodes);
    bool CanJoinWith(const Condition &cond, const PlanNode *left, const PlanNode *right);
    
    std::unique_ptr<PlanNode> ApplyProjection(
        std::unique_ptr<PlanNode> plan,
        const std::vector<RelAttr> &selectAttrs);
    
    std::unique_ptr<PlanNode> PushDownSelections(std::unique_ptr<PlanNode> plan);
    std::unique_ptr<PlanNode> PushDownProjections(std::unique_ptr<PlanNode> plan);
    std::unique_ptr<PlanNode> ReorderJoins(std::unique_ptr<PlanNode> plan, const QueryContext &context);
    
    std::unique_ptr<PlanNode> SelectAccessPathsRecursive(std::unique_ptr<PlanNode> plan, const QueryContext &context);
    std::unique_ptr<PlanNode> ConsiderIndexScan(ScanNode *scanNode, const QueryContext &context);
    
    bool HasIndexForCondition(const std::string &relation, const Condition &cond);
    int EstimateRelationSize(const std::string &relation);
    double EstimateSelectivity(const Condition &cond);
    double EstimateJoinCost(const PlanNode *left, const PlanNode *right, const Condition &joinCond);
    int EstimateNodeSize(const PlanNode *node);
    
    void PrintQueryContext(const QueryContext &context);
};

// 工具函数
namespace QLUtils {
    // 字符串比较（支持不同长度）
    int CompareStrings(const char *str1, int len1, 
                      const char *str2, int len2);
    
    // 类型转换
    RC ConvertValueType(const Value &src, AttrType targetType, 
                       void *target, int targetLen);
    
    // 属性名解析
    RC ParseAttrName(const char *attrSpec, char *relName, char *attrName);
    
    // 条件验证
    RC ValidateCondition(const Condition &condition, 
                        const std::vector<AttrDesc> &attrs);
    
    // 打印工具函数
    void PrintCondition(const Condition &condition, int indent = 0);
    void PrintValue(const Value &value);
    void PrintAttrDesc(const AttrDesc &attr, int indent = 0);
}

#endif // QL_INTERNAL_H