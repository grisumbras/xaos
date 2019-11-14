#ifndef XAOS_DETAIL_FUNCTION_HPP
#define XAOS_DETAIL_FUNCTION_HPP


#include <boost/core/empty_value.hpp>
#include <boost/core/pointer_traits.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/type_traits/copy_cv_ref.hpp>

#include <functional>
#include <memory>
#include <type_traits>


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
  using function_backend_base_part<Overloads>::call...;

protected:
  ~function_backend_base_helper() = default;
};


struct clone_base {
  virtual auto clone(void* alloc) const -> void* = 0;

protected:
  ~clone_base() = default;
};

template <class Result, class Traits>
using clone_support_base = std::conditional_t<
  is_copyability_enabled<Traits>::value,
  clone_base,
  boost::mp11::mp_identity<void>>;


template <class Signature, class Traits>
struct function_backend_base
  : boost::mp11::mp_apply_q<
      boost::mp11::
        mp_bind_front<function_backend_base_helper, Signature, Traits>,
      enabled_signature_overloads<Signature, Traits>>
  , clone_support_base<function_backend_base<Signature, Traits>, Traits> {
  virtual void delete_this(void* alloc) = 0;

protected:
  ~function_backend_base() = default;
};


template <class R, class T, class... Args>
auto forward_to_callable(T&& t, Args... args) -> R {
  using callable_t = typename std::remove_reference_t<T>::callable_t;
  using callable_ref = boost::copy_cv_ref_t<callable_t, T&&>;
  return std::invoke(static_cast<callable_ref>(t.callable()), args...);
}


