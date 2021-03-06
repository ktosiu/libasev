#include "sandbox.h"

#include "sysutil.h"
#include "utility.h"
#include <errno.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/filter.h>
#include <asm/unistd.h>

#ifndef PR_SET_SECCOMP
  #define PR_SET_SECCOMP 22
#endif

#ifndef PR_SET_NO_NEW_PRIVS
  #define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef __NR_openat
  #define __NR_openat 257
#endif

#ifndef O_LARGEFILE
  #define O_LARGEFILE 00100000
#endif

#ifndef O_DIRECTORY
  #define O_DIRECTORY 00200000
#endif

#ifndef O_CLOEXEC
  #define O_CLOEXEC 002000000
#endif

#define kMaxSyscalls 100

static const int kOpenFlags = O_CREAT|O_EXCL|O_APPEND|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_LARGEFILE;

static size_t s_syscall_index;
static size_t s_1_arg_validations;
static size_t s_2_arg_validations;
static size_t s_3_arg_validations;
static int s_syscalls[kMaxSyscalls];
static int s_errnos[kMaxSyscalls];
static int s_args_1[kMaxSyscalls];
static int s_vals_1[kMaxSyscalls];
static int s_args_2[kMaxSyscalls];
static int s_vals_2[kMaxSyscalls];
static int s_args_3[kMaxSyscalls];
static int s_vals_3[kMaxSyscalls];

static void
allow_nr(int nr)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
}

static void
reject_nr(int nr, int errcode)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (errcode < 0 || errcode > 255)
  {
    bug("bad errcode");
  }
  s_errnos[s_syscall_index] = errcode;
  s_syscalls[s_syscall_index++] = nr;
}

static void
allow_nr_1_arg_match(int nr, int arg, int val)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (arg < 1 || arg > 6)
  {
    bug("arg out of range");
  }
  s_args_1[s_syscall_index] = arg;
  s_vals_1[s_syscall_index] = val;
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
  s_1_arg_validations++;
}

static void
allow_nr_1_arg_mask(int nr, int arg, int val)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (arg < 1 || arg > 6)
  {
    bug("arg out of range");
  }
  s_args_1[s_syscall_index] = 100 + arg;
  s_vals_1[s_syscall_index] = val;
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
  s_1_arg_validations++;
}

static void
allow_nr_2_arg_match(int nr, int arg1, int val1, int arg2, int val2)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (arg1 < 1 || arg1 > 6)
  {
    bug("arg1 out of range");
  }
  if (arg2 < 1 || arg2 > 6)
  {
    bug("arg2 out of range");
  }
  s_args_1[s_syscall_index] = arg1;
  s_vals_1[s_syscall_index] = val1;
  s_args_2[s_syscall_index] = arg2;
  s_vals_2[s_syscall_index] = val2;
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
  s_2_arg_validations++;
}

static void
allow_nr_2_arg_mask_match(int nr, int arg1, int val1, int arg2, int val2)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (arg1 < 1 || arg1 > 6)
  {
    bug("arg1 out of range");
  }
  if (arg2 < 1 || arg2 > 6)
  {
    bug("arg2 out of range");
  }
  s_args_1[s_syscall_index] = 100 + arg1;
  s_vals_1[s_syscall_index] = val1;
  s_args_2[s_syscall_index] = arg2;
  s_vals_2[s_syscall_index] = val2;
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
  s_2_arg_validations++;
}

static void
allow_nr_3_arg_match(int nr, int arg1, int val1, int arg2, int val2, int arg3,
                     int val3)
{
  if (s_syscall_index >= kMaxSyscalls)
  {
    bug("out of syscall space");
  }
  if (nr < 0)
  {
    bug("negative syscall");
  }
  if (arg1 < 1 || arg1 > 6)
  {
    bug("arg1 out of range");
  }
  if (arg2 < 1 || arg2 > 6)
  {
    bug("arg2 out of range");
  }
  if (arg3 < 1 || arg3 > 6)
  {
    bug("arg3 out of range");
  }
  s_args_1[s_syscall_index] = arg1;
  s_vals_1[s_syscall_index] = val1;
  s_args_2[s_syscall_index] = arg2;
  s_vals_2[s_syscall_index] = val2;
  s_args_3[s_syscall_index] = arg3;
  s_vals_3[s_syscall_index] = val3;
  s_errnos[s_syscall_index] = 0;
  s_syscalls[s_syscall_index++] = nr;
  s_3_arg_validations++;
}

