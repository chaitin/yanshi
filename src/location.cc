#include "common.hh"
#include "location.hh"

#include <algorithm>
#include <cstdarg>
using namespace std;

LocationFile::LocationFile(const string& filename, const string& data) : filename(filename), data(data)
{
  long nlines = 1 + count(data.begin(), data.end(), '\n');
  linemap.assign(nlines+1, 0);
  long line = 1;
  for (long i = 0; i < data.size(); i++)
    if (data[i] == '\n')
      linemap[line++] = i+1;
  linemap[line] = data.size();
}

void LocationFile::locate(const Location& loc, const char* fmt, ...) const
{
  va_list va;
  va_start(va, fmt);
  long line1 = upper_bound(ALL(linemap), loc.start) - linemap.begin() - 1,
       line2 = upper_bound(ALL(linemap), max(loc.end-1, 0L)) - linemap.begin() - 1,
       col1 = loc.start - linemap[line1],
       col2 = loc.end - linemap[line2];
  if (line1 == line2) {
    fprintf(stderr, YELLOW "%s" CYAN ":%ld:%ld-%ld: " RED, filename.c_str(), line1+1, col1+1, col2);
    vfprintf(stderr, fmt, va);
    fputs("\n", stderr);
    fputs(SGR0, stderr);
    FOR(i, linemap[line1], line1+1 < linemap.size() ? linemap[line1+1] : data.size()) {
      if (i == loc.start)
        fputs(MAGENTA, stderr);
      fputc(data[i], stderr);
      if (i+1 == loc.end)
        fputs(SGR0, stderr);
    }
  } else {
    fprintf(stderr, YELLOW "%s" CYAN ":%ld-%ld:%ld-%ld: " RED, filename.c_str(), line1+1, line2+1, col1+1, col2);
    vfprintf(stderr, fmt, va);
    fputs("\n", stderr);
    fputs(SGR0, stderr);
    FOR(i, linemap[line1], linemap[line1+1]) {
      if (i == loc.start)
        fputs(MAGENTA, stderr);
      fputc(data[i], stderr);
    }
    if (line2-line1 < 8) {
      FOR(i, linemap[line1+1], linemap[line2-1])
        fputc(data[i], stderr);
    } else {
      FOR(i, linemap[line1+1], linemap[line1+4])
        fputc(data[i], stderr);
      fputs("........\n", stderr);
      FOR(i, linemap[line2-3], linemap[line2])
        fputc(data[i], stderr);
    }
    FOR(i, linemap[line2], line2+1 < linemap.size() ? linemap[line2+1] : data.size()) {
      fputc(data[i], stderr);
      if (i+1 == loc.end)
        fputs(SGR0, stderr);
    }
    fputs(SGR0, stderr);
  }
  va_end(va);
}
