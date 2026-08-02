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

The generated assembly for `rjk::duck` was identical to the `std::function` comparison.

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
        ldr     x1, [x0]    ; Load vtable pointer
        ldr     x1, [x1]    ; Load function pointer
        mov     x16, x1     ; ARM calling convention
        br      x16         ; Call the function pointer
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