#ifndef XAOS_FUNCTION_HPP
#define XAOS_FUNCTION_HPP


#include <xaos/detail/function.hpp>

#include <memory>


namespace xaos {


struct function_traits {
  static constexpr bool is_copyable = true;
  static constexpr bool lvalue_ref_call = true;
};

struct const_function_traits {
  static constexpr bool is_copyable = true;
  static constexpr bool const_lvalue_ref_call = true;
};

struct rvalue_function_traits {
  static constexpr bool rvalue_ref_call = true;
};


template <
  class Signature,
  class Traits,
  class Allocator = std::allocator<void>>
using basic_function = boost::mp11::mp_apply_q<
  boost::mp11::mp_bind_front<
    detail::basic_function,
    Signature,
    Traits,
    typename std::allocator_traits<Allocator>::template rebind_alloc<void>>,
  detail::enabled_signature_overloads<Signature, Traits>>;


template <class Signature, class Allocator = std::allocator<void>>
using function = basic_function<Signature, function_traits, Allocator>;

template <class Signature, class Allocator = std::allocator<void>>
using const_function
  = basic_function<Signature, const_function_traits, Allocator>;

template <class Signature, class Allocator = std::allocator<void>>
using rfunction = basic_function<Signature, rvalue_function_traits, Allocator>;


} // namespace xaos


#endif // XAOS_FUNCTION_HPP
