module;

#include "rjk/duck.hpp"

export module rjk.duck;

export namespace rjk {

using rjk::duck;
using rjk::duck_view;
using rjk::duck_ptr;
using rjk::bad_duck_access;

using rjk::get;
using rjk::get_if;
using rjk::typeid_of;
using rjk::make_narrowed;
using rjk::emplace;

using rjk::policy;
using rjk::copyable;
using rjk::has_fn;
using rjk::has_op;

using rjk::like;
using rjk::impl;
using rjk::lookup_rule;

using rjk::is_trait;
using rjk::duck_tag;
using rjk::satisfies;

using rjk::trait;
using rjk::right_side;
using rjk::both_sides;
using rjk::perf_options;

}