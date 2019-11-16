#ifndef XAOS_DETAIL_FUNCTION_HPP
#define XAOS_DETAIL_FUNCTION_HPP


#include <xaos/detail/function_alloc.hpp>
#include <xaos/detail/function_overloads.hpp>

#include <boost/core/empty_value.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/utility.hpp>


namespace xaos {
namespace detail {


template <class Signature, class Traits>
using function_backend_interface = boost::mp11::mp_apply<
  boost::mp11::mp_inherit,
  boost::mp11::mp_append<
    boost::mp11::mp_list<delete_interface>,
    maybe_clone_interface<Traits>,
    boost::mp11::mp_transform<
      call_overload_interface,
      enabled_overloads<Signature, Traits>>>>;


template <class Signature, class Traits, class Allocator, class Callable>
struct function_backend final
  : boost::mp11::mp_fold_q<
      enabled_overloads<Signature, Traits>,
      maybe_clone_implementation<
        Traits,
        function_backend<Signature, Traits, Allocator, Callable>,
        function_backend_interface<Signature, Traits>>,
      boost::mp11::mp_bind_front<
        call_overload,
        function_backend<Signature, Traits, Allocator, Callable>>>
  , boost::empty_value<Callable, 0>
  , pointer_storage_helper<
      function_backend<Signature, Traits, Allocator, Callable>,
      Allocator,
      1> {
  using allocator_type = Allocator;
  using callable_type = Callable;
  using interface_type = function_backend_interface<Signature, Traits>;
  using pointer_holder_t
    = pointer_storage_helper<function_backend, Allocator, 1>;

  function_backend(typename pointer_holder_t::pointer ptr, Callable callable)
    : boost::empty_value<Callable, 0>(
      boost::empty_init_t(), std::move(callable))
    , pointer_holder_t(std::move(ptr)) {}

  virtual void delete_this(void* type_erased_alloc) {
    auto alloc
      = get_allocator<allocator_type, function_backend>(type_erased_alloc);
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

template <class Signature, class Traits, class Callable, class Allocator>
auto make_backend(Callable callable, Allocator proto_alloc) -> std::unique_ptr<
  function_backend_interface<Signature, Traits>,
  backend_deleter<Allocator>> {
  using backend_t = function_backend<Signature, Traits, Allocator, Callable>;
  using alloc_traits = typename std::allocator_traits<
    Allocator>::template rebind_traits<backend_t>;

  auto alloc = typename alloc_traits::allocator_type(proto_alloc);
  auto const raw_ptr = new_backend(alloc, std::move(callable));

  using deleter_t = backend_deleter<Allocator>;
  return std::unique_ptr<backend_t, deleter_t>(
    raw_ptr, deleter_t(proto_alloc));
}


template <class Signature, class Traits, class Allocator>
struct noncopyable_backend_storage {
  using allocator_type = Allocator;
  using backend_type = function_backend_interface<Signature, Traits>;
  using deleter_type = backend_deleter<Allocator>;
  using backend_ptr = std::unique_ptr<backend_type, deleter_type>;

  noncopyable_backend_storage(allocator_type alloc)
    : backend_(nullptr, deleter_type(alloc)) {}

  template <class Callable>
  noncopyable_backend_storage(Callable callable, allocator_type alloc)
    : backend_(make_backend<Signature, Traits>(std::move(callable), alloc)) {}

  auto get_allocator() const -> allocator_type {
    return backend_.get_deleter().get_allocator();
  }

  backend_ptr backend_;
};


template <class Signature, class Traits, class Allocator>
struct copyable_backend_storage
  : noncopyable_backend_storage<Signature, Traits, Allocator> {
  using base_t = noncopyable_backend_storage<Signature, Traits, Allocator>;

  using base_t::base_t;

  copyable_backend_storage(copyable_backend_storage&&) = default;
  auto operator=(copyable_backend_storage &&)
    -> copyable_backend_storage& = default;

  copyable_backend_storage(copyable_backend_storage const& other)
    : copyable_backend_storage(other.get_allocator()) {
    auto alloc = this->get_allocator();
    using backend_type = typename base_t::backend_type;
    this->backend_.reset(static_cast<backend_type*>(
      other.backend_->clone(std::addressof(alloc))));
  }

  auto operator=(copyable_backend_storage const& other)
    -> copyable_backend_storage& {
    auto temp = other;
    return *this = std::move(temp);
  }
};


template <class Signature, class Traits, class Allocator>
using backend_storage = boost::mp11::mp_eval_if_not<
  is_copyability_enabled<Traits>,
  noncopyable_backend_storage<Signature, Traits, Allocator>,
  copyable_backend_storage,
  Signature,
  Traits,
  Allocator>;


template <class Signature, class Traits, class Allocator, class... Overloads>
class basic_function
  : parens_overload<
      basic_function<Signature, Traits, Allocator, Overloads...>,
      are_rvalue_overloads_enabled<Traits>::value,
      Overloads>...
  , backend_storage<Signature, Traits, Allocator>
{
private:
  using storage_t = backend_storage<Signature, Traits, Allocator>;

public:
  using allocator_type = typename storage_t::allocator_type;


  template <class Callable>
  basic_function(Callable callable, Allocator alloc = Allocator())
    : storage_t(callable, alloc) {}


  using parens_overload<
    basic_function<Signature, Traits, Allocator, Overloads...>,
    are_rvalue_overloads_enabled<Traits>::value,
    Overloads>::operator()...;

  using storage_t::get_allocator;

private:
  template <class, bool, class>
  friend struct parens_overload;
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_HPP
