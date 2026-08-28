// clang-format off

#ifndef RJK_STORAGE_HPP
#define RJK_STORAGE_HPP


#include <cassert>

#include "detail/vtable_caller.hpp"
#include "detail/duck_base.hpp"

#include <concepts>
#include <memory>
#include <utility>

// storage is effectively an implementation of a standard type-erased container
// like std::any. The primary use is in rjk::duck, where we use this to store
// the underlying data.
namespace rjk::detail {

    template <typename T, typename Alloc, typename... Args>
    constexpr T* heap_construct(Alloc& alloc, Args&&... args) {
        using traits = std::allocator_traits<Alloc>;

        auto* obj = traits::allocate(alloc, 1);
#ifdef __EXCEPTIONS
        try {
            traits::construct(alloc, obj, std::forward<Args>(args)...);
        } catch (...) {
            traits::deallocate(alloc, obj, 1);
            throw;
        }
#else
        traits::construct(alloc, obj, std::forward<Args>(args)...);
#endif
        return obj;
    }

    template <typename T, typename Alloc>
    constexpr void heap_destroy(Alloc& alloc, T* obj) {
        using traits = std::allocator_traits<Alloc>;
        traits::destroy(alloc, obj);
        traits::deallocate(alloc, obj, 1);
    }

    template <typename DuckVtableGenerator>
    class storage {
    private:
        using caller = vtable_caller<DuckVtableGenerator>;
        using options = options_data<DuckVtableGenerator>;

        template <typename OtherVtableGen>
        friend class storage;

        friend DuckVtableGenerator;
    public:
        using allocator_type = options::allocator;

        using traits = std::allocator_traits<allocator_type>;

        constexpr static bool pocca =
            traits::propagate_on_container_copy_assignment::value;

        constexpr static bool pocma =
            traits::propagate_on_container_move_assignment::value;

        constexpr static bool pocs = traits::propagate_on_container_swap::value;

        constexpr static bool always_equal = traits::is_always_equal::value;
    public:
        template <typename T>
        constexpr static bool fits_sbo = std::is_nothrow_move_constructible_v<T>
            && sizeof(T) <= options::sbo_size && alignof(T) <= options::sbo_alignment;

        template <typename T, typename... Args>
        constexpr explicit storage(const allocator_type& alloc,
            std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>)
            : m_ptr(create<T>(alloc, m_sbo.data(), std::forward<Args>(args)...))
            , m_caller(&DuckVtableGenerator::template owning_vtable<T>)
            , m_alloc(alloc)
        { }

        template <typename T, typename... Args>
        constexpr void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>) {

            m_caller.destroy(m_ptr, m_alloc);
            m_ptr = create<T>(m_alloc, m_sbo.data(), std::forward<Args>(args)...);

            m_caller = caller{&DuckVtableGenerator::template owning_vtable<T>};
        }

        constexpr storage(const storage& other) requires (DuckVtableGenerator::can_copy)
            : storage(traits::select_on_container_copy_construction(other.m_alloc), other)
        { }

        constexpr storage(const allocator_type& alloc, const storage& other)
            requires (DuckVtableGenerator::can_copy)
            : m_ptr(other.m_caller.copy(other.m_ptr, m_sbo.data(), alloc))
            , m_caller(other.m_caller)
            , m_alloc(alloc) {
        }

        template <typename OtherVtableGen>
        constexpr storage(const storage<OtherVtableGen>& other, const auto* vtable)
            requires (DuckVtableGenerator::can_copy)
            : storage(other, vtable, traits::select_on_container_copy_construction(other.m_alloc))
        { }

        template <typename OtherVtableGen>
        constexpr storage(const storage<OtherVtableGen>& other, const auto* vtable, const allocator_type& alloc)
            requires (DuckVtableGenerator::can_copy)
            : m_ptr(vtable->copy(other.m_ptr, m_sbo.data(), alloc))
            , m_caller(vtable)
            , m_alloc(alloc)
        { }

        constexpr storage(storage&& other) noexcept(always_equal)
            : storage(other.m_alloc, std::move(other))
        { }

