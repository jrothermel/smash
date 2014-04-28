/*  TODO(mkretz): license?

    Copyright (C) 2009-2014 Matthias Kretz <kretz@kde.org>

    Vc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    Vc is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Vc.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SRC_TESTS_UNITTEST_H_
#define SRC_TESTS_UNITTEST_H_

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <typeinfo>
#include <vector>

#if defined(__GNUC__) && !defined(_WIN32) && defined(_GLIBCXX_OSTREAM)
#define HACK_OSTREAM_FOR_TTY 1
#endif

#ifdef HACK_OSTREAM_FOR_TTY
#include <unistd.h>
#include <ext/stdio_sync_filebuf.h>
#endif

#include "tests/assert.h"
#include "tests/macros.h"
#include "tests/ulp.h"

#ifdef DOXYGEN
/**
 * \defgroup unittest Unit Testing
 * @{
 *
 * In SMASH we use a unit testing framework that was originally developed for
 * the [Vc library](http://code.compeng.uni-frankfurt.de/projects/vc). It
 * simplifies test creation to the bare minimum. The following code suffices to
 * run a test:
 * \code
 * #include "tests/unittest.h"
 *
 * TEST(test_name) {
 *   int test = 1 + 1;
 *   COMPARE(test, 2) << "more details";
 *   VERIFY(1 > 0);
 * }
 * \endcode
 * This creates one test function (called "test_name"). This function is called
 * without any further code and executes to checks. If, for some reason, the
 * compiler would determine that test needs to have the value 3, then the output
 * would be:
   \verbatim
    FAIL: ┍ at /home/mkretz/src/smash/src/tests/testfile.cc:5 (0x40451f):
    FAIL: │ test (3) == 2 (2) -> false more details
    FAIL: ┕ test_name

    Testing done. 0 tests passed. 1 tests failed.
   \endverbatim
 * Let's take a look at what this tells us.
 * 1. The test macro that failed was in testfile.cc in line 5.
 * 2. If you want to look at the disassembly, the failure was at 0x40451f.
 * 3. The COMPARE macro compared the expression `test` against the expression
 *    `2`. It shows that `test` had a value of `3` while `2` had a value of `2`
 *    (what a surprise). Since the values are not equal `test == 2` returns \c
 *    false.
 * 4. The COMPARE, FUZZY_COMPARE, VERIFY, and FAIL macros can be used as
 *    streams. The output will only appear on failure and will be printed right
 *    after the normal output of the macro.
 * 5. Finally the name of the failed test (the name specified inside the TEST()
 *    macro) is printed.
 * 6. At the end of the run, a summary of the test results is shown. This may be
 *    important when there are many TEST functions.
 *
 * If the test passed you'll see:
   \verbatim
    PASS: test_name

    Testing done. 1 tests passed. 0 tests failed.
   \endverbatim
 *
 * You can compile tests with the \c smash_add_unittest macro. You only need to
 * pass it the name of the \c .cc file (without the file extension). So, if your
 * test code above was saved in tests/testfile.cc, then you'd add the line
 * \code
 * smash_add_unittest(testfile)
 * \endcode
 * to the \c CMakeLists.txt .
 * You will then get two new targets that you can build with make: \c testfile
 * and \c run_testtest . The latter can be used to build and run a test quickly
 * in "code - compile - test" cycles in test-driven development.
 */

/**
 * \brief Defines a test function.
 *
 * Consider this to expand to `void
 * function_name()`. The function_name will also be the name that appears in the
 * output after PASS/FAIL.
 */
#define TEST(function_name)

/**
 * \brief Same as above, but expects the code to throw an exception of type \p
 * ExceptionType.
 *
 * If the code does not throw (or throws a different exception),
 * the test is considered failed.
 */
#define TEST_CATCH(function_name, ExceptionType)

/**
 * \brief Tests that should be tested with several types as template parameter
 * can use this macro.
 *
 * Your test function then has this signature: `template <typename
 * T> void function_name()`.
 */
#define TEST_BEGIN(T, function_name, typelist)

/**
 * \brief Test functions created with TEST_BEGIN need to end with TEST_END.
 */
#define TEST_END

