#ifndef PF_H
#define PF_H

#include <cstdio>
#include <cstddef>

/**
 * @file pf.h
 * @brief RedBase PF（Paged File）组件核心接口头文件
 *
 * 本文件集中定义：
 *  1. PF 模块所有返回码（错误码）
 *  2. PF_PrintError 函数接口
 *  3. PF 基本常量，如页大小、缓冲池大小
 *  4. 基础类型定义：RC, PageNum
 */

// ============================================================================
// 基础类型定义
// ============================================================================

// Return Code 类型
typedef int RC;

// PageNum 表示页号（文件中第几页）
typedef int PageNum;

// ============================================================================
// 常量定义
// ============================================================================

// 每个页面的大小（4092 字节用户数据 + 4 字节管理信息 = 4096 字节）
const size_t PF_PAGE_SIZE = 4092;

// 缓冲池中最多可以同时缓存的页面数
const size_t PF_BUFFER_SIZE = 40;

// 特殊页号常量
#define ALL_PAGES (-999)   // 表示所有页面（用于ForcePages函数）

// ============================================================================
// 错误码定义
// ============================================================================
// 所有错误码均为整数常量：
//  0            → 表示成功
//  正数（>0）   → 可恢复错误或正常的异常情况
//  负数（<0）   → 系统内部严重错误

// ---------- 正数错误码（可恢复或正常异常） ----------
#define PF_EOF             1   // 文件结尾
#define PF_PAGEPINNED      2   // 页面仍被 pinned，无法被替换或释放
#define PF_PAGENOTINBUF    3   // 页面不在缓冲池中
#define PF_PAGEUNPINNED    4   // 页面已经 unpinned
#define PF_PAGEFREE        5   // 页面已经处于空闲状态
#define PF_INVALIDPAGE     6   // 无效的页号
#define PF_FILEOPEN        7   // 文件句柄已经打开
#define PF_CLOSEDFILE      8   // 文件句柄已经关闭

// ---------- 负数错误码（严重错误） ----------
#define PF_NOMEM           -1  // 内存不足
#define PF_NOBUF           -2  // 缓冲池空间不足
#define PF_INCOMPLETEREAD  -3  // 从文件读取页面数据不完整
#define PF_INCOMPLETEWRITE -4  // 向文件写页面数据不完整
#define PF_HDRREAD         -5  // 读取文件头失败
#define PF_HDRWRITE        -6  // 写文件头失败

// ---- 内部 PF 错误 ----
#define PF_PAGEINBUF       -10 // 新分配的页面已经在缓冲池中
#define PF_HASHNOTFOUND    -11 // 哈希表未找到对应记录
#define PF_HASHPAGEEXIST   -12 // 页面已经存在于哈希表中
#define PF_INVALIDNAME     -13 // 无效的文件名
#define PF_UNIX            -14 // Unix 系统调用错误（errno）
#define PF_INVALIDSIZE     -15 // 无效的大小参数

// ============================================================================
// 错误信息输出函数接口
// ============================================================================

/**
 * @brief 打印 PF 错误信息到标准错误输出（stderr）
 *
 * @param rc PF 模块返回的错误码
 *
 * 调用场景：
 *  - 当 PF 方法返回非零值时，可以调用该函数打印错误信息。
 *
 * 示例：
 * @code
 * RC rc = fileHandle.UnpinPage(pageNum);
 * if (rc != 0) {
 *     PF_PrintError(rc);
 * }
 * @endcode
 */
void PF_PrintError(RC rc);

#endif // PF_H
