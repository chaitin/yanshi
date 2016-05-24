#pragma once
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <algorithm>
#include <cassert>
#include <string>
#include <type_traits>
#include <vector>
#include <map>
using namespace std;

#define REP(i, n) FOR(i, 0, n)
#define FOR(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (a); i < (b); i++)
#define ROF(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (b); --i >= (a); )