/**
 * \brief Verifies that \p condition is \c true.
 */
#define VERIFY(condition)

/**
 * \brief Verifies that \p test_value is equal to \p reference.
 */
#define COMPARE(test_value, reference)

/**
 * \brief Verifies that the difference between \p test_value and \p reference is
 * smaller than \p allowed_difference.
 *
 * If the test fails the output will show the actual difference between \p
 * test_value and \p reference. If this value is positive \p test_value is too
 * large. If it is negative \p test_value is too small.
 */
#define COMPARE_ABSOLUTE_ERROR(test_value, reference, allowed_difference)

/**
 * \brief Verifies that the difference between \p test_value and \p reference is
 * smaller than `allowed_relative_difference * reference`.
 *
 * If the test fails the output will show the actual difference between \p
 * test_value and \p reference. If this value is positive \p test_value is too
 * large. If it is negative \p test_value is too small.
 *
 * The following example tests that `a` is no more than 1% different from `b`:
 * \code
 * COMPARE_ABSOLUTE_ERROR(a, b, 0.01);
 * \endcode
 *
 * \note This test macro still works even if \p reference is set to 0. It will
 * then compare the difference against `allowed_relative_difference * <smallest
 * positive normalized value of reference type>`.
 */
#define COMPARE_RELATIVE_ERROR(test_value, reference, \
                               allowed_relative_difference)

/**
 * \brief Verifies that \p test_value is equal to \p reference within a
 * pre-defined distance in units of least precision (ulp).
 *
 * If the test fails it will print the distance in ulp between \p test_value and
 * \p reference as well as the maximum allowed distance. Often this difference
 * is not visible in the value because the conversion of a double/float to a
 * string needs to round the value to a sensible length.
 *
 * The allowed distance can be modified by calling:
 * \code
 * UnitTest::setFuzzyness<float>(4);
 * UnitTest::setFuzzyness<double>(7);
 * \endcode
 *
 * ### ulp
 * Unit of least precision is a unit that is derived from the the least
 * significant bit in the mantissa of a floating-point value. Consider a
 * single-precision number (23 mantissa bits) with exponent \f$e\f$. Then 1
 * ulp is \f$2^{e-23}\f$. Thus, \f$\log_2(u)\f$ signifies the the number
 * incorrect mantissa bits (with \f$u\f$ the distance in ulp).
 *
 * If \p test_value and \p reference have a different exponent the meaning of
 * ulp depends on the variable you look at. The FUZZY_COMPARE code always uses
 * \p reference to determine the magnitude of 1 ulp.
 *
 * Example:
 * The value `1.f` is `0x3f800000` in binary. The value
 * `1.00000011920928955078125f` with binary representation `0x3f800001`
 * therefore has a distance of 1 ulp.
 * A positive distance means the \p test_value is larger than the \p reference.
 * A negative distance means the \p test_value is smaller than the \p reference.
 * * `FUZZY_COMPARE(1.00000011920928955078125f, 1.f)` will show a distance of 1
 * * `FUZZY_COMPARE(1.f, 1.00000011920928955078125f)` will show a distance of -1
 *
 * The value `0.999999940395355224609375f` with binary representation
 * `0x3f7fffff` has a smaller exponent than `1.f`:
 * * `FUZZY_COMPARE(0.999999940395355224609375f, 1.f)` will show a distance of
 * -0.5
 * * `FUZZY_COMPARE(1.f, 0.999999940395355224609375f)` will show a distance of 1
 *
 * ### Comparing to 0
 * Distance to 0 is implemented as comparing to `std::numeric_limits<T>::min()`
 * instead and adding 1 to the resulting distance.
 */
#define FUZZY_COMPARE(test_value, reference)

/**
 * \brief Call this to fail a test.
 */
#define FAIL()

/**
 * \brief Wrap code that should fail an assertion with this macro.
 */
#define EXPECT_ASSERT_FAILURE(code)

/**
 * @}
 */

#else

