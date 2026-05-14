#ifndef IM_H
#define IM_H

#include <stddef.h>
#include <stdint.h>

#include "rust/std.h"
#include <cvec/cvec.h>

#define IM_LOAD_FACTOR 0.7
#define IM_EMPTY SIZE_MAX
#define IM_INIT_CAP 8

typedef size_t im_dense_idx;

typedef struct IM_Handle {
    size_t generation;
    im_dense_idx dense_idx; //SIZE_MAX = EMPTY
} IM_Handle;

typedef struct {
    IM_Handle handle;
    size_t hash;
} IM_Bucket;

typedef struct {
    IM_Bucket *buckets;
    size_t buckets_cap;
    size_t buckets_len;
} __attribute__((aligned(sizeof(size_t)))) IM_Header;

typedef enum {
    IM_OK,
    IM_BAD_ALLOC
} IM_Status;

#define IM_Entry(K, V) struct { \
    K key;                      \
    V value;                    \
    size_t generation;          \
    size_t hash;                \
}
///WARNING: DOES NOT WORK WITH char*, use constant char buffers: char[n]
#define IndexMap(K, V) IM_Entry(K, V)*
#define im_header(im) ((IM_Header*) (v_header(im)) - 1)
#define im_len(im) v_len(im)
#define im_buckets_cap(im) ((im) ? (im_header(im)->buckets_cap) : 0)
#define im_buckets_len(im) ( (im) ? (im_header(im)->buckets_len) : 0)
#define im_buckets(im) im_header(im)->buckets

DECLARE_RESULT(im_dense_idx, IM_Status);
DECLARE_OPTION(im_dense_idx);

Result(im_dense_idx, IM_Status) im_insert_impl(
    void** im,
    void* key,
    void* value,
    size_t value_size,
    size_t entry_size,
    size_t value_offset,
    size_t gen_offset,
    size_t hash_offset);

Option(im_dense_idx) im_get_impl(const void* im, void* key, size_t key_size, size_t entry_size);
bool im_remove_impl(void** im,
    void* key,
    size_t key_size,
    size_t entry_size,
    size_t hash_offset,
    void* out_entry);

void im_clear_impl(void* im, size_t entry_size, void (*dtor_entry)(void*));
void im_free_impl(void** im, size_t entry_size, void (*dtor_entry)(void*));
IM_Status im_reserve_impl(void** im, size_t new_cap, size_t entry_size);


#define im_insert(im, key, val) im_insert_impl( \
    (void**)&(im), \
    (void*)&(key), \
    (void*)&(val), \
    sizeof(typeof( ((typeof(im))0)->value)), \
    sizeof(typeof(*(im))), \
    offsetof(typeof(*(im)), value), \
    offsetof(typeof(*(im)), generation), \
    offsetof(typeof(*(im)), hash) \
)

#define im_get(im, k) im_get_impl(im, (void*)&k, sizeof(typeof( ((typeof(im))0)->key)), sizeof(typeof(*(im))))

#define im_remove(im, k, out_entry_ptr) im_remove_impl( \
    (void**)&(im), \
    (void*)&(k), \
    sizeof(((typeof(im))0)->key), \
    sizeof(typeof(*(im))), \
    offsetof(typeof(*(im)), hash), \
    out_entry_ptr \
)

#define im_clear_dtor(im, dtor_entry) im_clear_impl((void*)(im), sizeof(typeof(*(im))), dtor_entry)
#define im_free_dtor(im, dtor_entry) im_free_impl((void**)&(im), sizeof(typeof(*(im))), dtor_entry)

#define im_clear(im) im_clear_dtor(im, NULL)
#define im_free(im) im_free_dtor(im, NULL)

#define im_reserve(im, cap) im_reserve_impl((void**)&(im), cap, sizeof(typeof(*(im))))

#endif //IM_H
