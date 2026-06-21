// clang-format off

#ifndef RJK_ANY_STORAGE_HPP
#define RJK_ANY_STORAGE_HPP

#include <cassert>

#include "detail/storage.hpp"
#include "detail/duck_base.hpp"

#include <concepts>

// storage is effectively an implementation of a standard type-erased container
// like std::any. The primary use is in rjk::duck, where we use this to store
// the underlying data.
namespace rjk::detail {
    struct default_perf_options {
        std::size_t sbo_size = 32;
        std::size_t sbo_alignment = alignof(std::max_align_t);
    };

    template <typename DuckVtableGenerator>
    class storage {
    private:
        using options = [: std::invoke([] consteval {
            auto all_traits = template_arguments_of(^^DuckVtableGenerator);

            const auto has_perf_options = [](auto type) {
                return has_annotation(type, ^^perf_options);
            };

            const auto first_itr = std::ranges::find_if(all_traits, has_perf_options);
            if (first_itr == all_traits.end()) {
                return ^^default_perf_options;
            }

            const auto second_itr = std::ranges::find_if(std::next(first_itr),
                all_traits.end(), has_perf_options);
            if (second_itr != all_traits.end()) {
                std::string start{"Found two definitions with [[=rjk::perf_options]]: "};
                display_error(start + display_string_of(*first_itr) + " and "
                    + display_string_of(*second_itr));
            }

            const auto member_identifiers = [](std::meta::info class_type) {
                const auto ctx = std::meta::access_context::unprivileged();
                return members_of(class_type, ctx)
                    | std::views::filter(std::meta::has_identifier)
                    | std::views::transform(std::meta::identifier_of)
                    | std::ranges::to<std::vector>();
            };
            const auto fields = member_identifiers(*first_itr);
            for (const auto identifier : member_identifiers(^^default_perf_options)) {
                const bool has_member = std::ranges::contains(fields, identifier);
                if (!has_member) {
                    display_error(std::string{"customized performance options '"}
                        + identifier_of(*first_itr) +"' are missing field '"
                        + identifier + "'");
                }
            }
            return *first_itr;
        }) :];

        constexpr static auto sbo_size = options{}.sbo_size;
        constexpr static auto sbo_alignment = options{}.sbo_alignment;
    public:
        template <typename T>
        constexpr static bool fits_sbo = std::is_nothrow_move_constructible_v<T>
            && sizeof(T) <= sbo_size && alignof(T) <= sbo_alignment;

        friend DuckVtableGenerator;

        constexpr storage() = default;

        template <typename T, typename... Args>
        constexpr explicit storage(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && fits_sbo<std::decay_t<T>>)
            : m_vtable(&DuckVtableGenerator::template static_vtable_for<std::decay_t<T>>)
            , is_inline(!std::is_constant_evaluated() && fits_sbo<std::decay_t<T>>) {
            init_data<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        constexpr void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && fits_sbo<std::decay_t<T>>) {
            reset();
            is_inline = !std::is_constant_evaluated() && fits_sbo<T>;
            m_vtable = &DuckVtableGenerator::template static_vtable_for<T>;
            init_data<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        constexpr storage(const storage& other)
            : m_vtable(other.m_vtable)
            , is_inline(other.is_inline) {
            static_assert(DuckVtableGenerator::can_copy, "duck cannot be copied. Did you mean to use rjk::copyable?");
            copy_from(other);
        }

        constexpr storage(storage&& other) noexcept
            : m_vtable(std::exchange(other.m_vtable, nullptr))
            , is_inline(other.is_inline) {
            if (m_vtable == nullptr) {
                return;
            }

            if (other.is_inline) {
                m_vtable->move(static_cast<void*>(other.buf.data()), *this);
            } else {
                m_vtable->move(other.ptr, *this);
            }
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
                is_inline = other.is_inline;
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

                is_inline = other.is_inline;
                m_vtable = std::exchange(other.m_vtable, nullptr);

                if (m_vtable != nullptr) {
                    if (other.is_inline) {
                        m_vtable->move(static_cast<void*>(other.buf.data()), *this);
                    } else {
                        m_vtable->move(other.ptr, *this);
                    }
                }
            }
            return *this;
        }

        constexpr ~storage() {
            if (m_vtable != nullptr) {
                m_vtable->destroy(*this);
            }
        }

        constexpr bool using_sbo() const noexcept { return is_inline; }

        constexpr void* get() noexcept {
            return is_inline ? std::launder(buf.data()) : ptr;
        }

        constexpr const void* get() const noexcept {
            return is_inline ? std::launder(buf.data()) : ptr;
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
            is_inline = false;
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
                }
                else {
                    ptr = new T(std::forward<Args>(args)...);
                }
            }
        }

        constexpr void copy_from(const storage& other) {
            if (m_vtable && m_vtable->copy) {
                if (other.is_inline) {
                    m_vtable->copy(static_cast<const void*>(other.buf.data()), *this);
                } else {
                    m_vtable->copy(other.ptr, *this);
                }
            }
        }
    private:
        union {
            alignas(std::max_align_t) std::array<std::byte, sbo_size> buf;
            void* ptr;
        };

        const typename DuckVtableGenerator::vtable* m_vtable;

        bool is_inline = false;
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
                    dest.is_inline = false;
                } else {
                    if constexpr(fits_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                            *std::launder(reinterpret_cast<const T*>(src)));
                        dest.is_inline = true;
                    } else {
                        dest.ptr = new T(*static_cast<const T*>(src));
                        dest.is_inline = false;
                    }
                }
            };
        }

        static_vtable.destroy = [](StorageT& obj) noexcept {
            if consteval {
                delete static_cast<T*>(obj.ptr);
            } else {a
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
                dest.is_inline = false;
            } else {
                if constexpr(fits_sbo) {
                    std::construct_at(reinterpret_cast<T*>(dest.buf.data()),
                        std::move(*std::launder(reinterpret_cast<T*>(src))));
                    std::destroy_at(std::launder(reinterpret_cast<T*>(src)));
                    dest.is_inline = true;
                } else {
                    dest.ptr = std::exchange(src, nullptr);
                    dest.is_inline = false;
                }
            }
        };
    }

}

#endif
