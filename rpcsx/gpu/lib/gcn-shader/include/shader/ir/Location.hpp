#pragma once
#include "PrintableWrapper.hpp"
#include <cstdint>
#include <string>

namespace shader::ir {
struct LocationImpl;
struct CloneMap;
class Context;

template <typename ImplT> struct LocationWrapper : PrintableWrapper<ImplT> {
  using PrintableWrapper<ImplT>::PrintableWrapper;
  using PrintableWrapper<ImplT>::operator=;
};

using Location = LocationWrapper<LocationImpl>;

struct LocationImpl {
  virtual ~LocationImpl() {}
  virtual void print(std::ostream &os) = 0;
  virtual std::strong_ordering compare(const LocationImpl &other) const = 0;

  virtual Location clone(Context &context) const = 0;
  auto operator<=>(const LocationImpl &other) const { return compare(other); }
};

struct PathLocationImpl final : LocationImpl {
  struct Data {
    std::string path;
    auto operator<=>(const Data &other) const = default;
  } data;

  PathLocationImpl(std::string path) : data{.path = std::move(path)} {}

  void print(std::ostream &os) override { os << data.path; }

  std::strong_ordering compare(const LocationImpl &other) const override {
    if (this == &other) {
      return std::strong_ordering::equal;
    }

    if (auto p = dynamic_cast<const PathLocationImpl *>(&other)) {
      return this->data <=> p->data;
    }

    return this <=> &other;
  }

  Location clone(Context &context) const override;
};

struct PathLocation : LocationWrapper<PathLocationImpl> {
  using LocationWrapper::LocationWrapper;
  using LocationWrapper::operator=;
  const std::string &getPath() const { return impl->data.path; }
};

struct TextFileLocationImpl final : LocationImpl {
  struct Data {
    PathLocation file;
    std::uint64_t line;
    std::uint64_t column;
    auto operator<=>(const Data &other) const = default;

  } data;

  TextFileLocationImpl(PathLocation file, std::uint64_t line,
                       std::uint64_t column)
      : data{.file = file, .line = line, .column = column} {}

  void print(std::ostream &os) override {
    data.file.print(os);
    os << ':' << data.line << ':' << data.column;
  }

  auto operator<=>(const TextFileLocationImpl &other) const = default;
  std::strong_ordering compare(const LocationImpl &other) const override {
    if (this == &other) {
      return std::strong_ordering::equal;
    }

    if (auto p = dynamic_cast<const TextFileLocationImpl *>(&other)) {
      return *this <=> *p;
    }

    return this <=> &other;
  }

  Location clone(Context &context) const override;
};

struct TextFileLocation : LocationWrapper<TextFileLocationImpl> {
  using LocationWrapper::LocationWrapper;
  using LocationWrapper::operator=;
  PathLocation getFile() const { return impl->data.file; }
  std::uint64_t getLine() const { return impl->data.line; }
  std::uint64_t getColumn() const { return impl->data.column; }
};

struct OffsetLocationData {
  Location baseLocation;
  std::uint64_t offset;

  OffsetLocationData(Location baseLocation, std::uint64_t offset)
      : baseLocation(baseLocation), offset(offset) {}

  auto operator<=>(const OffsetLocationData &other) const = default;
};

struct OffsetLocationImpl final : OffsetLocationData, LocationImpl {
  OffsetLocationImpl(Location file, std::uint64_t offset)
      : OffsetLocationData(file, offset) {}

  void print(std::ostream &os) override {
    baseLocation.print(os);
    os << '+' << offset;
  }

  std::strong_ordering compare(const LocationImpl &other) const override {
    if (this == &other) {
      return std::strong_ordering::equal;
    }

    if (auto p = dynamic_cast<const OffsetLocationData *>(&other)) {
      return static_cast<const OffsetLocationData &>(*this) <=> *p;
    }

    return this <=> &other;
  }

  Location clone(Context &context) const override;
};

struct OffsetLocation : LocationWrapper<OffsetLocationImpl> {
  using LocationWrapper::LocationWrapper;
  using LocationWrapper::operator=;
  Location getBaseLocation() const { return impl->baseLocation; }
  std::uint64_t getOffset() const { return impl->offset; }
};

struct MemoryLocationImpl final : LocationImpl {
  struct Data {
    std::uint64_t address;
    std::uint64_t size;

    auto operator<=>(const Data &other) const = default;
  } data;

  MemoryLocationImpl(std::uint64_t address, std::uint64_t size)
      : data{.address = address, .size = size} {}

  void print(std::ostream &os) override {
    os << '(' << data.address << " - " << data.size << ')';
  }

  std::strong_ordering compare(const LocationImpl &other) const override {
    if (this == &other) {
      return std::strong_ordering::equal;
    }

    if (auto p = dynamic_cast<const MemoryLocationImpl *>(&other)) {
      return data <=> p->data;
    }

    return this <=> &other;
  }

  Location clone(Context &context) const override;
};

struct MemoryLocation : LocationWrapper<MemoryLocationImpl> {
  using LocationWrapper::LocationWrapper;
  using LocationWrapper::operator=;
  std::uint64_t getAddress() const { return impl->data.address; }
  std::uint64_t getSize() const { return impl->data.size; }
};

struct UnknownLocationImpl final : LocationImpl {
  void print(std::ostream &os) override { os << "unknown"; }

  std::strong_ordering compare(const LocationImpl &other) const override {
    if (this == &other) {
      return std::strong_ordering::equal;
    }

    if (dynamic_cast<const MemoryLocationImpl *>(&other)) {
      return std::strong_ordering::equal;
    }

    return this <=> &other;
  }

  Location clone(Context &context) const override;
};

struct UnknownLocation : LocationWrapper<UnknownLocationImpl> {
  using LocationWrapper::LocationWrapper;
  using LocationWrapper::operator=;
};
} // namespace shader::ir
