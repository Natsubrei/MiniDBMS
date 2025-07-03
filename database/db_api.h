#ifndef DB_API_H
#define DB_API_H

#include "sql_struct.h"

// 数据库操作API声明
void db_create_database(const char *name);
void db_use_database(const char *name);
void db_create_table(const char *name, struct ColumnDef *cols);
void db_show_tables();
void db_show_databases();
void db_drop_database(const char *name);
void db_drop_table(const char *name);
void db_insert(const char *table, struct ColumnList *cols, struct Value *values);
void db_select(struct ColumnList *tables, struct SelectList *sel, struct Condition *cond);
void db_update(const char *table, struct SetItem *set, struct Condition *cond);
void db_delete(const char *table, struct Condition *cond);
void db_exit();
void save_db();
void load_db();

// 工具函数声明
struct Database *find_db(const char *name);

#endif
