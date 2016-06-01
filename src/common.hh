#pragma once
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <type_traits>
#include <vector>
using std::vector;

#define LEN_OF(x) (sizeof(x)/sizeof(*x))
#define ALL(x) (x).begin(), (x).end()
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (a); i < (b); i++)
#define ROF(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (b); --i >= (a); )

#define SGR0 "\x1b[m"
#define RED "\x1b[1;31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[1;33m"
#define BLUE "\x1b[1;34m"
#define MAGENTA "\x1b[1;35m"
#define CYAN "\x1b[36m"

const size_t BUF_SIZE = 512;

void output_error(bool use_err, const char *format, va_list ap);
void err_msg(const char *format, ...);
void err_exit(int exitno, const char *format, ...);

long get_long(const char *arg);

void log_generic(const char *prefix, const char *format, va_list ap);
void log_event(const char *format, ...);
void log_action(const char *format, ...);
void log_status(const char *format, ...);

extern long debug_level;
extern FILE* debug_file;
#ifdef DEBUG
# define DP(level, ...)  do {          \
    if (level <= debug_level) {        \
      fprintf(debug_file, "%s:%d:", __FILE__, __LINE__); \
      fprintf(debug_file, __VA_ARG__); \
      fprintf(debug_file, "\n");       \
      fflush(debug_file);              \
    }                                  \
  } while (0)
#else
# define DP(level, ...)
#endif

template<class T>
void sorted_insert(vector<T>& a, const T& x)
{
  a.emplace_back();
  auto it = a.end();
  for (; a.begin() != it && x < it[-1]; --it)
    *it = it[-1];
  *it = x;
}
