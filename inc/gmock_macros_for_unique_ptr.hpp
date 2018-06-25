/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Przemyslaw Wos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
**/
#pragma once

#include <memory>
#include <type_traits>
#include <gmock/gmock.h>
#include "owned_pointer.hpp"

namespace pobu_gmock
{

template<const int i, const int n, typename T, typename... Args>
struct nth_arg
{
  typedef typename
      std::conditional< i == n, T, typename nth_arg<i, n + 1, Args...>::type >::type type;
};

template<const int i, const int n, typename T>
struct nth_arg<i, n, T> { typedef T type; };

template<typename T> T& _forward(T& p) { return p; }

template<typename T>
csp::owned_pointer<T> _forward(std::unique_ptr<T>& u)
{
  return { std::move(u) };
}

template<typename T>
struct func_signature;

template<typename R, typename... Args>
struct func_signature<R(Args...)>
{
  typedef R result;

  constexpr static unsigned int number_of_args = sizeof...(Args);
  template<const int i> using arg = typename nth_arg<i, 0, Args...>::type;
};

template<typename R>
struct func_signature<R()>
{
  typedef R result;

  constexpr static unsigned int number_of_args = 0;
  template<const int i> using arg = void;
};

template<typename T>
struct type_info
{
  typedef T type;
  static const bool is_unique = false;
};

template<typename T>
struct type_info<std::unique_ptr<T>>
{
  typedef T type;
  static const bool is_unique = true;
};

template<typename T>
struct type_info<const std::unique_ptr<T>>
{
  typedef T type;
  static const bool is_unique = true;
};

template<typename T, const bool swap>
struct mock_func_param_deduction
{
  static const unsigned int number_of_args = func_signature<T>::number_of_args;

  using result = typename std::conditional
  <
    type_info<typename func_signature<T>::result>::is_unique and swap,
    csp::owned_pointer<
      typename type_info<typename func_signature<T>::result>::type
    >,
    typename func_signature<T>::result
  >::type;

  template<const int i>
  using arg = typename std::conditional
  <
    type_info<typename func_signature<T>::template arg<i>>::is_unique and swap,
    csp::owned_pointer<
      typename type_info<typename func_signature<T>::template arg<i>>::type
    >,
    typename func_signature<T>::template arg<i>
  >::type;
};

} // namespace pobu_gmock

