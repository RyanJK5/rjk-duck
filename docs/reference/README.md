# Reference

Every public name in namespace `rjk`. See the [duck](../ducks/01_overview.md) and [trait](../traits/01_overview.md) guides
for explanations and examples.

## Ducks

- [`duck`](duck.md): owns an object that satisfies its traits.
  - [`make_narrowed`](duck.md#make_narrowed): copies or moves into a `duck` of fewer traits.
  - [`swap`](duck.md#swap): swaps two ducks' objects.
  - [`is_valueless`](duck.md#is_valueless): whether a `duck` holds no object.
  - [`emplace`](duck.md#emplace): replaces a `duck`'s object in place.
  - [`get_allocator`](duck.md#get_allocator): a `duck`'s allocator.
- [`duck_view`](duck_view.md): refers to an object without owning it.
- [`duck_ptr`](duck_ptr.md): a `duck_view` that may be empty.

## Accessing the object

These need [`rtti`](traits.md#rtti).

- [`get`](access.md#get): a reference to the object as a `T`.
- [`get_if`](access.md#get_if): a pointer to the object as a `T`, or `nullptr`.
- [`typeid_of`](access.md#typeid_of): the object's `std::type_info`.
- [`bad_duck_access`](access.md#bad_duck_access): thrown by `get` and `duck_ptr::value`.

## Traits

- [`satisfies`](traits.md#satisfies): whether a type satisfies traits.
- [`copyable`](traits.md#copyable): makes a `duck` copyable.
- [`rtti`](traits.md#rtti): stores the object's `std::type_info`.
- [`impl`](traits.md#impl): makes an existing type satisfy a trait.
- [`lookup_rule`](traits.md#lookup_rule): how a trait's member functions are matched.
- [`sbo_size`, `sbo_alignment`, `allocator`](options.md): performance options.

## Annotations

- [`direct`](annotations.md#direct): stores a function's pointer in the `duck` itself.
- [`right_side`](annotations.md#right_side): matches an operator's right operand.

## `like` and predicates

- [`like`](like.md#like): a trait made from an existing type.
- [`is_meta_predicate`](like.md#is_meta_predicate): a predicate for `like`.
- [`all_of`, `any_of`, `none_of`](like.md#all_of-any_of-none_of): combine predicates.
- [`include`, `exclude`](like.md#include-exclude): select members by name.
