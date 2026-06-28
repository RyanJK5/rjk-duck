// clang-format off

#ifndef RJK_ANY_STORAGE_HPP
#define RJK_ANY_STORAGE_HPP


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
    public:
        template <typename T>
        constexpr static bool fits_sbo = std::is_nothrow_move_constructible_v<T>
            && sizeof(T) <= caller::sbo_size && alignof(T) <= caller::sbo_alignment;

        friend DuckVtableGenerator;

        template <typename T, typename... Args>
        constexpr explicit storage(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && fits_sbo<std::decay_t<T>>)
            : m_vtable(&DuckVtableGenerator::template static_vtable_for<std::decay_t<T>>) {
            init_data<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        constexpr void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && fits_sbo<std::decay_t<T>>) {
            reset();
            m_vtable = &DuckVtableGenerator::template static_vtable_for<T>;
            init_data<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        constexpr storage(const storage& other)
            : m_vtable(other.m_vtable) {
            static_assert(DuckVtableGenerator::can_copy, "duck cannot be copied. Did you mean to use rjk::copyable?");
            copy_from(other);
        }

        constexpr storage(storage&& other) noexcept
            : m_vtable(std::exchange(other.m_vtable, nullptr)) {
            if (m_vtable == nullptr) {
                return;
            }

            m_vtable->move(other.ptr, *this);
        }

        // The following two functions are designed to handle copy/move construction
        // from a subsumed duck or duck_view. The overload resolution prevents a duck
        // from moving from a non-owning duck_view, but allows it to move from an
        // owning duck (if it contains a const object). The

        constexpr storage(const void* underlying, const auto* vtable, auto)
            : m_vtable(vtable) {
            static_assert(DuckVtableGenerator::can_copy, "duck cannot be copied. Did you mean to use rjk::copyable?");
            m_vtable->copy(underlying, *this);
        }

        template <bool AllowMove>
        constexpr storage(void* underlying, const auto* vtable, std::bool_constant<AllowMove>)
            : m_vtable(vtable) {
            if constexpr (AllowMove) {
                m_vtable->move(underlying, *this);
            } else {
                static_assert(DuckVtableGenerator::can_copy, "duck cannot be copied. Did you mean to use rjk::copyable?");
                m_vtable->copy(underlying, *this);
            }
        }

        constexpr storage& operator=(const storage& other) {
            static_assert(DuckVtableGenerator::can_copy, "duck cannot be copied. Did you mean to use rjk::copyable?");
            if (this != &other) {
                if (m_vtable != nullptr) {
                    m_vtable->destroy(*this);
                }
                m_vtable = other.m_vtable;
                copy_from(other);
            }
            return *this;
        }

        constexpr storage& operator=(storage&& other) noexcept {
            if (this != &other) {
                if (m_vtable != nullptr) {
                    m_vtable->destroy(*this);
                }

                m_vtable = std::exchange(other.m_vtable, nullptr);

                if (m_vtable != nullptr) {
                    m_vtable->move(static_cast<void*>(other.ptr), *this);
                }
            }
            return *this;
        }

        constexpr ~storage() {
            if (m_vtable != nullptr) {
                m_vtable->destroy(*this);
            }
        }

        constexpr void* get() noexcept {
            return ptr;
        }

        constexpr const void* get() const noexcept {
            return ptr;
        }

        constexpr bool has_value() const noexcept {
            return m_vtable != nullptr;
        }

        template <typename T>
        constexpr bool has_type() const noexcept {
            return m_vtable == &DuckVtableGenerator::template static_vtable_for<T>;
        }

        constexpr void reset() noexcept {
            if (m_vtable != nullptr) {
                m_vtable->destroy(*this);
            }
            m_vtable = nullptr;
        }

        constexpr const auto* vtable() const noexcept {
            return m_vtable;
        }
    private:
        template <typename T, typename... Args>
        constexpr void init_data(Args&&... args) {
            if consteval {
                ptr = new T(std::forward<Args>(args)...);
            } else {
                if constexpr(fits_sbo<T>) {
                    std::construct_at(reinterpret_cast<T*>(buf.data()), std::forward<Args>(args)...);
                    ptr = buf.data();
                }
                else {
                    ptr = new T(std::forward<Args>(args)...);
                }
            }
        }

        constexpr void copy_from(const storage& other) {
            if (m_vtable && m_vtable->copy) {
                m_vtable->copy(other.ptr, *this);
            }
        }
    private:
        alignas(caller::sbo_alignment) std::array<std::byte, caller::sbo_size> buf;
        void* ptr;

        const typename DuckVtableGenerator::vtable* m_vtable;
    };

    template <is_trait... Traits>
    template <typename T>
    consteval void vtable_generator<Traits...>::
        set_storage_functions(vtable& static_vtable) {
        using StorageT =
            storage<vtable_generator<Traits...>>;
        constexpr static bool fits_sbo = StorageT::template fits_sbo<T>;

        if constexpr (can_copy) {
            static_vtable.copy = [](const void* src, StorageT& dest) {
                if consteval {
                    dest.ptr = new T(*static_cast<const T*>(src));
                } else {
                    if constexpr(fits_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                            *std::launder(reinterpret_cast<const T*>(src)));
                        dest.ptr = dest.buf.data();
                    } else {
                        dest.ptr = new T(*static_cast<const T*>(src));
                    }
                }
            };
        }

        static_vtable.destroy = [](StorageT& obj) noexcept {
            if consteval {
                delete static_cast<T*>(obj.ptr);
            } else {
                if constexpr (fits_sbo) {
                    std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
                } else {
                    delete static_cast<T*>(obj.ptr);
                }
            }
        };

        static_vtable.move = [](void* src, StorageT& dest) noexcept {
            if consteval {
                dest.ptr = std::exchange(src, nullptr);
            } else {
                if constexpr(fits_sbo) {
                    std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                        std::move(*std::launder(reinterpret_cast<T*>(src))));
                    std::destroy_at(std::launder(reinterpret_cast<T*>(src)));
                    dest.ptr = dest.buf.data();
                } else {
                    dest.ptr = std::exchange(src, nullptr);
                }
            }
        };
    }

}

#endif
