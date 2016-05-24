#pragma once
#include "common.hh"

#include <cstdarg>

struct Location { long start, end; };

struct LocationFile {
  string filename;
  string data;
  vector<long> linemap;

  LocationFile(const string& filename, const string& data) : filename(filename), data(data) {
    long nlines = 1 + count(data.begin(), data.end(), '\n');
    linemap.assign(nlines, 0);
    long line = 1;
    for (long i = 0; i < data.size(); i++)
      if (data[i] == '\n')
        linemap[line++] = i+1;
    linemap[nlines] = data.size();
  }

  void locate(const Location& loc, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    long line = 0;
    while (linemap[line+1] <= loc.start)
      line++;
    fprintf(stderr, "line %ld:%ld,%ld\n", line+1, loc.start-linemap[line]+1, loc.end-linemap[line]);
    va_end(va);
  }
};
