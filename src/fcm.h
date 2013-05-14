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
  apr_interval_time_t itr_time;
  int verbose;
  int run_once;
  char *agent_dir;
  char *base_dir;
  char *data_dir;
  char *pause_dir;
  apr_file_t *out;
  apr_file_t *err;
  apr_pool_t *pool;
} fcm_opts_t;

void agent_loop(fcm_opts_t *opts);
int run_module(fcm_opts_t *opts, char *agent_file, char *data_file);
int fcm_parse_opts(fcm_opts_t *opts, int argc, char *argv[]);
int pid_hash_wait_with_timeout(fcm_opts_t *opts, apr_hash_t *pid_hash, int timeout);
void pid_hash_send_signal(fcm_opts_t *opts, apr_hash_t *pid_hash, int signal);
int find_or_merge_dir(fcm_opts_t *opts, char **dir);

#endif
