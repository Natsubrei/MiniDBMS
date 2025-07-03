#include "db_api.h"
#include "sql_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================== 内存数据库结构 ==================
struct Row
{
    struct Value *values;
    struct Row *next;
};

struct Table
{
    char *name;
    struct ColumnDef *columns;
    struct Row *rows;
    struct Table *next;
};

struct Database
{
    char *name;
    struct Table *tables;
    struct Database *next;
};

// 字段引用结构体，用于多表多字段选择
struct FieldRef
{
    int table_idx;    // 属于第几个表
    int col_idx;      // 属于表的第几列
    const char *name; // 字段名
};

struct Database *db_list = NULL;
struct Database *current_db = NULL;

// 辅助：不区分大小写字符串比较
// 返回0表示相等，非0表示不等
int strcasecmp_dbms(const char *a, const char *b)
{
    // 比较两个字符串（忽略大小写），相等返回0，不等返回差值
    // 用于数据库/表/字段名的比较，兼容Windows和Linux
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        // 将大写字母转为小写
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
        // 比较当前字符
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
        ++a;
        ++b;
    }
    // 比较字符串长度
    return (unsigned char)*a - (unsigned char)*b;
}

// 查找数据库链表中指定名称的数据库，找不到返回NULL
struct Database *find_db(const char *name)
{
    // 遍历数据库链表
    for (struct Database *db = db_list; db; db = db->next)
        // 比较数据库名（不区分大小写）
        if (strcasecmp_dbms(db->name, name) == 0)
            return db; // 找到则返回指针
    return NULL;       // 未找到返回NULL
}

// 查找当前数据库中指定名称的表，找不到返回NULL
struct Table *find_table(const char *name)
{
    if (!current_db)
        return NULL; // 未选择数据库
    // 遍历当前数据库的表链表
    for (struct Table *t = current_db->tables; t; t = t->next)
        // 比较表名（不区分大小写）
        if (strcasecmp_dbms(t->name, name) == 0)
            return t; // 找到则返回指针
    return NULL;      // 未找到返回NULL
}

// 获取列名在列链表中的索引，找不到返回-1
int col_index(struct ColumnDef *cols, const char *name)
{
    int idx = 0;
    // 遍历列链表，查找名称匹配的列，返回其索引
    for (struct ColumnDef *c = cols; c; c = c->next, ++idx)
        if (strcasecmp_dbms(c->name, name) == 0)
            return idx;
    return -1;
}

// 判断一行数据是否满足条件表达式（单表where）
// 返回1表示满足，0表示不满足
int row_match(struct Row *row, struct ColumnDef *cols, struct Condition *cond)
{
    // 没有条件，直接返回满足
    if (!cond)
        return 1;
    // 递归处理AND/OR条件
    if (cond->op == 6)
        return row_match(row, cols, cond->left) && row_match(row, cols, cond->right);
    if (cond->op == 7)
        return row_match(row, cols, cond->left) || row_match(row, cols, cond->right);
    // 查找条件字段在列链表中的索引
    int idx = col_index(cols, cond->col);
    if (idx < 0)
        return 0;
    // 找到对应字段的值
    struct Value *v = row->values;
    for (int i = 0; i < idx && v; ++i)
        v = v->next;
    if (!v)
        return 0;
    // 整型比较
    if (cond->value->is_int && v->is_int)
    {
        int a = v->int_val, b = cond->value->int_val;
        switch (cond->op)
        {
        case 0:
            return a == b;
        case 1:
            return a != b;
        case 2:
            return a > b;
        case 3:
            return a < b;
        case 4:
            return a >= b;
        case 5:
            return a <= b;
        }
    }
    // 字符串比较
    else if (!cond->value->is_int && !v->is_int)
    {
        int cmp = strcasecmp_dbms(v->str_val, cond->value->str_val);
        switch (cond->op)
        {
        case 0:
            return cmp == 0;
        case 1:
            return cmp != 0;
        case 2:
            return cmp > 0;
        case 3:
            return cmp < 0;
        case 4:
            return cmp >= 0;
        case 5:
            return cmp <= 0;
        }
    }
    return 0;
}