namespace UnitTest {

using std::vector;
using std::tuple;
using std::get;

namespace AnsiColor {
struct Type {
  const char *data;
};
static const Type green = {"\033[1;40;32m"};
static const Type yellow = {"\033[1;40;33m"};
static const Type blue = {"\033[1;40;34m"};
static const Type normal = {"\033[0m"};
}  // namespace AnsiColor

#ifdef HACK_OSTREAM_FOR_TTY
class hacked_ostream : public std::ostream {
 public:
  using std::ostream::_M_streambuf;
};
static __attribute__((__const__)) bool mayUseColor(const std::ostream &os) {
  std::basic_streambuf<char> *hack1 = const_cast<std::basic_streambuf<char> *>(
      os.*(&hacked_ostream::_M_streambuf));
  __gnu_cxx::stdio_sync_filebuf<char> *hack =
      dynamic_cast<__gnu_cxx::stdio_sync_filebuf<char> *>(hack1);
  if (!hack) {
    return false;
  }
  FILE *file = hack->file();
  return 1 == isatty(fileno(file));
}
#else
static bool mayUseColor(const std::ostream &) {
  return false;
}
#endif

static std::ostream &operator<<(std::ostream &s, const AnsiColor::Type &color) {
  if (mayUseColor(s)) {
    return s << color.data;
  } else {
    return s;
  }
}

static inline void printPass() {
  std::cout << AnsiColor::green << " PASS: " << AnsiColor::normal;
}

class UnitTestFailure {};

using TestFunction = void (*)(void);

class UnitTester {  // {{{1
 public:
  UnitTester()
      : status(true),
        expect_failure(false),
        assert_failure(0),
        expect_assert_failure(false),
        float_fuzzyness(1.f),
        double_fuzzyness(1.),
        only_name(0),
        m_finalized(false),
        failedTests(0),
        passedTests(0) {
  }

  int finalize() {
    m_finalized = true;
    std::cout << "\n Testing done. " << passedTests << " tests passed. "
              << failedTests << " tests failed." << std::endl;
    return failedTests;
  }

  void runTestInt(TestFunction fun, const char *name);

  bool status;
  bool expect_failure;
  int assert_failure;
  bool expect_assert_failure;
  float float_fuzzyness;
  double double_fuzzyness;
  const char *only_name;

 private:
  bool m_finalized;
  int failedTests;