template <class Derived, class Base, class Signature>
struct function_backend_part;

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...)&> : Base {
  using Base::call;

  auto call(Args... args) & -> R override {
    return forward_to_callable<R>(static_cast<Derived&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&> : Base {
  using Base::call;

  auto call(Args... args) const& -> R override {
    return forward_to_callable<R>(static_cast<Derived const&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) &&> : Base {
  using Base::call;

  auto call(Args... args) && -> R override {
    return forward_to_callable<R>(static_cast<Derived&&>(*this), args...);
  }
};

template <class Derived, class Base, class R, class... Args>
struct function_backend_part<Derived, Base, R(Args...) const&&> : Base {
  using Base::call;

  auto call(Args... args) const&& -> R override {
    return forward_to_callable<R>(
      static_cast<Derived const&&>(*this), args...);
  }
};


template <class Derived, class Base, class Allocator>
struct function_clone_impl : Base {
  auto clone(void* type_erased_alloc) const -> void* override {
    using proto_traits = std::allocator_traits<Allocator>;
    static_assert(std::is_void<typename proto_traits::value_type>::value);

    auto& proto_alloc = *reinterpret_cast<Allocator*>(type_erased_alloc);
    using alloc_traits =
      typename proto_traits::template rebind_traits<Derived>;
    auto alloc = typename alloc_traits::allocator_type(proto_alloc);

    auto fancy_ptr = alloc_traits::allocate(alloc, 1);
    auto const raw_ptr = boost::to_address(fancy_ptr);
    try {
      auto& self = static_cast<Derived const&>(*this);
      alloc_traits::construct(
        alloc, raw_ptr, self.callable(), std::move(fancy_ptr));
    } catch (...) { alloc_traits::deallocate(alloc, fancy_ptr, 1); }
    return static_cast<typename Derived::base_t*>(raw_ptr);
  }
};


template <class Traits, class Allocator, class Derived, class Base>
using clone_support = boost::mp11::mp_eval_if_c<
  !is_copyability_enabled<Traits>::value,
  Base,
  function_clone_impl,
  Derived,
  Base,
  Allocator>;


template <class Traits>
using pointer_to_result = decltype(
  Traits::pointer::pointer_to(std::declval<typename Traits::element_type&>()));

template <class Pointer>
using has_pointer_to = boost::mp11::mp_or<
  boost::mp11::mp_valid<pointer_to_result, std::pointer_traits<Pointer>>,
  std::is_pointer<Pointer>>;


template <class Pointer, class = void>
struct pointer_holder_impl : boost::empty_value<Pointer, 1> {
  using pointer = Pointer;
  using traits = typename std::pointer_traits<Pointer>;
  pointer_holder_impl(pointer ptr)
    : boost::empty_value<Pointer, 1>(boost::empty_init_t(), std::move(ptr)) {}

  auto pointer_to(typename traits::element_type&) -> Pointer {
    return boost::empty_value<Pointer, 1>::get();
  }
};

template <class Pointer>
struct pointer_holder_impl<
  Pointer,
  std::enable_if_t<has_pointer_to<Pointer>::value>> {
  using pointer = Pointer;
  using traits = typename std::pointer_traits<Pointer>;

  pointer_holder_impl(pointer) noexcept {}

  auto pointer_to(typename traits::element_type& obj) -> Pointer {
    return traits::pointer_to(obj);
  }
};

template <class T, class Allocator>
using pointer_holder = pointer_holder_impl<typename std::allocator_traits<
  Allocator>::template rebind_alloc<T>::pointer>;


template <class Signature, class Traits, class Allocator, class Callable>
struct function_backend final
  : boost::mp11::mp_fold_q<
      enabled_signature_overloads<Signature, Traits>,
      clone_support<
        Traits,
        Allocator,
        function_backend<Signature, Traits, Allocator, Callable>,
        function_backend_base<Signature, Traits>>,
      boost::mp11::mp_bind_front<
        function_backend_part,
        function_backend<Signature, Traits, Allocator, Callable>>>
  , boost::empty_value<Callable, 0>
  , pointer_holder<
      function_backend<Signature, Traits, Allocator, Callable>,
      Allocator> {
  using callable_t = Callable;
  using base_t = function_backend_base<Signature, Traits>;
  using pointer_holder_t = pointer_holder<
    function_backend<Signature, Traits, Allocator, Callable>,
    Allocator>;
  using allocated_ptr_t = typename pointer_holder_t::pointer;

  function_backend(Callable callable, allocated_ptr_t ptr)
    : boost::empty_value<Callable, 0>(
      boost::empty_init_t(), std::move(callable))
    , pointer_holder_t(std::move(ptr)) {}

  using function_backend_base<Signature, Traits>::call;

  virtual void delete_this(void* type_erased_alloc) {
    using proto_traits = std::allocator_traits<Allocator>;
    static_assert(std::is_void<typename proto_traits::value_type>::value);

    auto& proto_alloc = *reinterpret_cast<Allocator*>(type_erased_alloc);

    using alloc_traits =
      typename proto_traits::template rebind_traits<function_backend>;
    auto alloc = typename alloc_traits::allocator_type(proto_alloc);

    auto const ptr = this->pointer_to(*this);

    alloc_traits::destroy(alloc, this);
    alloc_traits::deallocate(alloc, ptr, 1);
  };


  auto callable() noexcept -> Callable& {
    return boost::empty_value<callable_t, 0>::get();
  }

  auto callable() const noexcept -> Callable const& {
    return boost::empty_value<callable_t, 0>::get();
  }
};


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


template <class Signature, class Traits, class Callable, class Allocator>
auto make_backend(Callable callable, Allocator proto_alloc) -> std::unique_ptr<
  function_backend_base<Signature, Traits>,
  backend_deleter<Allocator>> {
  using backend_t = function_backend<Signature, Traits, Allocator, Callable>;
  using deleter_t = backend_deleter<Allocator>;
  using alloc_traits = typename std::allocator_traits<
    Allocator>::template rebind_traits<backend_t>;

  auto alloc = typename alloc_traits::allocator_type(proto_alloc);
  auto fancy_ptr = alloc_traits::allocate(alloc, 1);
  auto const raw_ptr = boost::to_address(fancy_ptr);
  try {
    alloc_traits::construct(
      alloc, raw_ptr, std::move(callable), std::move(fancy_ptr));
  } catch (...) { alloc_traits::deallocate(alloc, fancy_ptr, 1); }

  return std::unique_ptr<backend_t, deleter_t>(
    raw_ptr, deleter_t(proto_alloc));
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


template <class Signature, class Traits, class Allocator>
struct noncopyable_backend_holder {
  using allocator_type = Allocator;
  using backend_type = function_backend_base<Signature, Traits>;
  using deleter_type = backend_deleter<Allocator>;
  using backend_ptr = std::unique_ptr<backend_type, deleter_type>;

  noncopyable_backend_holder(Allocator alloc = Allocator())
    : backend_(nullptr, deleter_type(alloc)) {}

  template <class Callable>
  noncopyable_backend_holder(Callable callable, Allocator alloc = Allocator())
    : backend_(make_backend<Signature, Traits>(std::move(callable), alloc)) {}

  auto get_allocator() const -> allocator_type const& {
    return backend_.get_deleter().get_allocator();
  }

  backend_ptr backend_;
};


template <class Signature, class Traits, class Allocator>
struct copyable_backend_holder
  : noncopyable_backend_holder<Signature, Traits, Allocator> {
  using base_t = noncopyable_backend_holder<Signature, Traits, Allocator>;

  using base_t::base_t;

  copyable_backend_holder(copyable_backend_holder const& other)
    : copyable_backend_holder(other.get_allocator()) {
    auto alloc = this->get_allocator();
    using backend_type = typename base_t::backend_type;
    this->backend_.reset(static_cast<backend_type*>(
      other.backend_->clone(std::addressof(alloc))));
  }

  auto operator=(copyable_backend_holder const& other)
    -> copyable_backend_holder& {
    auto temp = other;
    return *this = std::move(temp);
  }
};


template <class Signature, class Traits, class Allocator>
using copyability_support = boost::mp11::mp_eval_if_c<
  !is_copyability_enabled<Traits>::value,
  noncopyable_backend_holder<Signature, Traits, Allocator>,
  copyable_backend_holder,
  Signature,
  Traits,
  Allocator>;

template <class Signature, class Traits, class Allocator, class... Overloads>
class basic_function
  : public function_frontend_part<
      basic_function<Signature, Traits, Allocator, Overloads...>,
      are_rvalue_overloads_enabled<Traits>::value,
      Overloads>...
  , copyability_support<Signature, Traits, Allocator>
{
private:
  using holder_t = copyability_support<Signature, Traits, Allocator>;

public:
  using allocator_type = typename holder_t::allocator_type;

  using holder_t::holder_t;
  using function_frontend_part<
    basic_function<Signature, Traits, Allocator, Overloads...>,
    are_rvalue_overloads_enabled<Traits>::value,
    Overloads>::operator()...;

  auto get_allocator() const -> allocator_type {
    return holder_t::get_allocator();
  }

private:
  template <class, bool, class>
  friend struct detail::function_frontend_part;
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_HPP