        constexpr storage(const allocator_type& alloc, storage&& other)
            noexcept(always_equal)
            : m_alloc(alloc) {

            if constexpr (always_equal) {
                m_ptr = other.m_caller.fast_move(other.m_ptr, m_sbo.data());
            } else if (m_alloc == other.m_alloc) {
                m_ptr = other.m_caller.fast_move(other.m_ptr, m_sbo.data());
            } else {
                m_ptr = other.m_caller.slow_move(other.m_ptr, m_sbo.data(), alloc);
                other.m_caller.destroy(other.m_ptr, other.m_alloc);
            }

            m_caller = std::move(other.m_caller);

            other.m_caller.reset();
            other.m_ptr = nullptr;
        }

        template <typename OtherVtableGen>
        constexpr storage(storage<OtherVtableGen>&& other, const auto* vtable) noexcept
            : storage(std::move(other), vtable, other.m_alloc)
        { }

        template <typename OtherVtableGen>
        constexpr storage(storage<OtherVtableGen>&& other, const auto* vtable, const allocator_type& alloc)
            noexcept(always_equal)
            : m_caller(vtable)
            , m_alloc(alloc) {
            constexpr static bool same_alloc_types = std::same_as<allocator_type,
                typename storage<OtherVtableGen>::allocator_type>;

            if constexpr (same_alloc_types) {
                if constexpr (always_equal) {
                    m_ptr = m_caller.fast_move(other.m_ptr, m_sbo.data());
                } else if (m_alloc == other.m_alloc) {
                    m_ptr = m_caller.fast_move(other.m_ptr, m_sbo.data());
                } else {
                    m_ptr = m_caller.slow_move(other.m_ptr, m_sbo.data(), m_alloc);
                    other.m_caller.destroy(other.m_ptr, other.m_alloc);
                }
            } else {
                m_ptr = m_caller.slow_move(other.m_ptr, m_sbo.data(), m_alloc);
                other.m_caller.destroy(other.m_ptr, other.m_alloc);
            }

            other.m_ptr = nullptr;
            other.m_caller.reset();
        }

        constexpr storage& operator=(const storage& other) requires (DuckVtableGenerator::can_copy) {
            if (this == &other) {
                return *this;
            }

            if constexpr (pocca) {
                void* new_ptr = other.m_caller.copy(other.m_ptr, m_sbo.data(), other.m_alloc);

                m_caller.destroy(m_ptr, m_alloc);
                m_ptr = new_ptr;
                m_alloc = other.m_alloc;
            } else {
                void* new_ptr = other.m_caller.copy(other.m_ptr, m_sbo.data(), m_alloc);
                m_caller.destroy(m_ptr, m_alloc);
                m_ptr = new_ptr;
            }
            m_caller = other.m_caller;

            return *this;
        }

        constexpr storage& operator=(storage&& other) noexcept(always_equal || pocma) {
            if (this == &other) {
                return *this;
            }

            if constexpr (always_equal || pocma) {
                m_caller.destroy(m_ptr, m_alloc);
                m_ptr = other.m_caller.fast_move(other.m_ptr, m_sbo.data());
                if constexpr (pocma) {
                    m_alloc = std::move(other.m_alloc);
                }
            } else if (m_alloc == other.m_alloc) {
                m_caller.destroy(m_ptr, m_alloc);
                m_ptr = other.m_caller.fast_move(other.m_ptr, m_sbo.data());
            } else {
                void* new_ptr = other.m_caller.slow_move(other.m_ptr, m_sbo.data(), m_alloc);

                m_caller.destroy(m_ptr, m_alloc);
                other.m_caller.destroy(other.m_ptr, other.m_alloc);

                m_ptr = new_ptr;
            }

            m_caller = std::move(other.m_caller);
            other.m_ptr = nullptr;
            other.m_caller.reset();

            return *this;
        }

        constexpr bool is_sbo_resident() const noexcept {
            return m_ptr == static_cast<const void*>(m_sbo.data());
        }

