// clang-format off

#ifndef RJK_STORAGE_HPP
#define RJK_STORAGE_HPP


#include <cassert>

#include "detail/vtable_caller.hpp"
#include "detail/duck_base.hpp"

#include <concepts>

// storage is effectively an implementation of a standard type-erased container
// like std::any. The primary use is in rjk::duck, where we use this to store
// the underlying data.
namespace rjk::detail {
    template <typename DuckVtableGenerator>
    class storage {
    private:
        using caller = vtable_caller<DuckVtableGenerator>;
        using options = options_data<DuckVtableGenerator>;

        using alloc_t = options::allocator;
        static_assert(std::is_default_constructible_v<alloc_t>, "Stateful"
            "allocators are not currently supported");

        template <typename T>
        using rebound_alloc = std::allocator_traits<alloc_t>::template rebind_alloc<T>;
    public:
        template <typename T>
        constexpr static bool fits_sbo = std::is_nothrow_move_constructible_v<T>
            && sizeof(T) <= options::sbo_size && alignof(T) <= options::sbo_alignment;

        friend DuckVtableGenerator;

        template <typename T, typename... Args>
        constexpr explicit storage(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>)
            : m_caller(&DuckVtableGenerator::template static_vtable_for<T>) {
            init_data<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        constexpr void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>) {
            get_vtable()->destroy(*this);
            m_caller = caller{&DuckVtableGenerator::template static_vtable_for<T>};
            init_data<T>(std::forward<Args>(args)...);
        }

        constexpr storage(const storage& other) requires (DuckVtableGenerator::can_copy)
            : m_caller(other.m_caller) {
            copy_from(other);
        }

        constexpr storage(storage&& other) noexcept
            : m_caller(std::move(other.m_caller)) {
            other.m_caller.reset();
            if (get_vtable() != nullptr) {
                get_vtable()->move(other.ptr, *this);
            }
        }

        // The following two functions are designed to handle copy/move construction
        // from a subsumed duck or duck_view. The overload resolution prevents a duck
        // from moving from a non-owning duck_view, but allows it to move from an
        // owning duck (if it contains a const object). The

        constexpr storage(const void* underlying, const auto* vtable, auto)
            requires (DuckVtableGenerator::can_copy)
            : m_caller(vtable) {
            get_vtable()->copy(underlying, *this);
        }

        template <bool AllowMove> requires (AllowMove || DuckVtableGenerator::can_copy)
        constexpr storage(void* underlying, const auto* vtable, std::bool_constant<AllowMove>)
            : m_caller(vtable) {
            if constexpr (AllowMove) {
                get_vtable()->move(underlying, *this);
            } else {
                get_vtable()->copy(underlying, *this);
            }
        }

        constexpr storage& operator=(const storage& other) requires (DuckVtableGenerator::can_copy) {
            if (this != &other) {
                if (get_vtable() != nullptr) {
                    get_vtable()->destroy(*this);
                }
                m_caller = other.m_caller;
                copy_from(other);
            }
            return *this;
        }

        constexpr storage& operator=(storage&& other) noexcept {
            if (this != &other) {
                if (get_vtable() != nullptr) {
                    get_vtable()->destroy(*this);
                }

                m_caller = std::move(other.m_caller);
                other.m_caller.reset();

                if (get_vtable() != nullptr) {
                    get_vtable()->move(static_cast<void*>(other.ptr), *this);
                }
            }
            return *this;
        }

        constexpr ~storage() {
            if (get_vtable() != nullptr) {
                get_vtable()->destroy(*this);
            }
        }

        constexpr void* get() noexcept {
            return ptr;
        }

        constexpr const void* get() const noexcept {
            return ptr;
        }

        constexpr bool has_value() const noexcept {
            return get_vtable() != nullptr;
        }

        template <typename T>
        constexpr bool has_type() const noexcept {
            return get_vtable() == &DuckVtableGenerator::template static_vtable_for<T>;
        }

        constexpr const auto& callable() const noexcept {
            return m_caller;
        }

        constexpr const auto* get_vtable() const noexcept {
            return m_caller.get_vtable();
        }
    private:
        template <typename T, typename... Args>
        constexpr void init_data(Args&&... args) {
            using rebound = rebound_alloc<T>;
            using traits = std::allocator_traits<rebound>;

            ptr = [&] -> void* {
                if !consteval {
                    if constexpr(fits_sbo<T>) {
                        return std::construct_at(reinterpret_cast<T*>(buf.data()), std::forward<Args>(args)...);
                    }
                }

                rebound alloc{};
                auto* obj = traits::allocate(alloc, 1);

#ifdef __EXCEPTIONS
                try {
                    traits::construct(alloc, obj, std::forward<Args>(args)...);
                } catch (...) {
                    traits::deallocate(alloc, obj, 1);
                }
#else
                traits::construct(alloc, obj, std::forward<Args>(args)...);
#endif

                return obj;
            }();
        }

        constexpr void copy_from(const storage& other) {
            if (get_vtable()) {
                get_vtable()->copy(other.ptr, *this);
            }
        }
    private:
        void* ptr;
        caller m_caller;

        [[no_unique_address]] alignas(options::sbo_alignment)
            std::array<std::byte, options::sbo_size> buf;
    };

    template <typename... Traits>
    template <typename T>
    consteval void vtable_generator<Traits...>::
        set_storage_functions(vtable& static_vtable) {
        using StorageT =
            storage<vtable_generator<Traits...>>;
        constexpr static bool fits_sbo = StorageT::template fits_sbo<T>;

        using rebound = StorageT::template rebound_alloc<T>;
        using traits = std::allocator_traits<rebound>;

        if constexpr (can_copy) {
            static_vtable.copy = [](const void* src, StorageT& dest) {
                dest.ptr = [&] -> void* {
                    if !consteval {
                        if constexpr (fits_sbo) {
                            return std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                            *std::launder(reinterpret_cast<const T*>(src)));
                        }
                    }

                    rebound alloc{};
                    auto* obj = traits::allocate(alloc, 1);
#ifdef __EXCEPTIONS
                    try {
                        traits::construct(alloc, obj, *static_cast<const T*>(src));
                    } catch (...) {
                        traits::deallocate(alloc, obj, 1);
                        throw;
                    }
#else
                    traits::construct(alloc, obj, *static_cast<const T*>(src));
#endif
                    return obj;
                }();
            };
        }

        static_vtable.destroy = [](StorageT& obj) noexcept {
            if !consteval {
                if constexpr (fits_sbo) {
                    std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
                    return;
                }
            }

            rebound alloc{};
            auto* typed = static_cast<T*>(obj.ptr);
            traits::destroy(alloc, typed);
            traits::deallocate(alloc, typed, 1);
        };

        static_vtable.move = [](void* src, StorageT& dest) noexcept {
            dest.ptr = [&] -> void* {
                if !consteval {
                    if constexpr(fits_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                            std::move(*std::launder(reinterpret_cast<T*>(src))));
                        std::destroy_at(std::launder(reinterpret_cast<T*>(src)));
                        return dest.buf.data();
                    }
                }
                return std::exchange(src, nullptr);
            }();
        };
    }

}

#endif
