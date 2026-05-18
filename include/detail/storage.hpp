#ifndef RJK_ANY_STORAGE_HPP
#define RJK_ANY_STORAGE_HPP

#include "storage.hpp"

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

    class storage;

    struct storage_vtable {
        void (*destroy)(storage&) noexcept;
        void (*move)(storage&, storage&) noexcept;
        void (*copy)(const storage&, storage&); // null if not copyable

        template <typename T>
        consteval static storage_vtable make();
    };

    template <typename T>
    constexpr inline auto vtable_for = storage_vtable::make<T>();

    class storage {
    public:
        friend storage_vtable;

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
            return m_vtable == &vtable_for<T>;
        }

        void reset() noexcept {
            if (m_vtable) {
                m_vtable->destroy(*this);
            }
            m_vtable = nullptr;
            is_inline = false;
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

            m_vtable = &vtable_for<T>;
        }

        void copy_from(const storage& other) {
            is_inline = other.is_inline;
            m_vtable = other.m_vtable;

            if (m_vtable && m_vtable->copy) {
                m_vtable->copy(other, *this);
            }
        }
    private:
        const storage_vtable* m_vtable;

        union {
            alignas(std::max_align_t) std::array<std::byte, sbo_size> buf;
            void* ptr;
        };

        bool is_inline = false;
    };

    struct vtable_anchor {
            constexpr vtable_anchor() = default;
            ~vtable_anchor() = default;
            vtable_anchor(const vtable_anchor&) = delete;
            vtable_anchor(vtable_anchor&&) = delete;
            void operator=(const vtable_anchor&) = delete;
            void operator=(vtable_anchor&&) = delete;
        };

    template <typename T>
    consteval storage_vtable storage_vtable::make() {
        constexpr bool use_sbo = fits_sbo<T>;

        auto copy_func = std::invoke([] -> void(*)(const storage&, storage&) {
            if constexpr(std::is_copy_constructible_v<T>) {
                return [](const storage& src, storage& dest) {
                    if constexpr(use_sbo) {
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

        return storage_vtable{
            .destroy = [](storage& obj) noexcept {
                if constexpr (use_sbo) {
                    std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
                } else {
                    delete static_cast<T*>(obj.ptr);
                }
            },
            .move = [](storage& src, storage& dest) noexcept {
                if constexpr(use_sbo) {
                    std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                        std::move(*std::launder(reinterpret_cast<T*>(src.buf.data()))));
                    std::destroy_at(std::launder(reinterpret_cast<T*>(src.buf.data())));
                }
                else {
                    dest.ptr = std::exchange(src.ptr, nullptr);
                }
            },
            .copy = copy_func
        };
    }
}

#endif //RJK_ANY_STORAGE_H
