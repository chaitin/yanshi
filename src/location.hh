#pragma once
#include "common.hh"

#include <string>
#include <vector>

struct Location { long start, end; };

struct LocationFile {
  std::string filename, data;
  std::vector<long> linemap;

  LocationFile(const std::string& filename, const std::string& data);
  void locate(const Location& loc, const char* fmt, ...) const;
};
