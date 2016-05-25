#pragma once
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <cassert>
#include <type_traits>

#define LEN_OF(x) (sizeof(x)/sizeof(*x))
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (a); i < (b); i++)
#define ROF(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (b); --i >= (a); )
