#ifndef XAOS_DETAIL_FUNCTION_HPP
#define XAOS_DETAIL_FUNCTION_HPP


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/type_traits/copy_cv_ref.hpp>

#include <functional>
#include <memory>


namespace xaos {
namespace detail {


template <class Signature, class RefKind>
struct signature_overload_impl;

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&>
  : boost::mp11::mp_identity<R(Args...)&> {};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&>
  : boost::mp11::mp_identity<R(Args...) const&> {};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&&>
  : boost::mp11::mp_identity<R(Args...) &&> {};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&&>
  : boost::mp11::mp_identity<R(Args...) const&&> {};

template <class Signature, class RefKind>
using signature_overload =
  typename signature_overload_impl<Signature, RefKind>::type;

template <class Signature>
using signature_overloads = boost::mp11::mp_transform_q<
  boost::mp11::mp_bind_front<signature_overload, Signature>,
  boost::mp11::mp_list<int&, int const&, int&&, int const&&>>;


template <class Overload>
struct quoted_trait_for_overload;

template <class R, class... Args>
struct quoted_trait_for_overload<R(Args...)&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::lvalue_ref_call>;
};

template <class R, class... Args>
struct quoted_trait_for_overload<R(Args...) const&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::const_lvalue_ref_call>;
};

template <class R, class... Args>
struct quoted_trait_for_overload<R(Args...) &&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::rvalue_ref_call>;
};

template <class R, class... Args>
struct quoted_trait_for_overload<R(Args...) const&&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::const_rvalue_ref_call>;
};


template <class Traits, class Overload>
using trait_for_overload = boost::mp11::mp_eval_or_q<
  boost::mp11::mp_false,
  quoted_trait_for_overload<Overload>,
  Traits>;


template <class Signature, class Traits>
using enabled_signature_overloads = boost::mp11::mp_filter_q<
  boost::mp11::mp_bind_front<trait_for_overload, Traits>,
  signature_overloads<Signature>>;


template <class Traits>
using copyability_enabled_helper = boost::mp11::mp_bool<Traits::is_copyable>;

template <class Traits>
using is_copyability_enabled = boost::mp11::
  mp_eval_or<boost::mp11::mp_false, copyability_enabled_helper, Traits>;


struct function_backend_part_common {
protected:
  ~function_backend_part_common() = default;
};


template <class Signature>
struct function_backend_base_part;

template <class R, class... Args>
struct function_backend_base_part<R(Args...)&> : function_backend_part_common {
  virtual auto call(Args... args) & -> R = 0;
};

template <class R, class... Args>
struct function_backend_base_part<R(Args...) const&>
  : function_backend_part_common {
  virtual auto call(Args... args) const& -> R = 0;
};

template <class R, class... Args>
struct function_backend_base_part<R(Args...) &&>
  : function_backend_part_common {
  virtual auto call(Args... args) && -> R = 0;
};

template <class R, class... Args>
struct function_backend_base_part<R(Args...) const&&>
  : function_backend_part_common {
  virtual auto call(Args... args) const&& -> R = 0;
};


template <class Signature, class Traits, class... Overloads>
struct function_backend_base_helper
  : function_backend_base_part<Overloads>... {
  virtual ~function_backend_base_helper() = default;
  using function_backend_base_part<Overloads>::call...;
};


template <class Result>
struct clone_base {
  virtual auto clone() const -> std::unique_ptr<Result> = 0;
};

template <class Result, class Traits>
using clone_support_base = boost::mp11::mp_eval_if_c<
  !is_copyability_enabled<Traits>::value,
  boost::mp11::mp_identity<void>,
  clone_base,
  Result>;


template <class Signature, class Traits>
struct function_backend_base
  : boost::mp11::mp_apply_q<
      boost::mp11::
        mp_bind_front<function_backend_base_helper, Signature, Traits>,
      enabled_signature_overloads<Signature, Traits>>
  , clone_support_base<function_backend_base<Signature, Traits>, Traits> {};


template <class R, class T, class... Args>
auto forward_to_callable(T&& t, Args... args) -> R {
  using callable_t = typename std::remove_reference_t<T>::callable_t;
  using callable_ref = boost::copy_cv_ref_t<callable_t, T&&>;
  return std::invoke(static_cast<callable_ref>(t.callable), args...);
}