// 多表where条件判断：只支持字段名唯一的简单条件
// 返回1表示满足，0表示不满足
int row_match_multi(struct Row **rows, struct Table **tables, int n, struct Condition *cond)
{
    // 没有条件，直接返回满足
    if (!cond)
        return 1;
    switch (cond->op)
    {
    case 6: // AND
        return row_match_multi(rows, tables, n, cond->left) && row_match_multi(rows, tables, n, cond->right);
    case 7: // OR
        return row_match_multi(rows, tables, n, cond->left) || row_match_multi(rows, tables, n, cond->right);
    default:
    {
        // 遍历所有表，查找条件字段属于哪个表
        for (int i = 0; i < n; ++i)
        {
            int idx = col_index(tables[i]->columns, cond->col);
            if (idx >= 0)
            {
                // 找到字段，取对应行的值
                struct Value *v = rows[i]->values;
                for (int j = 0; j < idx && v; ++j)
                    v = v->next;
                if (!v)
                    return 0;
                // 整型比较
                if (cond->value->is_int && v->is_int)
                {
                    int a = v->int_val, b = cond->value->int_val;
                    switch (cond->op)
                    {
                    case 0:
                        return a == b;
                    case 1:
                        return a != b;
                    case 2:
                        return a > b;
                    case 3:
                        return a < b;
                    case 4:
                        return a >= b;
                    case 5:
                        return a <= b;
                    }
                }
                // 字符串比较
                else if (!cond->value->is_int && !v->is_int)
                {
                    int cmp = strcasecmp_dbms(v->str_val, cond->value->str_val);
                    switch (cond->op)
                    {
                    case 0:
                        return cmp == 0;
                    case 1:
                        return cmp != 0;
                    case 2:
                        return cmp > 0;
                    case 3:
                        return cmp < 0;
                    case 4:
                        return cmp >= 0;
                    case 5:
                        return cmp <= 0;
                    }
                }
                return 0;
            }
        }
        // 没有找到字段
        return 0;
    }
    }
}

// 多表select * 输出所有表所有字段
// 递归枚举所有表的行组合并输出
void print_rows_multi(int idx, struct Row **rows, int n, struct Table **table_arr, struct Condition *cond)
{
    // 递归出口：所有表的行都已选定
    if (idx == n)
    {
        // 判断当前行组合是否满足where条件
        if (!row_match_multi(rows, table_arr, n, cond))
            return;
        // 依次输出每个表的所有字段
        for (int i = 0; i < n; ++i)
        {
            struct Value *v = rows[i]->values;
            for (struct ColumnDef *c = table_arr[i]->columns; c; c = c->next)
            {
                if (v)
                {
                    if (v->is_int)
                        printf("%12d", v->int_val);
                    else
                        printf("%12s", v->str_val ? v->str_val : "NULL");
                    v = v->next;
                }
                else
                {
                    printf("%12s", "NULL");
                }
            }
        }
        printf("\n");
        return;
    }
    // 递归：枚举当前表的每一行
    for (struct Row *r = table_arr[idx]->rows; r; r = r->next)
    {
        rows[idx] = r;                                       // 记录当前表选中的行
        print_rows_multi(idx + 1, rows, n, table_arr, cond); // 递归处理下一个表
    }
}

