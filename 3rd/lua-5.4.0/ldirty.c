
#include "ldirty.h"
#include "lstate.h"
#include "lobject.h"
#include "ltable.h"
#include "limits.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fs_mbuf.h"

#define DIRTY_DEBUG
#define DIRTY_MAP_CHECK

static fs_free_array_t pool_key;
static fs_free_array_t pool_node;
static fs_free_array_t pool_manage;
static fs_free_array_t pool_manage_root;

#define PAGE_ELEMENT_CNT    (8)

#define MEM_POOL_ALLOC(which) fs_free_array_alloc(&(which))
#define MEM_POOL_FREE(which, ptr) fs_free_array_free(&(which), (ptr))

//lua macros
#define GET_NODEKEY(v, node) { \
    io_->value_ = n_->u.key_val; \
    io_->tt_ = n_->u.key_tt; \
}

#define GET_NODELAST(h)	gnode(h, cast_sizet(sizenode(h)))

typedef void (*traverse_cb_t)(TValue *);

void dirty_mem_pool_setup(void)
{
    fs_free_array_init(&pool_key, "dirty_key", sizeof(dirty_key_t), PAGE_ELEMENT_CNT);
    fs_free_array_init(&pool_node, "dirty_node", sizeof(dirty_node_t), PAGE_ELEMENT_CNT);
    fs_free_array_init(&pool_manage, "dirty_manage", sizeof(dirty_manage_t), PAGE_ELEMENT_CNT);
    fs_free_array_init(&pool_manage_root, "dirty_manage_root", sizeof(dirty_manage_t), PAGE_ELEMENT_CNT);
}

void dirty_mem_pool_clear(void)
{
    fs_free_array_destruct(&pool_key);
    fs_free_array_destruct(&pool_node);
    fs_free_array_destruct(&pool_manage);
    fs_free_array_destruct(&pool_manage_root);
}

void dirty_mem_pool_stat(void)
{
	fprintf(stderr, "dirty key total_size:%d,element_size:%d,total:%d,alloc:%d\n", 
		pool_key.mbuf.alloc_size, pool_key.element_size, 
		pool_key.element_total, pool_key.element_alloc);
	fprintf(stderr, "dirty node total_size:%d,element_size:%d,total:%d,alloc:%d\n", 
		pool_node.mbuf.alloc_size, pool_node.element_size, 
		pool_node.element_total, pool_node.element_alloc);
	fprintf(stderr, "dirty manage total_size:%d,element_size:%d,total:%d,alloc:%d\n", 
		pool_manage.mbuf.alloc_size, pool_manage.element_size,
		pool_manage.element_total, pool_manage.element_alloc);
	fprintf(stderr, "dirty manage_root total_size:%d,element_size:%d,total:%d,alloc:%d\n", 
		pool_manage_root.mbuf.alloc_size, pool_manage_root.element_size,
		pool_manage_root.element_total, pool_manage_root.element_alloc);
}

static void free_dirty_map_recurse(TValue *map);
static void clear_dirty_map_recurse(TValue *map);
static void clear_dirty_map(TValue *map);
static void assert_attach_dirty_map_recurse(TValue *svmap);

