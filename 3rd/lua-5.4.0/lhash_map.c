#include "lhash_map.h"
#include "khash.h"
#include <assert.h>

KHASH_MAP_INIT_INT64(hash_int64, void*);
typedef khash_t(hash_int64) hash_map_int64_t;

HASH_MAP_INLINE hash_map_t hash_int64_create() {
	return kh_init(hash_int64);
}

HASH_MAP_INLINE void hash_int64_release(hash_map_t hash) {
	kh_clear(hash_int64, hash);
	kh_destroy(hash_int64, hash);
}

HASH_MAP_INLINE void hash_int64_set(hash_map_t hash, int64_t k, void* v) {
	int ok;
	khiter_t itr = kh_put(hash_int64, hash, k, &ok);
	assert(ok == 1 || ok == 2);
	kh_value((hash_map_int64_t*)hash, itr) = v;
}

HASH_MAP_INLINE void hash_int64_del(hash_map_t hash, int64_t k) {
	khiter_t itr = kh_get(hash_int64, hash, k);
	assert(itr != kh_end((hash_map_int64_t*)hash));
	kh_del(hash_int64, hash, itr);
}

HASH_MAP_INLINE bool hash_int64_exist(hash_map_t hash, int64_t k) {
	khiter_t itr = kh_get(hash_int64, hash, k);
	return itr != kh_end((hash_map_int64_t*)hash);
}

HASH_MAP_INLINE void* hash_int64_find(hash_map_t hash, int64_t k) {
	khiter_t itr = kh_get(hash_int64, hash, k);
	return itr == kh_end((hash_map_int64_t*)hash) ? NULL : kh_value((hash_map_int64_t*)hash, itr);
}

HASH_MAP_INLINE void hash_int64_foreach(hash_map_t hash, hash_int64_foreach_func func, void* ud) {
	int64_t k = 0;
	void* v = NULL;
	kh_foreach((hash_map_int64_t*)hash, k, v, {
		func(k, v, ud);
	});
}

HASH_MAP_INLINE size_t hash_int64_size(hash_map_t hash) {
	return kh_size((hash_map_int64_t*)hash);
}

KHASH_MAP_INIT_STR(hash_str, void*);
typedef khash_t(hash_str) hash_map_str_t;

HASH_MAP_INLINE hash_map_t hash_str_create() {
	return kh_init(hash_str);
}

HASH_MAP_INLINE void hash_str_release(hash_map_t hash) {
	kh_clear(hash_str, hash);
	kh_destroy(hash_str, hash);
}

HASH_MAP_INLINE void hash_str_set(hash_map_t hash, char* k, void* v) {
	int ok;
	size_t len = strlen(k);
	char *buf = (char *)malloc(sizeof(char) * len + 1);
	memcpy(buf, k, len);
	buf[len] = '\0';
	khiter_t itr = kh_put(hash_str, hash, buf, &ok);
	assert(ok == 1 || ok == 2);
	kh_value((hash_map_str_t*)hash, itr) = v;
}

HASH_MAP_INLINE void hash_str_del(hash_map_t hash, char* k) {
	khiter_t itr = kh_get(hash_str, hash, k);
	assert(itr != kh_end((hash_map_str_t*)hash));
	kh_del(hash_str, hash, itr);
}

HASH_MAP_INLINE bool hash_str_exist(hash_map_t hash, char* k) {
	khiter_t itr = kh_get(hash_str, hash, k);
	return itr != kh_end((hash_map_str_t*)hash);
}

HASH_MAP_INLINE void* hash_str_find(hash_map_t hash, char* k) {
	khiter_t itr = kh_get(hash_str, hash, k);
	return itr == kh_end((hash_map_str_t*)hash) ? NULL : kh_value((hash_map_str_t*)hash, itr);
}

HASH_MAP_INLINE void hash_str_foreach(hash_map_t hash, hash_str_foreach_func func, void* ud) {
	const char* k = NULL;
	void* v = NULL;
	kh_foreach((hash_map_str_t*)hash, k, v, {
		func((char*)k, v, ud);
	});
}

HASH_MAP_INLINE size_t hash_str_size(hash_map_t hash) {
	return kh_size((hash_map_str_t*)hash);
}