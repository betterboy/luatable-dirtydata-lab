#ifndef lhash_map_h
#define lhash_map_h
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef HASH_MAP_INLINE
#define HASH_MAP_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif
	typedef void* hash_map_t;
	typedef void(*hash_str_foreach_func)(char*, void*, void*);
	typedef void(*hash_int64_foreach_func)(int64_t, void*, void*);

	HASH_MAP_INLINE hash_map_t hash_str_create();
	HASH_MAP_INLINE void hash_str_release(hash_map_t hash);
	HASH_MAP_INLINE void hash_str_set(hash_map_t hash, char* k, void* v);
	HASH_MAP_INLINE void hash_str_del(hash_map_t hash, char* k);
	HASH_MAP_INLINE bool hash_str_exist(hash_map_t hash, char* k);
	HASH_MAP_INLINE void* hash_str_find(hash_map_t hash, char* k);
	HASH_MAP_INLINE void hash_str_foreach(hash_map_t hash, hash_str_foreach_func func, void* ud);
	HASH_MAP_INLINE size_t hash_str_size(hash_map_t hash);

	HASH_MAP_INLINE hash_map_t hash_int64_create();
	HASH_MAP_INLINE void hash_int64_release(hash_map_t hash);
	HASH_MAP_INLINE void hash_int64_set(hash_map_t hash, int64_t k, void* v);
	HASH_MAP_INLINE void hash_int64_del(hash_map_t hash, int64_t k);
	HASH_MAP_INLINE bool hash_int64_exist(hash_map_t hash, int64_t k);
	HASH_MAP_INLINE void* hash_int64_find(hash_map_t hash, int64_t k);
	HASH_MAP_INLINE void hash_int64_foreach(hash_map_t hash, hash_int64_foreach_func func, void* ud);
	HASH_MAP_INLINE size_t hash_int64_size(hash_map_t hash);
#ifdef __cplusplus
}
#endif
#endif