// 递归枚举所有表的行组合，只输出指定字段
static void print_rows_multi_sel(int idx, struct Row **rows, int n, struct Table **table_arr, struct Condition *cond, struct FieldRef *fields, int field_count)
{
    // 递归出口：所有表的行都已选定
    if (idx == n)
    {
        // 判断当前行组合是否满足where条件
        if (!row_match_multi(rows, table_arr, n, cond))
            return;
        // 只输出指定字段
        for (int i = 0; i < field_count; ++i)
        {
            int t_idx = fields[i].table_idx;
            int c_idx = fields[i].col_idx;
            struct Value *v = rows[t_idx]->values;
            for (int j = 0; j < c_idx && v; ++j)
                v = v->next;
            if (v)
            {
                if (v->is_int)
                    printf("%12d", v->int_val);
                else
                    printf("%12s", v->str_val ? v->str_val : "NULL");
            }
            else
            {
                printf("%12s", "NULL");
            }
        }
        printf("\n");
        return;
    }
    // 递归：枚举当前表的每一行
    for (struct Row *r = table_arr[idx]->rows; r; r = r->next)
    {
        rows[idx] = r;                                                                // 记录当前表选中的行
        print_rows_multi_sel(idx + 1, rows, n, table_arr, cond, fields, field_count); // 递归处理下一个表
    }
}

// 显示所有数据库名
void db_show_databases()
{
    printf("[DB] Databases:\n");
    for (struct Database *db = db_list; db; db = db->next)
        printf("%12s\n", db->name);
}

// 创建数据库
void db_create_database(const char *name)
{
    if (find_db(name))
    {
        printf("[DB] Database exists: %s\n", name);
        return;
    }
    // 分配新数据库结构体
    struct Database *db = (struct Database *)malloc(sizeof(struct Database));
    db->name = strdup(name); // 拷贝数据库名
    db->tables = NULL;
    // 头插法插入数据库链表
    db->next = db_list;
    db_list = db;
    printf("[DB] Create database: %s\n", name);
}

// 切换当前数据库
void db_use_database(const char *name)
{
    struct Database *db = find_db(name);
    if (!db)
    {
        printf("[DB] Database not found: %s\n", name);
        return;
    }
    // 切换当前数据库指针
    current_db = db;
    printf("[DB] Use database: %s\n", name);
}

// 删除数据库及其所有表
void db_drop_database(const char *name)
{
    struct Database **p = &db_list;
    while (*p)
    {
        if (strcasecmp_dbms((*p)->name, name) == 0)
        {
            struct Database *del = *p;
            *p = del->next; // 从链表中移除
            // 释放所有表及其数据
            struct Table *t = del->tables;
            while (t)
            {
                struct Table *tmp = t;
                t = t->next;
                free(tmp->name);
                free_column_defs(tmp->columns);
                struct Row *r = tmp->rows;
                while (r)
                {
                    struct Row *tr = r;
                    r = r->next;
                    free_value_list(tr->values);
                    free(tr);
                }
                free(tmp);
            }
            free(del->name);
            free(del);
            if (current_db == del)
                current_db = NULL;
            printf("[DB] Drop database: %s\n", name);
            return;
        }
        p = &(*p)->next;
    }
    printf("[DB] Database not found: %s\n", name);
}

// 显示当前数据库所有表名
void db_show_tables()
{
    if (!current_db)
    {
        printf("[DB] No database selected\n");
        return;
    }
    printf("[DB] Tables in %s:\n", current_db->name);
    for (struct Table *t = current_db->tables; t; t = t->next)
        printf("%12s\n", t->name);
}

// 创建表，深拷贝列定义
void db_create_table(const char *name, struct ColumnDef *cols)
{
    if (!current_db)
    {
        printf("[DB] No database selected\n");
        return;
    }
    if (find_table(name))
    {
        printf("[DB] Table exists: %s\n", name);
        return;
    }
    // 分配新表结构体
    struct Table *t = (struct Table *)malloc(sizeof(struct Table));
    t->name = strdup(name); // 拷贝表名
    // 深拷贝列定义，防止外部free影响
    struct ColumnDef *src = cols, *dst_head = NULL, **dst_tail = &dst_head;
    while (src)
    {
        struct ColumnDef *c = (struct ColumnDef *)malloc(sizeof(struct ColumnDef));
        c->name = strdup(src->name);
        c->type = strdup(src->type);
        c->next = NULL;
        *dst_tail = c;
        dst_tail = &c->next;
        src = src->next;
    }
    t->columns = dst_head;
    t->rows = NULL;
    // 头插法插入表链表
    t->next = current_db->tables;
    current_db->tables = t;
    printf("[DB] Create table: %s\n", name);
    for (struct ColumnDef *c = t->columns; c; c = c->next)
        printf("  Column: %s %s\n", c->name, c->type);
}

