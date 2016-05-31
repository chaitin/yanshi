#pragma once
#include "common.hh"

#include <string>
#include <vector>

struct Location { long start, end; };

struct LocationFile {
  std::string filename, data;
  std::vector<long> linemap;

  LocationFile() = default;
  LocationFile(const std::string& filename, const std::string& data);
  LocationFile& operator=(const LocationFile&) = default;
  void locate(const Location& loc, const char* fmt, ...) const;
};
