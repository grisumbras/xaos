#include "xaos/detail/function.hpp"

#include <xaos/function.hpp>

#include <boost/core/lightweight_test.hpp>

#include <memory>
#include <string>
#include <type_traits>


namespace {


struct all_four {
  auto operator()() & -> std::string { return "&"; }
  auto operator()() const& -> std::string { return "const&"; }
  auto operator()() && -> std::string { return "&&"; }
  auto operator()() const&& -> std::string { return "const&&"; }
};


struct enable_all {
  static constexpr bool lvalue_ref_call = true;
  static constexpr bool const_lvalue_ref_call = true;
  static constexpr bool rvalue_ref_call = true;
  static constexpr bool const_rvalue_ref_call = true;
};


auto get_42() { return 42; }


struct with_mem_fn {
  int n = 0;
  auto get_n() { return n; }
};


template <class T>
class dumb_ptr
{
public:
  explicit dumb_ptr(T* ptr) : ptr_(ptr) {}

  auto get() const -> T* { return ptr_; }
  auto operator->() const -> T* { return get(); }

private:
  T* ptr_;
};

template <class T>
auto operator==(dumb_ptr<T> l, dumb_ptr<T> r) -> bool {
  return l.get() == r.get();
}

template <class T>
auto operator!=(dumb_ptr<T> l, dumb_ptr<T> r) -> bool {
  return !(l == r);
}


template <class T>
class counting_allocator;

struct counting_memory_resource {
  int max_allocated;
  int currently_allocated;

  inline auto get_allocator() -> counting_allocator<void>;
};

template <class T>
class counting_allocator
{
public:
  using value_type = T;
  using pointer = dumb_ptr<T>;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;

  template <class U>
  counting_allocator(counting_allocator<U> other)
    : counting_allocator(*other.res_) {}

  auto memory_resource() const -> counting_memory_resource& { return *res_; }

  auto allocate(std::size_t n) {
    auto alloc = std::allocator<T>();
    auto const result = alloc.allocate(n);
    auto const size = n * sizeof(T);
    res_->max_allocated += size;
    res_->currently_allocated += size;
    return dumb_ptr<T>(result);
  }

  void deallocate(pointer ptr, std::size_t n) {
    auto alloc = std::allocator<T>();
    alloc.deallocate(ptr.get(), n);
    res_->currently_allocated -= sizeof(T) * n;
  }

private:
  template <class U>
  friend class counting_allocator;
  friend struct counting_memory_resource;

  counting_allocator(counting_memory_resource& res) : res_(&res) {}

  counting_memory_resource* res_;
};

template <class L, class R>
auto operator==(counting_allocator<L> l, counting_allocator<R> r) -> bool {
  return &l.memory_resource() == &r.memory_resource();
}

template <class L, class R>
auto operator!=(counting_allocator<L> l, counting_allocator<R> r) -> bool {
  return !(l == r);
}

auto counting_memory_resource::get_allocator() -> counting_allocator<void> {
  return counting_allocator<void>(*this);
}


template <class T>
class tracking_allocator
{
public:
  using value_type = T;


  tracking_allocator(int& soccc) : soccc(&soccc) {}

  template <class U>
  tracking_allocator(tracking_allocator<U> other) : soccc(other.soccc) {}


  auto select_on_container_copy_construction() const -> tracking_allocator {
    ++(*soccc);
    return *this;
  }

  auto allocate(std::size_t n) -> T* {
    return std::allocator<T>().allocate(n);
  }

  void deallocate(T* ptr, std::size_t n) {
    std::allocator<T>().deallocate(ptr, n);
  }

private:
  template <class>
  friend class tracking_allocator;

  int* soccc;
};


} // namespace


