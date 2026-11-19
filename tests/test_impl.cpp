#include "rjk/duck.hpp"

#include <deque>
#include <fstream>
#include <filesystem>
#include <gtest/gtest.h>

struct Serializable {
    auto Serialize() const -> std::string;
};

struct SerializableToFile : Serializable {
    auto Serialize(const std::filesystem::path& file) const -> void;
};

template <std::ranges::range T>
struct rjk::impl<T, Serializable> {
    static auto Serialize(const auto& self) -> std::string {
        std::string out{};
        bool first = true;
        for (const auto& member : self) {
            if (!first)
                out += ',';
            out += std::to_string(member);
            first = false;
        }
        return out;
    }
};

template <rjk::satisfies<Serializable> T>
struct rjk::impl<T, SerializableToFile> {
    static auto Serialize(const auto& self, const std::filesystem::path& file) -> void {
        std::ofstream out{file};
        out << rjk::duck_view<const Serializable>{self}.Serialize();
    }
};

struct IntBox {
    int value;
};

template <>
struct rjk::impl<IntBox, Serializable> {
    static auto Serialize(const auto& self) -> std::string { return std::to_string(self.value); }
};

TEST(ImplSuite, DerivedTraitImplDelegatesIntoBaseImpl) {
    IntBox box{42};
    rjk::duck_view<SerializableToFile> view{box};

    auto outPath = std::filesystem::temp_directory_path() / "intbox.txt";
    view.Serialize(outPath);

    std::ifstream in{outPath};
    std::string content{std::istreambuf_iterator<char>(in), {}};
    EXPECT_EQ(content, "42");

    // Also verify the base trait still routes through impl correctly
    // when accessed directly
    rjk::duck_view<const Serializable> base_view{box};
    EXPECT_EQ(base_view.Serialize(), "42");
}

// An impl takes priority over the type's own members whenever it can be called,
// even if a member would be a better match: specializing impl is a declaration
// of intent. Here the impl's const overload also serves the mutable member.

struct MutableSerializer {
    auto Serialize() -> std::string;
    auto Serialize() const -> std::string;
};

struct DualSerializer {
    std::string mutable_tag = "mutable";
    std::string const_tag = "const";

    // Provides the mutable overload natively
    auto Serialize() -> std::string { return mutable_tag; }
};

// Provides the const overload through impl
template <>
struct rjk::impl<DualSerializer, MutableSerializer> {
    static auto Serialize(const auto& self) -> std::string { return self.const_tag; }
};

TEST(ImplSuite, MutableViewPrefersImpl) {
    DualSerializer ds{};
    rjk::duck_view<MutableSerializer> view{ds};
    EXPECT_EQ(view.Serialize(), "const");
}

TEST(ImplSuite, ConstViewPicksImplConstOverload) {
    DualSerializer ds{};
    rjk::duck_view<const MutableSerializer> view{ds};
    EXPECT_EQ(view.Serialize(), "const");
}

// Mixed native + impl in a single vtable

struct FormatTrait {
    auto Label() const -> std::string;
    auto Detail() const -> std::string;
};

struct HalfNative {
    // Provides Label natively
    auto Label() const -> std::string { return "native-label"; }
    // Does NOT provide Detail; relies on impl
};

template <>
struct rjk::impl<HalfNative, FormatTrait> {
    static auto Detail(const auto&) -> std::string { return "impl-detail"; }
};

TEST(ImplSuite, MixedNativeAndImplInSameVtable) {
    HalfNative obj{};
    rjk::duck_view<const FormatTrait> view{obj};
    EXPECT_EQ(view.Label(), "native-label");
    EXPECT_EQ(view.Detail(), "impl-detail");
}

// Overloaded impl: two overloads inside one specialization

struct MultiPathSerializer {
    auto Serialize(const std::filesystem::path& dest) const -> void;
    auto Serialize(std::string_view dest) const -> void;
};

struct MultiTarget {};

// Overloads take the object by its concrete type, so they rank like ordinary
// functions.
template <>
struct rjk::impl<MultiTarget, MultiPathSerializer> {
    static auto Serialize(const MultiTarget&, const std::filesystem::path& dest) -> void {
        std::ofstream{dest} << "path-overload";
    }
    static auto Serialize(const MultiTarget&, std::string_view dest) -> void {
        // Write to a path constructed from the string_view, so we can
        // verify which overload was called
        std::ofstream{std::string{dest}} << "string_view-overload";
    }
};