 public:
  int passedTests;
};

static UnitTester global_unit_test_object_;

static inline void EXPECT_FAILURE() {  // {{{1
  global_unit_test_object_.expect_failure = true;
}

static const char *_unittest_fail() {  // {{{1
  if (global_unit_test_object_.expect_failure) {
    return "XFAIL: ";
  }
  static const char *str = 0;
  if (str == 0) {
    if (mayUseColor(std::cout)) {
      static const char *fail = " \033[1;40;31mFAIL:\033[0m ";
      str = fail;
    } else {
      static const char *fail = " FAIL: ";
      str = fail;
    }
  }
  return str;
}
static void initTest(int argc, char **argv) {  // {{{1
  for (int i = 1; i < argc; ++i) {
    if (0 == std::strcmp(argv[i], "--help") ||
        0 == std::strcmp(argv[i], "-h")) {
      std::cout << "Usage: " << argv[0] << " [-h|--help] [--only <testname>]\n";
      exit(0);
    }
    if (0 == std::strcmp(argv[i], "--only") && i + 1 < argc) {
      global_unit_test_object_.only_name = argv[i + 1];
    }
  }
}
// setFuzzyness {{{1
template <typename T>
static inline void setFuzzyness(T);
template <>
inline void setFuzzyness<float>(float fuzz) {
  global_unit_test_object_.float_fuzzyness = fuzz;
}
template <>
inline void setFuzzyness<double>(double fuzz) {
  global_unit_test_object_.double_fuzzyness = fuzz;
}
void UnitTester::runTestInt(TestFunction fun, const char *name) {  // {{{1
  if (global_unit_test_object_.only_name &&
      0 != std::strcmp(name, global_unit_test_object_.only_name)) {
    return;
  }
  global_unit_test_object_.status = true;
  global_unit_test_object_.expect_failure = false;
  try {
    setFuzzyness<float>(1);
    setFuzzyness<double>(1);
    fun();
  }
  catch (UnitTestFailure) {
  }
  catch (std::exception &e) {
    std::cout << _unittest_fail() << "┍ " << name
              << " threw an unexpected exception:\n";
    std::cout << _unittest_fail() << "│ " << e.what() << '\n';
    global_unit_test_object_.status = false;
  }
  catch (...) {
    std::cout << _unittest_fail() << "┍ " << name
              << " threw an unexpected exception, of unknown type\n";
    global_unit_test_object_.status = false;
  }
  if (global_unit_test_object_.expect_failure) {
    if (!global_unit_test_object_.status) {
      std::cout << "XFAIL: " << name << std::endl;
    } else {
      std::cout
          << "unexpected PASS: " << name
          << "\n    This test should have failed but didn't. Check the code!"
          << std::endl;
      ++failedTests;
    }
  } else {
    if (!global_unit_test_object_.status) {
      std::cout << _unittest_fail() << "┕ " << name << std::endl;
      ++failedTests;
    } else {
      UnitTest::printPass();
      std::cout << name;
      std::cout << std::endl;
      ++passedTests;
    }
  }
}

// ulpDiffToReferenceWrapper {{{1
template <typename T>
T ulpDiffToReferenceWrapper(T a, T b) {
  const T diff = ulpDiffToReference(a, b);
  return diff;
}
// unittest_fuzzyCompareHelper {{{1
template <typename T>
static inline bool unittest_fuzzyCompareHelper(const T &a, const T &b) {
  return a == b;
}
template <>
inline bool unittest_fuzzyCompareHelper<float>(const float &a, const float &b) {
  return ulpDiffToReferenceWrapper(a, b) <=
         global_unit_test_object_.float_fuzzyness;
}
template <>
inline bool unittest_fuzzyCompareHelper<double>(const double &a,
                                                const double &b) {
  return ulpDiffToReferenceWrapper(a, b) <=
         global_unit_test_object_.double_fuzzyness;
}

// unitttest_comparePrintHelper {{{1
template <typename T1, typename T2, typename M>
inline void unitttest_comparePrintHelper(const T1 &a, const T2 &b, const M &m,
                                         const char *aa, const char *bb,
                                         const char *file, int line,
                                         double fuzzyness = 0.) {
  std::cout << "       " << aa << " (" << std::setprecision(10) << a
            << std::setprecision(6) << ") == " << bb << " ("
            << std::setprecision(10) << b << std::setprecision(6) << ") -> "
            << m;
  if (fuzzyness > 0.) {
    std::cout << " with fuzzyness " << fuzzyness;
  }
  std::cout << " at " << file << ":" << line << " failed.\n";
}

// unittest_fuzzynessHelper {{{1
template <typename T>
inline double unittest_fuzzynessHelper(const T &) {
  return 0.;
}
template <>
inline double unittest_fuzzynessHelper<float>(const float &) {
  return global_unit_test_object_.float_fuzzyness;
}
template <>
inline double unittest_fuzzynessHelper<double>(const double &) {
  return global_unit_test_object_.double_fuzzyness;
}

class _UnitTest_Compare {  // {{{1
  template <typename T, typename ET>
  static bool absoluteErrorTest(const T &a, const T &b, ET error) {
    if (a > b) {  // don't use abs(a - b) because it doesn't work for unsigned
                  // integers
      return a - b > error;
    } else {
      return b - a > error;
    }
  }

  template <typename T, typename ET>
  static bool relativeErrorTest(const T &a, const T &b, ET error) {
    if (b > 0) {
      error *= b;
    } else if (b < 0) {
      error *= -b;
    } else if (std::is_floating_point<T>::value) {
      // if the reference value is 0 then use the smallest normalized number
      error *= std::numeric_limits<T>::min();
    } else {
      // error *= 1;  // the smallest non-zero positive number is 1...
    }
    if (a > b) {  // don't use abs(a - b) because it doesn't work for unsigned
                  // integers
      return a - b > error;
    } else {
      return b - a > error;
    }
  }

 public:
  struct Fuzzy {};
  struct AbsoluteError {};
  struct RelativeError {};