int main() {
  // test support for various signatures
  BOOST_TEST_EQ(xaos::function<int()>([] { return 12; })(), 12);
  BOOST_TEST_EQ(xaos::function<int()>([] { return 13; })(), 13);
  BOOST_TEST_EQ(xaos::function<int(int)>([](auto n) { return n * n; })(5), 25);
  BOOST_TEST_EQ(xaos::function<double()>([] { return 0.5; })(), 0.5);

  // test convenience wrappers
  BOOST_TEST_EQ(xaos::function<int()>([] { return 1; })(), 1);
  BOOST_TEST_EQ(xaos::const_function<int()>([] { return 2; })(), 2);
  BOOST_TEST_EQ(xaos::rfunction<int()>([] { return 3; })(), 3);

  // test support for function pointers
  BOOST_TEST_EQ(xaos::function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::const_function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::rfunction<int()>(get_42)(), 42);

  // test support for members
  BOOST_TEST_EQ(
    xaos::function<int(with_mem_fn const&)>(&with_mem_fn::get_n)(
      with_mem_fn{7}),
    7);
  BOOST_TEST_EQ(
    xaos::function<int(with_mem_fn const&)>(&with_mem_fn::n)(with_mem_fn{4}),
    4);

  // test that operator()& calls mutable operator of the stored callable
  {
    auto f = xaos::function<int()>([n = 5]() mutable { return n++; });
    BOOST_TEST_EQ(f(), 5);
    BOOST_TEST_EQ(f(), 6);
  }

  // test that operator() overloads call corresponding overloads
  // of stored callable
  {
    using func_t = xaos::basic_function<std::string(), enable_all>;
    auto f = func_t(all_four());
    BOOST_TEST_EQ(f(), "&");
    BOOST_TEST_EQ(static_cast<func_t const&>(f)(), "const&");
    BOOST_TEST_EQ(std::move(f)(), "&&");
    BOOST_TEST_EQ(static_cast<func_t const&&>(f)(), "const&&");
  }

  // test copyability support
  {
    auto f = xaos::function<int()>([n = 11] { return n; });
    auto g = f;
    BOOST_TEST_EQ(g(), 11);

    f = [] { return 12; };
    BOOST_TEST_EQ(f(), 12);
  };

  // test allocator support
  {
    auto mem_rs = counting_memory_resource();
    auto alloc = mem_rs.get_allocator();
    BOOST_TEST_EQ(mem_rs.max_allocated, 0);

    {
      auto f = xaos::function<int(), decltype(alloc)>(
        [n = 90]() { return n; }, alloc);
      BOOST_TEST_GT(mem_rs.max_allocated, 0);
      BOOST_TEST_GT(mem_rs.currently_allocated, 0);

      BOOST_TEST(alloc == f.get_allocator());
    }
    BOOST_TEST_EQ(mem_rs.currently_allocated, 0);
  }

  // test SOCCC support
  {
    int soccc = 0;
    auto alloc = tracking_allocator<void>(soccc);
    BOOST_TEST_EQ(soccc, 0);
    auto const f = xaos::function<void(), decltype(alloc)>([] {}, alloc);

    auto g = f;
    BOOST_TEST_EQ(soccc, 1);

    g = f;
    BOOST_TEST_EQ(soccc, 1);
  }

  // test POCCA
  {
    auto mem_rs1 = counting_memory_resource();
    auto mem_rs2 = counting_memory_resource();
    auto alloc1 = mem_rs1.get_allocator();
    auto alloc2 = mem_rs2.get_allocator();

    using F = xaos::function<void(), decltype(alloc1)>;
    auto const f = F([] {}, alloc1);
    auto g = F([] {}, alloc2);
    BOOST_TEST_NE(
      &f.get_allocator().memory_resource(),
      &g.get_allocator().memory_resource());
    g = f;
    BOOST_TEST_EQ(
      &f.get_allocator().memory_resource(),
      &g.get_allocator().memory_resource());
  }

  // test POCMA
  {
    auto mem_rs1 = counting_memory_resource();
    auto mem_rs2 = counting_memory_resource();
    auto alloc1 = mem_rs1.get_allocator();
    auto alloc2 = mem_rs2.get_allocator();

    using F = xaos::function<void(), decltype(alloc1)>;
    auto f = F([] {}, alloc1);
    auto g = F([] {}, alloc2);
    BOOST_TEST_NE(
      &f.get_allocator().memory_resource(),
      &g.get_allocator().memory_resource());

    auto const h = f;
    BOOST_TEST_EQ(
      &f.get_allocator().memory_resource(),
      &h.get_allocator().memory_resource());

    g = std::move(f);
    BOOST_TEST_EQ(
      &g.get_allocator().memory_resource(),
      &h.get_allocator().memory_resource());
  }

  // test POCS
  {
    auto mem_rs1 = counting_memory_resource();
    auto mem_rs2 = counting_memory_resource();
    auto alloc1 = mem_rs1.get_allocator();
    auto alloc2 = mem_rs2.get_allocator();

    using F = xaos::function<void(), decltype(alloc1)>;
    auto f = F([] {}, alloc1);
    auto g = F([] {}, alloc2);
    BOOST_TEST_NE(
      &f.get_allocator().memory_resource(),
      &g.get_allocator().memory_resource());

    auto const f1 = f;
    BOOST_TEST_EQ(
      &f.get_allocator().memory_resource(),
      &f1.get_allocator().memory_resource());

    auto const g1 = g;
    BOOST_TEST_EQ(
      &g.get_allocator().memory_resource(),
      &g1.get_allocator().memory_resource());

    using std::swap;
    swap(f, g);
    BOOST_TEST_EQ(
      &f.get_allocator().memory_resource(),
      &g1.get_allocator().memory_resource());
    BOOST_TEST_EQ(
      &g.get_allocator().memory_resource(),
      &f1.get_allocator().memory_resource());
  }

  return boost::report_errors();
}
