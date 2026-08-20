## Table of Contents

* [Dispatch Benchmark](#dispatch-benchmark)
* [Lifetime Benchmark](#lifetime-benchmark)
* [Assembly Comparison](#assembly-comparison)
    * [`std::function`](#stdfunction)
    * [Virtual Functions](#virtual-functions)

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
struct Trait {
    auto getData() const -> int;
};

struct [[=rjk::perf_options]] Perf {
    using inlined_functions = Trait;
};

using InlinedDuck = rjk::duck<Trait, Perf>;
```

For more information about how this might change `rjk::duck`, see 
[05_performance_tuning.md](../docs/ducks/05_performance_tuning.md).

All source code is available in the [dispatch](./dispatch/) directory.

### Summary Table

| Benchmark             | Time     |
|-----------------------|----------|
| `rjk::duck`           | 0.894 ns |
| `rjk::duck` (inlined) | 0.864 ns |
| `std::unique_ptr`     | 1.09 ns  |
| `std::function`       | 0.877 ns |
| `aa::any_with`        | 0.874 ns |


### Discussion

Regular dispatch through `rjk::duck` and `std::function` have near-identical
performance impact. Though `std::function` uses a branching null check in its
dispatch, it also stores the function pointer inline, which could balance its
performance. By contrast, `rjk::duck` stores the vtable separately, requiring
two loads instead of one.

Virtual dispatch through a `std::unique_ptr` and `rjk::duck` with an inlined function pointer perform
similarly. Because compilers fully control how virtual functions are implemented,
this may allow for performance wins that are not attainable through `rjk::duck`'s
approach. Typical vtable dispatch still requires two loads, however, and therefore
may be balanced by the inlined `rjk::duck` holding the function pointer directly.

This benchmark answers a very specific question that is not necessarily applicable
to the typical use case of a type-erased object. Future development of a heterogeneous
container benchmark may reveal deeper insights into how access patterns affect the
performance of these types.

# Lifetime Benchmark

The lifetime benchmark computes the amortized cost of constructing
and destructing a `std::unique_ptr<ICounter>`, `rjk::duck<Counter>`,
and `std::function<int()>`. We measure the cost of these operations
across objects of various sizes to demonstrate the effects of small
buffer optimization (SBO). The results are consistent with both `rjk::duck` 
and GCC's `std::function` using a 16 byte SBO. A trivially constructible 
and destructible type was used to isolate construction and destruction 
without any noise from other operations.

All source code is available in [benchmark.cpp](lifetime/benchmark.cpp).

### Construction

| Payload Size | `rjk::duck` | `rjk::duck` (inlined) | `std::unique_ptr` | `std::function` |
|-------------:|------------:|----------------------:|------------------:|----------------:|
|        **8** |    0.784 ns |              0.916 ns |           7.83 ns |        0.720 ns |
|       **16** |    0.825 ns |              0.937 ns |           7.71 ns |        0.729 ns |
|       **32** |     8.20 ns |               8.13 ns |           7.76 ns |         8.34 ns |
|       **64** |     8.21 ns |               8.35 ns |           8.23 ns |         8.30 ns |
|      **128** |     11.0 ns |               10.8 ns |           10.7 ns |         10.9 ns |


### Destruction

| Payload Size | `rjk::duck` | `rjk::duck` (inlined) | `std::unique_ptr` | `std::function` |
|-------------:|------------:|----------------------:|------------------:|----------------:|
|        **8** |     1.27 ns |               1.35 ns |           6.39 ns |         1.93 ns |
|       **16** |     1.49 ns |               1.28 ns |           6.15 ns |         1.86 ns |
|       **32** |     6.14 ns |               6.36 ns |           6.30 ns |         7.21 ns |
|       **64** |     6.11 ns |               6.51 ns |           6.33 ns |         6.75 ns |
|      **128** |     10.0 ns |               10.2 ns |           10.3 ns |         11.0 ns |


### Discussion

The results illustrate that `rjk::duck` and `std::function` outperform
direct heap allocation when using SBO. For larger objects, all four
types had similar performance across both benchmarks.

`rjk::duck` and `std::function` had very similar construction time for
smaller objects. The results suggest `std::function` may be slightly
faster on average. `rjk::duck` with inlined functions appears to take
slightly longer to construct, likely because it must assign an additional
pointer inside the `duck` object. With more inlined functions, construction
can be expected to take even longer.

`rjk::duck` has slightly better performance for object destruction. This could potentially
be because `duck` eliminates branching in its destructor by storing a no-op "null" vtable for
moved-from `duck`s instead of using a null check.

# Assembly Comparison

In this section, we will compare the generated assembly for `rjk::duck` directly
against competitors.

All experiments were conducted on Compiler Explorer using both 
`x86-64 gcc (trunk)` and `ARM64 gcc (trunk)`. The following command-line
arguments were used:

```
-std=c++26 -freflection -O3 -fno-exceptions -fno-rtti 
```

Exceptions and RTTI were disabled to prevent as much noise as possible in
the generated assembly.

At the time of writing, all assembly generation is from `rjk::duck`
[version 0.2.1](https://github.com/RyanJK5/rjk-duck/releases/tag/v0.2.1).

## `std::function`

[Compiler Explorer](https://godbolt.org/z/Yd4EGYa7f)

In this example, we compare the generated assembly of the call operator for
`rjk::duck`, both with and without inlining, and `std::function`.

A convenient function alias can be made using `rjk::duck` as follows:

```c++
template <typename Func>
struct FunctionTrait;

template <typename Ret, typename... Args>
struct FunctionTrait<Ret(Args...)> {
    Ret operator()(Args...) const;
};

template <typename Func>
using Function = rjk::duck<FunctionTrait<Func>>;
```

We define `InlinedFunc` as follows:

```c++
template <typename Func>
struct [[=rjk::perf_options]] InlinePerf {
    using inlined_functions = FunctionTrait<Func>;
};

template <typename Func>
using InlinedFunc = rjk::duck<FunctionTrait<Func>, InlinePerf<Func>>;
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
"test1(rjk::duck<FunctionTrait<int ()> > const&)":
        mov     rax, QWORD PTR [rdi+8]  ; Load vtable
        mov     rdx, QWORD PTR [rdi]    ; Load the erased object
        mov     rax, QWORD PTR [rax+48] ; Look up the function pointer
        mov     rdi, rdx                ; Pass the erased object as an argument
        jmp     rax                     ; Jump to the function
"test2(rjk::duck<FunctionTrait<int ()>, InlinePerf<int ()> > const&)":
        mov     rdx, QWORD PTR [rdi]    ; Load the erased object
        mov     rax, QWORD PTR [rdi+8]  ; Load the function pointer
        mov     rdi, rdx                ; Pass the erased object as an argument
        jmp     rax                     ; Jump to the function
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

### ARM 64

```asm
; In all examples, x0 initially holds the 'this' pointer
test1(rjk::duck<FunctionTrait<int ()>> const&):
        ldp     x0, x1, [x0] ; Load the erased object and the vtable pointer
        ldr     x1, [x1, 48] ; Load the function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test2(rjk::duck<FunctionTrait<int ()>, InlinePerf<int ()>> const&):
        ldp     x0, x1, [x0] ; Load the erased object and the function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test3(std::function<int ()> const&):
        ldr     x2, [x0, 16] ; Load the erased object
        cbz     x2, .L9      ; If null, jump below for exception
        ldr     x1, [x0, 24] ; Load the function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
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

`InlinedFunc` was able to avoid loading a vtable, and therefore featured one fewer
indirection in dispatch. This comes at the cost of an additional pointer being stored
flatly in `InlinedFunc`. `std::function` similarly stored the function pointer
directly in the object.

This constrained example does not allow the compiler to perform most optimizations,
and therefore cannot be considered conclusive for standard use-cases.

> [!NOTE]
> Assembly generation was identical for `rjk::duck` and `rjk::duck_view` in this test.

## Virtual Functions

[Compiler Explorer](https://godbolt.org/z/GbdxqETPv)

We will use a similar example from the `std::function` comparison to see how single-function
virtual dispatch stacks against `rjk::duck`:

```c++
struct Trait {
    int foo() const;
};

struct ITrait {
    virtual int foo() const = 0;
};

template <typename Trait>
struct [[=rjk::perf_options]] Perf {
    using inlined_functions = Trait;
};

int test1(const rjk::duck<Trait>& d) {
    return d.foo();
}

int test2(const rjk::duck<Trait, Perf<Trait>>& d) {
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
        mov     rax, QWORD PTR [rdi+8]  ; Load vtable
        mov     rdx, QWORD PTR [rdi]    ; Load the erased object
        mov     rax, QWORD PTR [rax+48] ; Look up the function pointer
        mov     rdi, rdx                ; Pass the erased object as an argument
        jmp     rax                     ; Jump to the function
"test2(rjk::duck<Trait, Perf<Trait> > const&)":
        mov     rdx, QWORD PTR [rdi]    ; Load the erased object
        mov     rax, QWORD PTR [rdi+8]  ; Load the function pointer
        mov     rdi, rdx                ; Pass the erased object as an argument
        jmp     rax                     ; Jump to the function
"test3(ITrait const&)":
        mov     rax, QWORD PTR [rdi]    ; Load vtable pointer
        jmp     [QWORD PTR [rax]]       ; Look up function pointer and jump
```

### ARM64

```asm
; In all examples, x0 initially holds the 'this' pointer
test1(rjk::duck<Trait> const&):
        ldp     x0, x1, [x0] ; Load the erased object and the vtable pointer
        ldr     x1, [x1, 48] ; Load the function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test2(rjk::duck<Trait, Perf<Trait>> const&):
        ldp     x0, x1, [x0] ; Load the erased object and the function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
test3(ITrait const&):
        ldr     x1, [x0]     ; Load vtable pointer
        ldr     x1, [x1]     ; Load function pointer
        mov     x16, x1      ; ARM calling convention
        br      x16          ; Call the function pointer
```

### Discussion

Virtual dispatch is typically implemented using a vtable, much like `rjk::duck<Trait>`
in `test1`. Both require two dependent loads before the function can be jumped to. However,
the generated x86 for `test3` demonstrates that because of the compiler's ability to perfectly
arrange the layout of a virtual type, it can avoid some of the register-shuffling that is required
for `rjk::duck`. It is able to condense all the dispatch into two instructions, even though it
is still performing two loads.

This instruction-count edge does not appear to hold on ARM. x86 permits an indirect jump
to take a memory operand directly, whereas ARM's branch instruction can only accept a
register. ARM also offers the `ldp` instruction, which helpfully loads both the erased object
and function pointer at once for `rjk::duck`. The result of these two differences is that
`test1` generates very comparable assembly to virtual dispatch, and `test2` generates fewer
instructions.