TEST(ImplSuite, ImplOverloadSetDispatchesOnArgumentType) {
    MultiTarget obj{};
    rjk::duck_view<const MultiPathSerializer> view{obj};

    auto path_dest = std::filesystem::temp_directory_path() / "multi_path.txt";
    auto sv_dest = std::filesystem::temp_directory_path() / "multi_sv.txt";

    view.Serialize(path_dest);
    view.Serialize(std::string_view{sv_dest.generic_display_string()});

    auto read = [](const auto& p) {
        std::ifstream f{p};
        return std::string{std::istreambuf_iterator<char>(f), {}};
    };

    EXPECT_EQ(read(path_dest), "path-overload");
    EXPECT_EQ(read(sv_dest), "string_view-overload");
}

// impl returning duck_view: trait declares duck_view<Serializable>

struct ChainableTrait {
    auto AsSerializable() const -> rjk::duck_view<const Serializable>;
};

struct Chainable {
    std::vector<int> data;
    // Chainable satisfies Serializable via the range impl above
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
};

template <>
struct rjk::impl<Chainable, ChainableTrait> {
    static auto AsSerializable(const auto& self) -> const Chainable& { return self; }
};

TEST(ImplSuite, ImplReturningDuckViewViaConvertDuckReturn) {
    Chainable obj{{10, 20, 30}};
    rjk::duck_view<const ChainableTrait> view{obj};

    rjk::duck_view<const Serializable> ser = view.AsSerializable();
    EXPECT_EQ(ser.Serialize(), "10,20,30");
}

// impl on a like<>-based trait

struct LikeBase {
    virtual auto Describe() const -> std::string = 0;
    virtual ~LikeBase() = default;
};

using LikeTrait = rjk::like<LikeBase>;

struct NotDerivedFromBase {
    int code = 7;
    // Does NOT inherit LikeBase, so satisfies only through impl
};

template <>
struct rjk::impl<NotDerivedFromBase, LikeTrait> {
    static auto Describe(const auto& self) -> std::string {
        return "code=" + std::to_string(self.code);
    }
};

TEST(ImplSuite, ImplOnLikeBasedTrait) {
    NotDerivedFromBase obj{99};
    rjk::duck_view<const LikeTrait> view{obj};
    EXPECT_EQ(view.Describe(), "code=99");
}

// implementing base functionality for a derived trait

struct BaseTrait {
    int foo();
};

struct DerivedTrait : BaseTrait {
    int bar() const;
};

struct DummyStruct {};

template <>
struct rjk::impl<DummyStruct, DerivedTrait> {
    constexpr static int foo(auto&&) { return 10; }

    constexpr static int bar(const auto&) { return 20; }
};

TEST(ImplSuite, Inheritance) {
    rjk::duck<DerivedTrait> d{DummyStruct{}};
    EXPECT_EQ(d.foo(), 10);
    EXPECT_EQ(d.bar(), 20);
}

// Disambiguating by const-ness when writing an impl specialization

struct ConstOverloadBase {
    int foo();
};

struct ConstOverloadDerived : ConstOverloadBase {
    int foo() const;
};

template <>
struct rjk::impl<DummyStruct, ConstOverloadDerived> {
    constexpr static int foo(DummyStruct&) { return 20; }
    constexpr static int foo(const DummyStruct&) { return 40; }
};

// impl templates whose only template parameter is the object can't be ordered
// against each other (partial ordering isn't available), so overloading them is
// ambiguous: use the concrete type for the object parameter instead.
struct TemplateSelfTarget {};

template <>
struct rjk::impl<TemplateSelfTarget, ConstOverloadDerived> {
    constexpr static int foo(auto&&) { return 20; }
    constexpr static int foo(const auto&) { return 40; }
};
static_assert(rjk::satisfies<DummyStruct, ConstOverloadDerived>);
static_assert(!rjk::satisfies<TemplateSelfTarget, ConstOverloadDerived>);

