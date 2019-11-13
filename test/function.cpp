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


} // namespace


int main() {
  BOOST_TEST_EQ(xaos::function<int()>([] { return 12; })(), 12);
  BOOST_TEST_EQ(xaos::function<int()>([] { return 13; })(), 13);
  BOOST_TEST_EQ(xaos::function<int(int)>([](auto n) { return n * n; })(5), 25);
  BOOST_TEST_EQ(xaos::function<double()>([] { return 0.5; })(), 0.5);

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
