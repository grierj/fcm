/*
 * Currently the home of all functions dealing the the c port of fcm agent
 *
 */

#include "fcm.h"

int fcm_parse_opts(fcm_opts_t *opts, int argc, char *argv[]) {
  int c;
  char full_path[PATH_MAX];
  // Wat?
  //char with_base_path[PATH_MAX];

  // Args
  // -a = agent dir
  // -d = data dir
  // -h = help
  // -o = run once
  // -s = sleep time
  // -v = verbose
  //
  while ((c = getopt (argc, argv, "a:b:d:hos:v")) != -1)
  {
    switch (c)
    {
      case 'a':
        realpath(optarg, full_path);
        opts->agent_dir = apr_pmemdup(opts->pool, full_path, sizeof(full_path));
        break;
      case 'b':
        realpath(optarg, full_path);
        opts->base_dir = apr_pmemdup(opts->pool, full_path, sizeof(full_path));
        break;
      case 'd':
        realpath(optarg, full_path);
        opts->data_dir = apr_pmemdup(opts->pool, full_path, sizeof(full_path));
        break;
      case 'h':
        apr_file_printf(opts->out, "Usage: fcm-agent [adhsv]\n");
        apr_file_printf(opts->out, "-a: Agent directory\n");
        apr_file_printf(opts->out, "-b: Base FCM dir\n");
        apr_file_printf(opts->out, "-d: Data directory\n");
        apr_file_printf(opts->out, "-r: csv of scripts to run\n");
        apr_file_printf(opts->out, "-i: iteration time (in seconds)\n");
        apr_file_printf(opts->out, "-v: print verbose messages\n");
        apr_file_printf(opts->out, "-h: this help message\n");
        return 1;
      case 'i':
        opts->itr_time = apr_atoi64(optarg);
        break;
      case 'o':
        opts->run_once = 1;
        break;
      case 'v':
        opts->verbose = 1;
        break;
      case '?':
        if (optopt == 'a' || optopt == 'd' || optopt == 's')
          apr_file_printf(opts->err, "Option -%a requires and argument.\n", optopt);
        else if (isprint (optopt))
          apr_file_printf(opts->err, "Unknown option -%c\n", optopt);
        else
          apr_file_printf(opts->err, "Unknown options character \\x%x\n", optopt);
        return 1;
      default:
        abort();
    }
  }

  if (check_dir_exists(opts->base_dir) != 0)
  {
    apr_file_printf(opts->err, "Base directory '%s' does not exist\n",
        opts->base_dir);
    return 1;
  }

  if (check_dir_exists(opts->agent_dir) != 0)
  {
    //What was I doing here?
    //with_base_path = apr_pstrcat(opts->pool, opts->base_dir, "/", opts->agent_dir);
    opts->agent_dir = apr_pstrcat(opts->pool, opts->base_dir, "/", opts->agent_dir);
  }


  return 0;
}

