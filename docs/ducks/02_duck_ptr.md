## `duck_ptr`

Occasionally, you may want a possibly nullable view into a `duck`. While this
could be achieved via a `std::optional<rjk::duck_view<...>>`, that bloats the
size of the type with an unnecessary boolean. Instead, we provide `duck_ptr`,
which is simply a possibly nullable `duck_view`.

It is very similar in shape to `std::optional`:

```c++
struct [[=rjk::trait]] MyTrait {
    auto foo() -> int;
    
    [[=rjk::both_sides]]
    auto operator+(int) -> int;
};

rjk::duck<MyTrait> myDuck{...};

rjk::duck_ptr nullView{myDuck};

assert(nullView.has_value());

nullView->foo();
int result = *nullView + 5;

nullView.reset();
assert(!nullView);

// assertion with -fno-exceptions
try {
    nullView.value();
} catch (const rjk::bad_duck_access& e) {
    std::println("{}", e.what()); // duck_ptr is empty
}
```