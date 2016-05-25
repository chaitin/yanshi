#include "common.hh"
#include "parser.hh"

#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <errno.h>
#include <execinfo.h>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <unistd.h>
#include <sysexits.h>
#include <sys/time.h>
using namespace std;

const size_t BUF_SIZE = 512;
#define SGR0 "\x1b[m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"

///// Error

static const char *ENAME[] = {
    /*   0 */ "",
    /*   1 */ "EPERM", "ENOENT", "ESRCH", "EINTR", "EIO", "ENXIO",
    /*   7 */ "E2BIG", "ENOEXEC", "EBADF", "ECHILD",
    /*  11 */ "EAGAIN/EWOULDBLOCK", "ENOMEM", "EACCES", "EFAULT",
    /*  15 */ "ENOTBLK", "EBUSY", "EEXIST", "EXDEV", "ENODEV",
    /*  20 */ "ENOTDIR", "EISDIR", "EINVAL", "ENFILE", "EMFILE",
    /*  25 */ "ENOTTY", "ETXTBSY", "EFBIG", "ENOSPC", "ESPIPE",
    /*  30 */ "EROFS", "EMLINK", "EPIPE", "EDOM", "ERANGE",
    /*  35 */ "EDEADLK/EDEADLOCK", "ENAMETOOLONG", "ENOLCK", "ENOSYS",
    /*  39 */ "ENOTEMPTY", "ELOOP", "", "ENOMSG", "EIDRM", "ECHRNG",
    /*  45 */ "EL2NSYNC", "EL3HLT", "EL3RST", "ELNRNG", "EUNATCH",
    /*  50 */ "ENOCSI", "EL2HLT", "EBADE", "EBADR", "EXFULL", "ENOANO",
    /*  56 */ "EBADRQC", "EBADSLT", "", "EBFONT", "ENOSTR", "ENODATA",
    /*  62 */ "ETIME", "ENOSR", "ENONET", "ENOPKG", "EREMOTE",
    /*  67 */ "ENOLINK", "EADV", "ESRMNT", "ECOMM", "EPROTO",
    /*  72 */ "EMULTIHOP", "EDOTDOT", "EBADMSG", "EOVERFLOW",
    /*  76 */ "ENOTUNIQ", "EBADFD", "EREMCHG", "ELIBACC", "ELIBBAD",
    /*  81 */ "ELIBSCN", "ELIBMAX", "ELIBEXEC", "EILSEQ", "ERESTART",
    /*  86 */ "ESTRPIPE", "EUSERS", "ENOTSOCK", "EDESTADDRREQ",
    /*  90 */ "EMSGSIZE", "EPROTOTYPE", "ENOPROTOOPT",
    /*  93 */ "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
    /*  95 */ "EOPNOTSUPP/ENOTSUP", "EPFNOSUPPORT", "EAFNOSUPPORT",
    /*  98 */ "EADDRINUSE", "EADDRNOTAVAIL", "ENETDOWN", "ENETUNREACH",
    /* 102 */ "ENETRESET", "ECONNABORTED", "ECONNRESET", "ENOBUFS",
    /* 106 */ "EISCONN", "ENOTCONN", "ESHUTDOWN", "ETOOMANYREFS",
    /* 110 */ "ETIMEDOUT", "ECONNREFUSED", "EHOSTDOWN", "EHOSTUNREACH",
    /* 114 */ "EALREADY", "EINPROGRESS", "ESTALE", "EUCLEAN",
    /* 118 */ "ENOTNAM", "ENAVAIL", "EISNAM", "EREMOTEIO", "EDQUOT",
    /* 123 */ "ENOMEDIUM", "EMEDIUMTYPE", "ECANCELED", "ENOKEY",
    /* 127 */ "EKEYEXPIRED", "EKEYREVOKED", "EKEYREJECTED",
    /* 130 */ "EOWNERDEAD", "ENOTRECOVERABLE", "ERFKILL", "EHWPOISON"
};

#define MAX_ENAME 133

