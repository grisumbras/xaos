#include <xaos/function.hpp>


namespace {


struct enable_all {
  static constexpr bool lvalue_ref_call = true;
  static constexpr bool const_lvalue_ref_call = true;
  static constexpr bool rvalue_ref_call = true;
  static constexpr bool const_rvalue_ref_call = true;
};


class a_base_class
{};


} // namespace


static_assert(
  std::is_same_v<
    xaos::detail::signature_overloads<int(int)>,
    boost::mp11::
      mp_list<int(int) &, int(int) const&, int(int) &&, int(int) const&&>>);

static_assert(
  xaos::detail::trait_for_overload<xaos::function_traits, int(int) &>::value);
static_assert(!xaos::detail::trait_for_overload<
              xaos::function_traits,
              int(int) const&>::value);

static_assert(
  std::is_same_v<
    xaos::detail::enabled_signature_overloads<int(int), xaos::function_traits>,
    boost::mp11::mp_list<int(int) &>>);
static_assert(
  std::is_same_v<
    xaos::detail::
      enabled_signature_overloads<int(int), xaos::const_function_traits>,
    boost::mp11::mp_list<int(int) const&>>);
static_assert(
  std::is_same_v<
    xaos::detail::enabled_signature_overloads<int(int), enable_all>,
    boost::mp11::
      mp_list<int(int) &, int(int) const&, int(int) &&, int(int) const&&>>);

static_assert(std::is_base_of_v<
              xaos::detail::function_frontend_part<
                xaos::basic_function<int(), xaos::function_traits>,
                false,
                int() &>,
              xaos::basic_function<int(), xaos::function_traits>>);
static_assert(std::is_base_of_v<
              xaos::detail::function_frontend_part<
                xaos::basic_function<int(), enable_all>,
                true,
                int() &>,
              xaos::basic_function<int(), enable_all>>);
static_assert(std::is_base_of_v<
              xaos::detail::function_frontend_part<
                xaos::basic_function<int(), enable_all>,
                true,
                int() const&>,
              xaos::basic_function<int(), enable_all>>);
static_assert(std::is_base_of_v<
              xaos::detail::function_frontend_part<
                xaos::basic_function<int(), enable_all>,
                true,
                int() &&>,
              xaos::basic_function<int(), enable_all>>);

static_assert(
  xaos::detail::is_copyability_enabled<xaos::function_traits>::value);
static_assert(
  xaos::detail::is_copyability_enabled<xaos::const_function_traits>::value);
static_assert(
  !xaos::detail::is_copyability_enabled<xaos::rvalue_function_traits>::value);

static_assert(std::is_base_of_v<
              xaos::detail::noncopyable_backend_holder<
                int(),
                xaos::function_traits,
                std::allocator<void>>,
              xaos::function<int()>>);
static_assert(std::is_base_of_v<
              xaos::detail::copyable_backend_holder<
                int(),
                xaos::function_traits,
                std::allocator<void>>,
              xaos::function<int()>>);

static_assert(std::is_copy_constructible_v<xaos::function<int()>>);
static_assert(std::is_copy_assignable_v<xaos::function<int()>>);
static_assert(std::is_copy_constructible_v<xaos::const_function<int()>>);
static_assert(std::is_copy_assignable_v<xaos::const_function<int()>>);
static_assert(!std::is_copy_constructible_v<xaos::rfunction<int()>>);
static_assert(!std::is_copy_assignable_v<xaos::rfunction<int()>>);
