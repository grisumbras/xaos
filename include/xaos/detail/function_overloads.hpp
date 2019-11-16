#ifndef XAOS_DETAIL_FUNCTION_OVERLOADS_HPP
#define XAOS_DETAIL_FUNCTION_OVERLOADS_HPP


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/type_traits/copy_cv_ref.hpp>

#include <functional>


namespace xaos {
namespace detail {


template <class Signature, class RefKind>
struct signature_overload_impl;

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&> {
  using type = R(Args...) &;
};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&> {
  using type = R(Args...) const&;
};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&&> {
  using type = R(Args...) &&;
};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&&> {
  using type = R(Args...) const&&;
};

template <class Signature, class RefKind>
using signature_overload =
  typename signature_overload_impl<Signature, RefKind>::type;

template <class Signature>
using signature_overloads = boost::mp11::mp_transform_q<
  boost::mp11::mp_bind_front<signature_overload, Signature>,
  boost::mp11::mp_list<int&, int const&, int&&, int const&&>>;


template <class RefKind>
struct trait_for_ref_kind_impl;

template <class T>
struct trait_for_ref_kind_impl<T&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::lvalue_ref_call>;
};

template <class T>
struct trait_for_ref_kind_impl<T const&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::const_lvalue_ref_call>;
};

template <class T>
struct trait_for_ref_kind_impl<T&&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::rvalue_ref_call>;
};

template <class T>
struct trait_for_ref_kind_impl<T const&&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::const_rvalue_ref_call>;
};

template <class Traits, class RefKind>
using trait_for_ref_kind = boost::mp11::mp_eval_or_q<
  boost::mp11::mp_false,
  trait_for_ref_kind_impl<RefKind>,
  Traits>;


template <class Traits>
using enabled_ref_kinds = boost::mp11::mp_filter_q<
  boost::mp11::mp_bind_front<trait_for_ref_kind, Traits>,
  boost::mp11::mp_list<int&, int const&, int&&, int const&&>>;

template <class Signature, class Traits>
using enabled_overloads = boost::mp11::mp_transform_q<
  boost::mp11::mp_bind_front<signature_overload, Signature>,
  enabled_ref_kinds<Traits>>;


template <class Traits>
using are_rvalue_overloads_enabled = boost::mp11::mp_or<
  trait_for_ref_kind<Traits, int&&>,
  trait_for_ref_kind<Traits, int const&&>>;


template <class Overload>
struct call_overload_interface;

template <class R, class... Args>
struct call_overload_interface<R(Args...)&> {
  virtual auto call_l(Args... args) -> R = 0;

protected:
  ~call_overload_interface() = default;
};

template <class R, class... Args>
struct call_overload_interface<R(Args...) const&> {
  virtual auto call_cl(Args... args) const -> R = 0;

protected:
  ~call_overload_interface() = default;
};

template <class R, class... Args>
struct call_overload_interface<R(Args...) &&> {
  virtual auto call_r(Args... args) -> R = 0;

protected:
  ~call_overload_interface() = default;
};

template <class R, class... Args>
struct call_overload_interface<R(Args...) const&&> {
  virtual auto call_cr(Args... args) const -> R = 0;

protected:
  ~call_overload_interface() = default;
};


template <class R, class T, class... Args>
auto forward_to_callable(T&& t, Args... args) -> R {
  using callable_type = typename std::remove_reference_t<T>::callable_type;
  using callable_ref = boost::copy_cv_ref_t<callable_type, T&&>;
  return std::invoke(static_cast<callable_ref>(t.callable()), args...);
}


template <class Derived, class Base, class Signature>
struct call_overload;

template <class Derived, class Base, class R, class... Args>
struct call_overload<Derived, Base, R(Args...)&> : Base {
  auto call_l(Args... args) -> R override {
    return forward_to_callable<R>(static_cast<Derived&>(*this), args...);
  }

protected:
  ~call_overload() = default;
};

template <class Derived, class Base, class R, class... Args>
struct call_overload<Derived, Base, R(Args...) const&> : Base {
  auto call_cl(Args... args) const -> R override {
    return forward_to_callable<R>(static_cast<Derived const&>(*this), args...);
  }

protected:
  ~call_overload() = default;
};

template <class Derived, class Base, class R, class... Args>
struct call_overload<Derived, Base, R(Args...) &&> : Base {
  auto call_r(Args... args) -> R override {
    return forward_to_callable<R>(static_cast<Derived&&>(*this), args...);
  }

protected:
  ~call_overload() = default;
};

template <class Derived, class Base, class R, class... Args>
struct call_overload<Derived, Base, R(Args...) const&&> : Base {
  auto call_cr(Args... args) const -> R override {
    return forward_to_callable<R>(
      static_cast<Derived const&&>(*this), args...);
  }

protected:
  ~call_overload() = default;
};


template <class Derived, bool HasRvalueOverloads, class Overload>
struct parens_overload;

template <class Derived, class R, class... Args>
struct parens_overload<Derived, true, R(Args...)&> {
  auto operator()(Args... args) & -> R {
    auto& self = static_cast<Derived&>(*this);
    return self.backend_->call_l(args...);
  }

protected:
  ~parens_overload() = default;
};

template <class Derived, class R, class... Args>
struct parens_overload<Derived, false, R(Args...)&> {
  auto operator()(Args... args) -> R {
    auto& self = static_cast<Derived&>(*this);
    return self.backend_->call_l(args...);
  }

protected:
  ~parens_overload() = default;
};

template <class Derived, class R, class... Args>
struct parens_overload<Derived, true, R(Args...) const&> {
  auto operator()(Args... args) const& -> R {
    auto& self = static_cast<Derived const&>(*this);
    return self.backend_->call_cl(args...);
  }

protected:
  ~parens_overload() = default;
};

template <class Derived, class R, class... Args>
struct parens_overload<Derived, false, R(Args...) const&> {
  auto operator()(Args... args) const -> R {
    auto& self = static_cast<Derived const&>(*this);
    return self.backend_->call_cl(args...);
  }

protected:
  ~parens_overload() = default;
};

template <class Derived, bool HasRvalueOverloads, class R, class... Args>
struct parens_overload<Derived, HasRvalueOverloads, R(Args...) &&> {
  auto operator()(Args... args) && -> R {
    auto& self = static_cast<Derived&>(*this);
    return self.backend_->call_r(args...);
  }

protected:
  ~parens_overload() = default;
};

template <class Derived, bool HasRvalueOverloads, class R, class... Args>
struct parens_overload<Derived, HasRvalueOverloads, R(Args...) const&&> {
  auto operator()(Args... args) const&& -> R {
    auto& self = static_cast<Derived const&>(*this);
    return self.backend_->call_cr(args...);
  }

protected:
  ~parens_overload() = default;
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_OVERLOADS_HPP