template <class Derived, class Base, class Signature>
struct function_backend_part;

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...)&> : Base {
  auto call(Args... args) & -> R override {
    return forward_to_callable<R>(static_cast<Derived&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&> : Base {
  auto call(Args... args) const& -> R override {
    return forward_to_callable<R>(static_cast<Derived const&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) &&> : Base {
  auto call(Args... args) && -> R override {
    return forward_to_callable<R>(static_cast<Derived&&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&&> : Base {
  auto call(Args... args) const&& -> R override {
    return forward_to_callable<R>(
      static_cast<Derived const&&>(*this), args...);
  }
};


template <class Derived, class Base>
struct function_clone_impl : Base {
  auto clone() const -> std::unique_ptr<Base> override {
    auto& self = static_cast<Derived const&>(*this);
    return std::make_unique<Derived>(self.callable);
  }
};


template <class Traits, class Derived, class Base>
using clone_support = boost::mp11::mp_eval_if_c<
  !is_copyability_enabled<Traits>::value,
  Base,
  function_clone_impl,
  Derived,
  Base>;


template <class Signature, class Traits, class Callable>
struct function_backend
  : boost::mp11::mp_fold_q<
      enabled_signature_overloads<Signature, Traits>,
      clone_support<
        Traits,
        function_backend<Signature, Traits, Callable>,
        function_backend_base<Signature, Traits>>,
      boost::mp11::mp_bind_front<
        function_backend_part,
        function_backend<Signature, Traits, Callable>>> {
  using callable_t = Callable;

  function_backend(Callable callable) : callable(std::move(callable)) {}

  using function_backend_base<Signature, Traits>::call;

  Callable callable;
};

template <class Signature, class Traits, class Callable>
auto make_backend(Callable callable)
  -> std::unique_ptr<function_backend_base<Signature, Traits>> {
  using Backend = function_backend<Signature, Traits, Callable>;
  return std::make_unique<Backend>(std::move(callable));
}


template <class T, class Backend, class... Args>
auto forward_to_backend(Backend& backend, Args... args) {
  return static_cast<T>(backend).call(args...);
}


template <class Derived, bool HasRvalueOverloads, class Overload>
struct function_frontend_part;

template <class Derived, class R, class... Args>
struct function_frontend_part<Derived, true, R(Args...)&> {
  auto operator()(Args... args) & -> R {
    auto& self = static_cast<Derived&>(*this);
    return forward_to_backend<typename Derived::backend_type&>(
      *self.backend_, args...);
  }
};

template <class Derived, class R, class... Args>
struct function_frontend_part<Derived, false, R(Args...)&> {
  auto operator()(Args... args) -> R {
    auto& self = static_cast<Derived&>(*this);
    return forward_to_backend<typename Derived::backend_type&>(
      *self.backend_, args...);
  }
};

template <class Derived, class R, class... Args>
struct function_frontend_part<Derived, true, R(Args...) const&> {
  auto operator()(Args... args) const& -> R {
    auto& self = static_cast<Derived const&>(*this);
    return forward_to_backend<typename Derived::backend_type const&>(
      *self.backend_, args...);
  }
};

template <class Derived, class R, class... Args>
struct function_frontend_part<Derived, false, R(Args...) const&> {
  auto operator()(Args... args) const -> R {
    auto& self = static_cast<Derived const&>(*this);
    return forward_to_backend<typename Derived::backend_type const&>(
      *self.backend_, args...);
  }
};

template <class Derived, bool HasRvalueOverloads, class R, class... Args>
struct function_frontend_part<Derived, HasRvalueOverloads, R(Args...) &&> {
  auto operator()(Args... args) && -> R {
    auto& self = static_cast<Derived&>(*this);
    return forward_to_backend<typename Derived::backend_type&&>(
      *self.backend_, args...);
  }
};

template <class Derived, bool HasRvalueOverloads, class R, class... Args>
struct function_frontend_part<
  Derived,
  HasRvalueOverloads,
  R(Args...) const&&> {
  auto operator()(Args... args) const&& -> R {
    auto& self = static_cast<Derived const&>(*this);
    return forward_to_backend<typename Derived::backend_type const&&>(
      *self.backend_, args...);
  }
};


template <class Traits>
using are_rvalue_overloads_enabled = boost::mp11::mp_or<
  trait_for_overload<Traits, void() &&>,
  trait_for_overload<Traits, void() const&&>>;


template <class Signature, class Traits>
struct noncopyable_backend_holder {
  using backend_type = detail::function_backend_base<Signature, Traits>;

  noncopyable_backend_holder() noexcept {}

  template <class Callable>
  noncopyable_backend_holder(Callable callable)
    : backend_(detail::make_backend<Signature, Traits>(std::move(callable))) {}

  std::unique_ptr<backend_type> backend_;
};


template <class Signature, class Traits>
struct copyable_backend_holder
  : noncopyable_backend_holder<Signature, Traits> {
  using copyable_backend_holder::noncopyable_backend_holder::
    noncopyable_backend_holder;

  copyable_backend_holder(copyable_backend_holder&&) noexcept = default;
  auto operator=(copyable_backend_holder&&) noexcept
    -> copyable_backend_holder& = default;

  copyable_backend_holder(copyable_backend_holder const& other) {
    this->backend_ = other.backend_->clone();
  }

  auto operator=(copyable_backend_holder const& other)
    -> copyable_backend_holder& {
    auto temp = other;
    return (*this) = std::move(temp);
  }
};


template <class Signature, class Traits>
using copyability_support = boost::mp11::mp_eval_if_c<
  !is_copyability_enabled<Traits>::value,
  noncopyable_backend_holder<Signature, Traits>,
  copyable_backend_holder,
  Signature,
  Traits>;

template <class Signature, class Traits, class... Overloads>
class basic_function
  : public function_frontend_part<
      basic_function<Signature, Traits, Overloads...>,
      are_rvalue_overloads_enabled<Traits>::value,
      Overloads>...
  , copyability_support<Signature, Traits>
{
public:
  using copyability_support<Signature, Traits>::copyability_support;

  using function_frontend_part<
    basic_function<Signature, Traits, Overloads...>,
    are_rvalue_overloads_enabled<Traits>::value,
    Overloads>::operator()...;

private:
  template <class, bool, class>
  friend struct detail::function_frontend_part;
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_HPP
