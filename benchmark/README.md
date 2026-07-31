# Assembly Comparison

In this section, we will compare the generated assembly for `rjk::duck` directly
against other libraries. 

All experiments were conducted on Compiler Explorer using both 
`x86-64 gcc (trunk)` and `ARM64 gcc (trunk)`. The following command-line
arguments were used:

```
-std=c++26 -freflection -O3 -fno-exceptions -fno-rtti 
```

Exceptions and RTTI were disabled to prevent as much noise as possible in
the generated assembly.

## `std::function` Comparison

[Compiler Explorer](https://godbolt.org/z/qEbzveE55)

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
    struct inlined_functions : FunctionTrait<Func> {};
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