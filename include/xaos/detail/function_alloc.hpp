#ifndef XAOS_DETAIL_FUNCTION_ALLOC_HPP
#define XAOS_DETAIL_FUNCTION_ALLOC_HPP


#include <xaos/detail/backend_alloc.hpp>

#include <boost/core/empty_value.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/utility.hpp>


namespace xaos {
namespace detail {


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
  typename std::allocator_traits<Allocator>::template rebind_traits<
    T>::pointer,
  Index>;


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_ALLOC_HPP
