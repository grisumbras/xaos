#ifndef XAOS_DETAIL_BACKEND_ALLOC_HPP
#define XAOS_DETAIL_BACKEND_ALLOC_HPP


#include <boost/core/pointer_traits.hpp>


namespace xaos {
namespace detail {


struct alloc_interface {
  virtual auto relocate(void* alloc) -> void* = 0;
  virtual void delete_this(void* alloc) = 0;

protected:
  ~alloc_interface() = default;
};


struct clone_interface {
  virtual auto clone(void* alloc) const -> void* = 0;

protected:
  ~clone_interface() = default;
};


template <class Allocator, class T>
auto restore_allocator(void* type_erased_alloc) {
  using proto_traits = std::allocator_traits<Allocator>;
  static_assert(std::is_void<typename proto_traits::value_type>::value);
  auto& proto_alloc = *reinterpret_cast<Allocator*>(type_erased_alloc);

  using alloc_traits = typename proto_traits::template rebind_traits<T>;
  return typename alloc_traits::allocator_type(proto_alloc);
}


template <class Allocator, class... Args>
auto new_backend(Allocator& alloc, Args&&... args) {
  using traits = std::allocator_traits<Allocator>;
  auto fancy_ptr = traits::allocate(alloc, 1);
  auto const raw_ptr = boost::to_address(fancy_ptr);
  try {
    traits::construct(
      alloc, raw_ptr, std::move(fancy_ptr), static_cast<Args&&>(args)...);
  } catch (...) {
    traits::deallocate(alloc, fancy_ptr, 1);
    throw;
  }
  return raw_ptr;
}


template <class Derived, class Base>
struct clone_implementation : Base {
  auto clone(void* type_erased_alloc) const -> void* override {
    auto alloc = restore_allocator<typename Derived::allocator_type, Derived>(
      type_erased_alloc);
    auto& self = static_cast<Derived const&>(*this);
    auto const raw_ptr = new_backend(alloc, self.callable());
    return static_cast<typename Derived::interface_type*>(raw_ptr);
  }

protected:
  ~clone_implementation() = default;
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_BACKEND_ALLOC_HPP
