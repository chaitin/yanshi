#pragma once
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <type_traits>

#define LEN_OF(x) (sizeof(x)/sizeof(*x))
#define ALL(x) (x).begin(), (x).end()
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (a); i < (b); i++)
#define ROF(i, a, b) for (typename std::remove_cv<typename std::remove_reference<decltype(b)>::type>::type i = (b); --i >= (a); )

const size_t BUF_SIZE = 512;

void output_error(bool use_err, const char *format, va_list ap);
void err_msg(const char *format, ...);
void err_exit(int exitno, const char *format, ...);

long get_long(const char *arg);

void log_generic(const char *prefix, const char *format, va_list ap);
void log_event(const char *format, ...);
void log_action(const char *format, ...);
void log_status(const char *format, ...);
