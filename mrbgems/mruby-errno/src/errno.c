#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <mruby/presym.h>

static struct {
  mrb_sym sym;
  int eno;
} e2c[] = {
#include "known_errors_e2c.cstub"
  {0, 0},
};

static void
mrb_sce_init(mrb_state *mrb, mrb_value self, mrb_value m, mrb_int n)
{
  mrb_value str;
  struct RClass *m_errno;
  int no_errno = (n < 0);
  char buf[20];

  m_errno = mrb_module_get_id(mrb, MRB_SYM(Errno));
  if (!no_errno) {
    int i;

    for (i=0; e2c[i].sym != 0; i++) {
      if (e2c[i].eno == n) {
        mrb_basic_ptr(self)->c = mrb_class_get_under_id(mrb, m_errno, e2c[i].sym);
        str = mrb_str_new_cstr(mrb, strerror(n));
        break;
      }
    }
    if (e2c[i].sym == 0) {
      mrb_iv_set(mrb, self, MRB_SYM(errno), mrb_fixnum_value(n));
      str = mrb_str_new_cstr(mrb, "Unknown error: ");
      snprintf(buf, sizeof(buf), "%d", (int)n);
      mrb_str_cat2(mrb, str, buf);
    }
  }
  else {
    str = mrb_str_new_cstr(mrb, "unknown error");
  }
  if (!mrb_nil_p(m)) {
    mrb_str_cat2(mrb, str, " - ");
    mrb_str_append(mrb, str, m);
  }
  mrb_iv_set(mrb, self, MRB_SYM(mesg), str);
}

static mrb_value
mrb_sce_init_m(mrb_state *mrb, mrb_value self)
{
  mrb_value m;
  mrb_int n;

  if (mrb_get_args(mrb, "o|i", &m, &n) == 1) {
    if (mrb_fixnum_p(m)) {
      n = mrb_fixnum(m);
      m = mrb_nil_value();
    }
    else {
      n = -1;
    }
  }
  mrb_sce_init(mrb, self, m, n);
  return self;
}

static mrb_value
mrb_sce_errno(mrb_state *mrb, mrb_value self)
{
  struct RClass *c;
  mrb_sym sym;

  c = mrb_class(mrb, self);
  sym = MRB_SYM(Errno);
  if (mrb_const_defined_at(mrb, mrb_obj_value(c), sym)) {
    return mrb_const_get(mrb, mrb_obj_value(c), sym);
  } else {
    sym = MRB_SYM(errno);
    return mrb_attr_get(mrb, self, sym);
  }
}

static mrb_value
mrb_sce_to_s(mrb_state *mrb, mrb_value self)
{
  return mrb_attr_get(mrb, self, MRB_SYM(mesg));
}

static mrb_value
mrb_sce_sys_fail(mrb_state *mrb, mrb_value cls)
{
  struct RClass *sce;
  mrb_value msg;
  mrb_int no = -1;
  int argc;

  mrb->c->ci->mid = 0;
  sce = mrb_class_get_id(mrb, MRB_SYM(SystemCallError));
  argc = mrb_get_args(mrb, "i|S", &no, &msg);

  struct RBasic* e = mrb_obj_alloc(mrb, MRB_TT_EXCEPTION, sce);
  mrb_value exc = mrb_obj_value(e);
  if (argc == 1) {
    msg = mrb_nil_value();
  }
  exc = mrb_obj_value(e);
  mrb_sce_init(mrb, exc, msg, no);
  mrb_exc_raise(mrb, exc);
  return mrb_nil_value();  /* NOTREACHED */
}

static mrb_value
mrb_exxx_init(mrb_state *mrb, mrb_value self)
{
  mrb_value m, no, str;

  no = mrb_const_get(mrb, mrb_obj_value(mrb_class(mrb, self)), MRB_SYM(Errno));
  str = mrb_str_new_cstr(mrb, strerror(mrb_fixnum(no)));

  m = mrb_nil_value();
  mrb_get_args(mrb, "|S", &m);
  if (!mrb_nil_p(m)) {
    mrb_str_cat2(mrb, str, " - ");
    mrb_str_append(mrb, str, m);
  }
  mrb_iv_set(mrb, self, MRB_SYM(mesg), str);
  return self;
}

void
mrb_mruby_errno_gem_init(mrb_state *mrb)
{
  struct RClass *e, *eno, *sce, *ste;
  mrb_value noerror;

  ste = mrb_class_get_id(mrb, MRB_SYM(StandardError));

  sce = mrb_define_class(mrb, "SystemCallError", ste);
  mrb_define_class_method(mrb, sce, "_sys_fail", mrb_sce_sys_fail, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, sce, "errno", mrb_sce_errno, MRB_ARGS_NONE());
  mrb_define_method(mrb, sce, "to_s", mrb_sce_to_s, MRB_ARGS_NONE());
  mrb_define_method(mrb, sce, "initialize", mrb_sce_init_m, MRB_ARGS_ARG(1, 1));

  eno = mrb_define_module_id(mrb, MRB_SYM(Errno));

  e = mrb_define_class_under_id(mrb, eno, MRB_SYM(NOERROR), sce);
  mrb_define_const_id(mrb, e, MRB_SYM(Errno), mrb_fixnum_value(0));
  mrb_define_method(mrb, e, "initialize", mrb_exxx_init, MRB_ARGS_OPT(1));
  //mrb_define_method(mrb, e, "===", mrb_exxx_cmp, MRB_ARGS_REQ(1));
  noerror = mrb_obj_value(e);

#define itsdefined(SYM) \
  do {									\
    int ai = mrb_gc_arena_save(mrb);					\
    e = mrb_define_class_under(mrb, eno, #SYM, sce);			\
    mrb_define_const_id(mrb, e, MRB_SYM(Errno), mrb_fixnum_value(SYM)); \
    mrb_define_method(mrb, e, "initialize", mrb_exxx_init, MRB_ARGS_OPT(1)); \
    mrb_gc_arena_restore(mrb, ai);					\
  } while (0)

#define itsnotdefined(SYM) \
  do {									\
    mrb_define_const(mrb, eno, #SYM, noerror);				\
  } while (0)

#include "known_errors_def.cstub"
}

void
mrb_mruby_errno_gem_final(mrb_state *mrb)
{
}