TEST(ImplSuite, ConstDerivedTraits) {
    rjk::duck<ConstOverloadDerived> d{DummyStruct{}};
    rjk::duck_view<const ConstOverloadDerived> view{d};
    EXPECT_EQ(view.foo(), 40);
    EXPECT_EQ(d.foo(), 20);
}

struct SomeTrait {
    int foo() const;
};

struct SomeStruct {
    constexpr int foo() const { return 5; }
};

template <>
struct rjk::impl<SomeStruct, SomeTrait> {
    constexpr static int foo(const auto&) { return 10; }
};

TEST(ImplSuite, PreferImpl) {
    rjk::duck<SomeTrait> d{SomeStruct{}};
    EXPECT_EQ(d.foo(), 10);
}

// impl members are called like members with an explicit object parameter.

struct Counter {
    int count{};
};

struct Bumpable {
    void bump();
    int get() const;
};

template <>
struct rjk::impl<Counter, Bumpable> {
    static void bump(auto& self) { ++self.count; }
    static int get(const Counter& self) { return self.count; }
};

TEST(ImplSuite, LvalueSelfMutatesTheObject) {
    static_assert(rjk::satisfies<Counter, Bumpable>);
    Counter c{};
    rjk::duck_view<Bumpable> v{c};
    v.bump();
    v.bump();
    EXPECT_EQ(c.count, 2);
    EXPECT_EQ(v.get(), 2);
}

struct ConstSelfTarget {
    int value{3};
};

struct MutableGet {
    int get();
};

// A const self satisfies a mutable member, as a const member function would.
template <>
struct rjk::impl<ConstSelfTarget, MutableGet> {
    static int get(const ConstSelfTarget& self) { return self.value; }
};
static_assert(rjk::satisfies<ConstSelfTarget, MutableGet>);

struct ByValueTarget {
    int value{4};
};

// A by-value self is allowed; it works on a copy.
template <>
struct rjk::impl<ByValueTarget, MutableGet> {
    static int get(ByValueTarget self) { return self.value; }
};
static_assert(rjk::satisfies<ByValueTarget, MutableGet>);

struct Adder {
    int add(int);
};

struct GenericAddTarget {
    int base{10};
};

// Generic parameters besides self are deduced from the call.
template <>
struct rjk::impl<GenericAddTarget, Adder> {
    static int add(auto&& self, auto x) { return self.base + x; }
};

struct OverloadedAddTarget {};

// Overloaded impl functions are ranked like real overloads, conversions
// included, and default arguments work.
template <>
struct rjk::impl<OverloadedAddTarget, Adder> {
    static int add(OverloadedAddTarget&, long) { return 1; }
    static int add(OverloadedAddTarget&, int, int extra = 100) { return extra; }
};

TEST(ImplSuite, ImplCalledLikeExplicitObjectMembers) {
    rjk::duck<MutableGet> by_const{ConstSelfTarget{}};
    EXPECT_EQ(by_const.get(), 3);

    rjk::duck<MutableGet> by_value{ByValueTarget{}};
    EXPECT_EQ(by_value.get(), 4);

    rjk::duck<Adder> generic{GenericAddTarget{}};
    EXPECT_EQ(generic.add(5), 15);

    rjk::duck<Adder> overloaded{OverloadedAddTarget{}};
    EXPECT_EQ(overloaded.add(5), 100);
}

// Under strict lookup an impl function has to match the trait member exactly:
// the trait's object type, then its parameter types. impl templates are ignored.
struct StrictGet {
    rjk::lookup_rule function_lookup = rjk::lookup_rule::strict;
    int get() const;
};

struct StrictExact {};
struct StrictTemplate {};
struct StrictMutableSelf {};

template <>
struct rjk::impl<StrictExact, StrictGet> {
    static int get(const StrictExact&) { return 1; }
};
template <>
struct rjk::impl<StrictTemplate, StrictGet> {
    static int get(const auto&) { return 1; }
};
template <>
struct rjk::impl<StrictMutableSelf, StrictGet> {
    static int get(StrictMutableSelf&) { return 1; }
};
static_assert(rjk::satisfies<StrictExact, StrictGet>);
static_assert(!rjk::satisfies<StrictTemplate, StrictGet>);
static_assert(!rjk::satisfies<StrictMutableSelf, StrictGet>);
