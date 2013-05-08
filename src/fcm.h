#ifndef FCM_H
#define FCM_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <apr.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include <apr_time.h>
#include <apr_hash.h>

typedef struct {
  apr_interval_time_t sleep_time;
  int verbose;
  int run_once;
  char *agent_dir;
  char *base_dir;
  char *data_dir;
  apr_file_t *out;
  apr_file_t *err;
  apr_pool_t *pool;
} fcm_opts_t;

typedef struct {
  apr_time_t start_time;
  apr_time_t kill_after;
  pid_t pid;
} fcm_proc_t;

void agent_loop(fcm_opts_t *opts);
int run_module(fcm_opts_t *opts, char *agent_file, char *data_file);
int fcm_parse_opts(fcm_opts_t *opts, int argc, char *argv[]);

#endif