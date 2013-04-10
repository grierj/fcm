/*
 * A C implementation of the perl fcm-agent
 */

#include "fcm.h"

int main(int argc, char *argv[])
{
  // Let's talk variables!
  fcm_opts_t *opts = NULL;
  struct stat ad;
  struct stat dd;
  apr_pool_t *pool;
  apr_file_t *out;
  apr_file_t *err;
  int c;
  char full_path[PATH_MAX];

  // Initialize apr
  apr_initialize();
  atexit(apr_terminate);
  // delicious memory pool
  apr_pool_create(&pool, NULL);
  // Set up printing to the console
  apr_file_open_stdout(&out, pool);
  apr_file_open_stderr(&err, pool);

  //get our opts going
  opts = apr_palloc(pool, sizeof(fcm_opts_t));

  // Set up some defaults that we will blow away with command line args
  // if need be
  opts->agent_dir = apr_pstrdup(pool, "/var/fcm/agent");
  opts->data_dir = apr_pstrdup(pool, "/var/fcm/data");
  opts->sleep_time = 600;
  opts->verbose = 0;
  opts->run_once = 0;
  opts->out = out;
  opts->err = err;
  opts->pool = pool;

  //TODO(grier): Move arg parsing to its own function
  // -a = agent dir
  // -d = data dir
  // -h = help
  // -s = sleep time
  // -v = verbose
  while ((c = getopt (argc, argv, "a:d:hos:v")) != -1)
    switch (c)
    {
      case 'a':
        realpath(optarg, full_path);
        opts->agent_dir = apr_pmemdup(pool, full_path, sizeof(full_path));
        break;
      case 'd':
        realpath(optarg, full_path);
        opts->data_dir = apr_pmemdup(pool, full_path, sizeof(full_path));
        break;
      case 'h':
        apr_file_printf(out, "Usage: fcm-agent [adhsv]\n");
        apr_file_printf(out, "-a: Agent directory\n");
        apr_file_printf(out, "-d: Data directory\n");
        apr_file_printf(out, "-s: sleep time between runs\n");
        apr_file_printf(out, "-v: print verbose messages\n");
        apr_file_printf(out, "-h: this help message\n");
        return 0;
      case 'o':
        opts->run_once = 1;
        break;
      case 's':
        opts->sleep_time = atoi(optarg);
        break;
      case 'v':
        opts->verbose = 1;
        break;
      case '?':
        if (optopt == 'a' || optopt == 'd' || optopt == 's')
          apr_file_printf(err, "Option -%a requires and argument.\n", optopt);
        else if (isprint (optopt))
          apr_file_printf(err, "Unknown option -%c\n", optopt);
        else
          apr_file_printf(err, "Unknown options character \\x%x\n", optopt);
        return 1;
      default:
        abort();
    }

  if(stat(opts->agent_dir, &ad) != 0 || ! S_ISDIR(ad.st_mode))
  {
    apr_file_printf(err, "Agent directory does not exist: %s\n", opts->agent_dir);
    return 1;
  }
  else if(stat(opts->data_dir, &dd) != 0 || ! S_ISDIR(ad.st_mode))
  {
    apr_file_printf(err, "Data directory does not exist: %s\n", opts->data_dir);
    return 1;
  }

  agent_loop(opts);

  return 0;
}

void agent_loop(fcm_opts_t *opts)
{

  DIR *dir_h;
  struct dirent *file;
  struct stat *af;
  struct stat *df;
  char *agent_file = NULL;
  char *data_file = NULL;
  char *module_name = NULL;
  char *pid_string = NULL;
  char *cpid_string = NULL;
  apr_hash_t *pid_map = NULL;
  pid_t *cpid;
  int *mod_pid;
  int *cstatus;


  while(1)
  {
    apr_pool_t *subpool;
    apr_pool_create(&subpool, opts->pool);

    dir_h = apr_palloc(subpool, sizeof(dir_h));
    file = apr_palloc(subpool, sizeof(struct dirent));
    af = apr_palloc(subpool, sizeof(struct stat));
    df = apr_palloc(subpool, sizeof(struct stat));
    agent_file = apr_palloc(subpool, PATH_MAX);
    data_file = apr_palloc(subpool, PATH_MAX);
    module_name = apr_palloc(subpool, PATH_MAX);
    pid_string = apr_palloc(subpool, 8);
    cpid_string = apr_palloc(subpool, 8);
    pid_map = apr_hash_make(subpool);
    cpid = apr_palloc(subpool, sizeof(pid_t));
    mod_pid = apr_palloc(subpool, sizeof(int));
    cstatus = apr_palloc(subpool, sizeof(int));

    apr_file_printf(opts->out, "\nWaking up for run\n");
    dir_h = opendir(opts->agent_dir);
    while (dir_h)
    {
      if ((file = readdir(dir_h)) != NULL)
      {
        if (apr_strnatcmp(file->d_name, ".") == 0 || apr_strnatcmp(file->d_name, "..") == 0)
        {
          if (opts->verbose) apr_file_printf(opts->out, "Skipping: %s/%s\n", opts->agent_dir, file->d_name);
          continue;
        }
        else
        {
          if (opts->verbose) apr_file_printf(opts->out, "Found: %s/%s\n", opts->agent_dir, file->d_name);
          agent_file = apr_psprintf(subpool, "%s/%s", opts->agent_dir, file->d_name);
          data_file = apr_psprintf(subpool, "%s/%s", opts->data_dir, file->d_name);
          if (stat(agent_file, af) == 0 && S_ISREG(af->st_mode) && stat(data_file, df) == 0 && S_ISREG(df->st_mode))
          {
            apr_file_printf(opts->out, "Running: %s %s\n", agent_file, data_file);
            *mod_pid = run_module(opts, agent_file, data_file);
            pid_string = apr_itoa(subpool, *mod_pid);
            apr_hash_set(pid_map, pid_string, APR_HASH_KEY_STRING, agent_file);
          }
          else
          {
            if (opts->verbose) apr_file_printf(opts->out, "Skipping (not a file): %s/%s\n", opts->agent_dir, file->d_name);
          }
        }
      }
      else
      {
        closedir(dir_h);
        break;
      }
    }

    while ((*cpid = wait(cstatus)) > 0)
    {
      cpid_string = apr_itoa(subpool, *cpid);
      module_name = apr_hash_get(pid_map, cpid_string, APR_HASH_KEY_STRING);
      apr_file_printf(opts->out, "Child %s (%s) terminated with ret code %i\n", module_name, cpid_string, *cstatus);
    }

    apr_file_printf(opts->out, "Loop end, sleeping for %i\n", opts->sleep_time);

    if (opts->run_once) break;

    apr_pool_destroy(subpool);
    sleep(opts->sleep_time);
  }
}

int run_module(fcm_opts_t *opts, char *agent_file, char *data_file)
{
  pid_t pID = fork();

  if (pID == 0)
  {
    if (opts->verbose) apr_file_printf(opts->err, "Child to exec '%s %s'\n", agent_file, data_file);
    execl(agent_file, agent_file, data_file, (char *) 0);
    apr_file_printf(opts->err, "Failed to exec '%s %s'\n", agent_file, data_file);
    return 0;
  }
  else if (pID < 0)
  {
    apr_file_printf(opts->err, "Failed to fork '%s %s'\n", agent_file, data_file);
    return 0;
  }
  return pID;
}
