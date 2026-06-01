#ifndef RJK_ANY_STORAGE_HPP
#define RJK_ANY_STORAGE_HPP

#include "storage.hpp"
#include "duck_base.hpp"

#include "gtest/gtest-param-test.h"
#include <concepts>

// storage is effectively an implementation of a standard type-erased container
// like std::any. The primary use is in rjk::duck, where we use this to store
// the underlying data.
namespace rjk::detail {
    // TODO: Add customizability options for SBO
    constexpr static std::size_t sbo_size = 32;

    template <typename T>
    concept fits_sbo = (sizeof(T) <= sbo_size) &&
        (alignof(T) <= alignof(std::max_align_t)) &&
        std::is_nothrow_move_constructible_v<T>;

    template <typename DuckBase>
    class storage {
    public:
        friend DuckBase;

        constexpr storage() = default;

        template <typename T, typename... Args>
        explicit storage(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && fits_sbo<std::decay_t<T>>) {
            init<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && fits_sbo<std::decay_t<T>>) {
            reset();
            init<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        storage(const storage& other) {
            copy_from(other);
        }

        storage(storage&& other) noexcept
            : m_vtable(std::exchange(other.m_vtable, nullptr))
            , is_inline(other.is_inline) {
            if (m_vtable != nullptr) {
                m_vtable->move(other, *this);
            }
        }

        storage& operator=(const storage& other) {
            if (this != &other) {
                if (m_vtable != nullptr) {
                    m_vtable->destroy(*this);
                }
                copy_from(other);
            }
            return *this;
        }

        storage& operator=(storage&& other) noexcept {
            if (this != & other) {
                if (m_vtable != nullptr) {
                    m_vtable->destroy(*this);
                }

                is_inline = other.is_inline;
                m_vtable = std::exchange(other.m_vtable, nullptr);

                if (m_vtable != nullptr) {
                    m_vtable->move(other, *this);
                }
            }
            return *this;
        }

        ~storage() {
            if (m_vtable != nullptr) {
                m_vtable->destroy(*this);
            }
        }

        constexpr bool using_sbo() const noexcept { return is_inline; }

        void* get() noexcept {
            return is_inline ? std::launder(buf.data()) : ptr;
        }

        const void* get() const noexcept {
            return is_inline ? std::launder(buf.data()) : ptr;
        }

        bool has_value() const noexcept {
            return m_vtable != nullptr;
        }

        template <typename T>
        constexpr bool has_type() const noexcept {
            return m_vtable == &DuckBase::template static_vtable_for<T>;
        }

        void reset() noexcept {
            if (m_vtable != nullptr) {
                m_vtable->destroy(*this);
            }
            m_vtable = nullptr;
            is_inline = false;
        }

        const auto* vtable() const noexcept {
            return m_vtable;
        }
    private:
        template <typename T>
        static void call_destroy(storage& obj) noexcept {
            if constexpr (fits_sbo<T>) {
                std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
            } else {
                delete static_cast<T*>(obj.ptr);
            }
        }

        template <typename T, typename... Args>
        void init(Args&&... args) {
            is_inline = fits_sbo<T>;

            if constexpr(fits_sbo<T>) {
                std::construct_at(reinterpret_cast<T*>(buf.data()), std::forward<Args>(args)...);
            }
            else {
                ptr = new T(std::forward<Args>(args)...);
            }

            m_vtable = &DuckBase::template static_vtable_for<T>;
        }

        void copy_from(const storage& other) {
            is_inline = other.is_inline;
            m_vtable = other.m_vtable;

            if (m_vtable && m_vtable->copy) {
                m_vtable->copy(other, *this);
            }
        }
    private:
        union {
            alignas(std::max_align_t) std::array<std::byte, sbo_size> buf;
            void* ptr;
        };

        const typename DuckBase::static_duck_vtable* m_vtable;

        bool is_inline = false;
    };

    template <typename Derived, duck_tag... Tags>
    template <typename T>
    consteval void duck_base<Derived, Tags...>::set_storage_functions(static_duck_vtable& static_vtable) {
        using StorageT = storage<duck_base<Derived, Tags...>>;

        static_vtable.copy = std::invoke([] -> void(*)(const StorageT&, StorageT&) {
            if constexpr(std::is_copy_constructible_v<T>) {
                return [](const StorageT& src, StorageT& dest) {
                    if constexpr(fits_sbo<T>) {
                        std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                            *std::launder(reinterpret_cast<const T*>(src.buf.data())));
                    } else {
                        dest.ptr = new T(*static_cast<T*>(src.ptr));
                    }
                };
            } else {
                return nullptr;
            }
        });

        static_vtable.destroy = [](StorageT& obj) noexcept {
            if constexpr (fits_sbo<T>) {
                std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
            } else {
                delete static_cast<T*>(obj.ptr);
            }
        };

        static_vtable.move = [](StorageT& src, StorageT& dest) noexcept {
            if constexpr(fits_sbo<T>) {
                std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                    std::move(*std::launder(reinterpret_cast<T*>(src.buf.data()))));
                std::destroy_at(std::launder(reinterpret_cast<T*>(src.buf.data())));
            }
            else {
                dest.ptr = std::exchange(src.ptr, nullptr);
            }
        };
    }

}

#endif //RJK_ANY_STORAGE_H