static void
sandbox_setup_data_connections()
{
  allow_nr_3_arg_match(__NR_socket, 1, PF_INET, 2, SOCK_STREAM, 3, IPPROTO_TCP);
  allow_nr_3_arg_match(__NR_socket,
                       1, PF_INET6,
                       2, SOCK_STREAM,
                       3, IPPROTO_TCP);
  allow_nr(__NR_bind);
  allow_nr(__NR_select);

  allow_nr(__NR_connect);
  allow_nr_2_arg_match(__NR_getsockopt, 2, SOL_SOCKET, 3, SO_ERROR);
  allow_nr_2_arg_match(__NR_setsockopt, 2, SOL_SOCKET, 3, SO_REUSEADDR);
  allow_nr_1_arg_match(__NR_fcntl, 2, F_GETFL);
  allow_nr_2_arg_match(__NR_fcntl, 2, F_SETFL, 3, O_RDWR|O_NONBLOCK);
  allow_nr_2_arg_match(__NR_fcntl, 2, F_SETFL, 3, O_RDWR);

  allow_nr(__NR_listen);
  allow_nr(__NR_accept);
}

static void
sandbox_setup_base()
{
  /* Simple reads and writes on existing descriptors. */
  allow_nr(__NR_read);
  allow_nr(__NR_write);

  /* Needed for memory management. */
  allow_nr_2_arg_match(__NR_mmap,
                       3, PROT_READ|PROT_WRITE,
                       4, MAP_PRIVATE|MAP_ANON);
  allow_nr_1_arg_mask(__NR_mprotect, 3, PROT_READ);
  allow_nr(__NR_munmap);
  allow_nr(__NR_brk);
  /* glibc falls back gracefully if mremap() fails during realloc(). */
  reject_nr(__NR_mremap, ENOSYS);

  /* Misc simple low-risk calls. */
  allow_nr(__NR_rt_sigreturn); /* Used to handle SIGPIPE. */
  allow_nr(__NR_restart_syscall);
  allow_nr(__NR_close);

  /* Always need to be able to exit ! */
  allow_nr(__NR_exit_group);
}

void
sandbox_init()
{
  if (s_syscall_index != 0)
  {
    bug("bad state in sandbox_init");
  }
}

void
sandbox_setup()
{
  sandbox_setup_base();
  sandbox_setup_data_connections();

  /* Misc simple low-risk calls */
  allow_nr(__NR_nanosleep); /* Used for bandwidth / login throttling. */
  allow_nr(__NR_getpid); /* Used by logging. */
  allow_nr(__NR_shutdown); /* Used for QUIT or a timeout. */
  allow_nr_1_arg_match(__NR_fcntl, 2, F_GETFL);
  /* It's safe to allow O_RDWR in fcntl because these flags cannot be changed.
   * Also, sockets are O_RDWR.
   */
  allow_nr_2_arg_mask_match(__NR_fcntl, 3, kOpenFlags|O_ACCMODE, 2, F_SETFL);

  return;
}

