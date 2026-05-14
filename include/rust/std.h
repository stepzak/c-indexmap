#ifndef RUST_H
#define RUST_H

#include <assert.h>

#define DECLARE_RESULT(T, E) \
    struct __Result_ ## T ## _ ## E { \
        bool ok; \
        union { T res; E err; }; \
    }

#define Result(T, E) struct __Result_ ## T ## _ ## E

#define Ok(val) { .ok = true, .res = (val) }
#define Err(val) { .ok = false, .res = (val) }

#define is_ok(val) val.ok
#define is_err(val) !val.ok

#define unwrap(val) ({ \
    typeof(val) _val = (val); \
    assert(_val.ok && "Tried to unwrap an error!"); \
    _val.res; })

#define unwrap_err(val) ({ \
    typeof(val) _val = val; \
    assert(!_val.ok && "Tried to unwrap error but value was OK"); \
    _val.err; })



#define DECLARE_OPTION(T) \
    struct __Option_ ## T { \
        bool has_value; \
        T value; \
    }

#define Option(T) struct __Option_ ## T

#define Some(val) { .has_value = true, .value = (val) }
#define None { .has_value = false }

#define is_some(val) ((val).has_value)
#define is_none(val) (!(val).has_value)

#define unwrap_opt(val) ({ \
    typeof(val) _val = (val); \
    assert(_val.has_value && "Tried to unwrap None!"); \
    _val.value; \
})


#endif //RUST_H