template<typename T> using s  = pobu_gmock::mock_func_param_deduction<T, true>;
template<typename T> using r = pobu_gmock::mock_func_param_deduction<T, false>;

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD0(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 0, "Not proper arguments number");\
r<signature>::result name() override \
{ \
  return static_cast<r<signature>::result>(_ ## name()); \
}\
public:\
  MOCK_METHOD0(_ ## name, s<signature>::result())

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD0(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 0, "Not proper arguments number");\
r<signature>::result name() const override \
{ \
  return static_cast<r<signature>::result>(_ ## name()); \
}\
public:\
  MOCK_CONST_METHOD0(_ ## name, s<signature>::result())

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD1(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 1, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1) override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1))); \
}\
\
public:\
  MOCK_METHOD1(_ ## name, s<signature>::result(s<signature>::arg<0>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD1(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 1, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1) const override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1))); \
}\
\
public:\
  MOCK_CONST_METHOD1(_ ## name, s<signature>::result(s<signature>::arg<0>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD2(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 2, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                          r<signature>::arg<1> a2) override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2))); \
}\
\
public:\
  MOCK_METHOD2(_ ## name, s<signature>::result(s<signature>::arg<0>, s<signature>::arg<1>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD2(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 2, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2) const override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2))); \
}\
\
public:\
  MOCK_CONST_METHOD2(_ ## name, s<signature>::result(s<signature>::arg<0>, s<signature>::arg<1>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD3(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 3, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3) override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2), \
                                                     pobu_gmock::_forward(a3))); \
}\
\
public:\
  MOCK_METHOD3(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                               s<signature>::arg<1>, \
                                               s<signature>::arg<2>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD3(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 3, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                          r<signature>::arg<1> a2, \
                          r<signature>::arg<2> a3) const override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2), \
                                                     pobu_gmock::_forward(a3))); \
}\
\
public:\
  MOCK_CONST_METHOD3(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                     s<signature>::arg<1>, \
                                                     s<signature>::arg<2>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD4(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 4, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                          r<signature>::arg<1> a2, \
                          r<signature>::arg<2> a3, \
                          r<signature>::arg<3> a4) override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2), \
                                                     pobu_gmock::_forward(a3), \
                                                     pobu_gmock::_forward(a4))); \
}\
\
public:\
  MOCK_METHOD4(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                               s<signature>::arg<1>, \
                                               s<signature>::arg<2>, \
                                               s<signature>::arg<3>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD4(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 4, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                          r<signature>::arg<1> a2, \
                          r<signature>::arg<2> a3, \
                          r<signature>::arg<3> a4) const override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4))); \
}\
\
public:\
  MOCK_CONST_METHOD4(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                     s<signature>::arg<1>, \
                                                     s<signature>::arg<2>, \
                                                     s<signature>::arg<3>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD5(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 5, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5))); \
}\
\
public:\
    MOCK_METHOD5(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                 s<signature>::arg<1>, \
                                                 s<signature>::arg<2>, \
                                                 s<signature>::arg<3>, \
                                                 s<signature>::arg<4>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD5(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 5, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5) const override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5))); \
}\
\
public:\
    MOCK_CONST_METHOD5(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                       s<signature>::arg<1>, \
                                                       s<signature>::arg<2>, \
                                                       s<signature>::arg<3>, \
                                                       s<signature>::arg<4>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD6(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 6, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6))); \
}\
\
public:\
    MOCK_METHOD6(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                 s<signature>::arg<1>, \
                                                 s<signature>::arg<2>, \
                                                 s<signature>::arg<3>, \
                                                 s<signature>::arg<4>, \
                                                 s<signature>::arg<5>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD6(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 6, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6) const override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6))); \
}\
\
public:\
MOCK_CONST_METHOD6(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                   s<signature>::arg<1>, \
                                                   s<signature>::arg<2>, \
                                                   s<signature>::arg<3>, \
                                                   s<signature>::arg<4>, \
                                                   s<signature>::arg<5>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD7(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 7, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7))); \
}\
\
public:\
    MOCK_METHOD7(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                 s<signature>::arg<1>, \
                                                 s<signature>::arg<2>, \
                                                 s<signature>::arg<3>, \
                                                 s<signature>::arg<4>, \
                                                 s<signature>::arg<5>, \
                                                 s<signature>::arg<6>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD7(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 7, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7) const override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7))); \
}\
\
public:\
    MOCK_CONST_METHOD7(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                       s<signature>::arg<1>, \
                                                       s<signature>::arg<2>, \
                                                       s<signature>::arg<3>, \
                                                       s<signature>::arg<4>, \
                                                       s<signature>::arg<5>, \
                                                       s<signature>::arg<6>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD8(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 8, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7, \
                           r<signature>::arg<7> a8) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7), \
                                                        pobu_gmock::_forward(a8))); \
}\
\
public:\
    MOCK_METHOD8(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                 s<signature>::arg<1>, \
                                                 s<signature>::arg<2>, \
                                                 s<signature>::arg<3>, \
                                                 s<signature>::arg<4>, \
                                                 s<signature>::arg<5>, \
                                                 s<signature>::arg<6>, \
                                                 s<signature>::arg<7>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD8(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 8, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7, \
                           r<signature>::arg<7> a8) const override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7), \
                                                        pobu_gmock::_forward(a8))); \
}\
\
public:\
    MOCK_CONST_METHOD8(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                       s<signature>::arg<1>, \
                                                       s<signature>::arg<2>, \
                                                       s<signature>::arg<3>, \
                                                       s<signature>::arg<4>, \
                                                       s<signature>::arg<5>, \
                                                       s<signature>::arg<6>, \
                                                       s<signature>::arg<7>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD9(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 9, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7, \
                           r<signature>::arg<7> a8, \
                           r<signature>::arg<8> a9) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7), \
                                                        pobu_gmock::_forward(a8), \
                                                        pobu_gmock::_forward(a9))); \
}\
\
public:\
    MOCK_METHOD9(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                 s<signature>::arg<1>, \
                                                 s<signature>::arg<2>, \
                                                 s<signature>::arg<3>, \
                                                 s<signature>::arg<4>, \
                                                 s<signature>::arg<5>, \
                                                 s<signature>::arg<6>, \
                                                 s<signature>::arg<7>, \
                                                 s<signature>::arg<8>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD9(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 9, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7, \
                           r<signature>::arg<7> a8, \
                           r<signature>::arg<8> a9) const override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7), \
                                                        pobu_gmock::_forward(a8), \
                                                        pobu_gmock::_forward(a9))); \
}\
\
public:\
    MOCK_CONST_METHOD9(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                       s<signature>::arg<1>, \
                                                       s<signature>::arg<2>, \
                                                       s<signature>::arg<3>, \
                                                       s<signature>::arg<4>, \
                                                       s<signature>::arg<5>, \
                                                       s<signature>::arg<6>, \
                                                       s<signature>::arg<7>, \
                                                       s<signature>::arg<8>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_METHOD10(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 10, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1,\
                           r<signature>::arg<1> a2, \
                           r<signature>::arg<2> a3, \
                           r<signature>::arg<3> a4, \
                           r<signature>::arg<4> a5, \
                           r<signature>::arg<5> a6, \
                           r<signature>::arg<6> a7, \
                           r<signature>::arg<7> a8, \
                           r<signature>::arg<8> a9, \
                           r<signature>::arg<9> a10) override \
{ \
    return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                        pobu_gmock::_forward(a2), \
                                                        pobu_gmock::_forward(a3), \
                                                        pobu_gmock::_forward(a4), \
                                                        pobu_gmock::_forward(a5), \
                                                        pobu_gmock::_forward(a6), \
                                                        pobu_gmock::_forward(a7), \
                                                        pobu_gmock::_forward(a8), \
                                                        pobu_gmock::_forward(a9), \
                                                        pobu_gmock::_forward(a10))); \
}\
\
public:\
    MOCK_METHOD10(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                  s<signature>::arg<1>, \
                                                  s<signature>::arg<2>,\
                                                  s<signature>::arg<3>, \
                                                  s<signature>::arg<4>,\
                                                  s<signature>::arg<5>, \
                                                  s<signature>::arg<6>,\
                                                  s<signature>::arg<7>, \
                                                  s<signature>::arg<8>,\
                                                  s<signature>::arg<9>))

