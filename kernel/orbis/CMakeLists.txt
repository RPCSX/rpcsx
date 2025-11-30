set(CMAKE_POSITION_INDEPENDENT_CODE on)

add_library(obj.orbis-kernel OBJECT
  src/blockpool.cpp
  src/dmem.cpp
  src/event.cpp
  src/evf.cpp
  src/fmem.cpp
  src/IoDevice.cpp
  src/ipmi.cpp
  src/KernelAllocator.cpp
  src/KernelContext.cpp
  src/module.cpp
  src/pipe.cpp
  src/pmem.cpp
  src/sysvec.cpp
  src/umtx.cpp
  src/vmem.cpp
  src/sys/sys_acct.cpp
  src/sys/sys_audit.cpp
  src/sys/sys_capability.cpp
  src/sys/sys_context.cpp
  src/sys/sys_cpuset.cpp
  src/sys/sys_descrip.cpp
  src/sys/sys_environment.cpp
  src/sys/sys_event.cpp
  src/sys/sys_exec.cpp
  src/sys/sys_exit.cpp
  src/sys/sys_fork.cpp
  src/sys/sys_generic.cpp
  src/sys/sys_jail.cpp
  src/sys/sys_ktrace.cpp
  src/sys/sys_linker.cpp
  src/sys/sys_loginclass.cpp
  src/sys/sys_mac.cpp
  src/sys/sys_module.cpp
  src/sys/sys_msg.cpp
  src/sys/sys_ntptime.cpp
  src/sys/sys_p1003_1b.cpp
  src/sys/sys_pipe.cpp
  src/sys/sys_procdesc.cpp
  src/sys/sys_process.cpp
  src/sys/sys_prot.cpp
  src/sys/sys_pty_pts.cpp
  src/sys/sys_rctl.cpp
  src/sys/sys_resource.cpp
  src/sys/sys_route.cpp
  src/sys/sys_sce.cpp
  src/sys/sys_sem.cpp
  src/sys/sys_shm.cpp
  src/sys/sys_shutdown.cpp
  src/sys/sys_sig.cpp
  src/sys/sys_subr_prof.cpp
  src/sys/sys_swap_pager.cpp
  src/sys/sys_synch.cpp
  src/sys/sys_sysctl.cpp
  src/sys/sys_thr.cpp
  src/sys/sys_time.cpp
  src/sys/sys_uipc_mqueue.cpp
  src/sys/sys_uipc_sem.cpp
  src/sys/sys_uipc_shm.cpp
  src/sys/sys_uipc.cpp
  src/sys/sys_umtx.cpp
  src/sys/sys_uuid.cpp
  src/sys/sys_vfs_acl.cpp
  src/sys/sys_vfs_aio.cpp
  src/sys/sys_vfs_cache.cpp
  src/sys/sys_vfs_extattr.cpp
  src/sys/sys_vfs_mount.cpp
  src/sys/sys_vfs.cpp
  src/sys/sys_vm_mmap.cpp
  src/sys/sys_vm_unix.cpp

  src/thread/Process.cpp
  src/thread/Thread.cpp

  src/utils/Logs.cpp
)

target_link_libraries(obj.orbis-kernel PUBLIC orbis::kernel::config rx kernel)

target_include_directories(obj.orbis-kernel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include

    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/orbis
)

add_library(orbis-kernel STATIC)
add_library(orbis-kernel-shared SHARED)
add_library(orbis::kernel ALIAS orbis-kernel)
add_library(orbis::kernel-shared ALIAS orbis-kernel-shared)

target_link_libraries(orbis-kernel PUBLIC obj.orbis-kernel)
target_link_libraries(orbis-kernel-shared PUBLIC obj.orbis-kernel)
