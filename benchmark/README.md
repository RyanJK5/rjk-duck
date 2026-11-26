## Table of Contents

* [Dispatch Benchmark](#dispatch-benchmark)
* [Lifetime Benchmark](#lifetime-benchmark)
* [Assembly Comparison](#assembly-comparison)
    * [`std::function`](#stdfunction)
    * [Virtual Functions](#virtual-functions)
    * [Other Libraries](#other-libraries)

This document contains explanations and discussions of various benchmarks comparing
`rjk::duck` against alternative approaches to solving similar problems.
Currently, we compare against virtual functions and `std::function`, but
also hope to expand to compare against competing libraries as well.

The source code, written using Google Benchmark, is available for all benchmarks in
the subfolders of this directory.

# Dispatch Benchmark

Here we compare `std::unique_ptr`, `std::function`, and `rjk::duck`'s
performance executing single-function virtual dispatch. The implementation
includes a header with factory functions for each of the types, and an
implementation file that controls the runtime type returned from the factory
functions.

This is a very simple benchmark. It tests the types' ability to dispatch
through the same object repeatedly. The regular access pattern likely
causes aggressive optimization from the hardware, and therefore cannot
be considered conclusive in the general case.

For `rjk::duck`, we also inspect its performance when the indirect call
is inlined, like so:

```c++
template <bool Direct = false>
struct Trait {
    [[=rjk::direct(Direct)]]
    auto getData() const -> int;
};

using InlinedDuck = rjk::duck<Trait<true>>;
```

For more information about how this might change `rjk::duck`, see 
[05_performance_tuning.md](../docs/ducks/05_performance_tuning.md).

All source code is available in the [dispatch](./dispatch/) directory.

### Discussion

All six types dispatched in roughly one nanosecond, and the differences between them
were too small to be explained by the generated code. For instance, `rjk::duck` and
`std::unique_ptr` execute the same two instructions at the call site, but measured
0.2 ns apart. At this scale, the results suggest the optimizer and the hardware are
able to predict the behavior of each of the benchmarks, so this benchmark cannot
distinguish between them. See the [assembly comparison](#assembly-comparison) for a
closer look at dispatch. A more sophisticated benchmark is under development.

# Lifetime Benchmark

The lifetime benchmark computes the amortized cost of constructing
and destructing a `std::unique_ptr<ICounter>`, `rjk::duck<Counter>`,
and `std::function<int()>`. We measure the cost of these operations
across objects of various sizes to demonstrate the effects of small
buffer optimization (SBO). A trivially constructible and destructible type was
used to isolate construction and destruction without any noise from other operations.

All source code is available in [benchmark.cpp](lifetime/benchmark.cpp).

### Summary Table

| Type              | Buffer size |
|-------------------|------------:|
| `rjk::duck`       |    24 bytes |
| `std::function`   |    16 bytes |
| `aa::any_with`    |    40 bytes |
| `pro::proxy`      |    16 bytes |
| `std::unique_ptr` |        none |

### Discussion

The results illustrate that type-erased containers outperform
direct heap allocation when using SBO. Objects that fit in the buffer were
constructed and destroyed in roughly 1-2 ns, while objects that required
allocation took roughly 6-10 ns, regardless of the type used. Beyond the size
of each type's buffer, the differences between them were within noise.

# Assembly Comparison

In this section, we will compare the generated assembly for `rjk::duck` directly
against competitors.

All experiments were conducted on Compiler Explorer using both 
`x86-64 gcc (trunk)` and `ARM64 gcc (trunk)`. The following command-line
arguments were used:

```
-std=c++26 -freflection -O3 -fno-exceptions -fno-rtti -DNDEBUG
```

Exceptions and RTTI were disabled to prevent as much noise as possible in
the generated assembly. `NDEBUG` removes `rjk::duck`'s debug assertion that
it holds an object.

At the time of writing, all assembly generation is from `rjk::duck`
[version 0.3.0](https://github.com/RyanJK5/rjk-duck/releases/tag/v0.3.0).

## `std::function`

[Compiler Explorer](https://godbolt.org/z/839qjYErf)

In this example, we compare the generated assembly of the call operator for
`rjk::duck`, both with and without inlining, and `std::function`.

A convenient function alias can be made using `rjk::duck` as follows:

```c++
template <typename Func, bool Direct = false>
struct FunctionTrait;

template <typename Ret, typename... Args, bool Direct>
struct FunctionTrait<Ret(Args...), Direct> {
    [[=rjk::direct(Direct)]]
    Ret operator()(Args...) const;
};

template <typename Func>
using Function = rjk::duck<FunctionTrait<Func>>;

template <typename Func>
using InlinedFunc = rjk::duck<FunctionTrait<Func, true>>;
```

The generated assembly of the following three functions will be compared:

```c++
int test1(const Function<int()>& func) {
    return func();
}

int test2(const InlinedFunc<int()>& func) {
    return func();
}

int test3(const std::function<int()>& func) {
    return func();
}
```

### x86-64

```asm
; In all examples, rdi initially holds the 'this' pointer
"test1(rjk::duck<FunctionTrait<int (), false> > const&)":
        mov     rax, QWORD PTR [rdi+24] ; Load vtable
        jmp     [QWORD PTR [rax+48]]    ; Look up function pointer and jump
"test2(rjk::duck<FunctionTrait<int (), true> > const&)":
        jmp     [QWORD PTR [rdi+24]]    ; Jump to the inlined function pointer
"test3(std::function<int ()> const&)":
        cmp     QWORD PTR [rdi+16], 0   ; Check if the object is empty
        je      .L6                     ; If null, jump below for exception
        jmp     [QWORD PTR [rdi+24]]    ; Jump to the function pointer
"test3(std::function<int ()> const&) [clone .cold]":
.L6:
        ; Terminate due to -fno-exceptions
        push    rax
        call    "std::__throw_bad_function_call()"
```

### ARM64

```asm
; In all examples, x0 initially holds the 'this' pointer
test1(rjk::duck<FunctionTrait<int (), false>> const&):
        ldr     x1, [x0, 24] ; Load vtable pointer
        ldr     x1, [x1, 48] ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test2(rjk::duck<FunctionTrait<int (), true>> const&):
        ldr     x16, [x0, 24] ; Load the inlined function pointer
        br      x16           ; Call the function pointer
test3(std::function<int ()> const&):
        ldr     x2, [x0, 16] ; Load the manager pointer
        cbz     x2, .L9      ; If null, jump below for exception
        ldr     x16, [x0, 24] ; Load the function pointer
        br      x16           ; Call the function pointer
.L9:
        ; Terminate due to -fno-exceptions
        stp     x29, x30, [sp, -16]!
        mov     x29, sp
        bl      std::__throw_bad_function_call()
```

### Discussion

The example above demonstrates that the lack of an empty state for `rjk::duck`
prevents branching overhead before dispatch. Though debug assertions are provided,
this makes dispatch slightly more dangerous in release builds.

`rjk::duck` passes its own address to the function, so the object doesn't need to be
loaded first, and `this` is forwarded as-is. `InlinedFunc` also avoids loading a vtable,
so it dispatches with a single jump. This comes at the cost of an additional pointer
being stored flatly in `InlinedFunc`. `std::function` similarly stores the function
pointer directly in the object, but must check that it isn't empty first.

This constrained example does not allow the compiler to perform most optimizations,
and therefore cannot be considered conclusive for standard use-cases.

> [!NOTE]
> `rjk::duck_view` points to its object instead of storing it, so its calls pass that
> pointer instead of the view's own address.

## Virtual Functions

[Compiler Explorer](https://godbolt.org/z/szoMxfPTT)

We will use a similar example from the `std::function` comparison to see how single-function
virtual dispatch stacks against `rjk::duck`:

```c++
struct Trait {
    int foo() const;
};

struct ITrait {
    virtual int foo() const = 0;
};

struct DirectTrait {
    [[=rjk::direct]] int foo() const;
};

int test1(const rjk::duck<Trait>& d) {
    return d.foo();
}

int test2(const rjk::duck<DirectTrait>& d) {
    return d.foo();
}

int test3(const ITrait& t) {
    return t.foo();
}
```

The generated assembly for `rjk::duck` was predictably identical to the `std::function` comparison.

### x86-64

```asm
; In all examples, rdi initially holds the 'this' pointer
"test1(rjk::duck<Trait> const&)":
        mov     rax, QWORD PTR [rdi+24] ; Load vtable
        jmp     [QWORD PTR [rax+48]]    ; Look up function pointer and jump
"test2(rjk::duck<DirectTrait> const&)":
        jmp     [QWORD PTR [rdi+24]]    ; Jump to the inlined function pointer
"test3(ITrait const&)":
        mov     rax, QWORD PTR [rdi]    ; Load vtable
        jmp     [QWORD PTR [rax]]       ; Look up function pointer and jump
```

### ARM64

```asm
; In all examples, x0 initially holds the 'this' pointer
test1(rjk::duck<Trait> const&):
        ldr     x1, [x0, 24] ; Load vtable pointer
        ldr     x1, [x1, 48] ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test2(rjk::duck<DirectTrait> const&):
        ldr     x16, [x0, 24] ; Load the inlined function pointer
        br      x16           ; Call the function pointer
test3(ITrait const&):
        ldr     x1, [x0]     ; Load vtable pointer
        ldr     x1, [x1]     ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
```

### Discussion

Virtual dispatch is typically implemented using a vtable, much like `rjk::duck<Trait>`
in `test1`. Both require two dependent loads before the function can be jumped to, and
both pass the object's address through unchanged, so they generate the same instructions
on both architectures. The vtable is only stored at a different offset: `rjk::duck`
keeps its small buffer first.

`test2` skips the vtable entirely, needing a single load from the `duck` itself.

## Other Libraries

We will reuse the same test to compare `rjk::duck` directly against competitor libraries:
[AnyAny](https://github.com/kelbon/AnyAny) (v1.2.1) and [proxy](https://github.com/ngcpp/proxy)
(4.1.0), the versions used by the benchmarks above. AnyAny requires exceptions, so this
test drops `-fno-exceptions`.

```c++
struct Trait {
    int foo() const;
};

anyany_method(foo, (const& self) requires(self.foo()) -> int);
using AnyAny = aa::any_with<foo>;

PRO_DEF_MEM_DISPATCH(MemFoo, foo);
struct Facade : pro::facade_builder
    ::add_convention<MemFoo, int() const>
    ::build {};

int test1(const rjk::duck<Trait>& d) {
    return d.foo();
}

int test2(const AnyAny& a) {
    return a.foo();
}

int test3(const pro::proxy<Facade>& p) {
    return p->foo();
}
```

### x86-64

```asm
; In all examples, rdi initially holds the 'this' pointer
"test1(rjk::duck<Trait> const&)":
        mov     rax, QWORD PTR [rdi+24] ; Load vtable
        jmp     [QWORD PTR [rax+48]]    ; Look up function pointer and jump
"test2(aa::basic_any<std::allocator<std::byte>, 40ul, aa::destroy, foo> const&)":
        mov     rax, QWORD PTR [rdi]    ; Load vtable
        mov     rdi, QWORD PTR [rdi+8]  ; Load the erased object
        mov     rax, QWORD PTR [rax+8]  ; Load function pointer
        jmp     rax                     ; Jump to the function
"test3(pro::v4::proxy<Facade> const&)":
        jmp     [QWORD PTR [rdi+8]]     ; Jump to the inlined function pointer
```

### ARM64

```asm
; In all examples, x0 initially holds the 'this' pointer
test1(rjk::duck<Trait> const&):
        ldr     x1, [x0, 24] ; Load vtable pointer
        ldr     x1, [x1, 48] ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test2(aa::basic_any<std::allocator<std::byte>, 40ul, aa::destroy, foo> const&):
        ldp     x1, x0, [x0] ; Load vtable pointer and the erased object
        ldr     x1, [x1, 8]  ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test3(pro::v4::proxy<Facade> const&):
        ldr     x16, [x0, 8] ; Load the inlined function pointer
        br      x16          ; Call the function pointer
```

### Discussion

AnyAny stores a pointer to its object, even when the object is in its buffer, so it
must load that pointer before dispatching. proxy stores the function pointer of a
single-convention facade in the object itself, matching `rjk::duck` with
`[[=rjk::direct]]`.
