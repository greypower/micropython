#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "nlr.h"
#include "misc.h"
#include "mpconfig.h"
#include "obj.h"
#include "runtime0.h"
#include "runtime.h"

typedef struct _mp_obj_str_t {
    mp_obj_base_t base;
    qstr qstr;
} mp_obj_str_t;

void str_print(void (*print)(void *env, const char *fmt, ...), void *env, mp_obj_t self_in) {
    mp_obj_str_t *self = self_in;
    // TODO need to escape chars etc
    print(env, "'%s'", qstr_str(self->qstr));
}

mp_obj_t str_binary_op(int op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    mp_obj_str_t *lhs = lhs_in;
    const char *lhs_str = qstr_str(lhs->qstr);
    switch (op) {
        case RT_BINARY_OP_SUBSCR:
            // string access
            // XXX a massive hack!
            return mp_obj_new_int(lhs_str[mp_obj_get_int(rhs_in)]);

        case RT_BINARY_OP_ADD:
        case RT_BINARY_OP_INPLACE_ADD:
            if (MP_OBJ_IS_TYPE(rhs_in, &str_type)) {
                // add 2 strings
                const char *rhs_str = qstr_str(((mp_obj_str_t*)rhs_in)->qstr);
                int alloc_len = strlen(lhs_str) + strlen(rhs_str) + 1;
                char *val = m_new(char, alloc_len);
                stpcpy(stpcpy(val, lhs_str), rhs_str);
                return mp_obj_new_str(qstr_from_str_take(val, alloc_len));
            }
            break;
    }

    return MP_OBJ_NULL; // op not supported
}

mp_obj_t str_join(mp_obj_t self_in, mp_obj_t arg) {
    assert(MP_OBJ_IS_TYPE(self_in, &str_type));
    mp_obj_str_t *self = self_in;
    int required_len = strlen(qstr_str(self->qstr));

    // process arg, count required chars
    uint seq_len;
    mp_obj_t *seq_items;
    if (MP_OBJ_IS_TYPE(arg, &tuple_type)) {
        mp_obj_tuple_get(arg, &seq_len, &seq_items);
    } else if (MP_OBJ_IS_TYPE(arg, &list_type)) {
        mp_obj_list_get(arg, &seq_len, &seq_items);
    } else {
        goto bad_arg;
    }
    for (int i = 0; i < seq_len; i++) {
        if (!MP_OBJ_IS_TYPE(seq_items[i], &str_type)) {
            goto bad_arg;
        }
        required_len += strlen(qstr_str(mp_obj_str_get(seq_items[i])));
    }

    // make joined string
    char *joined_str = m_new(char, required_len + 1);
    joined_str[0] = 0;
    for (int i = 0; i < seq_len; i++) {
        const char *s2 = qstr_str(mp_obj_str_get(seq_items[i]));
        if (i > 0) {
            strcat(joined_str, qstr_str(self->qstr));
        }
        strcat(joined_str, s2);
    }
    return mp_obj_new_str(qstr_from_str_take(joined_str, required_len + 1));

bad_arg:
    nlr_jump(mp_obj_new_exception_msg(rt_q_TypeError, "?str.join expecting a list of str's"));
}

void vstr_printf_wrapper(void *env, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vstr_vprintf(env, fmt, args);
    va_end(args);
}

mp_obj_t str_format(int n_args, const mp_obj_t *args) {
    assert(MP_OBJ_IS_TYPE(args[0], &str_type));
    mp_obj_str_t *self = args[0];

    const char *str = qstr_str(self->qstr);
    int arg_i = 1;
    vstr_t *vstr = vstr_new();
    for (; *str; str++) {
        if (*str == '{') {
            str++;
            if (*str == '{') {
                vstr_add_char(vstr, '{');
            } else if (*str == '}') {
                if (arg_i >= n_args) {
                    nlr_jump(mp_obj_new_exception_msg(rt_q_IndexError, "tuple index out of range"));
                }
                mp_obj_print_helper(vstr_printf_wrapper, vstr, args[arg_i]);
                arg_i++;
            }
        } else {
            vstr_add_char(vstr, *str);
        }
    }

    return mp_obj_new_str(qstr_from_str_take(vstr->buf, vstr->alloc));
}

static MP_DEFINE_CONST_FUN_OBJ_2(str_join_obj, str_join);
static MP_DEFINE_CONST_FUN_OBJ_VAR(str_format_obj, 1, str_format);

const mp_obj_type_t str_type = {
    { &mp_const_type },
    "str",
    str_print, // print
    NULL, // call_n
    NULL, // unary_op
    str_binary_op, // binary_op
    NULL, // getiter
    NULL, // iternext
    { // method list
        { "join", &str_join_obj },
        { "format", &str_format_obj },
        { NULL, NULL }, // end-of-list sentinel
    },
};

mp_obj_t mp_obj_new_str(qstr qstr) {
    mp_obj_str_t *o = m_new_obj(mp_obj_str_t);
    o->base.type = &str_type;
    o->qstr = qstr;
    return o;
}

qstr mp_obj_str_get(mp_obj_t self_in) {
    assert(MP_OBJ_IS_TYPE(self_in, &str_type));
    mp_obj_str_t *self = self_in;
    return self->qstr;
}
