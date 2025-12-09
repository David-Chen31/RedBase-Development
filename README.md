This is a personal database development project referring to redbase. Welcome to browse!

注意本项目暂时只支持INT、FLOAT、STRING三种类型

## 新增功能：语法分析树输出

### 功能概述

RedBase2.0 现在支持输出SQL语句的语法分析树，方便学习和调试。

### 快速开始

1. **启用语法树输出**：
   ```sql
   SET PRINT_PARSE_TREE = ON
   ```

2. **执行SQL语句**：
   ```sql
   SELECT * FROM students WHERE age > 18
   ```
   
   将显示：
   ```
   === Syntax Parse Tree ===
   Statement Type: SELECT
   Selected Columns:
     - * (all columns)
   From Table: students
   Where Conditions:
     Condition 1:
       Left: age
       Operator: >
       Right: 18 (INT)
   =========================
   ```

3. **禁用输出**：
   ```sql
   SET PRINT_PARSE_TREE = OFF
   ```

### 查看详细文档

查看 `PARSE_TREE_FEATURE.md` 获取完整功能说明和使用示例。

---
最后更新：2025-12-09
