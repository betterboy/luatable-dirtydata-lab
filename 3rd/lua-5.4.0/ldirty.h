
#ifndef ldirty_h
#define ldirty_h

#include "queue.h"

typedef struct Table Table;
typedef struct TValue TValue;

struct dirty_manage_s;

#define DIRTY_SET (0)
#define DIRTY_ADD (1)
#define DIRTY_DEL (2)

typedef struct dirty_key_s
{
    TAILQ_ENTRY(dirty_key_s) entry;

    union key
    {
        TValue *map_key;
        int arr_index;
        TValue *del;
    };
    
    unsigned char dirty_op;
} dirty_key_t;

typedef union self_key_u
{
    TValue *map_key;
    int arr_index;
} self_key_t;

//脏节点
typedef struct dirty_node_s
{
    TAILQ_ENTRY(dirty_node_s) entry;
    TAILQ_HEAD(dirty_key_head_s, dirty_key_s) dirty_key_list;

    unsigned key_cnt;
    struct dirty_manage_s *mng;
} dirty_node_t;

//根节点管理
typedef struct dirty_root_s
{
    TAILQ_HEAD(dirty_node_head_s, dirty_node_s) dirty_node_list;
    unsigned int node_cnt;
} dirty_root_t;

typedef struct dirty_manage_s
{
    TValue *root;
    TValue *parent;

    TValue *self_key;
    TValue *self;

    dirty_node_t *dirty_node;
    //根才有这个节点
    dirty_root_t dirty_root[0];

} dirty_manage_t;

#define get_manage(tv) ((ttistable(tv) ? hvalue(tv)->dirty_mng : NULL))
#define is_dirty_root(mng) ((mng)->self == (mng)->root && (mng)->root != NULL)

void begin_dirty_manage_table(TValue *svtable, TValue *parent, self_key_t *self_key);
void free_dirty_table(TValue *svtable);
void set_dirty_table(TValue *svtable, TValue *key, unsigned char op);
void clear_dirty(TValue *svtable);

void dirty_mem_pool_setup();
void dirty_mem_pool_stat();

#endif // ldirty_h