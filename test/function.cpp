#include <xaos/function.hpp>

#include <boost/core/lightweight_test.hpp>

#include <string>


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


} // namespace


int main() {
  BOOST_TEST_EQ(xaos::function<int()>([] { return 12; })(), 12);
  BOOST_TEST_EQ(xaos::function<int()>([] { return 13; })(), 13);
  BOOST_TEST_EQ(xaos::function<int(int)>([](auto n) { return n * n; })(5), 25);
  BOOST_TEST_EQ(xaos::function<double()>([] { return 0.5; })(), 0.5);

  BOOST_TEST_EQ(xaos::function<int()>([] { return 1; })(), 1);
  BOOST_TEST_EQ(xaos::const_function<int()>([] { return 2; })(), 2);
  BOOST_TEST_EQ(xaos::rfunction<int()>([] { return 3; })(), 3);

  BOOST_TEST_EQ(xaos::function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::const_function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::rfunction<int()>(get_42)(), 42);

  BOOST_TEST_EQ(xaos::function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::const_function<int()>(get_42)(), 42);
  BOOST_TEST_EQ(xaos::rfunction<int()>(get_42)(), 42);

  BOOST_TEST_EQ(
    xaos::function<int(with_mem_fn const&)>(&with_mem_fn::get_n)(
      with_mem_fn{7}),
    7);
  BOOST_TEST_EQ(
    xaos::function<int(with_mem_fn const&)>(&with_mem_fn::n)(with_mem_fn{4}),
    4);

  {
    auto f = xaos::function<int()>([n = 5]() mutable { return n++; });
    BOOST_TEST_EQ(f(), 5);
    BOOST_TEST_EQ(f(), 6);
  }

  {
    auto const f = xaos::const_function<int()>([]() { return 7; });
    BOOST_TEST_EQ(f(), 7);
  }

  {
    using func_t = xaos::basic_function<std::string(), enable_all>;
    auto f = func_t(all_four());
    BOOST_TEST_EQ(f(), "&");
    BOOST_TEST_EQ(static_cast<func_t const&>(f)(), "const&");
    BOOST_TEST_EQ(std::move(f)(), "&&");
    BOOST_TEST_EQ(static_cast<func_t const&&>(f)(), "const&&");
  }

  return boost::report_errors();
}