  // Normal Compare ctor {{{2
  template <typename T1, typename T2>
  ALWAYS_INLINE _UnitTest_Compare(const T1 &a, const T2 &b, const char *_a,
                                  const char *_b, const char *_file, int _line)
      : m_ip(getIp()), m_failed(!(a == b)) {
    if (IS_UNLIKELY(m_failed)) {
      printFirst();
      printPosition(_file, _line);
      print(":\n");
      print(_a);
      print(" (");
      print(std::setprecision(10));
      print(a);
      print(") == ");
      print(_b);
      print(" (");
      print(std::setprecision(10));
      print(b);
      print(std::setprecision(6));
      print(") -> ");
      print(a == b);
    }
  }

  // Fuzzy Compare ctor {{{2
  template <typename T>
  ALWAYS_INLINE _UnitTest_Compare(const T &a, const T &b, const char *_a,
                                  const char *_b, const char *_file, int _line,
                                  Fuzzy)
      : m_ip(getIp()), m_failed(!unittest_fuzzyCompareHelper(a, b)) {
    if (IS_UNLIKELY(m_failed)) {
      printFirst();
      printPosition(_file, _line);
      print(":\n");
      print(_a);
      print(" (");
      print(std::setprecision(10));
      print(a);
      print(") ≈ ");
      print(_b);
      print(" (");
      print(std::setprecision(10));
      print(b);
      print(std::setprecision(6));
      print(") -> ");
      print(a == b);
      printFuzzyInfo(a, b);
    }
  }

  // Absolute Error Compare ctor {{{2
  template <typename T, typename ET>
  ALWAYS_INLINE _UnitTest_Compare(const T &a, const T &b, const char *_a,
                                  const char *_b, const char *_file, int _line,
                                  AbsoluteError, ET error)
      : m_ip(getIp()), m_failed(absoluteErrorTest(a, b, error)) {
    if (IS_UNLIKELY(m_failed)) {
      printFirst();
      printPosition(_file, _line);
      print(":\n");
      print(_a);
      print(" (");
      print(std::setprecision(10));
      print(a);
      print(") ≈ ");
      print(_b);
      print(" (");
      print(std::setprecision(10));
      print(b);
      print(std::setprecision(6));
      print(") -> ");
      print(a == b);
      print("\ndifference: ");
      if (a > b) {
        print(a - b);
      } else {
        print('-');
        print(b - a);
      }
      print(", allowed difference: ±");
      print(error);
      print("\ndistance: ");
      print(ulpDiffToReferenceSigned(a, b));
      print(" ulp");
    }
  }


  // Relative Error Compare ctor {{{2
  template <typename T, typename ET>
  ALWAYS_INLINE _UnitTest_Compare(const T &a, const T &b, const char *_a,
                                  const char *_b, const char *_file, int _line,
                                  RelativeError, ET error)
      : m_ip(getIp()), m_failed(relativeErrorTest(a, b, error)) {
    if (IS_UNLIKELY(m_failed)) {
      printFirst();
      printPosition(_file, _line);
      print(":\n");
      print(_a);
      print(" (");
      print(std::setprecision(10));
      print(a);
      print(") ≈ ");
      print(_b);
      print(" (");
      print(std::setprecision(10));
      print(b);
      print(std::setprecision(6));
      print(") -> ");
      print(a == b);
      print("\nrelative difference: ");
      if (a > b) {
        print((a - b) / (b > 0 ? b : -b));
      } else {
        print('-');
        print((b - a) / (b > 0 ? b : -b));
      }
      print(", allowed: ±");
      print(error);
      print("\nabsolute difference: ");
      if (a > b) {
        print(a - b);
      } else {
        print('-');
        print(b - a);
      }
      print(", allowed: ±");
      print(error * (b > 0 ? b : -b));
      print("\ndistance: ");
      print(ulpDiffToReferenceSigned(a, b));
      print(" ulp");
    }
  }
  // VERIFY ctor {{{2
  ALWAYS_INLINE _UnitTest_Compare(bool good, const char *cond,
                                  const char *_file, int _line)
      : m_ip(getIp()), m_failed(!good) {
    if (IS_UNLIKELY(m_failed)) {
      printFirst();
      printPosition(_file, _line);
      print(": ");
      print(cond);
    }
  }