void
sandbox_lockdown()
{
  size_t len = (s_syscall_index * 2) +
               (s_1_arg_validations * 3) +
               (s_2_arg_validations * 5) +
               (s_3_arg_validations * 7) +
               5;
  struct sock_filter filters[len];
  struct sock_filter* p_filter = filters;
  struct sock_fprog prog;
  size_t i;
  int ret;

  prog.len = len;
  prog.filter = filters;
  /* Validate the syscall architecture. */
  p_filter->code = BPF_LD+BPF_W+BPF_ABS;
  p_filter->jt = 0;
  p_filter->jf = 0;
  /* Offset 4 for syscall architecture. */
  p_filter->k = 4;
  p_filter++;
  p_filter->code = BPF_JMP+BPF_JEQ+BPF_K;
  p_filter->jt = 1;
  p_filter->jf = 0;
  /* AUDIT_ARCH_X86_64 */
  p_filter->k = 0xc000003e;
  p_filter++;
  p_filter->code = BPF_RET+BPF_K;
  p_filter->jt = 0;
  p_filter->jf = 0;
  /* SECCOMP_RET_KILL */
  p_filter->k = 0;
  p_filter++;

  /* Load the syscall number. */
  p_filter->code = BPF_LD+BPF_W+BPF_ABS;
  p_filter->jt = 0;
  p_filter->jf = 0;
  /* Offset 0 for syscall number. */
  p_filter->k = 0;
  p_filter++;

  for (i = 0; i < s_syscall_index; ++i)
  {
    int block_size = 1;
    if (s_args_3[i])
    {
      block_size = 8;
    }
    else if (s_args_2[i])
    {
      block_size = 6;
    }
    else if (s_args_1[i])
    {
      block_size = 4;
    }
    /* Check for syscall number match. */
    p_filter->code = BPF_JMP+BPF_JEQ+BPF_K;
    p_filter->jt = 0;
    p_filter->jf = block_size;
    p_filter->k = s_syscalls[i];
    p_filter++;
    /* Check argument matches if necessary. */
    if (s_args_3[i])
    {
      p_filter->code = BPF_LD+BPF_W+BPF_ABS;
      p_filter->jt = 0;
      p_filter->jf = 0;
      p_filter->k = 16 + ((s_args_3[i] - 1) * 8);
      p_filter++;
      p_filter->code = BPF_JMP+BPF_JEQ+BPF_K;
      p_filter->jt = 0;
      p_filter->jf = 5;
      p_filter->k = s_vals_3[i];
      p_filter++;
    }
    if (s_args_2[i])
    {
      p_filter->code = BPF_LD+BPF_W+BPF_ABS;
      p_filter->jt = 0;
      p_filter->jf = 0;
      p_filter->k = 16 + ((s_args_2[i] - 1) * 8);
      p_filter++;
      p_filter->code = BPF_JMP+BPF_JEQ+BPF_K;
      p_filter->jt = 0;
      p_filter->jf = 3;
      p_filter->k = s_vals_2[i];
      p_filter++;
    }
    if (s_args_1[i])
    {
      int arg = s_args_1[i];
      int code = BPF_JMP+BPF_JEQ+BPF_K;
      int val = s_vals_1[i];
      int jt = 0;
      int jf = 1;
      if (arg > 100)
      {
        arg -= 100;
        code = BPF_JMP+BPF_JSET+BPF_K;
        val = ~val;
        jt = 1;
        jf = 0;
      }
      p_filter->code = BPF_LD+BPF_W+BPF_ABS;
      p_filter->jt = 0;
      p_filter->jf = 0;
      p_filter->k = 16 + ((arg - 1) * 8);
      p_filter++;
      p_filter->code = code;
      p_filter->jt = jt;
      p_filter->jf = jf;
      p_filter->k = val;
      p_filter++;
    }
    p_filter->code = BPF_RET+BPF_K;
    p_filter->jt = 0;
    p_filter->jf = 0;
    if (!s_errnos[i])
    {
      /* SECCOMP_RET_ALLOW */
      p_filter->k = 0x7fff0000;
    }
    else
    {
      /* SECCOMP_RET_ERRNO */
      p_filter->k = 0x00050000 + s_errnos[i];
    }
    p_filter++;
    if (s_args_1[i])
    {
      /* We trashed the accumulator so put it back. */
      p_filter->code = BPF_LD+BPF_W+BPF_ABS;
      p_filter->jt = 0;
      p_filter->jf = 0;
      p_filter->k = 0;
      p_filter++;
    }
  }
  /* No "allow" matches so kill. */
  p_filter->code = BPF_RET+BPF_K;
  p_filter->jt = 0;
  p_filter->jf = 0;

  /* SECCOMP_RET_KILL */
  p_filter->k = 0;

  ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  if (ret != 0)
  {
    if (errno == EINVAL)
    {
      /* Kernel isn't good enough. */
      return;
    }
    die("prctl PR_SET_NO_NEW_PRIVS");
  }

  ret = prctl(PR_SET_SECCOMP, 2, &prog, 0, 0);
  if (ret != 0)
  {
    die("prctl PR_SET_SECCOMP failed");
  }
}
