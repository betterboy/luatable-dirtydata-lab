
#ifndef ldirty_h
#define ldirty_h

#include "queue.h"
#include "lobject.h"
// #include "lua.h"
// #include "llimits.h"

// struct TValue;
// struct Table;

typedef struct key_string_s {
    char contents[1];
} key_string_t;

typedef union key_number_s {
    lua_Number n;
    lua_Integer i;
} key_number_t;

typedef struct key_value_s {
    lu_byte tt_;
    size_t len;
    union {
        key_string_t key_s;
        key_number_t key_n;
    } u;
} key_value_t;

// struct dirty_manage_s;

#define DIRTY_SET (0)
#define DIRTY_ADD (1)
#define DIRTY_DEL (2)

typedef struct dirty_key_s
{
    TAILQ_ENTRY(dirty_key_s) entry;

    union key
    {
        key_value_t *map_key;
        int arr_index;
        key_value_t *del;
    } key;

    struct TValue realkey;
    unsigned char dirty_op;
} dirty_key_t;

typedef union self_key_u
{
    key_value_t *map_key;
    int arr_index;
} self_key_t;

//脏节点
typedef struct dirty_node_s
{
    TAILQ_ENTRY(dirty_node_s) entry;
    TAILQ_HEAD(dirty_key_head_s, dirty_key_s) dirty_key_list;

    unsigned key_cnt;
    struct dirty_manage_s *mng;
    char *full_key; //保存从根节点到当前节点的key path
} dirty_node_t;

//根节点管理
typedef struct dirty_root_s
{
    TAILQ_HEAD(dirty_node_head_s, dirty_node_s) dirty_node_list;
    unsigned int node_cnt;
} dirty_root_t;

typedef struct dirty_manage_s
{
    struct TValue *root;
    struct TValue *parent;

    self_key_t self_key;
    struct TValue *self;

    dirty_node_t *dirty_node;
    //根才有这个节点
    dirty_root_t dirty_root[0];

} dirty_manage_t;

#define is_dirty_root(mng) ((mng)->root != NULL && (mng)->self->value_.gc == (mng)->root->value_.gc)

void begin_dirty_root_map(struct TValue *svmap, const char *key);
void begin_dirty_manage_map(struct TValue *svmap, struct TValue *parent, self_key_t *self_key);
void free_dirty_map(struct Table *svmap);
void set_dirty_map(const struct TValue *svmap, struct TValue *key, struct TValue *value, unsigned char op);
void clear_dirty(struct TValue *svmap);

//debug: 获取table脏数据情况
// void dump_dirty_key_map(lua_State *L);

int key2str(char *buf, key_value_t *key_value);

void dirty_mem_pool_setup(void);
void dirty_mem_pool_clear(void);
void dirty_mem_pool_stat(void);

#endif // ldirty_h