#include "duck.hpp"
#include "duck_view.hpp"

#include <deque>
#include <fstream>
#include <filesystem>
#include <gtest/gtest.h>

struct [[=rjk::trait]] Serializable {
    auto Serialize() const -> std::string;
};

struct [[=rjk::trait]] SerializableToFile : Serializable {
    auto Serialize(const std::filesystem::path& file) const -> void;
};

template <std::ranges::range T>
struct rjk::impl<T, Serializable> {
    static auto Serialize(const auto& self) -> std::string {
        std::string out{};
        bool first = true;
        for (const auto& member : self) {
            if (!first) out += ',';
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
    static auto Serialize(const auto& self) -> std::string {
        return std::to_string(self.value);
    }
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

// Const-correctness: type with both a native mutable Serialize

struct [[=rjk::trait]] MutableSerializer {
    auto Serialize()       -> std::string;
    auto Serialize() const -> std::string;
};

struct DualSerializer {
    std::string mutable_tag  = "mutable";
    std::string const_tag    = "const";

    // Provides the mutable overload natively
    auto Serialize() -> std::string { return mutable_tag; }
};

// Provides the const overload through impl
template <>
struct rjk::impl<DualSerializer, MutableSerializer> {
    static auto Serialize(const auto& self) -> std::string {
        return self.const_tag;
    }
};

TEST(ImplSuite, MutableViewPicksNativeMember) {
    DualSerializer ds{};
    rjk::duck_view<MutableSerializer> view{ds};
    EXPECT_EQ(view.Serialize(), "mutable");
}

TEST(ImplSuite, ConstViewPicksImplConstOverload) {
    DualSerializer ds{};
    rjk::duck_view<const MutableSerializer> view{ds};
    EXPECT_EQ(view.Serialize(), "const");
}

// Mixed native + impl in a single vtable

struct [[=rjk::trait]] FormatTrait {
    auto Label()  const -> std::string;
    auto Detail() const -> std::string;
};

struct HalfNative {
    // Provides Label natively
    auto Label() const -> std::string { return "native-label"; }
    // Does NOT provide Detail; relies on impl
};

template <>
struct rjk::impl<HalfNative, FormatTrait> {
    static auto Detail(const auto&) -> std::string {
        return "impl-detail";
    }
};

TEST(ImplSuite, MixedNativeAndImplInSameVtable) {
    HalfNative obj{};
    rjk::duck_view<const FormatTrait> view{obj};
    EXPECT_EQ(view.Label(),  "native-label");
    EXPECT_EQ(view.Detail(), "impl-detail");
}

// Overloaded impl: two overloads inside one specialization

struct [[=rjk::trait]] MultiPathSerializer {
    auto Serialize(const std::filesystem::path& dest) const -> void;
    auto Serialize(std::string_view dest)              const -> void;
};

struct MultiTarget {};

template <>
struct rjk::impl<MultiTarget, MultiPathSerializer> {
    static auto Serialize(const auto&, const std::filesystem::path& dest) -> void {
        std::ofstream{dest} << "path-overload";
    }
    static auto Serialize(const auto&, std::string_view dest) -> void {
        // Write to a path constructed from the string_view, so we can
        // verify which overload was called
        std::ofstream{std::string{dest}} << "string_view-overload";
    }
};

TEST(ImplSuite, ImplOverloadSetDispatchesOnArgumentType) {
    MultiTarget obj{};
    rjk::duck_view<const MultiPathSerializer> view{obj};

    auto path_dest   = std::filesystem::temp_directory_path() / "multi_path.txt";
    auto sv_dest     = std::filesystem::temp_directory_path() / "multi_sv.txt";

    view.Serialize(path_dest);
    view.Serialize(std::string_view{sv_dest.string()});

    auto read = [](const auto& p) {
        std::ifstream f{p};
        return std::string{std::istreambuf_iterator<char>(f), {}};
    };

    EXPECT_EQ(read(path_dest), "path-overload");
    EXPECT_EQ(read(sv_dest),   "string_view-overload");
}

// impl returning duck_view: trait declares duck_view<Serializable>

struct [[=rjk::trait]] ChainableTrait {
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
    static auto AsSerializable(const auto& self) -> const Chainable& {
        return self;
    }
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