#include <array>
#include <orbis/error.hpp>
#include <orbis/thread.hpp>

namespace orbis {
using acl_type_t = sint;
using key_t = sint;
using semid_t = uint64_t;
using cpusetid_t = sint;
using cpuwhich_t = sint;
using cpulevel_t = sint;
using SceKernelModule = ModuleHandle;

struct ModuleInfo;
struct ModuleInfoEx;
struct KEvent;
struct timespec;
struct Stat;
struct stack_t;

SysResult nosys(Thread *thread);

SysResult sys_exit(Thread *thread, sint status);
SysResult sys_fork(Thread *thread);
SysResult sys_read(Thread *thread, sint fd, ptr<void> buf, size_t nbyte);
SysResult sys_write(Thread *thread, sint fd, ptr<const void> buf, size_t nbyte);
SysResult sys_open(Thread *thread, ptr<char> path, sint flags, sint mode);
SysResult sys_close(Thread *thread, sint fd);
SysResult sys_wait4(Thread *thread, sint pid, ptr<sint> status, sint options,
                    ptr<struct rusage> rusage);
SysResult sys_link(Thread *thread, ptr<char> path, ptr<char> link);
SysResult sys_unlink(Thread *thread, ptr<char> path);
SysResult sys_chdir(Thread *thread, ptr<char> path);
SysResult sys_fchdir(Thread *thread, sint fd);
SysResult sys_mknod(Thread *thread, ptr<char> path, sint mode, sint dev);
SysResult sys_chmod(Thread *thread, ptr<char> path, sint mode);
SysResult sys_chown(Thread *thread, ptr<char> path, sint uid, sint gid);
SysResult sys_obreak(Thread *thread, ptr<char> nsize);
SysResult sys_getpid(Thread *thread);
SysResult sys_mount(Thread *thread, ptr<char> type, ptr<char> path, sint flags,
                    caddr_t data);
SysResult sys_unmount(Thread *thread, ptr<char> path, sint flags);
SysResult sys_setuid(Thread *thread, uid_t uid);
SysResult sys_getuid(Thread *thread);
SysResult sys_geteuid(Thread *thread);
SysResult sys_ptrace(Thread *thread, sint req, pid_t pid, caddr_t addr,
                     sint data);
SysResult sys_recvmsg(Thread *thread, sint s, ptr<struct msghdr> msg,
                      sint flags);
SysResult sys_sendmsg(Thread *thread, sint s, ptr<struct msghdr> msg,
                      sint flags);
SysResult sys_recvfrom(Thread *thread, sint s, caddr_t buf, size_t len,
                       sint flags, ptr<struct sockaddr> from,
                       ptr<uint32_t> fromlenaddr);
SysResult sys_accept(Thread *thread, sint s, ptr<struct sockaddr> from,
                     ptr<uint32_t> fromlenaddr);
SysResult sys_getpeername(Thread *thread, sint fdes, ptr<struct sockaddr> asa,
                          ptr<uint32_t> alen);
SysResult sys_getsockname(Thread *thread, sint fdes, ptr<struct sockaddr> asa,
                          ptr<uint32_t> alen);
SysResult sys_access(Thread *thread, ptr<char> path, sint flags);
SysResult sys_chflags(Thread *thread, ptr<char> path, sint flags);
SysResult sys_fchflags(Thread *thread, sint fd, sint flags);
SysResult sys_sync(Thread *thread);
SysResult sys_kill(Thread *thread, sint pid, sint signum);
SysResult sys_getppid(Thread *thread);
SysResult sys_dup(Thread *thread, uint fd);
SysResult sys_pipe(Thread *thread);
SysResult sys_getegid(Thread *thread);
SysResult sys_profil(Thread *thread, caddr_t samples, size_t size,
                     size_t offset, uint scale);
SysResult sys_ktrace(Thread *thread, ptr<const char> fname, sint ops, sint facs,
                     sint pit);
SysResult sys_getgid(Thread *thread);
SysResult sys_getlogin(Thread *thread, ptr<char> namebuf, uint namelen);
SysResult sys_setlogin(Thread *thread, ptr<char> namebuf);
SysResult sys_acct(Thread *thread, ptr<char> path);
SysResult sys_sigaltstack(Thread *thread, ptr<stack_t> ss, ptr<stack_t> oss);
SysResult sys_ioctl(Thread *thread, sint fd, ulong com, caddr_t data);
SysResult sys_reboot(Thread *thread, sint opt);
SysResult sys_revoke(Thread *thread, ptr<char> path);
SysResult sys_symlink(Thread *thread, ptr<char> path, ptr<char> link);
SysResult sys_readlink(Thread *thread, ptr<char> path, ptr<char> buf,
                       size_t count);
SysResult sys_execve(Thread *thread, ptr<char> fname, ptr<ptr<char>> argv,
                     ptr<ptr<char>> envv);
SysResult sys_umask(Thread *thread, sint newmask);
SysResult sys_chroot(Thread *thread, ptr<char> path);
SysResult sys_msync(Thread *thread, ptr<void> addr, size_t len, sint flags);
SysResult sys_vfork(Thread *thread);
SysResult sys_sbrk(Thread *thread, sint incr);
SysResult sys_sstk(Thread *thread, sint incr);
SysResult sys_ovadvise(Thread *thread, sint anom);
SysResult sys_munmap(Thread *thread, ptr<void> addr, size_t len);
SysResult sys_mprotect(Thread *thread, ptr<const void> addr, size_t len,
                       sint prot);
SysResult sys_madvise(Thread *thread, ptr<void> addr, size_t len, sint behav);
SysResult sys_mincore(Thread *thread, ptr<const void> addr, size_t len,
                      ptr<char> vec);
SysResult sys_getgroups(Thread *thread, uint gidsetsize, ptr<gid_t> gidset);
SysResult sys_setgroups(Thread *thread, uint gidsetsize, ptr<gid_t> gidset);
SysResult sys_getpgrp(Thread *thread);
SysResult sys_setpgid(Thread *thread, sint pid, sint pgid);
SysResult sys_setitimer(Thread *thread, uint which, ptr<struct itimerval> itv,
                        ptr<struct itimerval> oitv);
SysResult sys_swapon(Thread *thread, ptr<char> name);
SysResult sys_getitimer(Thread *thread, uint which, ptr<struct itimerval> itv);
SysResult sys_getdtablesize(Thread *thread);
SysResult sys_dup2(Thread *thread, uint from, uint to);
SysResult sys_fcntl(Thread *thread, sint fd, sint cmd, slong arg);
SysResult sys_select(Thread *thread, sint nd, ptr<struct fd_set_t> in,
                     ptr<struct fd_set_t> out, ptr<struct fd_set_t> ex,
                     ptr<struct timeval> tv);
SysResult sys_fsync(Thread *thread, sint fd);
SysResult sys_setpriority(Thread *thread, sint which, sint who, sint prio);
SysResult sys_socket(Thread *thread, sint domain, sint type, sint protocol);
SysResult sys_connect(Thread *thread, sint s, caddr_t name, sint namelen);
SysResult sys_getpriority(Thread *thread, sint which, sint who);
SysResult sys_bind(Thread *thread, sint s, caddr_t name, sint namelen);
SysResult sys_setsockopt(Thread *thread, sint s, sint level, sint name,
                         caddr_t val, sint valsize);
SysResult sys_listen(Thread *thread, sint s, sint backlog);
SysResult sys_gettimeofday(Thread *thread, ptr<struct timeval> tp,
                           ptr<struct timezone> tzp);
SysResult sys_getrusage(Thread *thread, sint who, ptr<struct rusage> rusage);
SysResult sys_getsockopt(Thread *thread, sint s, sint level, sint name,
                         caddr_t val, ptr<sint> avalsize);
SysResult sys_readv(Thread *thread, sint fd, ptr<struct iovec> iovp,
                    uint iovcnt);
SysResult sys_writev(Thread *thread, sint fd, ptr<struct iovec> iovp,
                     uint iovcnt);
SysResult sys_settimeofday(Thread *thread, ptr<struct timeval> tp,
                           ptr<struct timezone> tzp);
SysResult sys_fchown(Thread *thread, sint fd, sint uid, sint gid);
SysResult sys_fchmod(Thread *thread, sint fd, sint mode);
SysResult sys_setreuid(Thread *thread, sint ruid, sint euid);
SysResult sys_setregid(Thread *thread, sint rgid, sint egid);
SysResult sys_rename(Thread *thread, ptr<char> from, ptr<char> to);
SysResult sys_flock(Thread *thread, sint fd, sint how);
SysResult sys_mkfifo(Thread *thread, ptr<char> path, sint mode);
SysResult sys_sendto(Thread *thread, sint s, caddr_t buf, size_t len,
                     sint flags, caddr_t to, sint tolen);
SysResult sys_shutdown(Thread *thread, sint s, sint how);
SysResult sys_socketpair(Thread *thread, sint domain, sint type, sint protocol,
                         ptr<sint> rsv);
SysResult sys_mkdir(Thread *thread, ptr<char> path, sint mode);
SysResult sys_rmdir(Thread *thread, ptr<char> path);
SysResult sys_utimes(Thread *thread, ptr<char> path, ptr<struct timeval> tptr);
SysResult sys_adjtime(Thread *thread, ptr<struct timeval> delta,
                      ptr<struct timeval> olddelta);
SysResult sys_setsid(Thread *thread);
SysResult sys_quotactl(Thread *thread, ptr<char> path, sint cmd, sint uid,
                       caddr_t arg);
SysResult sys_nlm_syscall(Thread *thread, sint debug_level, sint grace_period,
                          sint addr_count, ptr<ptr<char>> addrs);
SysResult sys_nfssvc(Thread *thread, sint flag, caddr_t argp);
SysResult sys_lgetfh(Thread *thread, ptr<char> fname, ptr<struct fhandle> fhp);
SysResult sys_getfh(Thread *thread, ptr<char> fname, ptr<struct fhandle> fhp);
SysResult sys_sysarch(Thread *thread, sint op, ptr<char> parms);
SysResult sys_rtprio(Thread *thread, sint function, pid_t pid,
                     ptr<struct rtprio> rtp);
SysResult sys_semsys(Thread *thread, sint which, sint a2, sint a3, sint a4,
                     sint a5);
SysResult sys_msgsys(Thread *thread, sint which, sint a2, sint a3, sint a4,
                     sint a5, sint a6);
SysResult sys_shmsys(Thread *thread, sint which, sint a2, sint a3, sint a4);
SysResult sys_freebsd6_pread(Thread *thread, sint fd, ptr<void> buf,
                             size_t nbyte, sint pad, off_t offset);
SysResult sys_freebsd6_pwrite(Thread *thread, sint fd, ptr<const void> buf,
                              size_t nbyte, sint pad, off_t offset);
SysResult sys_setfib(Thread *thread, sint fib);
SysResult sys_ntp_adjtime(Thread *thread, ptr<struct timex> tp);
SysResult sys_setgid(Thread *thread, gid_t gid);
SysResult sys_setegid(Thread *thread, gid_t egid);
SysResult sys_seteuid(Thread *thread, uid_t euid);
SysResult sys_stat(Thread *thread, ptr<char> path, ptr<Stat> ub);
SysResult sys_fstat(Thread *thread, sint fd, ptr<Stat> ub);
SysResult sys_lstat(Thread *thread, ptr<char> path, ptr<Stat> ub);
SysResult sys_pathconf(Thread *thread, ptr<char> path, sint name);
SysResult sys_fpathconf(Thread *thread, sint fd, sint name);
SysResult sys_getrlimit(Thread *thread, uint which, ptr<struct rlimit> rlp);
SysResult sys_setrlimit(Thread *thread, uint which, ptr<struct rlimit> rlp);
SysResult sys_getdirentries(Thread *thread, sint fd, ptr<char> buf, uint count,
                            ptr<slong> basep);
SysResult sys_freebsd6_mmap(Thread *thread, caddr_t addr, size_t len, sint prot,
                            sint flags, sint fd, sint pad, off_t pos);
SysResult sys_freebsd6_lseek(Thread *thread, sint fd, sint pad, off_t offset,
                             sint whence);
SysResult sys_freebsd6_truncate(Thread *thread, ptr<char> path, sint pad,
                                off_t length);
SysResult sys_freebsd6_ftruncate(Thread *thread, sint fd, sint pad,
                                 off_t length);
SysResult sys___sysctl(Thread *thread, ptr<sint> name, uint namelen,
                       ptr<void> old, ptr<size_t> oldenp, ptr<void> new_,
                       size_t newlen);
SysResult sys_mlock(Thread *thread, ptr<const void> addr, size_t len);
SysResult sys_munlock(Thread *thread, ptr<const void> addr, size_t len);
SysResult sys_undelete(Thread *thread, ptr<char> path);
SysResult sys_futimes(Thread *thread, sint fd, ptr<struct timeval> tptr);
SysResult sys_getpgid(Thread *thread, pid_t pid);
SysResult sys_poll(Thread *thread, ptr<struct pollfd> fds, uint nfds,
                   sint timeout);
SysResult sys_semget(Thread *thread, key_t key, sint nsems, sint semflg);
SysResult sys_semop(Thread *thread, sint semid, ptr<struct sembuf> sops,
                    size_t nspos);
SysResult sys_msgget(Thread *thread, key_t key, sint msgflg);
SysResult sys_msgsnd(Thread *thread, sint msqid, ptr<const void> msgp,
                     size_t msgsz, sint msgflg);
SysResult sys_msgrcv(Thread *thread, sint msqid, ptr<void> msgp, size_t msgsz,
                     slong msgtyp, sint msgflg);
SysResult sys_shmat(Thread *thread, sint shmid, ptr<const void> shmaddr,
                    sint shmflg);
SysResult sys_shmdt(Thread *thread, ptr<const void> shmaddr);
SysResult sys_shmget(Thread *thread, key_t key, size_t size, sint shmflg);
SysResult sys_clock_gettime(Thread *thread, clockid_t clock_id,
                            ptr<timespec> tp);
SysResult sys_clock_settime(Thread *thread, clockid_t clock_id,
                            ptr<const timespec> tp);
SysResult sys_clock_getres(Thread *thread, clockid_t clock_id,
                           ptr<timespec> tp);
SysResult sys_ktimer_create(Thread *thread, clockid_t clock_id,
                            ptr<struct sigevent> evp, ptr<sint> timerid);
SysResult sys_ktimer_delete(Thread *thread, sint timerid);
SysResult sys_ktimer_settime(Thread *thread, sint timerid, sint flags,
                             ptr<const struct itimerspec> value,
                             ptr<struct itimerspec> ovalue);
SysResult sys_ktimer_gettime(Thread *thread, sint timerid,
                             ptr<struct itimerspec> value);
SysResult sys_ktimer_getoverrun(Thread *thread, sint timerid);
SysResult sys_nanosleep(Thread *thread, ptr<const timespec> rqtp,
                        ptr<timespec> rmtp);
SysResult sys_ntp_gettime(Thread *thread, ptr<struct ntptimeval> ntvp);
SysResult sys_minherit(Thread *thread, ptr<void> addr, size_t len,
                       sint inherit);
SysResult sys_rfork(Thread *thread, sint flags);
SysResult sys_openbsd_poll(Thread *thread, ptr<struct pollfd> fds, uint nfds,
                           sint timeout);
SysResult sys_issetugid(Thread *thread);
SysResult sys_lchown(Thread *thread, ptr<char> path, sint uid, sint gid);
SysResult sys_aio_read(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_aio_write(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_lio_listio(Thread *thread, sint mode,
                         ptr<cptr<struct aiocb>> aiocbp, sint nent,
                         ptr<struct sigevent> sig);
SysResult sys_getdents(Thread *thread, sint fd, ptr<char> buf, size_t count);
SysResult sys_lchmod(Thread *thread, ptr<char> path, mode_t mode);
SysResult sys_lutimes(Thread *thread, ptr<char> path, ptr<struct timeval> tptr);
SysResult sys_nstat(Thread *thread, ptr<char> path, ptr<struct nstat> ub);
SysResult sys_nfstat(Thread *thread, sint fd, ptr<struct nstat> sb);
SysResult sys_nlstat(Thread *thread, ptr<char> path, ptr<struct nstat> ub);
SysResult sys_preadv(Thread *thread, sint fd, ptr<struct iovec> iovp,
                     uint iovcnt, off_t offset);
SysResult sys_pwritev(Thread *thread, sint fd, ptr<struct iovec> iovp,
                      uint iovcnt, off_t offset);
SysResult sys_fhopen(Thread *thread, ptr<const struct fhandle> u_fhp,
                     sint flags);
SysResult sys_fhstat(Thread *thread, ptr<const struct fhandle> u_fhp,
                     ptr<Stat> sb);
SysResult sys_modnext(Thread *thread, sint modid);
SysResult sys_modstat(Thread *thread, sint modid, ptr<struct module_stat> stat);
SysResult sys_modfnext(Thread *thread, sint modid);
SysResult sys_modfind(Thread *thread, ptr<const char> name);
SysResult sys_kldload(Thread *thread, ptr<const char> file);
SysResult sys_kldunload(Thread *thread, sint fileid);
SysResult sys_kldfind(Thread *thread, ptr<const char> name);
SysResult sys_kldnext(Thread *thread, sint fileid);
SysResult sys_kldstat(Thread *thread, sint fileid,
                      ptr<struct kld_file_stat> stat);
SysResult sys_kldfirstmod(Thread *thread, sint fileid);
SysResult sys_getsid(Thread *thread, pid_t pid);
SysResult sys_setresuid(Thread *thread, uid_t ruid, uid_t euid, uid_t suid);
SysResult sys_setresgid(Thread *thread, gid_t rgid, gid_t egid, gid_t sgid);
SysResult sys_aio_return(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_aio_suspend(Thread *thread, ptr<struct aiocb> aiocbp, sint nent,
                          ptr<const timespec> timeout);
SysResult sys_aio_cancel(Thread *thread, sint fd, ptr<struct aiocb> aiocbp);
SysResult sys_aio_error(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_oaio_read(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_oaio_write(Thread *thread, ptr<struct aiocb> aiocbp);
SysResult sys_olio_listio(Thread *thread, sint mode,
                          ptr<cptr<struct aiocb>> acb_list, sint nent,
                          ptr<struct osigevent> sig);
SysResult sys_yield(Thread *thread);
SysResult sys_mlockall(Thread *thread, sint how);
SysResult sys_munlockall(Thread *thread);
SysResult sys___getcwd(Thread *thread, ptr<char> buf, uint buflen);
SysResult sys_sched_setparam(Thread *thread, pid_t pid,
                             ptr<const struct sched_param> param);
SysResult sys_sched_getparam(Thread *thread, pid_t pid,
                             ptr<struct sched_param> param);
SysResult sys_sched_setscheduler(Thread *thread, pid_t pid, sint policy,
                                 ptr<const struct sched_param> param);
SysResult sys_sched_getscheduler(Thread *thread, pid_t pid);
SysResult sys_sched_yield(Thread *thread);
SysResult sys_sched_get_priority_max(Thread *thread, sint policy);
SysResult sys_sched_get_priority_min(Thread *thread, sint policy);
SysResult sys_sched_rr_get_interval(Thread *thread, pid_t pid,
                                    ptr<timespec> interval);
SysResult sys_utrace(Thread *thread, ptr<const void> addr, size_t len);
SysResult sys_kldsym(Thread *thread, sint fileid, sint cmd, ptr<void> data);
SysResult sys_jail(Thread *thread, ptr<struct jail> jail);
SysResult sys_nnpfs_syscall(Thread *thread, sint operation, ptr<char> a_pathP,
                            sint opcode, ptr<void> a_paramsP,
                            sint a_followSymlinks);
SysResult sys_sigprocmask(Thread *thread, sint how, ptr<uint64_t> set,
                          ptr<uint64_t> oset);
SysResult sys_sigsuspend(Thread *thread, ptr<const struct sigset> set);
SysResult sys_sigpending(Thread *thread, ptr<struct sigset> set);
SysResult sys_sigtimedwait(Thread *thread, ptr<const struct sigset> set,
                           ptr<struct siginfo> info,
                           ptr<const timespec> timeout);
SysResult sys_sigwaitinfo(Thread *thread, ptr<const struct sigset> set,
                          ptr<struct siginfo> info);
SysResult sys___acl_get_file(Thread *thread, ptr<char> path, acl_type_t type,
                             ptr<struct acl> aclp);
SysResult sys___acl_set_file(Thread *thread, ptr<char> path, acl_type_t type,
                             ptr<struct acl> aclp);
SysResult sys___acl_get_fd(Thread *thread, sint filedes, acl_type_t type,
                           ptr<struct acl> aclp);
SysResult sys___acl_set_fd(Thread *thread, sint filedes, acl_type_t type,
                           ptr<struct acl> aclp);
SysResult sys___acl_delete_file(Thread *thread, ptr<char> path,
                                acl_type_t type);
SysResult sys___acl_delete_fd(Thread *thread, sint filedes, acl_type_t type);
SysResult sys___acl_aclcheck_file(Thread *thread, ptr<char> path,
                                  acl_type_t type, ptr<struct acl> aclp);
SysResult sys___acl_aclcheck_fd(Thread *thread, sint filedes, acl_type_t type,
                                ptr<struct acl> aclp);
SysResult sys_extattrctl(Thread *thread, ptr<char> path, char cmd,
                         ptr<const char> filename, sint attrnamespace,
                         ptr<const char> attrname);
SysResult sys_extattr_set_file(Thread *thread, ptr<char> path,
                               sint attrnamespace, ptr<const char> filename,
                               ptr<void> data, size_t nbytes);
SysResult sys_extattr_get_file(Thread *thread, ptr<char> path,
                               sint attrnamespace, ptr<const char> filename,
                               ptr<void> data, size_t nbytes);
SysResult sys_extattr_delete_file(Thread *thread, ptr<char> path,
                                  sint attrnamespace, ptr<const char> attrname);
SysResult sys_aio_waitcomplete(Thread *thread, ptr<ptr<struct aiocb>> aiocbp,
                               ptr<timespec> timeout);
SysResult sys_getresuid(Thread *thread, ptr<uid_t> ruid, ptr<uid_t> euid,
                        ptr<uid_t> suid);
SysResult sys_getresgid(Thread *thread, ptr<gid_t> rgid, ptr<gid_t> egid,
                        ptr<gid_t> sgid);
SysResult sys_kqueue(Thread *thread);
SysResult sys_kevent(Thread *thread, sint fd, ptr<KEvent> changelist,
                     sint nchanges, ptr<KEvent> eventlist, sint nevents,
                     ptr<const timespec> timeout);
SysResult sys_extattr_set_fd(Thread *thread, sint fd, sint attrnamespace,
                             ptr<const char> attrname, ptr<void> data,
                             size_t nbytes);
SysResult sys_extattr_get_fd(Thread *thread, sint fd, sint attrnamespace,
                             ptr<const char> attrname, ptr<void> data,
                             size_t nbytes);
SysResult sys_extattr_delete_fd(Thread *thread, sint fd, sint attrnamespace,
                                ptr<const char> attrname);
SysResult sys___setugid(Thread *thread, sint flags);
SysResult sys_eaccess(Thread *thread, ptr<char> path, sint flags);
SysResult sys_afs3_syscall(Thread *thread, slong syscall, slong param1,
                           slong param2, slong param3, slong param4,
                           slong param5, slong param6);
SysResult sys_nmount(Thread *thread, ptr<struct iovec> iovp, uint iovcnt,
                     sint flags);
SysResult sys___mac_get_proc(Thread *thread, ptr<struct mac> mac_p);
SysResult sys___mac_set_proc(Thread *thread, ptr<struct mac> mac_p);
SysResult sys___mac_get_fd(Thread *thread, sint fd, ptr<struct mac> mac_p);
SysResult sys___mac_get_file(Thread *thread, ptr<const char> path,
                             ptr<struct mac> mac_p);
SysResult sys___mac_set_fd(Thread *thread, sint fd, ptr<struct mac> mac_p);
SysResult sys___mac_set_file(Thread *thread, ptr<const char> path,
                             ptr<struct mac> mac_p);
SysResult sys_kenv(Thread *thread, sint what, ptr<const char> name,
                   ptr<char> value, sint len);
SysResult sys_lchflags(Thread *thread, ptr<const char> path, sint flags);
SysResult sys_uuidgen(Thread *thread, ptr<struct uuid> store, sint count);
SysResult sys_sendfile(Thread *thread, sint fd, sint s, off_t offset,
                       size_t nbytes, ptr<struct sf_hdtr> hdtr,
                       ptr<off_t> sbytes, sint flags);
SysResult sys_mac_syscall(Thread *thread, ptr<const char> policy, sint call,
                          ptr<void> arg);
SysResult sys_getfsstat(Thread *thread, ptr<struct statfs> buf, slong bufsize,
                        sint flags);
SysResult sys_statfs(Thread *thread, ptr<char> path, ptr<struct statfs> buf);
SysResult sys_fstatfs(Thread *thread, sint fd, ptr<struct statfs> buf);
SysResult sys_fhstatfs(Thread *thread, ptr<const struct fhandle> u_fhp,
                       ptr<struct statfs> buf);
SysResult sys_ksem_close(Thread *thread, semid_t id);
SysResult sys_ksem_post(Thread *thread, semid_t id);
SysResult sys_ksem_wait(Thread *thread, semid_t id);
SysResult sys_ksem_trywait(Thread *thread, semid_t id);
SysResult sys_ksem_init(Thread *thread, ptr<semid_t> idp, uint value);
SysResult sys_ksem_open(Thread *thread, ptr<semid_t> idp, ptr<const char> name,
                        sint oflag, mode_t mode, uint value);
SysResult sys_ksem_unlink(Thread *thread, ptr<const char> name);
SysResult sys_ksem_getvalue(Thread *thread, semid_t id, ptr<sint> value);
SysResult sys_ksem_destroy(Thread *thread, semid_t id);
SysResult sys___mac_get_pid(Thread *thread, pid_t pid, ptr<struct mac> mac_p);
SysResult sys___mac_get_link(Thread *thread, ptr<const char> path_p,
                             ptr<struct mac> mac_p);
SysResult sys___mac_set_link(Thread *thread, ptr<const char> path_p,
                             ptr<struct mac> mac_p);
SysResult sys_extattr_set_link(Thread *thread, ptr<const char> path,
                               sint attrnamespace, ptr<const char> attrname,
                               ptr<void> data, size_t nbytes);
SysResult sys_extattr_get_link(Thread *thread, ptr<const char> path,
                               sint attrnamespace, ptr<const char> attrname,
                               ptr<void> data, size_t nbytes);
SysResult sys_extattr_delete_link(Thread *thread, ptr<const char> path,
                                  sint attrnamespace, ptr<const char> attrname);
SysResult sys___mac_execve(Thread *thread, ptr<char> fname, ptr<ptr<char>> argv,
                           ptr<ptr<char>> envv, ptr<struct mac> mac_p);
SysResult sys_sigaction(Thread *thread, sint sig, ptr<struct sigaction> act,
                        ptr<struct sigaction> oact);
SysResult sys_sigreturn(Thread *thread, ptr<struct ucontext> sigcntxp);
SysResult sys_getcontext(Thread *thread, ptr<struct ucontext> ucp);
SysResult sys_setcontext(Thread *thread, ptr<struct ucontext> ucp);
SysResult sys_swapcontext(Thread *thread, ptr<struct ucontext> oucp,
                          ptr<struct ucontext> ucp);
SysResult sys_swapoff(Thread *thread, ptr<const char> name);
SysResult sys___acl_get_link(Thread *thread, ptr<const char> path,
                             acl_type_t type, ptr<struct acl> aclp);
SysResult sys___acl_set_link(Thread *thread, ptr<const char> path,
                             acl_type_t type, ptr<struct acl> aclp);
SysResult sys___acl_delete_link(Thread *thread, ptr<const char> path,
                                acl_type_t type);
SysResult sys___acl_aclcheck_link(Thread *thread, ptr<const char> path,
                                  acl_type_t type, ptr<struct acl> aclp);
SysResult sys_sigwait(Thread *thread, ptr<const struct sigset> set,
                      ptr<sint> sig);
SysResult sys_thr_create(Thread *thread, ptr<struct ucontext> ctxt,
                         ptr<slong> arg, sint flags);
SysResult sys_thr_exit(Thread *thread, ptr<slong> state);
SysResult sys_thr_self(Thread *thread, ptr<slong> id);
SysResult sys_thr_kill(Thread *thread, slong id, sint sig);
SysResult sys__umtx_lock(Thread *thread, ptr<struct umtx> umtx);
SysResult sys__umtx_unlock(Thread *thread, ptr<struct umtx> umtx);
SysResult sys_jail_attach(Thread *thread, sint jid);
SysResult sys_extattr_list_fd(Thread *thread, sint fd, sint attrnamespace,
                              ptr<void> data, size_t nbytes);
SysResult sys_extattr_list_file(Thread *thread, ptr<const char> path,
                                sint attrnamespace, ptr<void> data,
                                size_t nbytes);
SysResult sys_extattr_list_link(Thread *thread, ptr<const char> path,
                                sint attrnamespace, ptr<void> data,
                                size_t nbytes);
SysResult sys_ksem_timedwait(Thread *thread, semid_t id,
                             ptr<const timespec> abstime);
SysResult sys_thr_suspend(Thread *thread, ptr<const timespec> timeout);
SysResult sys_thr_wake(Thread *thread, slong id);
SysResult sys_kldunloadf(Thread *thread, slong fileid, sint flags);
SysResult sys_audit(Thread *thread, ptr<const void> record, uint length);
SysResult sys_auditon(Thread *thread, sint cmd, ptr<void> data, uint length);
SysResult sys_getauid(Thread *thread, ptr<uid_t> auid);
SysResult sys_setauid(Thread *thread, ptr<uid_t> auid);
SysResult sys_getaudit(Thread *thread, ptr<struct auditinfo> auditinfo);
SysResult sys_setaudit(Thread *thread, ptr<struct auditinfo> auditinfo);
SysResult sys_getaudit_addr(Thread *thread,
                            ptr<struct auditinfo_addr> auditinfo_addr,
                            uint length);
SysResult sys_setaudit_addr(Thread *thread,
                            ptr<struct auditinfo_addr> auditinfo_addr,
                            uint length);
SysResult sys_auditctl(Thread *thread, ptr<char> path);
SysResult sys__umtx_op(Thread *thread, ptr<void> obj, sint op, ulong val,
                       ptr<void> uaddr1, ptr<void> uaddr2);
SysResult sys_thr_new(Thread *thread, ptr<struct thr_param> param,
                      sint param_size);
SysResult sys_sigqueue(Thread *thread, pid_t pid, sint signum, ptr<void> value);
SysResult sys_kmq_open(Thread *thread, ptr<const char> path, sint flags,
                       mode_t mode, ptr<const struct mq_attr> attr);
SysResult sys_kmq_setattr(Thread *thread, sint mqd,
                          ptr<const struct mq_attr> attr,
                          ptr<struct mq_attr> oattr);
SysResult sys_kmq_timedreceive(Thread *thread, sint mqd,
                               ptr<const char> msg_ptr, size_t msg_len,
                               ptr<uint> msg_prio,
                               ptr<const timespec> abstimeout);
SysResult sys_kmq_timedsend(Thread *thread, sint mqd, ptr<char> msg_ptr,
                            size_t msg_len, ptr<uint> msg_prio,
                            ptr<const timespec> abstimeout);
SysResult sys_kmq_notify(Thread *thread, sint mqd,
                         ptr<const struct sigevent> sigev);
SysResult sys_kmq_unlink(Thread *thread, ptr<const char> path);
SysResult sys_abort2(Thread *thread, ptr<const char> why, sint narg,
                     ptr<ptr<void>> args);
SysResult sys_thr_set_name(Thread *thread, slong id, ptr<const char> name);
SysResult sys_aio_fsync(Thread *thread, sint op, ptr<struct aiocb> aiocbp);
SysResult sys_rtprio_thread(Thread *thread, sint function, lwpid_t lwpid,
                            ptr<struct rtprio> rtp);
SysResult sys_sctp_peeloff(Thread *thread, sint sd, uint32_t name);
SysResult sys_sctp_generic_sendmsg(Thread *thread, sint sd, caddr_t msg,
                                   sint mlen, caddr_t to, __socklen_t tolen,
                                   ptr<struct sctp_sndrcvinfo> sinfo,
                                   sint flags);
SysResult sys_sctp_generic_sendmsg_iov(Thread *thread, sint sd,
                                       ptr<struct iovec> iov, sint iovlen,
                                       caddr_t to, __socklen_t tolen,
                                       ptr<struct sctp_sndrcvinfo> sinfo,
                                       sint flags);
SysResult sys_sctp_generic_recvmsg(Thread *thread, sint sd,
                                   ptr<struct iovec> iov, sint iovlen,
                                   caddr_t from, __socklen_t fromlen,
                                   ptr<struct sctp_sndrcvinfo> sinfo,
                                   sint flags);
SysResult sys_pread(Thread *thread, sint fd, ptr<void> buf, size_t nbyte,
                    off_t offset);
SysResult sys_pwrite(Thread *thread, sint fd, ptr<const void> buf, size_t nbyte,
                     off_t offset);
SysResult sys_mmap(Thread *thread, caddr_t addr, size_t len, sint prot,
                   sint flags, sint fd, off_t pos);
SysResult sys_lseek(Thread *thread, sint fd, off_t offset, sint whence);
SysResult sys_truncate(Thread *thread, ptr<char> path, off_t length);
SysResult sys_ftruncate(Thread *thread, sint fd, off_t length);
SysResult sys_thr_kill2(Thread *thread, pid_t pid, slong id, sint sig);
SysResult sys_shm_open(Thread *thread, ptr<const char> path, sint flags,
                       mode_t mode);
SysResult sys_shm_unlink(Thread *thread, ptr<const char> path);
SysResult sys_cpuset(Thread *thread, ptr<cpusetid_t> setid);
SysResult sys_cpuset_setid(Thread *thread, cpuwhich_t which, id_t id,
                           cpusetid_t setid);
SysResult sys_cpuset_getid(Thread *thread, cpulevel_t level, cpuwhich_t which,
                           id_t id, ptr<cpusetid_t> setid);
SysResult sys_cpuset_getaffinity(Thread *thread, cpulevel_t level,
                                 cpuwhich_t which, id_t id, size_t cpusetsize,
                                 ptr<cpuset> mask);
SysResult sys_cpuset_setaffinity(Thread *thread, cpulevel_t level,
                                 cpuwhich_t which, id_t id, size_t cpusetsize,
                                 ptr<const cpuset> mask);
SysResult sys_faccessat(Thread *thread, sint fd, ptr<char> path, sint mode,
                        sint flag);
SysResult sys_fchmodat(Thread *thread, sint fd, ptr<char> path, mode_t mode,
                       sint flag);
SysResult sys_fchownat(Thread *thread, sint fd, ptr<char> path, uid_t uid,
                       gid_t gid, sint flag);
SysResult sys_fexecve(Thread *thread, sint fd, ptr<ptr<char>> argv,
                      ptr<ptr<char>> envv);
SysResult sys_fstatat(Thread *thread, sint fd, ptr<char> path, ptr<Stat> buf,
                      sint flag);
SysResult sys_futimesat(Thread *thread, sint fd, ptr<char> path,
                        ptr<struct timeval> times);
SysResult sys_linkat(Thread *thread, sint fd1, ptr<char> path1, sint fd2,
                     ptr<char> path2, sint flag);
SysResult sys_mkdirat(Thread *thread, sint fd, ptr<char> path, mode_t mode);
SysResult sys_mkfifoat(Thread *thread, sint fd, ptr<char> path, mode_t mode);
SysResult sys_mknodat(Thread *thread, sint fd, ptr<char> path, mode_t mode,
                      dev_t dev);
SysResult sys_openat(Thread *thread, sint fd, ptr<char> path, sint flag,
                     mode_t mode);
SysResult sys_readlinkat(Thread *thread, sint fd, ptr<char> path, ptr<char> buf,
                         size_t bufsize);
SysResult sys_renameat(Thread *thread, sint oldfd, ptr<char> old, sint newfd,
                       ptr<char> new_);
SysResult sys_symlinkat(Thread *thread, ptr<char> path1, sint fd,
                        ptr<char> path2);
SysResult sys_unlinkat(Thread *thread, sint fd, ptr<char> path, sint flag);
SysResult sys_posix_openpt(Thread *thread, sint flags);
SysResult sys_gssd_syscall(Thread *thread, ptr<char> path);
SysResult sys_jail_get(Thread *thread, ptr<struct iovec> iovp, uint iovcnt,
                       sint flags);
SysResult sys_jail_set(Thread *thread, ptr<struct iovec> iovp, uint iovcnt,
                       sint flags);
SysResult sys_jail_remove(Thread *thread, sint jid);
SysResult sys_closefrom(Thread *thread, sint lowfd);
SysResult sys___semctl(Thread *thread, sint semid, sint semnum, sint cmd,
                       ptr<union semun> arg);
SysResult sys_msgctl(Thread *thread, sint msqid, sint cmd,
                     ptr<struct msqid_ds> buf);
SysResult sys_shmctl(Thread *thread, sint shmid, sint cmd,
                     ptr<struct shmid_ds> buf);
SysResult sys_lpathconf(Thread *thread, ptr<char> path, sint name);
SysResult sys_cap_new(Thread *thread, sint fd, uint64_t rights);
SysResult sys_cap_getrights(Thread *thread, sint fd, ptr<uint64_t> rights);
SysResult sys_cap_enter(Thread *thread);
SysResult sys_cap_getmode(Thread *thread, ptr<uint> modep);
SysResult sys_pdfork(Thread *thread, ptr<sint> fdp, sint flags);
SysResult sys_pdkill(Thread *thread, sint fd, sint signum);
SysResult sys_pdgetpid(Thread *thread, sint fd, ptr<pid_t> pidp);
SysResult sys_pselect(Thread *thread, sint nd, ptr<fd_set> in, ptr<fd_set> ou,
                      ptr<fd_set> ex, ptr<const timespec> ts,
                      ptr<const sigset_t> sm);
SysResult sys_getloginclass(Thread *thread, ptr<char> namebuf, size_t namelen);
SysResult sys_setloginclass(Thread *thread, ptr<char> namebuf);
SysResult sys_rctl_get_racct(Thread *thread, ptr<const void> inbufp,
                             size_t inbuflen, ptr<void> outbuf,
                             size_t outbuflen);
SysResult sys_rctl_get_rules(Thread *thread, ptr<const void> inbufp,
                             size_t inbuflen, ptr<void> outbuf,
                             size_t outbuflen);
SysResult sys_rctl_get_limits(Thread *thread, ptr<const void> inbufp,
                              size_t inbuflen, ptr<void> outbuf,
                              size_t outbuflen);
SysResult sys_rctl_add_rule(Thread *thread, ptr<const void> inbufp,
                            size_t inbuflen, ptr<void> outbuf,
                            size_t outbuflen);
SysResult sys_rctl_remove_rule(Thread *thread, ptr<const void> inbufp,
                               size_t inbuflen, ptr<void> outbuf,
                               size_t outbuflen);
SysResult sys_posix_fallocate(Thread *thread, sint fd, off_t offset, off_t len);
SysResult sys_posix_fadvise(Thread *thread, sint fd, off_t offset, off_t len,
                            sint advice);

SysResult sys_netcontrol(Thread *thread, sint fd, uint op, ptr<void> buf,
                         uint nbuf);
SysResult sys_netabort(Thread *thread /* TODO */);
SysResult sys_netgetsockinfo(Thread *thread /* TODO */);
SysResult sys_socketex(Thread *thread, ptr<const char> name, sint domain,
                       sint type, sint protocol);
SysResult sys_socketclose(Thread *thread /* TODO */);
SysResult sys_netgetiflist(Thread *thread /* TODO */);
SysResult sys_kqueueex(Thread *thread /* TODO */);
SysResult sys_mtypeprotect(Thread *thread /* TODO */);
SysResult sys_regmgr_call(Thread *thread, uint32_t op, uint32_t id,
                          ptr<void> result, ptr<void> value, uint64_t type);
SysResult sys_jitshm_create(Thread *thread /* TODO */);
SysResult sys_jitshm_alias(Thread *thread /* TODO */);
SysResult sys_dl_get_list(Thread *thread /* TODO */);
SysResult sys_dl_get_info(Thread *thread /* TODO */);
SysResult sys_dl_notify_event(Thread *thread /* TODO */);
SysResult sys_evf_create(Thread *thread, ptr<const char[32]> name, sint attrs,
                         uint64_t initPattern);
SysResult sys_evf_delete(Thread *thread, sint id);
SysResult sys_evf_open(Thread *thread, ptr<const char[32]> name);
SysResult sys_evf_close(Thread *thread, sint id);
SysResult sys_evf_wait(Thread *thread, sint id, uint64_t patternSet,
                       uint64_t mode, ptr<uint64_t> pPatternSet,
                       ptr<uint> pTimeout);
SysResult sys_evf_trywait(Thread *thread, sint id, uint64_t patternSet,
                          uint64_t mode,
                          ptr<uint64_t> pPatternSet); // FIXME: verify args
SysResult sys_evf_set(Thread *thread, sint id, uint64_t value);
SysResult sys_evf_clear(Thread *thread, sint id, uint64_t value);
SysResult sys_evf_cancel(Thread *thread, sint id, uint64_t value,
                         ptr<sint> pNumWaitThreads);
SysResult sys_query_memory_protection(Thread *thread /* TODO */);
SysResult sys_batch_map(Thread *thread /* TODO */);
SysResult sys_osem_create(Thread *thread /* TODO */);
SysResult sys_osem_delete(Thread *thread /* TODO */);
SysResult sys_osem_open(Thread *thread /* TODO */);
SysResult sys_osem_close(Thread *thread /* TODO */);
SysResult sys_osem_wait(Thread *thread /* TODO */);
SysResult sys_osem_trywait(Thread *thread /* TODO */);
SysResult sys_osem_post(Thread *thread /* TODO */);
SysResult sys_osem_cancel(Thread *thread /* TODO */);
SysResult sys_namedobj_create(Thread *thread, ptr<const char[32]> name,
                              ptr<void> object, uint16_t type);
SysResult sys_namedobj_delete(Thread *thread, uint16_t id, uint16_t type);
SysResult sys_set_vm_container(Thread *thread /* TODO */);
SysResult sys_debug_init(Thread *thread /* TODO */);
SysResult sys_suspend_process(Thread *thread, pid_t pid);
SysResult sys_resume_process(Thread *thread, pid_t pid);
SysResult sys_opmc_enable(Thread *thread /* TODO */);
SysResult sys_opmc_disable(Thread *thread /* TODO */);
SysResult sys_opmc_set_ctl(Thread *thread /* TODO */);
SysResult sys_opmc_set_ctr(Thread *thread /* TODO */);
SysResult sys_opmc_get_ctr(Thread *thread /* TODO */);
SysResult sys_budget_create(Thread *thread /* TODO */);
SysResult sys_budget_delete(Thread *thread /* TODO */);
SysResult sys_budget_get(Thread *thread /* TODO */);
SysResult sys_budget_set(Thread *thread /* TODO */);
SysResult sys_virtual_query(Thread *thread, ptr<void> addr, uint64_t unk,
                            ptr<void> info, size_t infosz);
SysResult sys_mdbg_call(Thread *thread /* TODO */);
SysResult sys_obs_sblock_create(Thread *thread /* TODO */);
SysResult sys_obs_sblock_delete(Thread *thread /* TODO */);
SysResult sys_obs_sblock_enter(Thread *thread /* TODO */);
SysResult sys_obs_sblock_exit(Thread *thread /* TODO */);
SysResult sys_obs_sblock_xenter(Thread *thread /* TODO */);
SysResult sys_obs_sblock_xexit(Thread *thread /* TODO */);
SysResult sys_obs_eport_create(Thread *thread /* TODO */);
SysResult sys_obs_eport_delete(Thread *thread /* TODO */);
SysResult sys_obs_eport_trigger(Thread *thread /* TODO */);
SysResult sys_obs_eport_open(Thread *thread /* TODO */);
SysResult sys_obs_eport_close(Thread *thread /* TODO */);
SysResult sys_is_in_sandbox(Thread *thread /* TODO */);
SysResult sys_dmem_container(Thread *thread);
SysResult sys_get_authinfo(Thread *thread, pid_t pid, ptr<void> info);
SysResult sys_mname(Thread *thread, ptr<void> address, uint64_t length,
                    ptr<const char> name);
SysResult sys_dynlib_dlopen(Thread *thread /* TODO */);
SysResult sys_dynlib_dlclose(Thread *thread /* TODO */);
SysResult sys_dynlib_dlsym(Thread *thread, SceKernelModule handle,
                           ptr<const char> symbol, ptr<ptr<void>> addrp);
SysResult sys_dynlib_get_list(Thread *thread, ptr<SceKernelModule> pArray,
                              size_t numArray, ptr<size_t> pActualNum);
SysResult sys_dynlib_get_info(Thread *thread, SceKernelModule handle,
                              ptr<ModuleInfo> pInfo);
SysResult sys_dynlib_load_prx(Thread *thread, ptr<const char> name,
                              uint64_t arg1, ptr<ModuleHandle> pHandle,
                              uint64_t arg3);
SysResult sys_dynlib_unload_prx(Thread *thread,
                                SceKernelModule handle /* TODO*/);
SysResult sys_dynlib_do_copy_relocations(Thread *thread);
SysResult sys_dynlib_prepare_dlclose(Thread *thread /* TODO */);
SysResult sys_dynlib_get_proc_param(Thread *thread, ptr<ptr<void>> procParam,
                                    ptr<uint64_t> procParamSize);
SysResult sys_dynlib_process_needed_and_relocate(Thread *thread);
SysResult sys_sandbox_path(Thread *thread /* TODO */);
SysResult sys_mdbg_service(Thread *thread, uint32_t op, ptr<void> arg0,
                           ptr<void> arg1);
SysResult sys_randomized_path(Thread *thread /* TODO */);
SysResult sys_rdup(Thread *thread /* TODO */);
SysResult sys_dl_get_metadata(Thread *thread /* TODO */);
SysResult sys_workaround8849(Thread *thread /* TODO */);
SysResult sys_is_development_mode(Thread *thread /* TODO */);
SysResult sys_get_self_auth_info(Thread *thread /* TODO */);
SysResult sys_dynlib_get_info_ex(Thread *thread, SceKernelModule handle,
                                 ptr<struct Unk> unk,
                                 ptr<ModuleInfoEx> destModuleInfoEx);
SysResult sys_budget_getid(Thread *thread);
SysResult sys_budget_get_ptype(Thread *thread, sint budgetId);
SysResult sys_get_paging_stats_of_all_threads(Thread *thread /* TODO */);
SysResult sys_get_proc_type_info(Thread *thread, ptr<sint> destProcessInfo);
SysResult sys_get_resident_count(Thread *thread, pid_t pid);
SysResult sys_prepare_to_suspend_process(Thread *thread, pid_t pid);
SysResult sys_get_resident_fmem_count(Thread *thread, pid_t pid);
SysResult sys_thr_get_name(Thread *thread, lwpid_t lwpid);
SysResult sys_set_gpo(Thread *thread /* TODO */);
SysResult sys_get_paging_stats_of_all_objects(Thread *thread /* TODO */);
SysResult sys_test_debug_rwmem(Thread *thread /* TODO */);
SysResult sys_free_stack(Thread *thread /* TODO */);
SysResult sys_suspend_system(Thread *thread /* TODO */);
SysResult sys_ipmimgr_call(Thread *thread, uint op, uint kid, ptr<uint> result,
                           ptr<void> params, uint64_t paramsz, uint64_t arg6);
SysResult sys_get_gpo(Thread *thread /* TODO */);
SysResult sys_get_vm_map_timestamp(Thread *thread /* TODO */);
SysResult sys_opmc_set_hw(Thread *thread /* TODO */);
SysResult sys_opmc_get_hw(Thread *thread /* TODO */);
SysResult sys_get_cpu_usage_all(Thread *thread /* TODO */);
SysResult sys_mmap_dmem(Thread *thread /* TODO */);
SysResult sys_physhm_open(Thread *thread /* TODO */);
SysResult sys_physhm_unlink(Thread *thread /* TODO */);
SysResult sys_resume_internal_hdd(Thread *thread /* TODO */);
SysResult sys_thr_suspend_ucontext(Thread *thread /* TODO */);
SysResult sys_thr_resume_ucontext(Thread *thread /* TODO */);
SysResult sys_thr_get_ucontext(Thread *thread /* TODO */);
SysResult sys_thr_set_ucontext(Thread *thread /* TODO */);
SysResult sys_set_timezone_info(Thread *thread /* TODO */);
SysResult sys_set_phys_fmem_limit(Thread *thread /* TODO */);
SysResult sys_utc_to_localtime(Thread *thread /* TODO */);
SysResult sys_localtime_to_utc(Thread *thread /* TODO */);
SysResult sys_set_uevt(Thread *thread /* TODO */);
SysResult sys_get_cpu_usage_proc(Thread *thread /* TODO */);
SysResult sys_get_map_statistics(Thread *thread /* TODO */);
SysResult sys_set_chicken_switches(Thread *thread /* TODO */);
SysResult sys_extend_page_table_pool(Thread *thread);
SysResult sys_extend_page_table_pool2(Thread *thread);
SysResult sys_get_kernel_mem_statistics(Thread *thread /* TODO */);
SysResult sys_get_sdk_compiled_version(Thread *thread /* TODO */);
SysResult sys_app_state_change(Thread *thread /* TODO */);
SysResult sys_dynlib_get_obj_member(Thread *thread, SceKernelModule handle,
                                    uint64_t index, ptr<ptr<void>> addrp);
SysResult sys_budget_get_ptype_of_budget(Thread *thread /* TODO */);
SysResult sys_prepare_to_resume_process(Thread *thread /* TODO */);
SysResult sys_process_terminate(Thread *thread /* TODO */);
SysResult sys_blockpool_open(Thread *thread /* TODO */);
SysResult sys_blockpool_map(Thread *thread /* TODO */);
SysResult sys_blockpool_unmap(Thread *thread /* TODO */);
SysResult sys_dynlib_get_info_for_libdbg(Thread *thread /* TODO */);
SysResult sys_blockpool_batch(Thread *thread /* TODO */);
SysResult sys_fdatasync(Thread *thread /* TODO */);
SysResult sys_dynlib_get_list2(Thread *thread /* TODO */);
SysResult sys_dynlib_get_info2(Thread *thread /* TODO */);
SysResult sys_aio_submit(Thread *thread /* TODO */);
SysResult sys_aio_multi_delete(Thread *thread /* TODO */);
SysResult sys_aio_multi_wait(Thread *thread /* TODO */);
SysResult sys_aio_multi_poll(Thread *thread /* TODO */);
SysResult sys_aio_get_data(Thread *thread /* TODO */);
SysResult sys_aio_multi_cancel(Thread *thread /* TODO */);
SysResult sys_get_bio_usage_all(Thread *thread /* TODO */);
SysResult sys_aio_create(Thread *thread /* TODO */);
SysResult sys_aio_submit_cmd(Thread *thread /* TODO */);
SysResult sys_aio_init(Thread *thread /* TODO */);
SysResult sys_get_page_table_stats(Thread *thread /* TODO */);
SysResult sys_dynlib_get_list_for_libdbg(Thread *thread /* TODO */);
SysResult sys_blockpool_move(Thread *thread /* TODO */);
SysResult sys_virtual_query_all(Thread *thread /* TODO */);
SysResult sys_reserve_2mb_page(Thread *thread /* TODO */);
SysResult sys_cpumode_yield(Thread *thread /* TODO */);

SysResult sys_wait6(Thread *thread /* TODO */);
SysResult sys_cap_rights_limit(Thread *thread /* TODO */);
SysResult sys_cap_ioctls_limit(Thread *thread /* TODO */);
SysResult sys_cap_ioctls_get(Thread *thread /* TODO */);
SysResult sys_cap_fcntls_limit(Thread *thread /* TODO */);
SysResult sys_cap_fcntls_get(Thread *thread /* TODO */);
SysResult sys_bindat(Thread *thread /* TODO */);
SysResult sys_connectat(Thread *thread /* TODO */);
SysResult sys_chflagsat(Thread *thread /* TODO */);
SysResult sys_accept4(Thread *thread /* TODO */);
SysResult sys_pipe2(Thread *thread /* TODO */);
SysResult sys_aio_mlock(Thread *thread /* TODO */);
SysResult sys_procctl(Thread *thread /* TODO */);
SysResult sys_ppoll(Thread *thread /* TODO */);
SysResult sys_futimens(Thread *thread /* TODO */);
SysResult sys_utimensat(Thread *thread /* TODO */);
SysResult sys_numa_getaffinity(Thread *thread /* TODO */);
SysResult sys_numa_setaffinity(Thread *thread /* TODO */);
SysResult sys_apr_submit(Thread *thread /* TODO */);
SysResult sys_apr_resolve(Thread *thread /* TODO */);
SysResult sys_apr_stat(Thread *thread /* TODO */);
SysResult sys_apr_wait(Thread *thread /* TODO */);
SysResult sys_apr_ctrl(Thread *thread /* TODO */);
SysResult sys_get_phys_page_size(Thread *thread /* TODO */);
SysResult sys_begin_app_mount(Thread *thread /* TODO */);
SysResult sys_end_app_mount(Thread *thread /* TODO */);
SysResult sys_fsc2h_ctrl(Thread *thread /* TODO */);
SysResult sys_streamwrite(Thread *thread /* TODO */);
SysResult sys_app_save(Thread *thread /* TODO */);
SysResult sys_app_restore(Thread *thread /* TODO */);
SysResult sys_saved_app_delete(Thread *thread /* TODO */);
SysResult sys_get_ppr_sdk_compiled_version(Thread *thread /* TODO */);
SysResult sys_notify_app_event(Thread *thread /* TODO */);
SysResult sys_ioreq(Thread *thread /* TODO */);
SysResult sys_openintr(Thread *thread /* TODO */);
SysResult sys_dl_get_info_2(Thread *thread /* TODO */);
SysResult sys_acinfo_add(Thread *thread /* TODO */);
SysResult sys_acinfo_delete(Thread *thread /* TODO */);
SysResult sys_acinfo_get_all_for_coredump(Thread *thread /* TODO */);
SysResult sys_ampr_ctrl_debug(Thread *thread /* TODO */);
SysResult sys_workspace_ctrl(Thread *thread /* TODO */);
} // namespace orbis
