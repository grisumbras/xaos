#ifndef XAOS_DETAIL_FUNCTION_ALLOCATOR_AWARE_POLYMORPHIC_STORAGE_HPP
#define XAOS_DETAIL_FUNCTION_ALLOCATOR_AWARE_POLYMORPHIC_STORAGE_HPP


#include <xaos/detail/backend_alloc.hpp>

#include <boost/core/empty_value.hpp>
#include <boost/mp11/integral.hpp>

#include <memory>


namespace xaos {
namespace detail {


template <class BackendPtr>
auto copy_construct_stored(BackendPtr const& backend) -> BackendPtr {
  auto const deleter = backend.get_deleter();

  using deleter_type = typename BackendPtr::deleter_type;
  using allocator_type = typename deleter_type::allocator_type;
  using allocator_traits = std::allocator_traits<allocator_type>;
  auto alloc = allocator_traits::select_on_container_copy_construction(
    deleter.get_allocator());

  using backend_interface = typename BackendPtr::element_type;
  auto const void_ptr = backend->clone(std::addressof(alloc));
  auto const iface_ptr = static_cast<backend_interface*>(void_ptr);
  return BackendPtr(iface_ptr, deleter_type(alloc));
}


template <class Allocator>
auto pick_allocator(
  Allocator const& if_true, Allocator const&, std::true_type) {
  return if_true;
}

template <class Allocator>
auto pick_allocator(
  Allocator const&, Allocator const& if_false, std::false_type) {
  return if_false;
}


template <
  class Allocator,
  std::enable_if_t<
    std::allocator_traits<Allocator>::propagate_on_container_swap::value,
    int> = 0>
auto swap_allocators(Allocator& l, Allocator& r) {
  using std::swap;
  swap(l, r);
}

template <
  class Allocator,
  std::enable_if_t<
    !std::allocator_traits<Allocator>::propagate_on_container_swap::value,
    int> = 0>
void swap_allocators(Allocator const&, Allocator const&) {}


template <class Allocator>
struct backend_deleter : boost::empty_value<Allocator> {
  static_assert(std::is_void<typename Allocator::value_type>::value);

  using allocator_type = Allocator;

  backend_deleter(Allocator alloc)
    : boost::empty_value<Allocator>(boost::empty_init_t(), std::move(alloc)) {}

  auto get_allocator() const -> allocator_type {
    return boost::empty_value<Allocator>::get();
  }

  void operator()(alloc_interface* ptr) {
    auto alloc = get_allocator();
    ptr->delete_this(std::addressof(alloc));
  }
};


template <class BackendInterface, class Allocator, class Callable>
struct function_backend;

template <class Result, class Allocator, class Callable>
auto make_backend(Allocator& proto_alloc, Callable callable) -> Result {
  using deleter_type = typename Result::deleter_type;
  using backend_interface = typename Result::element_type;
  using backend = function_backend<backend_interface, Allocator, Callable>;
  using allocator_traits =
    typename std::allocator_traits<Allocator>::template rebind_traits<backend>;
  using allocator_type = typename allocator_traits::allocator_type;
  auto alloc = allocator_type(proto_alloc);
  auto const raw_ptr = new_backend(alloc, callable);
  return Result(raw_ptr, deleter_type(proto_alloc));
}


template <class BackendBase, class Allocator>
class backend_pointer
{
public:
  using backend_interface = BackendBase;
  using deleter_type = backend_deleter<Allocator>;
  using allocator_type = typename deleter_type::allocator_type;
  using stored_ptr = std::unique_ptr<backend_interface, deleter_type>;

  template <class... Args>
  backend_pointer(allocator_type alloc, Args... args)
    : backend_pointer(make_backend<stored_ptr>(alloc, args...)) {}

  backend_pointer(backend_pointer&& other) noexcept = default;

  auto operator=(backend_pointer&& other) -> backend_pointer& {
    using allocator_traits = std::allocator_traits<allocator_type>;
    auto alloc = pick_allocator(
      other.get_allocator(),
      get_allocator(),
      boost::mp11::mp_bool<
        allocator_traits::propagate_on_container_move_assignment::value>());

    if (alloc != other.get_allocator()) {
      auto const impl_ptr = other.stored_->relocate(std::addressof(alloc));
      auto const iface_ptr = static_cast<backend_interface*>(impl_ptr);
      this->stored_ = stored_ptr(iface_ptr, deleter_type(alloc));
    } else {
      this->stored_ = std::move(other.stored_);
    }

    return *this;
  }

  void swap(backend_pointer& other) {
    auto this_alloc = get_allocator();
    auto other_alloc = other.get_allocator();
    swap_allocators(this_alloc, other_alloc);

    if (other_alloc != this_alloc) {
      auto const other_impl_ptr
        = other.stored_->relocate(std::addressof(other_alloc));
      auto const other_iface_ptr
        = static_cast<backend_interface*>(other_impl_ptr);
      auto other_stored
        = stored_ptr(other_iface_ptr, deleter_type(other_alloc));

      auto const this_impl_ptr = stored_->relocate(std::addressof(this_alloc));
      auto const this_iface_ptr
        = static_cast<backend_interface*>(this_impl_ptr);
      other.stored_ = stored_ptr(this_iface_ptr, deleter_type(this_alloc));

      stored_ = std::move(other_stored);
    } else {
      using std::swap;
      swap(this->stored_, other.stored_);
    }
  }

  auto get_allocator() const -> allocator_type {
    return stored_.get_deleter().get_allocator();
  }

  auto operator->() -> backend_interface* { return stored_.get(); }
  auto operator->() const -> backend_interface const* { return stored_.get(); }

  auto operator*() -> backend_interface& { return *operator->(); }
  auto operator*() const -> backend_interface const& { return *operator->(); }

protected:
  backend_pointer(stored_ptr stored) : stored_(std::move(stored)) {}

  stored_ptr stored_;
};


template <class BackendBase, class Allocator>
class copyable_backend_pointer : public backend_pointer<BackendBase, Allocator>
{
private:
  using base_t = backend_pointer<BackendBase, Allocator>;

public:
  using allocator_type = typename base_t::allocator_type;
  using stored_ptr = typename base_t::stored_ptr;
  using deleter_type = typename base_t::deleter_type;

  using base_t::base_t;

  copyable_backend_pointer(copyable_backend_pointer&&) = default;
  auto operator=(copyable_backend_pointer &&)
    -> copyable_backend_pointer& = default;

  copyable_backend_pointer(copyable_backend_pointer const& other)
    : base_t(copy_construct_stored(other.stored_)) {}

  auto operator=(copyable_backend_pointer const& other)
    -> copyable_backend_pointer& {
    using allocator_traits = std::allocator_traits<allocator_type>;
    auto alloc = pick_allocator(
      other.get_allocator(),
      this->get_allocator(),
      boost::mp11::mp_bool<
        allocator_traits::propagate_on_container_copy_assignment::value>());

    auto const impl_ptr = other.stored_->clone(std::addressof(alloc));

    using backend_interface = typename base_t::backend_interface;
    auto const iface_ptr = static_cast<backend_interface*>(impl_ptr);

    this->stored_ = stored_ptr(iface_ptr, deleter_type(alloc));

    return *this;
  }
};


} // namespace detail
} // namespace xaos


#endif // XAOS_DETAIL_FUNCTION_ALLOCATOR_AWARE_POLYMORPHIC_STORAGE_HPP