// 删除表及其所有数据
void db_drop_table(const char *name)
{
    if (!current_db)
    {
        printf("[DB] No database selected\n");
        return;
    }
    struct Table **p = &current_db->tables;
    while (*p)
    {
        if (strcasecmp_dbms((*p)->name, name) == 0)
        {
            struct Table *del = *p;
            *p = del->next; // 从链表中移除
            free(del->name);
            free_column_defs(del->columns);
            struct Row *r = del->rows;
            while (r)
            {
                struct Row *tr = r;
                r = r->next;
                free_value_list(tr->values);
                free(tr);
            }
            free(del);
            printf("[DB] Drop table: %s\n", name);
            return;
        }
        p = &((*p)->next);
    }
    printf("[DB] Table not found: %s\n", name);
}

// 向指定表插入一行数据
// table: 表名
// cols: 指定插入的列名链表（可为NULL，表示所有列顺序插入）
// values: 插入的值链表
void db_insert(const char *table, struct ColumnList *cols, struct Value *values)
{
    // 查找目标表
    struct Table *t = find_table(table);
    if (!t)
    {
        printf("[DB] Table not found: %s\n", table);
        return;
    }
    struct Value *vt = values;
    if (vt)
    {
        // 创建新行结构体
        struct Row *row = (struct Row *)malloc(sizeof(struct Row));
        struct Value *newvals = NULL, **tail = &newvals;
        if (!cols)
        {
            // 未指定列名，按表定义顺序插入
            struct Value *v = vt;
            for (struct ColumnDef *c = t->columns; c; c = c->next)
            {
                struct Value *nv;
                if (v)
                {
                    nv = copy_value(v); // 拷贝值
                    v = v->next;
                }
                else
                {
                    // 不足的列补默认值
                    nv = (struct Value *)calloc(1, sizeof(struct Value));
                    if (strncmp(c->type, "CHAR", 4) == 0)
                    {
                        nv->is_int = 0;
                        nv->str_val = NULL;
                    }
                    else
                    {
                        nv->is_int = 1;
                        nv->int_val = 0;
                    }
                }
                nv->next = NULL;
                *tail = nv;
                tail = &nv->next;
            }
        }
        else
        {
            // 指定列名插入，未指定的列补默认值
            for (struct ColumnDef *c = t->columns; c; c = c->next)
            {
                struct Value *v = NULL;
                int col_idx = 0, match_idx = -1;
                // 查找当前列在插入列名链表中的索引
                for (struct ColumnList *cl = cols; cl; cl = cl->next, ++col_idx)
                {
                    if (strcasecmp_dbms(cl->name, c->name) == 0)
                    {
                        match_idx = col_idx;
                        break;
                    }
                }
                if (match_idx >= 0)
                {
                    v = vt;
                    for (int i = 0; i < match_idx && v; ++i)
                        v = v->next;
                    if (v)
                    {
                        struct Value *nv = copy_value(v);
                        *tail = nv;
                        tail = &nv->next;
                        continue;
                    }
                }
                // 未指定的列补默认值
                struct Value *nv = (struct Value *)calloc(1, sizeof(struct Value));
                if (strncmp(c->type, "CHAR", 4) == 0)
                {
                    nv->is_int = 0;
                    nv->str_val = NULL;
                }
                else
                {
                    nv->is_int = 1;
                    nv->int_val = 0;
                }
                nv->next = NULL;
                *tail = nv;
                tail = &nv->next;
            }
        }
        row->values = newvals;
        // 头插法插入行链表
        row->next = t->rows;
        t->rows = row;
    }
    printf("[DB] Insert into %s\n", table);
}

