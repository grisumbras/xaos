#ifndef XAOS_DETAIL_FUNCTION_ALLOC_HPP
#define XAOS_DETAIL_FUNCTION_ALLOC_HPP


#include <boost/core/empty_value.hpp>
#include <boost/core/pointer_traits.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>

#include <memory>


namespace xaos {
namespace detail {


struct delete_interface {
  virtual void delete_this(void* alloc) = 0;

protected:
  ~delete_interface() = default;
};


struct clone_interface {
  virtual auto clone(void* alloc) const -> void* = 0;

protected:
  ~clone_interface() = default;
};


template <class Traits>
using copyability_enabled_helper = boost::mp11::mp_bool<Traits::is_copyable>;

template <class Traits>
using is_copyability_enabled = boost::mp11::
  mp_eval_or<boost::mp11::mp_false, copyability_enabled_helper, Traits>;


template <class Traits>
using maybe_clone_interface = boost::mp11::mp_if<
  is_copyability_enabled<Traits>,
  boost::mp11::mp_list<clone_interface>,
  boost::mp11::mp_list<>>;


template <class Allocator, class T>
auto get_allocator(void* type_erased_alloc) {
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
    auto alloc = get_allocator<typename Derived::allocator_type, Derived>(
      type_erased_alloc);
    auto& self = static_cast<Derived const&>(*this);
    auto const raw_ptr = new_backend(alloc, self.callable());
    return static_cast<typename Derived::interface_type*>(raw_ptr);
  }

protected:
  ~clone_implementation() = default;
};


template <class Traits, class Derived, class Base>
using maybe_clone_implementation = boost::mp11::mp_eval_if_not<
  is_copyability_enabled<Traits>,
  Base,
  clone_implementation,
  Derived,
  Base>;


template <class Traits>
using pointer_to_result = decltype(
  Traits::pointer::pointer_to(std::declval<typename Traits::element_type&>()));

template <class Pointer>
using has_pointer_to = boost::mp11::mp_or<
  boost::mp11::mp_valid<pointer_to_result, std::pointer_traits<Pointer>>,
  std::is_pointer<Pointer>>;


template <class Pointer, unsigned Index, class = void>
struct pointer_storage : boost::empty_value<Pointer, Index> {
  using pointer = Pointer;
  using traits = typename std::pointer_traits<Pointer>;
  pointer_storage(pointer ptr)
    : boost::empty_value<Pointer, Index>(
      boost::empty_init_t(), std::move(ptr)) {}

  auto pointer_to(typename traits::element_type&) -> Pointer {
    return boost::empty_value<Pointer, Index>::get();
  }
};

template <class Pointer, unsigned Index>
struct pointer_storage<
  Pointer,
  Index,
  std::enable_if_t<has_pointer_to<Pointer>::value>> {
  using pointer = Pointer;
  using traits = typename std::pointer_traits<Pointer>;

  pointer_storage(pointer) noexcept {}

  auto pointer_to(typename traits::element_type& obj) -> Pointer {
    return traits::pointer_to(obj);
  }
};

template <class T, class Allocator, unsigned Index>
using pointer_storage_helper = pointer_storage<
  typename std::allocator_traits<Allocator>::template rebind_alloc<T>::pointer,
  Index>;


template <class Allocator>
struct backend_deleter : boost::empty_value<Allocator> {
  using allocator_type = Allocator;

  backend_deleter(Allocator alloc)
    : boost::empty_value<Allocator>(boost::empty_init_t(), std::move(alloc)) {}

  auto get_allocator() const -> allocator_type const& {
    return boost::empty_value<Allocator>::get();
  }

  template <class T>
  void operator()(T* ptr) {
    auto alloc = get_allocator();
    ptr->delete_this(std::addressof(alloc));
  }
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_ALLOC_HPP
