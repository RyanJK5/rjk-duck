# Annotations

Placed on a trait's member functions.

## `direct`

```c++
[[=rjk::direct]]                                                        // (1)
[[=rjk::direct(bool)]]                                                  // (2)
```

1. Stores the function's pointer in the `duck` or `duck_view` itself instead of
   its vtable, saving a load per call at the cost of one pointer of size. See
   [05_performance_tuning.md](../ducks/05_performance_tuning.md#inlining-vtable-calls).
2. (1) if the argument is `true`, otherwise no effect.

## `right_side`

```c++
[[=rjk::right_side]]                                                    // (1)
[[=rjk::right_side(bool)]]                                              // (2)
```

1. On a binary operator, matches the trait's object as the right operand
   instead of the left: `[[=rjk::right_side]] auto operator-(int) const -> int`
   matches `5 - x`. See [03_operators.md](../traits/03_operators.md).
2. (1) if the argument is `true`, otherwise no effect.
