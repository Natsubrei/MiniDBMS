#ifndef SQL_STRUCT_H
#define SQL_STRUCT_H
#include <stddef.h>

struct ColumnDef
{
    char *name;
    char *type;
    struct ColumnDef *next;
};

struct ColumnList
{
    char *name;
    struct ColumnList *next;
};

struct Value
{
    int is_int;
    int int_val;
    char *str_val;
    struct Value *next;
};

struct SelectList
{
    char *name;
    struct SelectList *next;
};

struct Condition
{
    char *col;
    int op;
    struct Value *value;
    struct Condition *left;
    struct Condition *right;
};

struct SetItem
{
    char *col;
    struct Value *value;
    struct SetItem *next;
};

// 构造/释放/copy函数声明
struct ColumnDef *create_column_defs(struct ColumnDef *list, struct ColumnDef *new_item);
struct ColumnDef *create_column_def(char *name, char *type);
void free_column_defs(struct ColumnDef *list);
struct ColumnList *create_column_list(char *name, struct ColumnList *next);
void free_column_list(struct ColumnList *list);
struct Value *create_value_int(int v);
struct Value *create_value_str(char *s);
struct Value *create_value_list(struct Value *v, struct Value *next);
void free_value_list(struct Value *list);
struct SelectList *create_select_list(char *name, struct SelectList *next);
void free_select_list(struct SelectList *list);
struct Condition *create_condition(char *col, int op, struct Value *v);
struct Condition *create_condition_and(struct Condition *l, struct Condition *r);
struct Condition *create_condition_or(struct Condition *l, struct Condition *r);
void free_condition(struct Condition *c);
struct SetItem *create_set_item(char *col, struct Value *v);
struct SetItem *create_set_list(struct SetItem *item, struct SetItem *next);
void free_set_list(struct SetItem *list);
struct Value *copy_value(const struct Value *v);

// Condition操作符常量
enum
{
    EQ = 0,
    NEQ_OP = 1,
    GT = 2,
    LT = 3,
    GE = 4,
    LE = 5
};
#endif