void output_error(bool use_err, const char *format, va_list ap)
{
  char text[BUF_SIZE], msg[BUF_SIZE], buf[BUF_SIZE];
  vsnprintf(msg, BUF_SIZE, format, ap);
  if (use_err)
    snprintf(text, BUF_SIZE, "[%s %s] ", 0 < errno && errno < MAX_ENAME ? ENAME[errno] : "?UNKNOWN?", strerror(errno));
  else
    strcpy(text, "");
  snprintf(buf, BUF_SIZE, RED "%s%s\n", text, msg);
  fputs(buf, stderr);
  fputs(SGR0, stderr);
  fflush(stderr);
}

void err_msg(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int saved = errno;
  output_error(errno > 0, format, ap);
  errno = saved;
  va_end(ap);
}
#define err_msg_g(...) ({err_msg(__VA_ARGS__); goto quit;})

void err_exit(int exitno, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  int saved = errno;
  output_error(errno > 0, format, ap);
  errno = saved;
  va_end(ap);

  void *bt[99];
  char buf[1024];
  int nptrs = backtrace(bt, LEN_OF(buf));
  int i = sprintf(buf, "addr2line -Cfipe %s", program_invocation_name), j = 0;
  while (j < nptrs && i+30 < sizeof buf)
    i += sprintf(buf+i, " %#x", bt[j++]);
  strcat(buf, ">&2");
  fputs("\n", stderr);
  system(buf);
  //backtrace_symbols_fd(buf, nptrs, STDERR_FILENO);
  exit(exitno);
}


long get_long(const char *arg)
{
  char *end;
  errno = 0;
  long ret = strtol(arg, &end, 0);
  if (errno)
    err_exit(EX_USAGE, "get_long: %s", arg);
  if (*end)
    err_exit(EX_USAGE, "get_long: nonnumeric character");
  return ret;
}

void print_help(FILE *fh)
{
  fprintf(fh, "Usage: %s [OPTIONS] dir\n", program_invocation_short_name);
  fputs(
        "\n"
        "Options:\n"
        "  -h, --help                display this help and exit\n"
        "\n"
        "Examples:\n"
        , fh);
  exit(fh == stdout ? 0 : EX_USAGE);
}

///// log

void log_generic(const char *prefix, const char *format, va_list ap)
{
  char buf[BUF_SIZE];
  timeval tv;
  tm tm;
  gettimeofday(&tv, NULL);
  fputs(prefix, stdout);
  if (localtime_r(&tv.tv_sec, &tm)) {
    strftime(buf, sizeof buf, "%T.%%06u ", &tm);
    printf(buf, tv.tv_usec);
  }
  vprintf(format, ap);
  fputs(SGR0, stdout);
  fflush(stdout);
}

void log_event(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  log_generic(CYAN, format, ap);
  va_end(ap);
}

void log_action(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  log_generic(GREEN, format, ap);
  va_end(ap);
}

void log_status(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  log_generic(YELLOW, format, ap);
  va_end(ap);
}

int main(int argc, char *argv[])
{
  int opt;
  static struct option long_options[] = {
    {"help",                no_argument,       0,   'h'},
    {"output",              required_argument, 0,   'O'},
    {0,                     0,                 0,   0},
  };

  char* opt_output_filename = NULL;

  while ((opt = getopt_long(argc, argv, "Dho:", long_options, NULL)) != -1) {
    switch (opt) {
    case 'D':
      break;
    case 'h':
      print_help(stdout);
      break;
    case 'o':
      opt_output_filename = optarg;
      break;
    case '?':
      print_help(stderr);
      break;
    }
  }
  argc -= optind;
  argv += optind;
  FILE* file;
  if (! argc)
    file = stdin;
  else if (argc > 1)
    print_help(stderr);
  else
    file = fopen(argv[0], "r");
  if (! file)
    err_exit(EX_OSFILE, "fopen '%s'", ! argc ? "stdin" : argv[0]);

  long r;
  char buf[BUF_SIZE];
  string data;
  while ((r = fread(buf, 1, sizeof buf, file)) > 0)
    data += string(buf, buf+r);
  LocationFile locfile("-", data);
  Stmt* toplevel = NULL;
  int errors = parse(locfile, toplevel);
  if (! errors) {
    StmtPrinter visitor;
    for (Stmt* x = toplevel; x; x = x->next)
      visitor.visit(*x);
    stmt_free(toplevel);
  }

  fclose(file);
}
