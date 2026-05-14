#include <stddef.h>
#include <stdint.h>
#include "../include/im.h"

#include <string.h>

#if UINTPTR_MAX == 0xffffffff
    #define FNV_OFFSET 2166136261U
    #define FNV_PRIME  16777619U
#else
    #define FNV_OFFSET 14695981039346656037ULL
    #define FNV_PRIME  1099511628211ULL
#endif

#if defined(__GNUC__) && __GNUC__ >= 3 || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) !!(x)
#endif

static size_t im_hash(const void* key, size_t len) {
    const unsigned char* p = (const unsigned char*)key;
    size_t hash = FNV_OFFSET;
    
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

IM_Status im_reserve_impl(void** im, size_t new_cap, size_t entry_size) {
    IM_Header* h = (*im) ? im_header(*im) : NULL;

    size_t cap = IM_INIT_CAP;
    while (cap < new_cap) cap <<= 1;

    size_t min_buckets_cap = (size_t)((float)cap / IM_LOAD_FACTOR) + 1;
    size_t new_buckets_cap = IM_INIT_CAP;
    while (new_buckets_cap < min_buckets_cap) new_buckets_cap <<= 1;

    if (h && new_buckets_cap <= h->buckets_cap) {
        if (vector_reserve_impl(im, entry_size, cap, sizeof(IM_Header)) != CVEC_SUCCESS)
            return IM_BAD_ALLOC;
        return IM_OK;
    }

    IM_Bucket* old_buckets = h ? h->buckets : NULL;
    size_t old_buckets_cap = h ? h->buckets_cap : 0;

    IM_Bucket* new_buckets = calloc(new_buckets_cap, sizeof(IM_Bucket));
    if (unlikely(!new_buckets)) return IM_BAD_ALLOC;
    for (size_t i = 0; i < new_buckets_cap; i++) new_buckets[i].handle.dense_idx = IM_EMPTY;

    if (vector_reserve_impl(im, entry_size, cap, sizeof(IM_Header)) != CVEC_SUCCESS) {
        free(new_buckets);
        return IM_BAD_ALLOC;
    }

    if (old_buckets) {
        for (size_t i = 0; i < old_buckets_cap; i++) {
            if (old_buckets[i].handle.dense_idx != IM_EMPTY) {
                size_t n_idx = old_buckets[i].hash & (new_buckets_cap - 1);
                while (new_buckets[n_idx].handle.dense_idx != IM_EMPTY) {
                    n_idx = (n_idx + 1) & (new_buckets_cap - 1);
                }
                new_buckets[n_idx] = old_buckets[i];
            }
        }
        free(old_buckets);
    }

    h = im_header(*im);
    h->buckets = new_buckets;
    h->buckets_cap = new_buckets_cap;

    return IM_OK;
}

static IM_Status im_resize(void** im, size_t entry_size) {
    if (*im == NULL) {
        if (im_reserve_impl(im, IM_INIT_CAP, entry_size) != IM_OK) return IM_BAD_ALLOC;
        return IM_OK;
    }
    IM_Header* h = im_header(*im);
    if ((float)(h->buckets_len + 1) / h->buckets_cap >= IM_LOAD_FACTOR) {
        if (im_reserve_impl(im, v_cap(*im) * 2, entry_size) != IM_OK) return IM_BAD_ALLOC;
        h = im_header(*im);
    }
    return IM_OK;
}

Result(im_dense_idx, IM_Status) im_insert_impl(
    void** im,
    void* key,
    void* value,
    size_t value_size,
    size_t entry_size,
    size_t value_offset,
    size_t gen_offset,
    size_t hash_offset) {

    if (*im == NULL) {
        if (im_resize(im, entry_size) != IM_OK)
            return (Result(im_dense_idx, IM_Status)) Err(IM_BAD_ALLOC);
    }

    IM_Header* h = im_header(*im);

    if ((float)(h->buckets_len + 1) / h->buckets_cap >= IM_LOAD_FACTOR) {
        if (im_resize(im, entry_size) != IM_OK)
            return (Result(im_dense_idx, IM_Status)) Err(IM_BAD_ALLOC);
        h = im_header(*im);
    }

    size_t key_size = value_offset;
    size_t hash = im_hash(key, key_size);
    size_t cap = h->buckets_cap;
    size_t idx = hash & (cap - 1);

    while (h->buckets[idx].handle.dense_idx != IM_EMPTY) {
        size_t d_idx = h->buckets[idx].handle.dense_idx;
        void* entry_ptr = (char*)(*im) + (d_idx * entry_size);
        if (memcmp(entry_ptr, key, key_size) == 0) {
            void* existing_value = (char*)entry_ptr + value_offset;
            memcpy(existing_value, value, value_size);
            return (Result(im_dense_idx, IM_Status)) Ok(d_idx);
        }
        idx = (idx + 1) & (cap - 1);
    }

    char temp_entry[entry_size];
    memset(temp_entry, 0, entry_size);
    memcpy(temp_entry, key, key_size);
    memcpy(temp_entry + value_offset, value, value_size);

    size_t new_gen = h->buckets[idx].handle.generation;
    memcpy(temp_entry + gen_offset, &new_gen, sizeof(size_t));
    memcpy(temp_entry + hash_offset, &hash, sizeof(size_t));

    if (v_push_raw(im, (void*)temp_entry, entry_size, sizeof(IM_Header)) != CVEC_SUCCESS) {
        return (Result(im_dense_idx, IM_Status)) Err(IM_BAD_ALLOC);
    }

    h = im_header(*im);

    h->buckets[idx].hash = hash;
    h->buckets[idx].handle.dense_idx = v_len(*im) - 1;
    h->buckets[idx].handle.generation = new_gen;
    h->buckets_len++;

    return (Result(im_dense_idx, IM_Status)) Ok(v_len(*im) - 1);
}

Option(im_dense_idx) im_get_impl(const void* im, void* key, size_t key_size, size_t entry_size) {
    if (im == NULL) return (Option(im_dense_idx)) None;

    IM_Header* h = im_header(im);
    size_t hash = im_hash(key, key_size);
    size_t cap = h->buckets_cap;
    size_t idx = hash & (cap - 1);

    while (h->buckets[idx].handle.dense_idx != IM_EMPTY) {
        if (h->buckets[idx].hash == hash) {
            size_t d_idx = h->buckets[idx].handle.dense_idx;
            void* entry_ptr = (char*)(im) + (d_idx * entry_size);

            if (memcmp(entry_ptr, key, key_size) == 0) {
                return (Option(im_dense_idx)) Some(d_idx);
            }
        }
        idx = (idx + 1) & (cap - 1);
    }
    return (Option(im_dense_idx)) None;

}

bool im_remove_impl(void** im, void* key, size_t key_size, size_t entry_size, size_t hash_offset, void* out_entry) {
    if (im == NULL || *im == NULL) return false;

    IM_Header* h = im_header(*im);
    size_t hash = im_hash(key, key_size);
    size_t cap = h->buckets_cap;
    size_t idx = hash & (cap - 1);
    while (h->buckets[idx].handle.dense_idx != IM_EMPTY) {
        if (h->buckets[idx].hash == hash) {
            size_t d_idx = h->buckets[idx].handle.dense_idx;
            void* entry_ptr = (char*)(*im) + (d_idx * entry_size);
            if (memcmp(entry_ptr, key, key_size) == 0) {
                if (out_entry != NULL) memcpy(out_entry, entry_ptr, entry_size);

                size_t last_idx = v_len(*im) - 1;
                if (d_idx != last_idx) {
                    void* last_entry_ptr = (char*)(*im) + (last_idx * entry_size);
                    size_t last_hash;
                    memcpy(&last_hash, (char*)last_entry_ptr + hash_offset, sizeof(size_t));
                    size_t last_b_idx = last_hash & (cap - 1);

                    while (h->buckets[last_b_idx].handle.dense_idx != last_idx) {
                        last_b_idx = (last_b_idx + 1) & (cap - 1);
                    }
                    h->buckets[last_b_idx].handle.dense_idx = d_idx;

                    memcpy(entry_ptr, last_entry_ptr, entry_size);
                }
                h->buckets[idx].handle.dense_idx = IM_EMPTY;
                h->buckets[idx].handle.generation++;
                h->buckets_len--;
                v_pop(*im);
                return true;
            }
        }
        idx = (idx + 1) & (cap - 1);
    }
    return false;
}

void im_clear_impl(void* im, size_t entry_size, void (*dtor_entry)(void*)) {
    if (im == NULL) return;
    IM_Header* h = im_header(im);
    if (dtor_entry != NULL) {
        for (size_t i = 0; i < v_len(im); i++) {
            void* entry_ptr = (char*)(im) + (i * entry_size);
            dtor_entry(entry_ptr);
        }
    }
    v_header(im)->length = 0;
    h->buckets_len = 0;
    for (size_t i = 0; i < h->buckets_cap; i++) {
        h->buckets[i].handle.dense_idx = IM_EMPTY;
        h->buckets[i].handle.generation++;
    }
}

void im_free_impl(void** im, size_t entry_size, void (*dtor_entry)(void*)) {
    if (im == NULL || *im == NULL) return;

    im_clear_impl(*im, entry_size, dtor_entry);

    IM_Header* h = im_header(*im);
    if (h->buckets) {
        free(h->buckets);
        h->buckets = NULL;
    }

    free(h);

    *im = NULL;
}

#define im_foreach(im, var_name) \
    for (typeof(*(im))* var_name = (im); \
        (im) && var_name < (im) + im_len(im); \
        var_name++)