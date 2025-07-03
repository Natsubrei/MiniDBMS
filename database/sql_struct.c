#include "sql_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 创建列定义链表，将 new_item 插入 list 尾部
struct ColumnDef *create_column_defs(struct ColumnDef *list, struct ColumnDef *new_item)
{
    if (!list)
        return new_item;
    // 尾插法
    struct ColumnDef *cur = list;
    while (cur->next)
        cur = cur->next;
    cur->next = new_item;
    return list;
}

// 创建单个列定义节点
struct ColumnDef *create_column_def(char *name, char *type)
{
    struct ColumnDef *c = (struct ColumnDef *)malloc(sizeof(struct ColumnDef));
    c->name = strdup(name);
    c->type = strdup(type);
    c->next = NULL;
    return c;
}

// 释放列定义链表的所有节点和字符串
void free_column_defs(struct ColumnDef *list)
{
    while (list)
    {
        struct ColumnDef *tmp = list;
        list = list->next;
        if (tmp->name)
            free(tmp->name);
        if (tmp->type)
            free(tmp->type);
        free(tmp);
    }
}

// 创建列名链表节点
struct ColumnList *create_column_list(char *name, struct ColumnList *next)
{
    struct ColumnList *c = (struct ColumnList *)malloc(sizeof(struct ColumnList));
    c->name = strdup(name);
    c->next = next;
    return c;
}

// 释放列名链表
void free_column_list(struct ColumnList *list)
{
    while (list)
    {
        struct ColumnList *tmp = list;
        list = list->next;
        if (tmp->name)
            free(tmp->name);
        free(tmp);
    }
}

// 创建整型值节点
struct Value *create_value_int(int v)
{
    struct Value *val = (struct Value *)malloc(sizeof(struct Value));
    val->is_int = 1;
    val->int_val = v;
    val->str_val = NULL;
    val->next = NULL;
    return val;
}

// 创建字符串值节点
struct Value *create_value_str(char *s)
{
    struct Value *val = (struct Value *)malloc(sizeof(struct Value));
    val->is_int = 0;
    val->str_val = strdup(s);
    val->next = NULL;
    return val;
}

// 创建值链表，将 v 连接到 next
struct Value *create_value_list(struct Value *v, struct Value *next)
{
    v->next = next;
    return v;
}

// 释放值链表（包括字符串）
void free_value_list(struct Value *list)
{
    while (list)
    {
        struct Value *tmp = list;
        list = list->next;
        if (!tmp->is_int && tmp->str_val)
            free(tmp->str_val);
        tmp->str_val = NULL;
        free(tmp);
    }
}

// 创建字段选择链表节点
struct SelectList *create_select_list(char *name, struct SelectList *next)
{
    struct SelectList *s = (struct SelectList *)malloc(sizeof(struct SelectList));
    s->name = strdup(name);
    s->next = next;
    return s;
}

// 释放字段选择链表
void free_select_list(struct SelectList *list)
{
    while (list)
    {
        struct SelectList *tmp = list;
        list = list->next;
        free(tmp->name);
        free(tmp);
    }
}

// 创建简单条件节点（如 col op value）
struct Condition *create_condition(char *col, int op, struct Value *v)
{
    struct Condition *c = (struct Condition *)malloc(sizeof(struct Condition));
    c->col = strdup(col);
    c->op = op;
    c->value = v;
    c->left = c->right = NULL;
    return c;
}

// 创建 AND 条件节点
struct Condition *create_condition_and(struct Condition *l, struct Condition *r)
{
    struct Condition *c = (struct Condition *)malloc(sizeof(struct Condition));
    c->op = 6;
    c->left = l;
    c->right = r;
    c->col = NULL;
    c->value = NULL;
    return c;
}

// 创建 OR 条件节点
struct Condition *create_condition_or(struct Condition *l, struct Condition *r)
{
    struct Condition *c = (struct Condition *)malloc(sizeof(struct Condition));
    c->op = 7;
    c->left = l;
    c->right = r;
    c->col = NULL;
    c->value = NULL;
    return c;
}

// 递归释放条件表达式树
void free_condition(struct Condition *c)
{
    if (!c)
        return;
    if (c->col)
        free(c->col);
    if (c->value)
        free_value_list(c->value);
    free_condition(c->left);
    free_condition(c->right);
    free(c);
}

// 创建 SET 子句节点（如 col = value）
struct SetItem *create_set_item(char *col, struct Value *v)
{
    struct SetItem *s = (struct SetItem *)malloc(sizeof(struct SetItem));
    s->col = strdup(col);
    s->value = v;
    s->next = NULL;
    return s;
}

// 创建 SET 子句链表，将 item 连接到 next
struct SetItem *create_set_list(struct SetItem *item, struct SetItem *next)
{
    item->next = next;
    return item;
}

// 释放 SET 子句链表
void free_set_list(struct SetItem *list)
{
    while (list)
    {
        struct SetItem *tmp = list;
        list = list->next;
        free(tmp->col);
        if (tmp->value)
            free_value_list(tmp->value);
        free(tmp);
    }
}

// 深拷贝值链表
struct Value *copy_value(const struct Value *v)
{
    if (!v)
        return NULL;
    struct Value *head = NULL, **tail = &head;
    while (v)
    {
        struct Value *nv = (struct Value *)malloc(sizeof(struct Value));
        nv->is_int = v->is_int;
        if (v->is_int)
        {
            nv->int_val = v->int_val;
            nv->str_val = NULL;
        }
        else
        {
            nv->str_val = v->str_val ? strdup(v->str_val) : NULL;
        }
        nv->next = NULL;
        *tail = nv;
        tail = &nv->next;
        v = v->next;
    }
    return head;
}
