#pragma once

namespace orbis {
enum class ErrorCode : int {
  PERM = 1,    // Operation not permitted
  NOENT = 2,   // No such file or directory
  SRCH = 3,    // No such process
  INTR = 4,    // Interrupted system call
  IO = 5,      // Input/output error
  NXIO = 6,    // Device not configured
  TOOBIG = 7,  // Argument list too long
  NOEXEC = 8,  // Exec format error
  BADF = 9,    // Bad file descriptor
  CHILD = 10,  // No child processes
  DEADLK = 11, // Resource deadlock avoided

  NOMEM = 12,  // Cannot allocate memory
  ACCES = 13,  // Permission denied
  FAULT = 14,  // Bad address
  NOTBLK = 15, // Block device required
  BUSY = 16,   // Device busy
  EXIST = 17,  // File exists
  XDEV = 18,   // Cross-device link
  NODEV = 19,  // Operation not supported by device
  NOTDIR = 20, // Not a directory
  ISDIR = 21,  // Is a directory
  INVAL = 22,  // Invalid argument
  NFILE = 23,  // Too many open files in system
  MFILE = 24,  // Too many open files
  NOTTY = 25,  // Inappropriate ioctl for device
  TXTBSY = 26, // Text file busy
  FBIG = 27,   // File too large
  NOSPC = 28,  // No space left on device
  SPIPE = 29,  // Illegal seek
  ROFS = 30,   // Read-only filesystem
  MLINK = 31,  // Too many links
  PIPE = 32,   // Broken pipe

  DOM = 33,   // Numerical argument out of domain
  RANGE = 34, // Result too large

  AGAIN = 35,         // Resource temporarily unavailable
  WOULDBLOCK = AGAIN, // Operation would block
  INPROGRESS = 36,    // Operation now in progress
  ALREADY = 37,       // Operation already in progress

  NOTSOCK = 38,        // Socket operation on non-socket
  DESTADDRREQ = 39,    // Destination address required
  MSGSIZE = 40,        // Message too long
  PROTOTYPE = 41,      // Protocol wrong type for socket
  NOPROTOOPT = 42,     // Protocol not available
  PROTONOSUPPORT = 43, // Protocol not supported
  SOCKTNOSUPPORT = 44, // Socket type not supported
  OPNOTSUPP = 45,      // Operation not supported
  NOTSUP = OPNOTSUPP,  // Operation not supported
  PFNOSUPPORT = 46,    // Protocol family not supported
  AFNOSUPPORT = 47,    // Address family not supported by protocol family
  ADDRINUSE = 48,      // Address already in use
  ADDRNOTAVAIL = 49,   // Can't assign requested address

  NETDOWN = 50,     // Network is down
  NETUNREACH = 51,  // Network is unreachable
  NETRESET = 52,    // Network dropped connection on reset
  CONNABORTED = 53, // Software caused connection abort
  CONNRESET = 54,   // Connection reset by peer
  NOBUFS = 55,      // No buffer space available
  ISCONN = 56,      // Socket is already connected
  NOTCONN = 57,     // Socket is not connected
  SHUTDOWN = 58,    // Can't send after socket shutdown
  TOOMANYREFS = 59, // Too many references: can't splice
  TIMEDOUT = 60,    // Operation timed out
  CONNREFUSED = 61, // Connection refused

  LOOP = 62,         // Too many levels of symbolic links
  NAMETOOLONG = 63,  // File name too long
  HOSTDOWN = 64,     // Host is down
  HOSTUNREACH = 65,  // No route to host
  NOTEMPTY = 66,     // Directory not empty
  PROCLIM = 67,      // Too many processes
  USERS = 68,        // Too many users
  DQUOT = 69,        // Disc quota exceeded
  STALE = 70,        // Stale NFS file handle
  REMOTE = 71,       // Too many levels of remote in path
  BADRPC = 72,       // RPC struct is bad
  RPCMISMATCH = 73,  // RPC version wrong
  PROGUNAVAIL = 74,  // RPC prog. not avail
  PROGMISMATCH = 75, // Program version wrong
  PROCUNAVAIL = 76,  // Bad procedure for program
  NOLCK = 77,        // No locks available
  NOSYS = 78,        // Function not implemented
  FTYPE = 79,        // Inappropriate file type or format
  AUTH = 80,         // Authentication error
  NEEDAUTH = 81,     // Need authenticator
  IDRM = 82,         // Identifier removed
  NOMSG = 83,        // No message of desired type
  OVERFLOW = 84,     // Value too large to be stored in data type
  CANCELED = 85,     // Operation canceled
  ILSEQ = 86,        // Illegal byte sequence
  NOATTR = 87,       // Attribute not found

  DOOFUS = 88, // Programming error

  BADMSG = 89,   // Bad message
  MULTIHOP = 90, // Multihop attempted
  NOLINK = 91,   // Link has been severed
  PROTO = 92,    // Protocol error

  NOTCAPABLE = 93, // Capabilities insufficient
  CAPMODE = 94,    // Not permitted in capability mode
};
} // namespace orbis