  // FAIL ctor {{{2
  ALWAYS_INLINE _UnitTest_Compare(const char *_file, int _line)
      : m_ip(getIp()), m_failed(true) {
    printFirst();
    printPosition(_file, _line);
    print(":\n");
  }

  // stream operators {{{2
  template <typename T>
  ALWAYS_INLINE const _UnitTest_Compare &operator<<(const T &x) const {
    if (IS_UNLIKELY(m_failed)) {
      print(x);
    }
    return *this;
  }

  ALWAYS_INLINE const _UnitTest_Compare &operator<<(const char *str) const {
    if (IS_UNLIKELY(m_failed)) {
      print(str);
    }
    return *this;
  }

  ALWAYS_INLINE const _UnitTest_Compare &operator<<(const char ch) const {
    if (IS_UNLIKELY(m_failed)) {
      print(ch);
    }
    return *this;
  }

  ALWAYS_INLINE const _UnitTest_Compare &operator<<(bool b) const {
    if (IS_UNLIKELY(m_failed)) {
      print(b);
    }
    return *this;
  }

  ALWAYS_INLINE ~_UnitTest_Compare()  // {{{2
#ifdef _NO_NOEXCEPT
      throw(UnitTestFailure)
#else
      noexcept(false)
#endif
  {
    if (IS_UNLIKELY(m_failed)) {
      printLast();
    }
  }