        constexpr void swap(storage& other) noexcept(always_equal || pocs) {
            if (this == &other) {
                return;
            }

            if constexpr (!pocs && !always_equal) {
                // swapping storages with unequal, non-propagating allocators
                // is undefined behavior
                [[maybe_unused]] const bool both_sbo = is_sbo_resident() && other.is_sbo_resident();
                [[maybe_unused]] const bool both_empty = !has_value() && !other.has_value();
                [[maybe_unused]] const bool equal_allocs = m_alloc == other.m_alloc;
                assert((both_sbo || both_empty || equal_allocs) &&
                       "storage::swap: allocators must compare equal unless "
                       "propagate_on_container_swap is true");
            }

            alignas(options::sbo_alignment) std::array<std::byte, options::sbo_size> tmp;
            void* this_relocated = m_caller.fast_move(m_ptr, tmp.data());

            m_ptr = other.m_caller.fast_move(other.m_ptr, m_sbo.data());
            other.m_ptr = m_caller.fast_move(this_relocated, other.m_sbo.data());

            using std::swap;
            swap(m_caller, other.m_caller);
            if constexpr (pocs) {
                swap(m_alloc, other.m_alloc);
            }
        }

        constexpr ~storage() {
            m_caller.destroy(m_ptr, m_alloc);
        }

        constexpr void* get() noexcept {
            return m_ptr;
        }

        constexpr const void* get() const noexcept {
            return m_ptr;
        }

        constexpr bool has_value() const noexcept {
            return m_caller.has_value();
        }

        template <typename T>
        constexpr bool has_type() const noexcept {
            return get_vtable() == &DuckVtableGenerator::template owning_vtable<T>;
        }

        constexpr const auto& callable() const noexcept {
            return m_caller;
        }

        constexpr const auto* get_vtable() const noexcept {
            return m_caller.get_vtable();
        }

        constexpr const allocator_type& get_allocator() const noexcept {
            return m_alloc;
        }
    private:
        template <typename T, typename... Args>
        constexpr static void* create(const allocator_type& alloc, std::byte* dest, Args&&... args) {
            if !consteval {
                if constexpr (fits_sbo<T>) {
                    return std::construct_at(reinterpret_cast<T*>(dest), std::forward<Args>(args)...);
                }
            }

            typename traits::template rebind_alloc<T> rebound{alloc};
            return heap_construct<T>(rebound, std::forward<Args>(args)...);
        }
    private:
        void* m_ptr;
        caller m_caller;

        [[no_unique_address]] alignas(options::sbo_alignment)
            std::array<std::byte, options::sbo_size> m_sbo;

        [[no_unique_address]] allocator_type m_alloc;
    };

    template <typename... Traits>
    template <typename T>
    consteval void vtable_generator<Traits...>::
        set_storage_functions(vtable& static_vtable) {

        using storage_t =
            storage<vtable_generator>;
        using allocator = storage_t::allocator_type;
        using rebound_t = storage_t::traits::template rebind_alloc<T>;

        constexpr static bool fits_sbo = storage_t::template fits_sbo<T>;

        if constexpr (can_copy) {
            static_vtable.copy = [](const void* src, std::byte* dest, const allocator& alloc) -> void* {
                if !consteval {
                    if constexpr (fits_sbo) {
                        return std::construct_at(reinterpret_cast<T*>(dest),
                            *std::launder(static_cast<const T*>(src)));
                    }
                }

                rebound_t rebound{alloc};
                return heap_construct<T>(rebound, *static_cast<const T*>(src));
            };
        }

        static_vtable.destroy = [](void* src, const allocator& alloc) noexcept {
            if !consteval {
                if constexpr (fits_sbo) {
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        std::destroy_at(std::launder(static_cast<T*>(src)));
                    }
                    return;
                }
            }

            rebound_t rebound{alloc};
            heap_destroy(rebound, static_cast<T*>(src));
        };

        static_vtable.slow_move = [](void* src, std::byte* dest, const allocator& alloc)
            noexcept(fits_sbo) -> void* {
            if !consteval {
                if constexpr (fits_sbo) {
                    return std::construct_at(reinterpret_cast<T*>(dest),
                        std::move(*std::launder(static_cast<T*>(src))));
                }
            }

            rebound_t rebound{alloc};
            return heap_construct<T>(rebound, std::move(*static_cast<T*>(src)));
        };

        static_vtable.fast_move = [](void* src, std::byte* dest) noexcept -> void* {
            if !consteval {
                if constexpr (fits_sbo) {
                    void* ret = std::construct_at(reinterpret_cast<T*>(dest),
                        std::move(*std::launder(static_cast<T*>(src))));
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        std::destroy_at(std::launder(static_cast<T*>(src)));
                    }
                    return ret;
                }
            }
            return src;
        };
    }

}

#endif