//-----------------------------------------------------------------------------------------------------------

#define MOCK_UNIQUE_CONST_METHOD10(name, signature) \
private:\
static_assert(r<signature>::number_of_args == 10, "Not proper arguments number");\
r<signature>::result name(r<signature>::arg<0> a1, \
                          r<signature>::arg<1> a2, \
                          r<signature>::arg<2> a3, \
                          r<signature>::arg<3> a4, \
                          r<signature>::arg<4> a5, \
                          r<signature>::arg<5> a6, \
                          r<signature>::arg<6> a7, \
                          r<signature>::arg<7> a8, \
                          r<signature>::arg<8> a9, \
                          r<signature>::arg<9> a10) const override \
{ \
  return static_cast<r<signature>::result>(_ ## name(pobu_gmock::_forward(a1), \
                                                     pobu_gmock::_forward(a2), \
                                                     pobu_gmock::_forward(a3), \
                                                     pobu_gmock::_forward(a4), \
                                                     pobu_gmock::_forward(a5), \
                                                     pobu_gmock::_forward(a6), \
                                                     pobu_gmock::_forward(a7), \
                                                     pobu_gmock::_forward(a8), \
                                                     pobu_gmock::_forward(a9), \
                                                     pobu_gmock::_forward(a10))); \
}\
\
public:\
  MOCK_CONST_METHOD10(_ ## name, s<signature>::result(s<signature>::arg<0>, \
                                                      s<signature>::arg<1>, \
                                                      s<signature>::arg<2>, \
                                                      s<signature>::arg<3>, \
                                                      s<signature>::arg<4>, \
                                                      s<signature>::arg<5>, \
                                                      s<signature>::arg<6>, \
                                                      s<signature>::arg<7>, \
                                                      s<signature>::arg<8>, \
                                                      s<signature>::arg<9>))

//-----------------------------------------------------------------------------------------------------------
