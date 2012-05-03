#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>
#include  <version.h>
#include  <cmd/cmd.h>
#include  "brcmd.h"

#define UPGRADE_DIR   "/upgrade"

static char curr_client_ver[1024];

static int read_file_line(const char *path, char *buf)
{
  FILE *f;
  int len;
  if( !(f=fopen(path, "r")) )
  {
    return 0;
  }
  if( !fgets(buf, 1024, f) )
  {
    fclose(f);
    return 0;
  }

  len = strlen(buf);
  /* remove ending '\n' */
  if( buf[len-1] == '\n' )
    buf[len-1] = 0;
  fclose(f);
  return 1;
}

void load_client_version()
{
  if( !read_file_line(UPGRADE_DIR "/client.version", curr_client_ver) )
  {
    /* can't find current client version. */
    curr_client_ver[0] = 0;
  }
}

const char *get_client_version()
{
  return curr_client_ver;
}

/* returns true if client needs upgrade. */
static int check_client_version()
{
  const char *cur_ver = curr_client_ver;
  char new_ver[1024];
  if( cur_ver[0]==0 )
  {
    /* can't find current client version.
     * should notify upgrade. */
    return 1;
  }
  if( !read_file_line(UPGRADE_DIR "/temp/client.version", new_ver) )
    return 0;
  if( strcmp(cur_ver, new_ver)==0 )
    return 0;
  else
    return 1;
}

static int check_server_version()
{
  const char *cur_ver = build_rev;
  char new_ver[1024];
  int l;
  if( !read_file_line(UPGRADE_DIR "/temp/server.version", new_ver) )
    return 0;
  l = strlen(new_ver);
  return strcmp(cur_ver, new_ver);
}

static void notify_client_upgrade()
{
  char buf[1024];
  struct cmd cmd;
  cmd.rep = buf;
  cmd.rl = sprintf(cmd.rep, "0 upgrade %s\n", curr_client_ver);
  brcast_cmd_to_all(&cmd);
}

int upgrade()
{
  int r;
  int new_client;
  int new_server;
  char cmd[1024];

  /* extract the archive. it succeeds only if
   * the archive passes all validations. */
  sprintf(cmd, "cd %s && ./prepare.sh", UPGRADE_DIR);
  r = system(cmd);
  if( r!=0 )
    return 0;

  new_client = check_client_version();

  new_server = check_server_version();

  /* commit the files. */
  sprintf(cmd, "cd %s && mv temp/* ./", UPGRADE_DIR);
  r = system(cmd);
  if( r!=0 )
    return 0;

  if( new_client )
  {
    load_client_version();
    notify_client_upgrade();
    if(new_server)
      /* server will be killed. stay 10 seconds for notifying. */
      sleep(10);
  }

  if( new_server )
  {
    /* upgrade server if needed.
     * we may be killed during upgrade! */
    sprintf(cmd, "cd %s && ./upgrade.sh '%s' %d", UPGRADE_DIR, build_rev, getpid());
    r = system(cmd);
    if( r!=0 )
      return 0;
  }

  return 1;
}
