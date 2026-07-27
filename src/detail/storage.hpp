// clang-format off

#ifndef RJK_STORAGE_HPP
#define RJK_STORAGE_HPP


#include <cassert>

#include "detail/vtable_caller.hpp"
#include "detail/duck_base.hpp"
#include "rjk/duck.hpp"

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

        using allocator_type = options::allocator;

        using alloc_traits = std::allocator_traits<allocator_type>;

        template <typename T>
        using rebound_alloc = alloc_traits::template rebind_alloc<T>;

        template <typename T>
        constexpr static bool rebind_is_constructible =
            std::constructible_from<rebound_alloc<T>, const allocator_type&>;
    public:
        template <typename T>
        constexpr static bool fits_sbo = std::is_nothrow_move_constructible_v<T>
            && sizeof(T) <= options::sbo_size && alignof(T) <= options::sbo_alignment;

        friend DuckVtableGenerator;

        template <typename T, typename... Args>
            requires std::default_initializable<allocator_type>
        constexpr explicit storage(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>)
            : m_caller(&DuckVtableGenerator::template static_vtable_for<T>) {
            init_data<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
            requires (rebind_is_constructible<T>)
        constexpr explicit storage(std::allocator_arg_t, const allocator_type& alloc,
            std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>)
            : m_caller(&DuckVtableGenerator::template static_vtable_for<T>)
            , m_alloc(alloc) {
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
            : m_caller(other.m_caller)
            , m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)) {
            copy_from(other);
        }

        constexpr storage(storage&& other) noexcept
            : m_caller(std::move(other.m_caller))
            , m_alloc(std::move(other.m_alloc)) {
            other.m_caller.reset();
            if (get_vtable() != nullptr) {
                get_vtable()->move_construct(other, *this);
            }
        }

        // Copying from a duck_view
        constexpr storage(const void* underlying, const auto* vtable)
            requires (DuckVtableGenerator::can_copy)
            : m_caller(vtable) {
            get_vtable()->copy(underlying, *this);
        }

        constexpr storage& operator=(const storage& other) requires (DuckVtableGenerator::can_copy) {
            if (this != &other) {
                if (get_vtable() != nullptr) {
                    get_vtable()->destroy(*this);
                }
                m_caller = other.m_caller;
                if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
                    m_alloc = other.m_alloc;
                }
                copy_from(other);
            }
            return *this;
        }

        constexpr storage& operator=(storage&& other) noexcept(alloc_traits::is_always_equal::value ||
            alloc_traits::propagate_on_container_move_assignment::value) {
            if (this == &other) {
                return *this;
            }

            if (get_vtable() != nullptr) {
                get_vtable()->destroy(*this);
            }

            m_caller = std::move(other.m_caller);
            other.m_caller.reset();

            if (get_vtable() != nullptr) {
                get_vtable()->move_assign(other, *this);
            }

            return *this;
        }

        constexpr ~storage() {
            if (get_vtable() != nullptr) {
                get_vtable()->destroy(*this);
            }
        }

        constexpr void* get() noexcept {
            return m_ptr;
        }

        constexpr const void* get() const noexcept {
            return m_ptr;
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
            m_ptr = [&] -> void* {
                if !consteval {
                    if constexpr(fits_sbo<T>) {
                        return std::construct_at(reinterpret_cast<T*>(m_sbo.data()), std::forward<Args>(args)...);
                    }
                }

                rebound_alloc<T> alloc{m_alloc};
                return heap_construct<T>(alloc, std::forward<Args>(args)...);
            }();
        }

        constexpr void copy_from(const storage& other) {
            if (get_vtable()) {
                get_vtable()->copy(other.m_ptr, *this);
            }
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
        using StorageT =
            storage<vtable_generator<Traits...>>;
        constexpr static bool fits_sbo = StorageT::template fits_sbo<T>;

        using rebound_t = StorageT::template rebound_alloc<T>;

        if constexpr (can_copy) {
            static_vtable.copy = [](const void* src, StorageT& dest) {
                dest.m_ptr = [&] -> void* {
                    if !consteval {
                        if constexpr (fits_sbo) {
                            return std::construct_at(reinterpret_cast<T*>(dest.m_sbo.data()),
                            *std::launder(reinterpret_cast<const T*>(src)));
                        }
                    }

                    rebound_t alloc{dest.m_alloc};
                    return heap_construct<T>(alloc, *static_cast<const T*>(src));
                }();
            };
        }

        static_vtable.destroy = [](StorageT& obj) noexcept {
            if !consteval {
                if constexpr (fits_sbo) {
                    std::destroy_at(std::launder(reinterpret_cast<T*>(obj.m_sbo.data())));
                    return;
                }
            }

            rebound_t alloc{obj.m_alloc};
            heap_destroy(alloc, static_cast<T*>(obj.m_ptr));
        };

        static_vtable.move_construct = [](StorageT& src, StorageT& dest) noexcept {
            dest.m_ptr = [&] -> void* {
                if !consteval {
                    if constexpr (fits_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.m_sbo.data()),
                            std::move(*std::launder(reinterpret_cast<T*>(src.m_ptr))));
                        std::destroy_at(std::launder(reinterpret_cast<T*>(src.m_ptr)));
                        return dest.m_sbo.data();
                    }
                }
                return std::exchange(src.m_ptr, nullptr);
            }();
        };

        constexpr static auto pocma =
            StorageT::alloc_traits::propagate_on_container_move_assignment::value;
        constexpr static auto always_equal =
            StorageT::alloc_traits::is_always_equal::value;
        static_vtable.move_assign = [](StorageT& src, StorageT& dest)
            noexcept(fits_sbo || always_equal || pocma) {

            bool can_steal = true;
            if constexpr (!always_equal) {
                if constexpr (pocma) {
                    dest.m_alloc = std::move(src.m_alloc);
                } else {
                    can_steal = (dest.m_alloc == src.m_alloc);
                }
            }

            dest.m_ptr = [&] -> void* {
                if !consteval {
                    if constexpr (fits_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.m_sbo.data()),
                            std::move(*std::launder(reinterpret_cast<T*>(src.m_ptr))));
                        std::destroy_at(std::launder(reinterpret_cast<T*>(src.m_ptr)));
                        return dest.m_sbo.data();
                    }
                }

                // Compile-time guaranteed fast path
                if constexpr (always_equal || pocma) {
                    return std::exchange(src.m_ptr, nullptr);
                } else if (can_steal) { // Run-time guaranteed fast path
                    return std::exchange(src.m_ptr, nullptr);
                }

                rebound_t dest_alloc{dest.m_alloc};
                auto* obj = heap_construct<T>(dest_alloc, std::move(*static_cast<T*>(src.m_ptr)));

                rebound_t src_alloc{src.m_alloc};
                heap_destroy(src_alloc, static_cast<T*>(src.m_ptr));
                return obj;
            }();
        };
    }

}

#endif