  // }}}2
 private:
  static ALWAYS_INLINE size_t getIp() {  // {{{2
    size_t _ip;
#ifdef __GNUC__
#ifdef __x86_64__
    asm volatile("lea 0(%%rip),%0" : "=r"(_ip));
#else
    // asm volatile("call 1f\n\t1: pop %0" : "=r"(_ip));
    asm volatile("1: movl $1b,%0" : "=r"(_ip));
#endif
#else
    _ip = 0;
#endif
    return _ip;
  }
  static void printFirst() {  // {{{2
    std::cout << _unittest_fail() << "┍ ";
  }
  // print overloads {{{2
  template <typename T>
  static inline void print(const T &x) {
    std::cout << x;
  }
  static void print(const std::type_info &x) {
    std::cout << x.name();
  }
  static void print(const char *str) {
    const char *pos = 0;
    if (0 != (pos = std::strchr(str, '\n'))) {
      if (pos == str) {
        std::cout << '\n' << _unittest_fail() << "│ " << &str[1];
      } else {
        char *left = strdup(str);
        left[pos - str] = '\0';
        std::cout << left << '\n' << _unittest_fail() << "│ " << &pos[1];
        free(left);
      }
    } else {
      std::cout << str;
    }
  }
  static void print(const char ch) {
    if (ch == '\n') {
      std::cout << '\n' << _unittest_fail() << "│ ";
    } else {
      std::cout << ch;
    }
  }
  static void print(bool b) {
    std::cout << (b ? "true" : "false");
  }
  // printLast {{{2
  static void printLast() {
    std::cout << std::endl;
    global_unit_test_object_.status = false;
    throw UnitTestFailure();
  }
  // printPosition {{{2
  void printPosition(const char *_file, int _line) {
    std::cout << "at " << _file << ':' << _line << " (0x" << std::hex << m_ip
              << std::dec << ')';
  }
  // printFuzzy... {{{2
  template <typename T>
  static inline void printFuzzyInfo(T, T) {
  }
  template <typename T>
  static inline void printFuzzyInfoImpl(T a, T b, double fuzzyness) {
    print("\ndistance: ");
    print(ulpDiffToReferenceSigned(a, b));
    print(" ulp, allowed distance: ±");
    print(fuzzyness);
    print(" ulp");
  }
  // member variables {{{2
  const size_t m_ip;
  const bool m_failed;
};
// printFuzzyInfo specializations for float and double {{{1
template <>
inline void _UnitTest_Compare::printFuzzyInfo(float a, float b) {
  printFuzzyInfoImpl(a, b, global_unit_test_object_.float_fuzzyness);
}
template <>
inline void _UnitTest_Compare::printFuzzyInfo(double a, double b) {
  printFuzzyInfoImpl(a, b, global_unit_test_object_.double_fuzzyness);
}

// FUZZY_COMPARE {{{1
// Workaround for clang: The "<< ' '" is only added to silence the warnings
// about unused return values.
#define FUZZY_COMPARE(a, b)                                         \
  UnitTest::_UnitTest_Compare(a, b, #a, #b, __FILE__, __LINE__,     \
                              UnitTest::_UnitTest_Compare::Fuzzy()) \
      << ' '
// COMPARE_ABSOLUTE_ERROR {{{1
#define COMPARE_ABSOLUTE_ERROR(a__, b__, error__)                           \
  UnitTest::_UnitTest_Compare(a__, b__, #a__, #b__, __FILE__, __LINE__,     \
                              UnitTest::_UnitTest_Compare::AbsoluteError(), \
                              error__)                                      \
      << ' '
// COMPARE_RELATIVE_ERROR {{{1
#define COMPARE_RELATIVE_ERROR(a__, b__, error__)                           \
  UnitTest::_UnitTest_Compare(a__, b__, #a__, #b__, __FILE__, __LINE__,     \
                              UnitTest::_UnitTest_Compare::RelativeError(), \
                              error__)                                      \
      << ' '
// COMPARE {{{1
#define COMPARE(a, b) \
  UnitTest::_UnitTest_Compare(a, b, #a, #b, __FILE__, __LINE__) << ' '
// VERIFY {{{1
#define VERIFY(cond) \
  UnitTest::_UnitTest_Compare(cond, #cond, __FILE__, __LINE__) << ' '
// FAIL {{{1
#define FAIL() UnitTest::_UnitTest_Compare(__FILE__, __LINE__) << ' '

// ADD_PASS() << "text" {{{1
class ADD_PASS {
 public:
  ADD_PASS() {
    ++global_unit_test_object_.passedTests;
    printPass();
  }
  ~ADD_PASS() {
    std::cout << std::endl;
  }
  template <typename T>
  ADD_PASS &operator<<(const T &x) {
    std::cout << x;
    return *this;
  }
};
// unittest_assert (called from assert macro) {{{1
void unittest_assert(bool cond, const char *code, const char *file, int line) {
  if (!cond) {
    if (global_unit_test_object_.expect_assert_failure) {
      ++global_unit_test_object_.assert_failure;
    } else {
      _UnitTest_Compare(file, line) << "assert(" << code << ") failed.";
    }
  }
}
// EXPECT_ASSERT_FAILURE {{{1
#define EXPECT_ASSERT_FAILURE(code)                                          \
  UnitTest::global_unit_test_object_.expect_assert_failure = true;           \
  UnitTest::global_unit_test_object_.assert_failure = 0;                     \
  code;                                                                      \
  if (UnitTest::global_unit_test_object_.assert_failure == 0) {              \
    /* failure expected but it didn't fail */                                \
    std::cout << "       " << #code << " at " << __FILE__ << ":" << __LINE__ \
              << " did not fail as was expected.\n";                         \
    UnitTest::global_unit_test_object_.status = false;                       \
    throw UnitTestFailure();                                                 \
    return;                                                                  \
  }                                                                          \
  UnitTest::global_unit_test_object_.expect_assert_failure = false
}  // namespace UnitTest
namespace UnitTest {  // {{{1
// typeToString {{{2
template <typename T>
inline std::string typeToString();

template <typename T>
inline std::string typeToString_impl(T) {
  return typeid(T).name();
}

template <typename T>
inline std::string typeToString() {
  return typeToString_impl(T());
}
template <>
inline std::string typeToString<void>() {
  return "";
}

template <>
inline std::string typeToString<long double>() {
  return "long double";
}
template <>
inline std::string typeToString<double>() {
  return "double";
}
template <>
inline std::string typeToString<float>() {
  return " float";
}
/* NOLINT(runtime/int):
 * The warning is disabled here because this code is not about using integers
 * but about recognizing integer types. And the following types are the
 * different builtin types. It doesn't make sense to check for aliased types
 * here.
 */
template <>
inline std::string typeToString<long long>() {  // NOLINT(runtime/int)
  return " long long";
}
template <>
inline std::string typeToString<unsigned long long>() {  // NOLINT(runtime/int)
  return "ulong long";
}
template <>
inline std::string typeToString<long>() {  // NOLINT(runtime/int)
  return "  long";
}
template <>
inline std::string typeToString<unsigned long>() {  // NOLINT(runtime/int)
  return " ulong";
}
template <>
inline std::string typeToString<int>() {
  return "   int";
}
template <>
inline std::string typeToString<unsigned int>() {
  return "  uint";
}
template <>
inline std::string typeToString<short>() {  // NOLINT(runtime/int)
  return " short";
}
template <>
inline std::string typeToString<unsigned short>() {  // NOLINT(runtime/int)
  return "ushort";
}
template <>
inline std::string typeToString<char>() {
  return "  char";
}
template <>
inline std::string typeToString<unsigned char>() {
  return " uchar";
}
template <>
inline std::string typeToString<signed char>() {
  return " schar";
}
// runAll and TestData {{{2
typedef tuple<TestFunction, std::string> TestData;
vector<TestData> g_allTests;

static void runAll() {
  for (const auto &data : g_allTests) {
    global_unit_test_object_.runTestInt(get<0>(data), get<1>(data).c_str());
  }
}
// class Test {{{2
template <typename T, typename Exception = void, typename TestImpl = void>
class Test : TestImpl {
 private:
  static void wrapper() {
    try {
      TestImpl::test_function();
    }
    catch (Exception &e) {
      return;
    }
    FAIL() << "Test was expected to throw, but it didn't";
  }

 public:
  Test(std::string name) {
    if (!std::is_same<T, void>()) {
      name += '<' + typeToString<T>() + '>';
    }
    g_allTests.emplace_back(wrapper, name);
  }
};
template <typename T>
class Test<T, void, void> {
 public:
  Test(TestFunction fun, std::string name) {
    if (!std::is_same<T, void>()) {
      name += '<' + typeToString<T>() + '>';
    }
    g_allTests.emplace_back(fun, name);
  }
};

// class Test2 {{{2
template <template <typename V> class TestFunctor, typename... TestTypes>
class Test2;

template <template <typename V> class TestFunctor>
class Test2<TestFunctor> {
 protected:
  explicit Test2(const std::string &) {
  }
};

template <template <typename V> class TestFunctor, typename TestType0,
          typename... TestTypes>
class Test2<TestFunctor, TestType0, TestTypes...> : public Test2<TestFunctor,
                                                                 TestTypes...> {
  typedef Test2<TestFunctor, TestTypes...> Base;

 public:
  static void call() {
    TestFunctor<TestType0>()();
  }

  explicit Test2(std::string name) : Base(name) {
    name += '<' + typeToString<TestType0>() + '>';
    g_allTests.emplace_back(&call, name);
  }
};
// hackTypelist {{{2
template <template <typename V> class F, typename... Typelist>
UnitTest::Test2<F, Typelist...> hackTypelist(void (*)(Typelist...));
}  // namespace UnitTest
// TEST_BEGIN / TEST_END / TEST macros {{{1
#define TEST_BEGIN(V__, fun__, typelist__)                                     \
  template <typename V__>                                                      \
  struct fun__;                                                                \
  static auto test_##fun__##__ = decltype(                                     \
      UnitTest::hackTypelist<fun__>(std::declval<void typelist__>()))(#fun__); \
  template <typename V__>                                                      \
  struct fun__ {                                                               \
    void operator()() {
#define TEST_END \
  }              \
  };

#define TEST(fun__)                                             \
  void fun__();                                                 \
  static UnitTest::Test<void> test_##fun__##__(&fun__, #fun__); \
  void fun__()

#define TEST_CATCH(fun__, exception__)                                      \
  struct fun__ {                                                            \
    static void test_function();                                            \
  };                                                                        \
  static UnitTest::Test<void, exception__, fun__> test_##fun__##__(#fun__); \
  void fun__::test_function()

// main {{{1
int main(int argc, char **argv) {
  UnitTest::initTest(argc, argv);
  UnitTest::runAll();
  return UnitTest::global_unit_test_object_.finalize();
}
// }}}1
#endif  // DOXYGEN
#endif  // SRC_TESTS_UNITTEST_H_

// vim: foldmethod=marker sw=2
