#pragma once
#include "common.hh"

#include <string>
#include <utility>
#include <vector>

struct Location { long start, end; };

struct LocationFile {
  std::string filename, data;
  std::vector<long> linemap;

  LocationFile() = default;
  LocationFile(const std::string& filename, const std::string& data);
  LocationFile& operator=(const LocationFile&) = default;
  void context(const Location& loc) const;
  void locate(const Location& loc, long& line1, long& col1, long& line2, long& col2) const;
  void error(const Location& loc, const char* fmt, ...) const;
  template<class... Args>
  void error_context(const Location& loc, const char* fmt, Args&&... args) const {
    error(loc, fmt, std::forward<Args>(args)...);
    context(loc);
  }
};