// 执行select语句，支持单表/多表、字段选择、where条件
// tables: 表名链表
// sel: 字段名链表（为NULL表示select *）
// cond: where条件表达式
void db_select(struct ColumnList *tables, struct SelectList *sel, struct Condition *cond)
{
    int table_count = 0;
    struct Table *table_arr[8];
    struct ColumnList *tl = tables;
    // 解析表名链表，查找所有表指针
    while (tl && table_count < 8)
    {
        struct Table *t = find_table(tl->name);
        if (!t)
        {
            printf("[DB] Table not found: %s\n", tl->name);
            return;
        }
        table_arr[table_count++] = t;
        tl = tl->next;
    }
    if (table_count == 0)
    {
        printf("[DB] No table specified\n");
        return;
    }
    struct Row *rows[8]; // 存放每个表当前枚举到的行
    if (table_count > 1)
    {
        struct FieldRef field_refs[64]; // 存放多表多字段选择的字段映射
        int field_count = 0;
        if (sel)
        {
            // 构建字段映射：确定每个字段属于哪个表及其列索引
            for (struct SelectList *s = sel; s; s = s->next)
            {
                int found = 0;
                for (int i = 0; i < table_count; ++i)
                {
                    int idx = col_index(table_arr[i]->columns, s->name);
                    if (idx >= 0)
                    {
                        field_refs[field_count].table_idx = i;
                        field_refs[field_count].col_idx = idx;
                        field_refs[field_count].name = s->name;
                        ++field_count;
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    printf("Field not found: %s\n", s->name);
                    return;
                }
            }
            // 打印表头
            for (int i = 0; i < field_count; ++i)
                printf("%12s", field_refs[i].name);
            printf("\n");
            // 递归枚举所有表的行组合并输出指定字段
            print_rows_multi_sel(0, rows, table_count, table_arr, cond, field_refs, field_count);
        }
        else
        {
            // select *，输出所有表所有字段
            for (int i = 0; i < table_count; ++i)
                for (struct ColumnDef *c = table_arr[i]->columns; c; c = c->next)
                    printf("%12s", c->name);
            printf("\n");
            // 递归枚举所有表的行组合并输出
            print_rows_multi(0, rows, table_count, table_arr, cond);
        }
        return;
    }
    // 单表，支持字段和where
    struct Table *t = table_arr[0];
    // 打印表头
    if (!sel)
    {
        for (struct ColumnDef *c = t->columns; c; c = c->next)
            printf("%12s", c->name);
    }
    else
    {
        for (struct SelectList *s = sel; s; s = s->next)
            printf("%12s", s->name);
    }
    printf("\n");
    // 打印数据
    for (struct Row *r = t->rows; r; r = r->next)
    {
        if (!row_match(r, t->columns, cond))
            continue;
        struct Value *v = r->values;
        if (!sel)
        {
            // 输出所有字段
            for (struct ColumnDef *c = t->columns; c; c = c->next)
            {
                int idx = col_index(t->columns, c->name);
                struct Value *v2 = r->values;
                for (int i = 0; i < idx && v2; ++i)
                    v2 = v2->next;
                if (v2)
                {
                    if (v2->is_int)
                        printf("%12d", v2->int_val);
                    else
                        printf("%12s", v2->str_val ? v2->str_val : "NULL");
                }
                else
                {
                    printf("%12s", "NULL");
                }
            }
        }
        else
        {
            // 只输出指定字段
            for (struct SelectList *s = sel; s; s = s->next)
            {
                int idx = col_index(t->columns, s->name);
                struct Value *v2 = r->values;
                for (int i = 0; i < idx && v2; ++i)
                    v2 = v2->next;
                if (v2)
                {
                    if (v2->is_int)
                        printf("%12d", v2->int_val);
                    else
                        printf("%12s", v2->str_val ? v2->str_val : "NULL");
                }
                else
                {
                    printf("%12s", "NULL");
                }
            }
        }
        printf("\n");
    }
}

// 执行update语句，按条件批量更新
// table: 表名
// set: 要更新的字段及新值链表
// cond: where条件表达式
void db_update(const char *table, struct SetItem *set, struct Condition *cond)
{
    struct Table *t = find_table(table);
    if (!t)
    {
        printf("[DB] Table not found: %s\n", table);
        return;
    }
    // 遍历所有行，判断是否满足条件
    for (struct Row *r = t->rows; r; r = r->next)
    {
        if (!row_match(r, t->columns, cond))
            continue;
        // 遍历所有要更新的字段
        for (struct SetItem *s = set; s; s = s->next)
        {
            int idx = col_index(t->columns, s->col);
            struct Value *v = r->values;
            for (int i = 0; i < idx && v; ++i)
                v = v->next;
            if (v)
            {
                if (v->is_int && s->value->is_int)
                    v->int_val = s->value->int_val;
                else if (!v->is_int && !s->value->is_int)
                {
                    if (v->str_val)
                        free(v->str_val); // 释放原字符串
                    v->str_val = strdup(s->value->str_val);
                }
            }
        }
    }
    printf("[DB] Update %s\n", table);
}

// 执行delete语句，按条件批量删除
// table: 表名
// cond: where条件表达式
void db_delete(const char *table, struct Condition *cond)
{
    struct Table *t = find_table(table);
    if (!t)
    {
        printf("[DB] Table not found: %s\n", table);
        return;
    }
    struct Row **p = &t->rows;
    // 遍历所有行，删除满足条件的行
    while (*p)
    {
        if (row_match(*p, t->columns, cond))
        {
            struct Row *del = *p;
            *p = del->next; // 从链表中移除
            free_value_list(del->values);
            free(del);
        }
        else
        {
            p = &(*p)->next;
        }
    }
    printf("[DB] Delete from %s\n", table);
}

// 退出数据库系统，保存数据并释放所有内存
void db_exit()
{
    save_db(); // 退出时自动保存数据库
    // 释放所有内存
    while (db_list)
    {
        struct Database *db = db_list;
        db_list = db->next;
        while (db->tables)
        {
            struct Table *t = db->tables;
            db->tables = t->next;
            free(t->name);
            free_column_defs(t->columns);
            struct Row *r = t->rows;
            while (r)
            {
                struct Row *tr = r;
                r = r->next;
                free_value_list(tr->values);
                free(tr);
            }
            free(t);
        }
        free(db->name);
        free(db);
    }
    printf("[DB] Exit\n");
    exit(0);
}

// ================== 持久化存储 ==================
#define DB_DUMP_FILE "data.db"

// 保存当前所有数据库、表结构和数据到文件，实现持久化存储
void save_db()
{
    FILE *fp = fopen(DB_DUMP_FILE, "w"); // 以写模式打开数据文件
    if (!fp)
        return;
    // 遍历所有数据库
    for (struct Database *db = db_list; db; db = db->next)
    {
        fprintf(fp, "DB %s\n", db->name); // 写入数据库名
        // 遍历数据库下所有表
        for (struct Table *t = db->tables; t; t = t->next)
        {
            fprintf(fp, "TABLE %s\n", t->name); // 写入表名
            int col_cnt = 0;
            for (struct ColumnDef *c = t->columns; c; c = c->next)
                ++col_cnt;
            fprintf(fp, "COLS %d\n", col_cnt); // 写入列数
            // 写入每个字段的名字和类型
            for (struct ColumnDef *c = t->columns; c; c = c->next)
                fprintf(fp, "%s %s\n", c->name, c->type);
            // 写入所有数据行
            for (struct Row *r = t->rows; r; r = r->next)
            {
                fprintf(fp, "ROW"); // 行起始标记
                struct Value *v = r->values;
                for (struct ColumnDef *c = t->columns; c; c = c->next)
                {
                    if (v)
                    {
                        if (v->is_int)
                            fprintf(fp, " I %d", v->int_val); // 整型数据
                        else
                            fprintf(fp, " S %s", v->str_val ? v->str_val : ""); // 字符串数据
                        v = v->next;
                    }
                    else
                    {
                        fprintf(fp, " N"); // 空值
                    }
                }
                fprintf(fp, "\n"); // 行结束
            }
        }
    }
    fclose(fp); // 关闭文件
}

// 从文件加载数据库、表结构和数据到内存，实现持久化恢复
void load_db()
{
    FILE *fp = fopen(DB_DUMP_FILE, "r"); // 以读模式打开数据文件
    if (!fp)
    {
        printf("[LOAD_DB] Cannot open %s\n", DB_DUMP_FILE);
        return;
    }
    char buf[256];
    struct Table *cur_table = NULL; // 当前正在处理的表
    int line = 0;
    while (fgets(buf, sizeof(buf), fp))
    {
        ++line;
        // 解析数据库名
        if (strncmp(buf, "DB ", 3) == 0)
        {
            char name[128];
            sscanf(buf + 3, "%s", name);
            db_create_database(name); // 创建数据库
            db_use_database(name);    // 切换当前数据库
            cur_table = NULL;
        }
        // 解析表结构
        else if (strncmp(buf, "TABLE ", 6) == 0)
        {
            char tname[128];
            sscanf(buf + 6, "%s", tname);
            int col_cnt = 0;
            fgets(buf, sizeof(buf), fp);
            ++line;
            sscanf(buf, "COLS %d", &col_cnt);
            struct ColumnDef *cols = NULL, **tail = &cols;
            // 读取每个字段的名字和类型
            for (int i = 0; i < col_cnt; ++i)
            {
                fgets(buf, sizeof(buf), fp);
                ++line;
                char cname[64], ctype[64];
                sscanf(buf, "%s %s", cname, ctype);
                struct ColumnDef *c = (struct ColumnDef *)malloc(sizeof(struct ColumnDef));
                c->name = strdup(cname);
                c->type = strdup(ctype);
                c->next = NULL;
                *tail = c;
                tail = &c->next;
            }
            db_create_table(tname, cols); // 创建表
            free_column_defs(cols);       // 释放临时列定义
            cur_table = find_table(tname);
        }
        // 解析数据行
        else if (strncmp(buf, "ROW", 3) == 0)
        {
            if (!cur_table)
            {
                continue;
            }
            struct Value *vlist = NULL, **vtail = &vlist;
            char *p = buf + 3;
            // 依次读取每个字段的值
            for (struct ColumnDef *c = cur_table->columns; c; c = c->next)
            {
                while (*p == ' ')
                    ++p;
                if (*p == 'I')
                {
                    int ival;
                    p += 2;
                    sscanf(p, "%d", &ival);
                    struct Value *v = (struct Value *)calloc(1, sizeof(struct Value));
                    v->is_int = 1;
                    v->int_val = ival;
                    *vtail = v;
                    vtail = &v->next;
                    while (*p && *p != ' ')
                        ++p;
                }
                else if (*p == 'S')
                {
                    char sval[128];
                    p += 2;
                    sscanf(p, "%s", sval);
                    struct Value *v = (struct Value *)calloc(1, sizeof(struct Value));
                    v->is_int = 0;
                    v->str_val = strdup(sval);
                    *vtail = v;
                    vtail = &v->next;
                    while (*p && *p != ' ')
                        ++p;
                }
                else if (*p == 'N')
                {
                    struct Value *v = (struct Value *)calloc(1, sizeof(struct Value));
                    v->is_int = 1;
                    v->int_val = 0;
                    *vtail = v;
                    vtail = &v->next;
                    ++p;
                }
            }
            db_insert(cur_table->name, NULL, vlist); // 插入数据行
            free_value_list(vlist);                  // 释放临时值链表
        }
    }
    fclose(fp); // 关闭文件
}
