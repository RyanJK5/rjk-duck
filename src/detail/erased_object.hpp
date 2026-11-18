// clang-format off

#ifndef RJK_ERASED_OBJECT_HPP
#define RJK_ERASED_OBJECT_HPP


#include <cassert>

#include "detail/vtable_caller.hpp"
#include "detail/duck_base.hpp"

#include <concepts>
#include <memory>
#include <utility>

// erased_object is effectively an implementation of a standard type-erased container
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
    class erased_object {
    private:
        using caller = vtable_caller<DuckVtableGenerator, typename DuckVtableGenerator::owning_table>;
        using options = options_data<DuckVtableGenerator>;
        using storage_type = DuckVtableGenerator::storage_type;

        template <typename OtherVtableGen>
        friend class erased_object;

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

        // Whether this erased_object can take over another's object as-is: only
        // if both would store it in the same place, with the same allocator type.
        template <typename OtherVtableGen>
        constexpr static bool can_take_over = std::same_as<allocator_type,
                typename erased_object<OtherVtableGen>::allocator_type>
            && options::sbo_size == options_data<OtherVtableGen>::sbo_size
            && options::sbo_alignment == options_data<OtherVtableGen>::sbo_alignment;

        template <typename T, typename... Args>
        constexpr explicit erased_object(const allocator_type& alloc,
            std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>)
            : m_caller(&DuckVtableGenerator::template owning_vtable<T>)
            , m_alloc(alloc) {
            create<T>(m_alloc, m_storage, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        constexpr void emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<T, Args...> && fits_sbo<T>) {
            const caller new_caller{&DuckVtableGenerator::template owning_vtable<T>};

            if constexpr (fits_sbo<T> && std::is_nothrow_constructible_v<T, Args...>) {
                // Construction can't fail, so build directly in place.
                m_caller.destroy(m_storage, m_alloc);
                create<T>(m_alloc, m_storage, std::forward<Args>(args)...);
                m_caller = new_caller;
            } else {
                // Construct the new object before destroying the old one, so
                // a throwing constructor leaves *this unchanged.
                storage_type tmp;
                create<T>(m_alloc, tmp, std::forward<Args>(args)...);
                adopt(tmp, new_caller);
            }
        }

        constexpr erased_object(const erased_object& other) requires (DuckVtableGenerator::can_copy)
            : erased_object(traits::select_on_container_copy_construction(other.m_alloc), other)
        { }

        constexpr erased_object(const allocator_type& alloc, const erased_object& other)
            requires (DuckVtableGenerator::can_copy)
            : m_caller(other.m_caller)
            , m_alloc(alloc) {
            m_caller.copy(other.get(), m_storage, m_alloc);
        }

        template <typename OtherVtableGen>
        constexpr erased_object(const erased_object<OtherVtableGen>& other, const auto* vtable)
            requires (DuckVtableGenerator::can_copy)
            : erased_object(other, vtable, traits::select_on_container_copy_construction(other.m_alloc))
        { }

        template <typename OtherVtableGen>
        constexpr erased_object(const erased_object<OtherVtableGen>& other, const auto* vtable, const allocator_type& alloc)
            requires (DuckVtableGenerator::can_copy)
            : m_caller(vtable)
            , m_alloc(alloc) {
            m_caller.copy(other.get(), m_storage, m_alloc);
        }

        constexpr erased_object(erased_object&& other) noexcept(always_equal)
            : erased_object(other.m_alloc, std::move(other))
        { }

        constexpr erased_object(const allocator_type& alloc, erased_object&& other)
            noexcept(always_equal)
            : m_caller(other.m_caller)
            , m_alloc(alloc) {
            take_from(other);
        }

        template <typename OtherVtableGen>
        constexpr erased_object(erased_object<OtherVtableGen>&& other, const auto* vtable)
            noexcept(can_take_over<OtherVtableGen>)
            : erased_object(std::move(other), vtable, other.m_alloc)
        { }

        template <typename OtherVtableGen>
        constexpr erased_object(erased_object<OtherVtableGen>&& other, const auto* vtable, const allocator_type& alloc)
            noexcept(always_equal && can_take_over<OtherVtableGen>)
            : m_caller(vtable)
            , m_alloc(alloc) {
            take_from(other);
        }

        constexpr erased_object& operator=(const erased_object& other) requires (DuckVtableGenerator::can_copy) {
            if (this == &other) {
                return *this;
            }

            // Copy into a temporary first: if the copy throws, *this is left
            // untouched.
            const allocator_type& copy_alloc = pocca ? other.m_alloc : m_alloc;
            storage_type tmp;
            other.m_caller.copy(other.get(), tmp, copy_alloc);
            adopt(tmp, other.m_caller);
            if constexpr (pocca) {
                m_alloc = other.m_alloc;
            }

            return *this;
        }

        constexpr erased_object& operator=(erased_object&& other) noexcept(always_equal || pocma) {
            if (this == &other) {
                return *this;
            }

            if constexpr (always_equal || pocma) {
                adopt(other.m_storage, other.m_caller);
                if constexpr (pocma) {
                    m_alloc = std::move(other.m_alloc);
                }
            } else if (m_alloc == other.m_alloc) {
                adopt(other.m_storage, other.m_caller);
            } else {
                // Move into a temporary first, so a throwing move leaves *this
                // untouched.
                storage_type tmp;
                other.m_caller.move(other.get(), tmp, m_alloc);
                other.m_caller.destroy(other.m_storage, other.m_alloc);
                adopt(tmp, other.m_caller);
            }

            other.m_caller.reset();

            return *this;
        }

        // Every object is on the heap in constant evaluation, where the buffer
        // can't hold one.
        constexpr storage_kind get_kind() const noexcept {
            if consteval {
                if (m_caller.get_kind() == storage_kind::buffered) {
                    return storage_kind::heap;
                }
            }
            return m_caller.get_kind();
        }

        constexpr void swap(erased_object& other) noexcept(always_equal || pocs) {
            if (this == &other) {
                return;
            }

            if constexpr (!pocs && !always_equal) {
                // Swapping heap objects between unequal, non-propagating
                // allocators is undefined behavior.
                [[maybe_unused]] const bool neither_on_heap = get_kind() != storage_kind::heap
                    && other.get_kind() != storage_kind::heap;
                [[maybe_unused]] const bool equal_allocs = m_alloc == other.m_alloc;
                assert((neither_on_heap || equal_allocs) &&
                       "erased_object::swap: allocators must compare equal unless "
                       "propagate_on_container_swap is true");
            }

            storage_type tmp;
            m_caller.relocate(m_storage, tmp);
            other.m_caller.relocate(other.m_storage, m_storage);
            m_caller.relocate(tmp, other.m_storage);

            using std::swap;
            swap(m_caller, other.m_caller);
            if constexpr (pocs) {
                swap(m_alloc, other.m_alloc);
            }
        }

        constexpr ~erased_object() {
            m_caller.destroy(m_storage, m_alloc);
        }

        constexpr void* get() noexcept {
            return address_in(m_storage, get_kind());
        }

        constexpr const void* get() const noexcept {
            return address_in(m_storage, get_kind());
        }

        constexpr bool has_value() const noexcept {
            return m_caller.get_kind() != storage_kind::none;
        }

        template <typename T>
        constexpr bool has_type() const noexcept {
            return m_caller.template has_type<T>();
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
        // The object's address. A buffered object's has to be laundered once
        // it's cast to the object's type.
        template <typename Storage>
        constexpr static auto* address_in(Storage& storage, storage_kind kind) noexcept {
            using address_t = std::conditional_t<std::is_const_v<Storage>, const void*, void*>;
            if (kind == storage_kind::buffered) {
                return static_cast<address_t>(storage.buffer.data());
            }
            return static_cast<address_t>(storage.ptr);
        }

        template <typename T, typename... Args>
        constexpr static void create(const allocator_type& alloc, storage_type& dest, Args&&... args) {
            if !consteval {
                if constexpr (fits_sbo<T>) {
                    std::construct_at(reinterpret_cast<T*>(dest.start_buffer()),
                        std::forward<Args>(args)...);
                    return;
                }
            }

            typename traits::template rebind_alloc<T> rebound{alloc};
            dest.ptr = heap_construct<T>(rebound, std::forward<Args>(args)...);
        }

        // Takes other's object, which m_caller describes, leaving other empty.
        template <typename OtherVtableGen>
        constexpr void take_from(erased_object<OtherVtableGen>& other) {
            if constexpr (can_take_over<OtherVtableGen>) {
                if constexpr (always_equal) {
                    m_caller.relocate(other.m_storage, m_storage);
                } else if (m_alloc == other.m_alloc) {
                    m_caller.relocate(other.m_storage, m_storage);
                } else {
                    m_caller.move(other.get(), m_storage, m_alloc);
                    other.m_caller.destroy(other.m_storage, other.m_alloc);
                }
            } else {
                m_caller.move(other.get(), m_storage, m_alloc);
                other.m_caller.destroy(other.m_storage, other.m_alloc);
            }

            other.m_caller.reset();
        }

        // Replaces the current object with one that has already been built in
        // another storage of this duck, leaving that storage empty.
        constexpr void adopt(storage_type& built, caller new_caller) noexcept {
            m_caller.destroy(m_storage, m_alloc);
            new_caller.relocate(built, m_storage);
            m_caller = new_caller;
        }
    private:
        storage_type m_storage{};

        caller m_caller;

        [[no_unique_address]] allocator_type m_alloc;
    };

    template <typename... Traits>
    template <typename T>
    consteval void vtable_generator<Traits...>::
        set_storage_functions(owning_table& static_vtable) {

        using object_t = erased_object<vtable_generator>;
        using allocator = object_t::allocator_type;
        using rebound_t = object_t::traits::template rebind_alloc<T>;

        constexpr static bool fits_sbo = object_t::template fits_sbo<T>;
        static_vtable.kind = fits_sbo ? storage_kind::buffered : storage_kind::heap;

        static_vtable.destroy = [](storage_type& storage, const allocator& alloc) noexcept {
            if !consteval {
                if constexpr (fits_sbo) {
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        std::destroy_at(std::launder(reinterpret_cast<T*>(storage.buffer.data())));
                    }
                    storage.ptr = nullptr;
                    return;
                }
            }

            rebound_t rebound{alloc};
            heap_destroy(rebound, static_cast<T*>(storage.ptr));
            storage.ptr = nullptr;
        };

        static_vtable.relocate = [](storage_type& src, storage_type& dest) noexcept {
            if !consteval {
                if constexpr (fits_sbo) {
                    T* obj = std::launder(reinterpret_cast<T*>(src.buffer.data()));
                    std::construct_at(reinterpret_cast<T*>(dest.start_buffer()), std::move(*obj));
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        std::destroy_at(obj);
                    }
                    src.ptr = nullptr;
                    return;
                }
            }

            dest.ptr = src.ptr;
            src.ptr = nullptr;
        };

        static_vtable.move = [](void* src, storage_type& dest, const allocator& alloc) noexcept(fits_sbo) {
            object_t::template create<T>(alloc, dest, std::move(*std::launder(static_cast<T*>(src))));
        };

        if constexpr (can_copy) {
            static_vtable.copy = [](const void* src, storage_type& dest, const allocator& alloc) {
                object_t::template create<T>(alloc, dest, *std::launder(static_cast<const T*>(src)));
            };
        }
    }

}

#endif
