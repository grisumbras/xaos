#ifndef XAOS_DETAIL_FUNCTION_HPP
#define XAOS_DETAIL_FUNCTION_HPP


#include <xaos/detail/backend_pointer.hpp>
#include <xaos/detail/function_alloc.hpp>
#include <xaos/detail/function_overloads.hpp>

#include <boost/core/empty_value.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>


namespace xaos {
namespace detail {


template <class Signature, class Traits>
struct function_backend_interface
  : alloc_interface
  , boost::mp11::mp_apply<
      boost::mp11::mp_inherit,
      boost::mp11::mp_append<
        maybe_clone_interface<Traits>,
        boost::mp11::mp_transform<
          call_overload_interface,
          enabled_overloads<Signature, Traits>>>> {
  using signature = Signature;
  using traits = Traits;
};


template <class BackendInterface, class Allocator, class Callable>
struct function_backend final
  : boost::mp11::mp_fold_q<
      enabled_overloads<
        typename BackendInterface::signature,
        typename BackendInterface::traits>,
      maybe_clone_implementation<
        typename BackendInterface::traits,
        function_backend<BackendInterface, Allocator, Callable>,
        BackendInterface>,
      boost::mp11::mp_bind_front<
        call_overload,
        function_backend<BackendInterface, Allocator, Callable>>>
  , boost::empty_value<Callable, 0>
  , pointer_storage_helper<
      function_backend<BackendInterface, Allocator, Callable>,
      Allocator,
      1> {
  using allocator_type = Allocator;
  using callable_type = Callable;
  using interface_type = BackendInterface;
  using pointer_holder_t
    = pointer_storage_helper<function_backend, Allocator, 1>;

  function_backend(typename pointer_holder_t::pointer ptr, Callable callable)
    : boost::empty_value<Callable, 0>(
      boost::empty_init_t(), std::move(callable))
    , pointer_holder_t(std::move(ptr)) {}

  virtual auto relocate(void* type_erased_alloc) -> void* {
    auto alloc
      = restore_allocator<allocator_type, function_backend>(type_erased_alloc);
    auto const raw_ptr = new_backend(alloc, std::move(callable()));
    return static_cast<interface_type*>(raw_ptr);
  };

  virtual void delete_this(void* type_erased_alloc) {
    auto alloc
      = restore_allocator<allocator_type, function_backend>(type_erased_alloc);
    auto const ptr = this->pointer_to(*this);

    using proto_traits = std::allocator_traits<Allocator>;
    using alloc_traits =
      typename proto_traits::template rebind_traits<function_backend>;
    alloc_traits::destroy(alloc, this);
    alloc_traits::deallocate(alloc, ptr, 1);
  };

  auto callable() noexcept -> Callable& {
    return boost::empty_value<Callable, 0>::get();
  }

  auto callable() const noexcept -> Callable const& {
    return boost::empty_value<Callable, 0>::get();
  }
};

template <class Signature, class Traits, class Allocator>
using backend_storage = boost::mp11::mp_apply_q<
  boost::mp11::mp_if<
    is_copyability_enabled<Traits>,
    boost::mp11::mp_quote<copyable_backend_pointer>,
    boost::mp11::mp_quote<backend_pointer>>,
  boost::mp11::
    mp_list<function_backend_interface<Signature, Traits>, Allocator>>;


template <class Signature, class Traits, class Allocator, class... Overloads>
class basic_function
  : parens_overload<
      basic_function<Signature, Traits, Allocator, Overloads...>,
      are_rvalue_overloads_enabled<Traits>::value,
      Overloads>...
{
private:
  template <class, bool, class>
  friend struct parens_overload;

  using storage_t = backend_storage<Signature, Traits, Allocator>;
  storage_t storage_;

public:
  using allocator_type = typename storage_t::allocator_type;

  template <class Callable>
  basic_function(Callable callable, Allocator alloc = Allocator())
    : storage_(alloc, callable) {}

  using parens_overload<
    basic_function<Signature, Traits, Allocator, Overloads...>,
    are_rvalue_overloads_enabled<Traits>::value,
    Overloads>::operator()...;

  auto get_allocator() const -> allocator_type {
    return storage_.get_allocator();
  }

  void swap(basic_function& other) { storage_.swap(other.storage_); }
};


template <class Signature, class Traits, class Allocator, class... Overloads>
void swap(
  basic_function<Signature, Traits, Allocator, Overloads...>& l,
  basic_function<Signature, Traits, Allocator, Overloads...>& r) {
  l.swap(r);
}


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_HPP
