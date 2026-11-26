# Annotations

Annotations placed on a trait's member functions.

## `direct`

```c++
[[=rjk::direct]]
[[=rjk::direct(bool)]]
```

Stores the function's pointer in the `duck` or `duck_view` itself instead of
its vtable, saving a load per call at the cost of one pointer of size.
`direct(false)` has no effect. See
[05_performance_tuning.md](../ducks/05_performance_tuning.md#inlining-vtable-calls).

## `right_side`

```c++
[[=rjk::right_side]]
[[=rjk::right_side(bool)]]
```

On a binary operator, matches the trait's object as the right operand instead
of the left: `[[=rjk::right_side]] auto operator-(int) const -> int` matches
`5 - x`. `right_side(false)` has no effect. See
[03_operators.md](../traits/03_operators.md).