static void dirty_log(const char *fmt, ...)
{
#ifdef DIRTY_DEBUG
    char buf[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(buf, fmt, argptr);
    va_end(argptr);
	fprintf(stderr, "log: %s\n", buf); 
#endif
}

static void *dirty_malloc(size_t size)
{
    return malloc(size);
}

static void dirty_free(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

static int samekey(key_value_t *arg1, TValue *arg2)
{
    if (rawtt(arg1) != rawtt(arg2)) {
        // not the same variants
        return 0;
    }

    switch (ttypetag(arg1))
    {
    case LUA_VNUMINT:
        return (arg1->u.key_n.i == ivalue(arg2));
        break;

    case LUA_VNUMFLT:
        return luai_numeq(arg1->u.key_n.n, fltvalue(arg2));
        break;

    case LUA_VSHRSTR:
    case LUA_VLNGSTR: {
        TString *str = tsvalue(arg2);
        size_t len = ttisshrstring(arg2) ? str->shrlen : str->u.lnglen;
        return (len == arg1->u.key_s.len) && (memcmp(arg1->u.key_s.contents, getstr(str), len) == 0);
    }
    
    default:
        break;
    }

    return 0;
}

inline static dirty_manage_t *get_manage(TValue *value)
{
    dirty_manage_t *mng = NULL;
#ifdef USE_DIRTY_DATA
    if (ttistable(value)) {
        mng = hvalue(value)->dirty_mng;
    }
#endif

    return mng;
}

static void free_map_dirty_key(dirty_key_t *dk, int free)
{
    if (dk->dirty_op == DIRTY_DEL) {
        dirty_free(dk->key.del);
    } else {
        if (free) {
            dirty_free(dk->key.map_key);
        }
    }

    dirty_log("free_map_dirty_key: free=%d", free);
    dk->realkey = NULL;
    MEM_POOL_FREE(pool_key, dk);
}

static key_value_t *new_map_key_value(TValue *key)
{
    //根据key类型生成自己的key
    key_value_t *key_value = NULL;
    if (ttisstring(key)) {
        TString *ts = gco2ts(key);
        size_t len = ttisshrstring(key) ? ts->shrlen : ts->u.lnglen;
        key_value = (key_value_t *)dirty_malloc(sizeof(key_value_t) + len);
        memset(key_value, 0, sizeof(key_value));
        key_value->u.key_s.len = len;
        memcpy(key_value->u.key_s.contents, getstr(ts), len);
        key_value->u.key_s.contents[len] = '\0';
    } else if (ttisnumber(key)) {
        key_value = (key_value_t *)dirty_malloc(sizeof(key_value_t));
        memset(key_value, 0, sizeof(key_value));
        if (ttisfloat(key)) {
            key_value->u.key_n.n = fltvalue(key);
        } else {
            key_value->u.key_n.i = ivalue(key);
        }
    } else {
        dirty_log("db key type error: %d", ttype(key));
        assert(0);
    }
    key_value->tt_ = rawtt(key);

    return key_value;
}

static dirty_key_t *new_map_dirty_key(TValue *key, unsigned char op)
{
    key_value_t *key_value;
    dirty_key_t *dk;

    dk = (dirty_key_t *)MEM_POOL_ALLOC(pool_key);
    memset(dk, 0, sizeof(dk));
    dk->dirty_op = op;
    dk->realkey = key;

    //根据key类型生成自己的key
    key_value = new_map_key_value(key);
    if (op == DIRTY_DEL) {
        dk->key.del = key_value;
    } else {
        dk->key.map_key = key_value;
    }

    return dk;
}

//TODO 优化key的重写逻辑
static void overwrite_map_dirty_key(dirty_key_t *dk, TValue *key, unsigned char op)
{
    key_value_t *key_value;
    dk->realkey = key;
    if (dk->dirty_op == op) {
        return;
    }

    if (dk->dirty_op == DIRTY_DEL) {
        dirty_free(dk->key.del);
    } else {
        dirty_free(dk->key.map_key);
    }

    dk->dirty_op = op;
    key_value = new_map_key_value(key);
    if (op == DIRTY_DEL) {
        dk->key.del = key_value;
    } else {
        dk->key.map_key = key_value;
    }
}

static void dirty_root_init(dirty_root_t *dirty_root, TValue *root)
{
    (void)root;
    TAILQ_INIT(&dirty_root->dirty_node_list);
    dirty_root->node_cnt = 0;
}

static void dirty_root_add(dirty_root_t *dirty_root, dirty_node_t *dirty_node)
{
    TAILQ_INSERT_TAIL(&dirty_root->dirty_node_list, dirty_node, entry);
    dirty_root->node_cnt++;
}

static void dirty_root_remove(dirty_root_t *dirty_root, dirty_node_t *dirty_node)
{
    TAILQ_REMOVE(&dirty_root->dirty_node_list, dirty_node, entry);
    dirty_root->node_cnt--;
}

static dirty_node_t *new_dirty_node(dirty_manage_t *mng)
{
    dirty_manage_t *root_mng;
    dirty_node_t *dirty_node;

    dirty_node = (dirty_node_t *)MEM_POOL_ALLOC(pool_node);
    dirty_node->key_cnt = 0;
    TAILQ_INIT(&dirty_node->dirty_key_list);

    root_mng = get_manage(mng->root);
    dirty_root_add(root_mng->dirty_root, dirty_node);

    dirty_node->mng = mng;
    mng->dirty_node = dirty_node;

    return dirty_node;
}

inline static void destroy_dirty_node(dirty_manage_t *mng)
{
    dirty_root_t *dirty_root = get_manage(mng->root)->dirty_root;
    dirty_root_remove(dirty_root, mng->dirty_node);

    mng->dirty_node->mng = NULL;
    MEM_POOL_FREE(pool_node, mng->dirty_node);

    mng->dirty_node = NULL;
}

static void free_dirty_node(dirty_manage_t *mng)
{
    dirty_key_t *dk, *next;
    dirty_node_t *dirty_node;

    dirty_node = mng->dirty_node;
    switch (ttype(mng->self))
    {
    case LUA_TTABLE:
        TAILQ_FOREACH_SAFE(dk, &dirty_node->dirty_key_list, entry, next) {
            free_map_dirty_key(dk, 1);
        }
        break;
    
    default:
        break;
    }

    destroy_dirty_node(mng);
}

static void dump_dirty_info(dirty_manage_t *mng, dirty_key_t *dk, TValue *value, const char *msg)
{
    (void)mng;
    (void)dk;
    (void)value;
    (void)msg;
}

static void assert_attach(dirty_manage_t *dirty_mng, dirty_key_t *dk, TValue *value)
{
#ifdef DIRTY_DEBUG
    if (ttistable(value)) {
        dump_dirty_info(dirty_mng, dk, value , "try assert attach");
    }
#endif

    switch (ttype(value)) {
    case LUA_TTABLE:
        assert_attach_dirty_map_recurse(value);
        break;

    default:
        break;
    }
}

static void assert_detach(int op, TValue *value)
{
    if (op != DIRTY_ADD) {
        switch (ttype(value)) {
            case LUA_TTABLE:
                #ifdef DEBUG
                fprintf(stderr, "detach map.op=%d,svalue=%p", op, value);
                #endif
                break;

            default:
                break;
        }
    }
}

static void attach_node(TValue *value, TValue *parent, self_key_t *self_key)
{
    switch(ttype(value)) {
        case LUA_TTABLE:
            begin_dirty_manage_map(value, parent, self_key);
            break;
        default:
            dirty_free(self_key->map_key);
            break;
    }
}

//延迟处理，在清理脏数据的key的时候才把新增进来的mapping启动脏数据管理
static void clear_dirty_node(dirty_manage_t *mng)
{
    dirty_key_t *dk, *next;
    key_value_t *k;
    TValue *realkey;
    TValue *value;
    self_key_t self_key;

    dirty_node_t *dirty_node = mng->dirty_node;
    TValue *node = mng->self;

    assert(dirty_node);
    switch (ttype(node))
    {
    case LUA_TTABLE:
        TAILQ_FOREACH_SAFE(dk, &dirty_node->dirty_key_list, entry, next) {
            switch (dk->dirty_op)
            {
            case DIRTY_ADD:
            case DIRTY_SET: {
                k = dk->key.map_key;
                realkey = dk->realkey;
                self_key.map_key = k;
                value = luaH_get(hvalue(node), realkey);
                #ifdef DIRTY_MAP_CHECK
                assert_attach(mng, dk, value);
                #endif
                if (ttistable(value)) {
                    attach_node(value, node, &self_key);
                }
                break;
            }
            
            default:
                break;
            }

            free_map_dirty_key(dk, 0);
        }
        break;
    
    default:
        assert(0);
    }

    destroy_dirty_node(mng);
}

static dirty_key_t *dirty_node_insert_map_key(dirty_node_t *dirty_node, TValue *key, unsigned char op)
{
    dirty_key_t *dk = new_map_dirty_key(key, op);
    TAILQ_INSERT_TAIL(&dirty_node->dirty_key_list, dk, entry);
    dirty_node->key_cnt++;

    return dk;
}

static dirty_key_t *dirty_node_find_map_key(dirty_node_t *dirty_node, TValue *key)
{
    dirty_key_t *dk;
    TAILQ_FOREACH(dk, &dirty_node->dirty_key_list, entry) {
        if (dk->dirty_op == DIRTY_DEL) {
            if (samekey(dk->key.del, key)) {
                return dk;
            }
        } else {
            if (samekey(dk->key.map_key, key)) {
                return dk;
            }
        }
    }

    return NULL;
}

static void dirty_manage_init(dirty_manage_t *mng, TValue *self, TValue *root, TValue *parent, self_key_t *self_key)
{
    mng->root = root;
    mng->self = self;
    mng->parent = parent;
    if (self_key) {
        mng->self_key = *self_key;
    } else {
        memset(&mng->self_key, 0, sizeof(self_key_t));
    }

    mng->dirty_node = NULL;
}

static void free_dirty_manage(dirty_manage_t *mng)
{
    int is_root = is_dirty_root(mng);
    if (mng->dirty_node) {
        free_dirty_node(mng);
    }
    mng->root = NULL;
    mng->parent = NULL;
    mng->self = NULL;
    if (mng->self_key.map_key) {
        dirty_free(mng->self_key.map_key);
    }

    if (is_root) {
        MEM_POOL_FREE(pool_manage_root, mng);
    } else {
        MEM_POOL_FREE(pool_manage, mng);
    }
}

static dirty_manage_t *new_dirty_manage(TValue *self, TValue *parent, self_key_t *self_key)
{
    dirty_manage_t *mng;
    TValue *root;
    if (parent == NULL) {
        //root
        mng = (dirty_manage_t *)MEM_POOL_ALLOC(pool_manage_root);
        dirty_manage_init(mng, self, self, NULL, NULL);
        dirty_root_init(mng->dirty_root, self);
    } else {
        mng = (dirty_manage_t *)MEM_POOL_ALLOC(pool_manage);
        root = get_manage(parent)->root;
        dirty_manage_init(mng, self, root, parent, self_key);
    }

    return mng;
}

static void clear_dirty_manage(dirty_manage_t *mng)
{
    if (mng != NULL) {
        if (mng->dirty_node) {
            clear_dirty_node(mng);
        }
    }

    //need not to handle dirty_root
    //dirty_root is affected by dirty_node of manage
}

static void detach_node(int op, TValue *value)
{
    //todo check the added content if dirty
    //op == DIRTY_ADD has to delay, we do not know what will be added.
    //此刻不知道ADD或者SET进来的数据是什么，要延迟做检查处理。
#ifdef DIRTY_MAP_CHECK
    assert_detach(op, value);
#endif
    if (op != DIRTY_ADD) {
        switch (ttype(value))
        {
        case LUA_TTABLE:
            free_dirty_map_recurse(value);
            break;
        
        default:
            break;
        }

    }

}

static void map_accept_dirty_key(Table *map, TValue *key, unsigned char op, TValue *value)
{
    dirty_manage_t *mng = NULL;
    dirty_key_t *dk;
#ifdef USE_DIRTY_DATA
    mng = map->dirty_mng;
#endif

    if (mng->dirty_node == NULL) {
        new_dirty_node(mng);
    }

    //Is has the same key ?
    dk = dirty_node_find_map_key(mng->dirty_node, key);
    if (dk == NULL) {
        dk = dirty_node_insert_map_key(mng->dirty_node, key, op);
        detach_node(op, value);
    } else {
        detach_node(op, value);
        overwrite_map_dirty_key(dk, key, op);
    }
}

void set_dirty_map(TValue *svmap, TValue *key, TValue *value, unsigned char op)
{
#ifdef USE_DIRTY_DATA
    assert(ttistable(svmap));
    if (hvalue(svmap)->dirty_mng) {
        map_accept_dirty_key(hvalue(svmap), key, op, value);
    }
#endif
}

static void clear_dirty_map(TValue *map)
{
#ifdef USE_DIRTY_DATA
    assert(ttistable(map));
    if (hvalue(map)->dirty_mng) {
        clear_dirty_manage(hvalue(map)->dirty_mng);
    }
#endif
}

void free_dirty_map(Table *map)
{
#ifdef USE_DIRTY_DATA
    if (map->dirty_mng) {
        free_dirty_manage(map->dirty_mng);
        map->dirty_mng = NULL;
    }
#endif
}

static void traverse_mapping(TValue *map, traverse_cb_t cb) 
{
    //traverse table and assert
    Table *t = hvalue(map);
    Node *n, *limit = GET_NODELAST(t);
    TValue *v;
    unsigned int i;
    unsigned int asize = luaH_realasize(t);
    //traverse array part
    for (i = 0; i < asize; i++) {
        v = &t->array[i];
        if (isempty(v)) {
            continue;
        }

        switch (ttype(v))
        {
        case LUA_TTABLE:
            cb(v);
            break;
        
        default:
            break;
        }
    }

    //traverse hash part
    for (n = gnode(t, 0); n < limit; n++) {
        if (isempty(gval(n))) {
            continue;
        }

        switch (ttype(gval(n)))
        {
        case LUA_TTABLE:
            cb(gval(n));
            break;
        
        default:
            break;
        }
    }

}

static void assert_attach_dirty_map_recurse(TValue *svmap)
{
    Node *n, *limit;
    Table *t;
    TValue *v;
    unsigned int i, asize;

    t = hvalue(svmap);
    assert(ttistable(svmap));
#ifdef USE_DIRTY_DATA
    if (t->dirty_mng) {
        dump_dirty_info(t->dirty_mng, NULL, svmap, "assert fail map from");
    } else {
    #ifdef DIRTY_DUMP_DEBUG
        dirty_log("assert ok attach map. tvalue=%p,contents=", svmap);
    #endif
    }

    assert(t->dirty_mng == NULL);
#endif

    //traverse table and assert
    limit = GET_NODELAST(t);
    asize = luaH_realasize(t);
    //traverse array part
    for (i = 0; i < asize; i++) {
        v = &t->array[i];
        if (isempty(v)) {
            continue;
        }

        switch (ttype(v))
        {
        case LUA_TTABLE:
            assert_attach_dirty_map_recurse(v);
            break;
        
        default:
            break;
        }
    }

    //traverse hash part
    for (n = gnode(t, 0); n < limit; n++) {
        if (isempty(gval(n))) {
            continue;
        }

        switch (ttype(gval(n)))
        {
        case LUA_TTABLE:
            assert_attach_dirty_map_recurse(gval(n));
            break;
        
        default:
            break;
        }
    }

}

static void clear_dirty_map_recurse(TValue *map)
{
    clear_dirty_map(map);

    //traverse table and clear dirty map
    traverse_mapping(map, clear_dirty_map_recurse);
}

static void free_dirty_map_recurse(TValue *map)
{
    traverse_mapping(map, free_dirty_map_recurse);

    //root要最后清除,但这样就不能尾递归了
    free_dirty_map(map);
}

void begin_dirty_manage_map(TValue *svmap, TValue *parent, self_key_t *key)
{
    self_key_t self_key;
    Table *map;
    Node *n, *limit;
    TValue *v, *node_key = NULL;
    unsigned int i, asize;

    if (ttype(svmap) != LUA_TTABLE) {
        if (key) {
            dirty_free(key->map_key);
        }
        return;
    }

    map = hvalue(svmap);
#ifdef USE_DIRTY_DATA
    if (map->dirty_mng) {
        if (key) {
            dirty_free(key->map_key);
        }
        return;
    }

    map->dirty_mng = new_dirty_manage(svmap, parent, key);
#endif

    //traverse table and assert
    limit = GET_NODELAST(map);
    asize = luaH_realasize(map);
    //traverse array part
    for (i = 0; i < asize; i++) {
        v = &map->array[i];
        if (isempty(v)) {
            continue;
        }

        switch (ttype(v))
        {
        case LUA_TTABLE: {
            self_key.map_key = (key_value_t *)dirty_malloc(sizeof(key_value_t));
            self_key.map_key->tt_ = LUA_VNUMINT;
            self_key.map_key->u.key_n.i = i + 1; //加1是因为lua的数组从1开始
            begin_dirty_manage_map(v, svmap, &self_key);
            break;
        }
        
        default:
            break;
        }
    }

    //traverse hash part
    for (n = gnode(map, 0); n < limit; n++) {
        if (isempty(gval(n))) {
            continue;
        }

        switch (ttype(gval(n)))
        {
        case LUA_TTABLE:
            node_key->value_ = n->u.key_val;
            node_key->tt_ = n->u.key_tt;
            self_key.map_key = new_map_key_value(node_key);
            begin_dirty_manage_map(gval(n), svmap, &self_key);
            break;
        
        default:
            break;
        }
    }
}

void clear_dirty(TValue *svmap)
{
    TValue *node;
    dirty_node_t *dirty_node, *next;
    dirty_root_t *dirty_root;

    dirty_manage_t *mng = get_manage(svmap);
    if (mng != NULL) {
        assert(is_dirty_root(mng));
        dirty_root = mng->dirty_root;
        TAILQ_FOREACH_SAFE(dirty_node, &dirty_root->dirty_node_list, entry, next) {
            node = dirty_node->mng->self;
            switch (ttype(node))
            {
            case LUA_TTABLE:
                clear_dirty_map(node);
                break;
            
            default:
                break;
            }
        }
    }
}