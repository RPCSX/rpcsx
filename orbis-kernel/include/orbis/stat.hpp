#pragma once

#include "orbis-config.hpp"
#include "time.hpp"

namespace orbis {
struct Stat {
  uint32_t dev;     // inode's device
  uint32_t ino;     // inode's number
  uint16_t mode;    // inode protection mode
  uint16_t nlink;   // number of hard links
  uint32_t uid;     // user ID of the file's owner
  uint32_t gid;     // group ID of the file's group
  uint32_t rdev;    // device type
  timespec atim;    // time of last access
  timespec mtim;    // time of last data modification
  timespec ctim;    // time of last file status change
  off_t size;       // file size, in bytes
  int64_t blocks;   // blocks allocated for file
  uint32_t blksize; // optimal blocksize for I/O
  uint32_t flags;   // user defined flags for file
  uint32_t gen;     // file generation number
  int32_t lspare;
  timespec birthtim; // time of file creation
};
} // namespace orbis
