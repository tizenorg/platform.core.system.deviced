#ifndef _SS_COMMON_H
#define _SS_COMMON_H

#define ARRAY_SIZE(name) (sizeof(name)/sizeof(name[0]))

FILE * open_proc_oom_adj_file(int pid, const char *mode);
int get_exec_pid(const char *execpath);
int get_cmdline_name(pid_t pid, char *cmdline, size_t cmdline_size);
int is_vip(int pid);

#endif	/* _SS_COMMON_H */

