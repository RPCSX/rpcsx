#pragma once

#include "error/ErrorCode.hpp" // IWYU pragma: export
#include "error/SysResult.hpp" // IWYU pragma: export
#include <system_error>

namespace orbis {
static orbis::ErrorCode toErrorCode(std::errc errc) {
  if (errc == std::errc{}) {
    return {};
  }

  switch (errc) {
  case std::errc::address_family_not_supported:
    return orbis::ErrorCode::AFNOSUPPORT;
  case std::errc::address_in_use:
    return orbis::ErrorCode::ADDRINUSE;
  case std::errc::address_not_available:
    return orbis::ErrorCode::ADDRNOTAVAIL;
  case std::errc::already_connected:
    return orbis::ErrorCode::ISCONN;
  case std::errc::argument_out_of_domain:
    return orbis::ErrorCode::DOM;
  case std::errc::bad_address:
    return orbis::ErrorCode::FAULT;
  case std::errc::bad_file_descriptor:
    return orbis::ErrorCode::BADF;
  case std::errc::bad_message:
    return orbis::ErrorCode::BADMSG;
  case std::errc::broken_pipe:
    return orbis::ErrorCode::PIPE;
  case std::errc::connection_aborted:
    return orbis::ErrorCode::CONNABORTED;
  case std::errc::connection_already_in_progress:
    return orbis::ErrorCode::ALREADY;
  case std::errc::connection_refused:
    return orbis::ErrorCode::CONNREFUSED;
  case std::errc::connection_reset:
    return orbis::ErrorCode::CONNRESET;
  case std::errc::cross_device_link:
    return orbis::ErrorCode::XDEV;
  case std::errc::destination_address_required:
    return orbis::ErrorCode::DESTADDRREQ;
  case std::errc::device_or_resource_busy:
    return orbis::ErrorCode::BUSY;
  case std::errc::directory_not_empty:
    return orbis::ErrorCode::NOTEMPTY;
  case std::errc::executable_format_error:
    return orbis::ErrorCode::NOEXEC;
  case std::errc::file_exists:
    return orbis::ErrorCode::EXIST;
  case std::errc::file_too_large:
    return orbis::ErrorCode::FBIG;
  case std::errc::filename_too_long:
    return orbis::ErrorCode::NAMETOOLONG;
  case std::errc::function_not_supported:
    return orbis::ErrorCode::NOSYS;
  case std::errc::host_unreachable:
    return orbis::ErrorCode::HOSTUNREACH;
  case std::errc::identifier_removed:
    return orbis::ErrorCode::IDRM;
  case std::errc::illegal_byte_sequence:
    return orbis::ErrorCode::ILSEQ;
  case std::errc::inappropriate_io_control_operation:
    return orbis::ErrorCode::NOTTY;
  case std::errc::interrupted:
    return orbis::ErrorCode::INTR;
  case std::errc::invalid_argument:
    return orbis::ErrorCode::INVAL;
  case std::errc::invalid_seek:
    return orbis::ErrorCode::SPIPE;
  case std::errc::io_error:
    return orbis::ErrorCode::IO;
  case std::errc::is_a_directory:
    return orbis::ErrorCode::ISDIR;
  case std::errc::message_size:
    return orbis::ErrorCode::MSGSIZE;
  case std::errc::network_down:
    return orbis::ErrorCode::NETDOWN;
  case std::errc::network_reset:
    return orbis::ErrorCode::NETRESET;
  case std::errc::network_unreachable:
    return orbis::ErrorCode::NETUNREACH;
  case std::errc::no_buffer_space:
    return orbis::ErrorCode::NOBUFS;
  case std::errc::no_child_process:
    return orbis::ErrorCode::CHILD;
  case std::errc::no_link:
    return orbis::ErrorCode::NOLINK;
  case std::errc::no_lock_available:
    return orbis::ErrorCode::NOLCK;
  case std::errc::no_message:
    return orbis::ErrorCode::NOMSG;
  case std::errc::no_protocol_option:
    return orbis::ErrorCode::NOPROTOOPT;
  case std::errc::no_space_on_device:
    return orbis::ErrorCode::NOSPC;
  case std::errc::no_such_device_or_address:
    return orbis::ErrorCode::NXIO;
  case std::errc::no_such_device:
    return orbis::ErrorCode::NODEV;
  case std::errc::no_such_file_or_directory:
    return orbis::ErrorCode::NOENT;
  case std::errc::no_such_process:
    return orbis::ErrorCode::SRCH;
  case std::errc::not_a_directory:
    return orbis::ErrorCode::NOTDIR;
  case std::errc::not_a_socket:
    return orbis::ErrorCode::NOTSOCK;
  case std::errc::not_connected:
    return orbis::ErrorCode::NOTCONN;
  case std::errc::not_enough_memory:
    return orbis::ErrorCode::NOMEM;
  case std::errc::not_supported:
    return orbis::ErrorCode::NOTSUP;
  case std::errc::operation_canceled:
    return orbis::ErrorCode::CANCELED;
  case std::errc::operation_in_progress:
    return orbis::ErrorCode::INPROGRESS;
  case std::errc::operation_not_permitted:
    return orbis::ErrorCode::PERM;
  case std::errc::operation_would_block:
    return orbis::ErrorCode::WOULDBLOCK;
  case std::errc::permission_denied:
    return orbis::ErrorCode::ACCES;
  case std::errc::protocol_error:
    return orbis::ErrorCode::PROTO;
  case std::errc::protocol_not_supported:
    return orbis::ErrorCode::PROTONOSUPPORT;
  case std::errc::read_only_file_system:
    return orbis::ErrorCode::ROFS;
  case std::errc::resource_deadlock_would_occur:
    return orbis::ErrorCode::DEADLK;
  case std::errc::result_out_of_range:
    return orbis::ErrorCode::RANGE;
  case std::errc::text_file_busy:
    return orbis::ErrorCode::TXTBSY;
  case std::errc::timed_out:
    return orbis::ErrorCode::TIMEDOUT;
  case std::errc::too_many_files_open_in_system:
    return orbis::ErrorCode::NFILE;
  case std::errc::too_many_files_open:
    return orbis::ErrorCode::MFILE;
  case std::errc::too_many_links:
    return orbis::ErrorCode::MLINK;
  case std::errc::too_many_symbolic_link_levels:
    return orbis::ErrorCode::LOOP;
  case std::errc::value_too_large:
    return orbis::ErrorCode::OVERFLOW;
  case std::errc::wrong_protocol_type:
    return orbis::ErrorCode::PROTOTYPE;
  default:
    return orbis::ErrorCode::FAULT;
  }
}

inline constexpr orbis::ErrorCode toErrorCode(const std::error_code &code) {
  if (!code) {
    return {};
  }
  if (code.category() != std::generic_category()) {
    return orbis::ErrorCode::DOOFUS;
  }
  return toErrorCode(static_cast<std::errc>(code.value()));
}
} // namespace orbis
