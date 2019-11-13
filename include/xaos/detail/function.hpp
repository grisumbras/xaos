#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>

#include <memory>


namespace xaos {
namespace detail {


template <class Signature, class RefKind> struct signature_overload_impl;

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&>
  : boost::mp11::mp_identity<R(Args...)&>
{};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&>
  : boost::mp11::mp_identity<R(Args...) const&>
{};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T&&>
  : boost::mp11::mp_identity<R(Args...)&&>
{};

template <class R, class... Args, class T>
struct signature_overload_impl<R(Args...), T const&&>
  : boost::mp11::mp_identity<R(Args...) const&&>
{};

template <class Signature, class RefKind>
using signature_overload
  = typename signature_overload_impl<Signature, RefKind>::type;

template <class Signature>
using signature_overloads
  = boost::mp11::mp_transform_q<
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
struct quoted_trait_for_overload<R(Args...)&&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::rvalue_ref_call>;
};

template <class R, class... Args>
struct quoted_trait_for_overload<R(Args...) const&&> {
  template <class Traits>
  using fn = boost::mp11::mp_bool<Traits::const_rvalue_ref_call>;
};


template <class Traits, class Overload>
using trait_for_overload
  = boost::mp11::mp_eval_or_q<
      boost::mp11::mp_false,
      quoted_trait_for_overload<Overload>,
      Traits>;


template <class Signature, class Traits>
using enabled_signature_overloads
  = boost::mp11::mp_filter_q<
      boost::mp11::mp_bind_front<trait_for_overload, Traits>,
      signature_overloads<Signature>>;



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
  : function_backend_part_common
{
  virtual auto call(Args... args) const& -> R = 0;
};

template <class R, class... Args>
struct function_backend_base_part<R(Args...)&&>
  : function_backend_part_common
{
  virtual auto call(Args... args) && -> R = 0;
};

template <class R, class... Args>
struct function_backend_base_part<R(Args...) const&&>
  : function_backend_part_common
{
  virtual auto call(Args... args) const&& -> R = 0;
};


template <class Signature, class Traits, class... Overloads >
struct function_backend_base_helper : function_backend_base_part<Overloads>...
{
  virtual ~function_backend_base_helper() = default;
  using function_backend_base_part<Overloads>::call...;
};


template <class Signature, class Traits>
using function_backend_base
  = boost::mp11::mp_apply_q<
      boost::mp11::mp_bind_front<
        function_backend_base_helper, Signature, Traits>,
      enabled_signature_overloads<Signature, Traits>>;


template <class Derived, class Base, class Signature>
struct function_backend_part;

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...)&> : Base {
  auto call(Args... args) & -> R override {
    auto& self = static_cast<Derived&>(*this);
    return self.callable(args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&> : Base {
  auto call(Args... args) const& -> R override {
    auto& self = static_cast<Derived const&>(*this);
    return self.callable(args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...)&&> : Base {
  auto call(Args... args) && -> R override {
    auto& self = static_cast<Derived&>(*this);
    using callable_t = typename Derived::callable_t;
    return static_cast<callable_t&&>(self.callable)(args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&&> : Base {
  auto call(Args... args) const&& -> R override {
    auto& self = static_cast<Derived const&>(*this);
    using callable_t = typename Derived::callable_t;
    return static_cast<callable_t const&&>(self.callable)(args...);
  }
};


template <class Signature, class Traits, class Callable>
struct function_backend
  : boost::mp11::mp_fold_q<
      enabled_signature_overloads<Signature, Traits>,
      function_backend_base<Signature, Traits>,
      boost::mp11::mp_bind_front<
        function_backend_part,
        function_backend<Signature, Traits, Callable>>>
{
  using callable_t = Callable;

  function_backend(Callable callable) : callable(std::move(callable)) {}

  using function_backend_base<Signature, Traits>::call;

  Callable callable;
};

template <class Signature, class Traits, class Callable>
auto make_backend(Callable callable)
  -> std::unique_ptr<function_backend_base<Signature, Traits>>
{
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
struct function_frontend_part<Derived, HasRvalueOverloads, R(Args...)&&> {
  auto operator()(Args... args) && -> R {
    auto& self = static_cast<Derived&>(*this);
    return forward_to_backend<typename Derived::backend_type&&>(
      *self.backend_, args...);
  }
};

template <class Derived, bool HasRvalueOverloads, class R, class... Args>
struct function_frontend_part<Derived, HasRvalueOverloads, R(Args...) const&&>
{
  auto operator()(Args... args) const&& -> R {
    auto& self = static_cast<Derived const&>(*this);
    return forward_to_backend<typename Derived::backend_type const&&>(
      *self.backend_, args...);
  }
};


template <class Traits>
using are_rvalue_overloads_enabled
  = boost::mp11::mp_or<
      trait_for_overload<Traits, void()&&>,
      trait_for_overload<Traits, void() const&&>>;


template <class Signature, class Traits, class... Overloads>
class basic_function
  : public function_frontend_part<
      basic_function<Signature, Traits, Overloads...>,
      are_rvalue_overloads_enabled<Traits>::value,
      Overloads>...
{
public:
  using function_frontend_part<
    basic_function<Signature, Traits, Overloads...>,
    are_rvalue_overloads_enabled<Traits>::value,
    Overloads>::operator()...;

  template <class Callable>
  basic_function(Callable callable);

private:
  template <class, bool, class> friend struct detail::function_frontend_part;

  using backend_type = detail::function_backend_base<Signature, Traits>;

  std::unique_ptr<backend_type> backend_;
};

template <class Signature, class Traits, class... Overloads>
template <class Callable>
basic_function<Signature, Traits, Overloads...>
::basic_function(Callable callable)
  : backend_(detail::make_backend<Signature, Traits>(std::move(callable)))
{}


} // namespace detail
} // namespace xaos
