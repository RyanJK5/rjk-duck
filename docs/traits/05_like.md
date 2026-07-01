# Using Existing Code with `like`

`like` is a special trait that can accept non-trait types and turn them into traits.

```c++
class IDrawable {
  public:
    virtual auto draw(Graphics& g) const -> void = 0;
};

class TypeA : public IDrawable { ... };

class TypeB {
  public:
    auto draw(Graphics& g) const -> void { ... }
};

using DrawableTrait = rjk::like<IDrawable>;

static_assert(rjk::satisfies<TypeA, DrawableTrait>);
static_assert(rjk::satisfies<TypeB, DrawableTrait>);
```

The first template argument to `like` is the type to model. It will take in all **public** member
functions of the type, and any member operator functions. Notably, it does *not* find free
function operators.

`like` will implicitly exclude member functions that are not user-declared and unnamed members
like constructors and destructors.

The second template parameter to `like` is optional but allows you to further control what
member functions to take from the type.

```c++
class Interface {
  public:
    virtual auto importantFunc(int arg) -> void = 0;
    
    auto unimportantFunc() const -> int { ... }
};

// Take in importantFunc, but NOT unimportantFunc.
using Trait = rjk::like<Interface, std::meta::is_virtual>;
```

The second template argument can be any predicate with the signature
`auto(std::meta::info) -> bool`. The filter is then applied to all of the
type's member functions.

Some useful predicates are provided by the library:

```c++
// all_of: ANDs together several predicates
using TraitA = rjk::like<MyType, rjk::all_of<
    std::meta::is_virtual,
    std::meta::is_const
>>;

// any_of: ORs together several predicates
using TraitB = rjk::like<MyType, rjk::any_of<
    std::meta::is_noexcept,
    std::meta::is_const,
    std::meta::is_lvalue_reference_qualified
>>;

// none_of: ORs together predicates, then NOTs the result
using TraitC = rjk::like<MyType, rjk::none_of<
    std::meta::is_volatile,
    std::meta::is_override
>>;

// include / exclude member functions based on their name
using TraitD = rjk::like<MyType, rjk::exclude<
    "uselessFunc", "otherUselessFunc"
>>;

using TraitE = rjk::like<MyType, rjk::include<
    "importantFunc", "otherImportantFunc"
>>;
```

And of course, custom predicates can be used as well:

```c++
consteval auto validFunction(std::meta::info func) -> bool {
    return std::ranges::contains_subrange(identifier_of(func), "foo"sv);
}

using MyLike = rjk::like<SomeType, validFunction>;
```