int check_dir_exists(char *dir)
{
  struct stat dir_stat;
  if(stat(dir, &dir_stat) != 0 || ! S_ISDIR(dir_stat.st_mode))
  {
    return 1;
  }
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
  apr_hash_t *running_pids = NULL;
  apr_hash_index_t *hi;
  apr_time_t *now_micro;
  apr_time_t *until_micro;
  apr_time_t *sleep_micro;
  pid_t *cpid;
  pid_t *curr_pid;
  int *mod_pid;
  int *cstatus;


  while(1)
  {
    // Make a subpool for each run of the loop
    apr_pool_t *subpool;
    apr_pool_create(&subpool, opts->pool);

    // ALLOCATE ALL THE THINGS
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
    running_pids = apr_hash_make(subpool);
    //hi = apr_palloc(subpool, sizeof(apr_hash_index_t));
    now_micro = apr_palloc(subpool, sizeof(apr_time_t));
    until_micro = apr_palloc(subpool, sizeof(apr_time_t));
    // allocate in the main pool
    sleep_micro = apr_palloc(opts->pool, sizeof(apr_time_t));
    curr_pid = apr_palloc(subpool, sizeof(pid_t));
    cpid = apr_palloc(subpool, sizeof(pid_t));
    mod_pid = apr_palloc(subpool, sizeof(int));
    cstatus = apr_palloc(subpool, sizeof(int));

    apr_file_printf(opts->out, "\nWaking up for run\n");
    dir_h = opendir(opts->agent_dir);

    // Now is the start of the iteration, until is when we should start
    // killing processes
    *now_micro = apr_time_now();
    until_micro = (now_micro + (opts->itr_time * 1000000));

    while (dir_h)
    {
      if ((file = readdir(dir_h)) != NULL)
      {
        if (apr_strnatcmp(file->d_name, ".") == 0 ||apr_strnatcmp(file->d_name, "..") == 0)
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
            apr_hash_set(running_pids, pid_string, APR_HASH_KEY_STRING, now_micro);
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

    // Old blocking method
    //while ((*cpid = wait(cstatus)) > 0)
    //{
    //  cpid_string = apr_itoa(subpool, *cpid);
    //  module_name = apr_hash_get(pid_map, cpid_string, APR_HASH_KEY_STRING);
    //  apr_file_printf(opts->out, "Child %s (%s) terminated with ret code %i\n", module_name, cpid_string, *cstatus);
    //}

    // New non-blocking method
    // Iterate over the children until they're all gone or we run out of time
    apr_file_printf(opts->out, "Looking at pids\n");
    while ((apr_hash_count(running_pids) != 0) && (*until_micro > apr_time_now()))
    {
      apr_file_printf(opts->out, "Looking at hash\n");
      for (hi = apr_hash_first(opts->pool, running_pids); hi; hi = apr_hash_next(hi))
      {
        // don't care about the length or value
        apr_hash_this(hi, (void *)cpid_string, NULL, NULL);
        // We've hit the end if we're null
        if (cpid_string != NULL)
        {
          *curr_pid = apr_atoi64(cpid_string);
          *cpid = waitpid(*curr_pid, cstatus, WNOHANG);
          // WNOHANG returns -1 on state change, 0 on no change
          if (cpid != 0)
          {
            // remove the running pid entry
            apr_hash_set(running_pids, cpid_string, APR_HASH_KEY_STRING, NULL);
            // get our module name
            module_name = apr_hash_get(pid_map, cpid_string, APR_HASH_KEY_STRING);
            // print some stuff
            apr_file_printf(opts->out, "Child %s (%s) terminated with ret code %i\n", module_name, cpid_string, *cstatus);
          }
        }
      }
    }
    apr_file_printf(opts->out, "Done looking at pids\n");
    // Still some running pids
    if (apr_hash_count(running_pids) != 0)
    {
      for (hi = apr_hash_first(opts->pool, running_pids); hi; hi = apr_hash_next(hi))
      {
        // don't care about the length or value
        apr_hash_this(hi, (void *)cpid_string, NULL, NULL);
        // We've hit the end if we're null
        if (cpid_string != NULL)
        {
          *curr_pid = apr_atoi64(cpid_string);
          kill(*curr_pid, SIGKILL);
        }
      }
    }
    apr_file_printf(opts->out, "Done killing pids\n");

    // don't sleep if we're only running once
    if (opts->run_once) break;

    // Still time left to sleep?
    *now_micro = apr_time_now();
    if (until_micro > now_micro)
    {
      *sleep_micro = until_micro - now_micro;
      apr_file_printf(opts->out, "Loop end, sleeping for %i seconds\n", *sleep_micro/1000000);
    } 
    else
    {
      *sleep_micro = 0;
    }

    // remove the subpool every run
    apr_pool_destroy(subpool);

    // Sleep after the pool destroy so we can more easily test for leaks
    apr_sleep(*sleep_micro);
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

int check_pause(fcm_opts_t *opts, char *agent_file)
{
}
