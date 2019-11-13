#ifndef XAOS_FUNCTION_HPP
#define XAOS_FUNCTION_HPP


#include <xaos/detail/function.hpp>

#include <memory>


namespace xaos {


struct function_traits {
  static constexpr bool lvalue_ref_call = true;
};


struct const_function_traits {
  static constexpr bool const_lvalue_ref_call = true;
};


template <class Signature, class Traits>
using basic_function = boost::mp11::mp_apply_q<
  boost::mp11::mp_bind_front<detail::basic_function, Signature, Traits>,
  detail::enabled_signature_overloads<Signature, Traits>>;


template <class Signature>
using function = basic_function<Signature, function_traits>;

template <class Signature>
using const_function = basic_function<Signature, const_function_traits>;


} // namespace xaos


#endif // XAOS_FUNCTION_HPP
