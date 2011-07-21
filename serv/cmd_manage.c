#include "cmd_handler.h"
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "db/md.h"
#include "include/debug.h"

#define BUFLEN 20480

#define append(fmt, a...) l+=sprintf(buf+l,fmt,##a)
#define flush() send(s, buf, l, 0)


#define shift(c) \
({ \
  if( !(p = (c)->args[a++]) ) \
  { \
    trace_err("bad cmd arg\n"); \
    append("FAIL 2\n"); \
    flush(); \
    break; \
  } \
  p; \
})
#define argis(arg) \
  (strcmp(p, (arg))==0)


#include "db/mng.h"


#define m_get(type, c) \
do \
{ \
  iter _it; \
  struct db_##type *_d; \
\
  l = 0; \
  md_iterate_##type##_begin(&_it); \
  append("table " #type "\n"); \
  while( (_d = md_iterate_##type##_next(&_it)) ) \
  { \
    write_##type(_d); \
    if( l>=BUFLEN ) \
    { \
      flush(); \
      l = 0; \
    } \
  } \
  append("\n"); \
  flush(); \
}while(0)

#define m_add(type, c) \
do \
{ \
  struct db_##type _d, *_p = &_d; \
  int _i; \
  read_##type(_p, c); \
  _i = md_add_##type(_p); \
  if( _i==0 ) \
    append("OK\n"); \
  else \
    append("FAIL %d\n", _i); \
  flush(); \
}while(0)

#define m_update(type, c) \
do \
{ \
  int _id = atoi( (c)->args[1] ); \
  struct db_##type *_p = md_find_##type(_id); \
  int _i; \
  read_##type(_p, c); \
  _i = md_update_##type(_p); \
  if( _i==0 ) \
    append("OK\n"); \
  else \
    append("FAIL %d\n", _i); \
  flush(); \
}while(0)

#define m_delete(type, c) \
do \
{ \
  int _id = atoi( (c)->args[1] ); \
  int _i; \
  _i = md_del_##type(_id); \
  if( _i==0 ) \
    append("OK\n"); \
  else \
    append("FAIL %d\n", _i); \
  flush(); \
}while(0)

#define m_get_all(c) \
do \
{ \
  m_get(device, c); \
  m_get(tag, c); \
  m_get(discuss, c); \
  m_get(vote, c); \
}while(0)

static void *serv_manage(void *arg)
{
  int s = (int) arg;

  char buf[BUFLEN+200];
  int l;

  struct cmd c = {0};
  int a=0;
  char *p;

  //c.sock = s;

  const char *op;
  const char *target;

  /* loop until the client closes */
  while( (l=recv(s, buf, 2048, 0)) > 0 )
  {
    char rep[2048];
    int rl;

    buf[l]=0;
    trace_info("manager cmd: %s\n", buf);

    /* copy the command for (possible) reply use */
    strcpy(rep, buf);
    rl = l;
    if (rep[rl-1] == '\n')
      rep[--rl] = 0;

    parse_cmd(buf, &c);

    op = c.cmd;

    if( strcmp(op, "cmd")==0 )
    {
      char *cmd = c.args[0];
      if( strcmp(cmd,"discctrl")==0 ||
          strcmp(cmd,"votectrl")==0 ||
          strcmp(cmd,"regist")==0
        )
      {
        /* pass it to the normal cmd handler. */
        struct cmd newcmd;
        int i;

        memset(&newcmd, 0, sizeof newcmd);
        newcmd.device_id = 0;
        newcmd.cmd = cmd;
        i = 1;
        while( c.args[i] )
        {
          newcmd.args[i-1] = c.args[i];
          i++;
        }

        newcmd.rep = rep;
        newcmd.rl = rl;

        i = handle_cmd(&newcmd);
        if( i != 0 )
          sprintf(rep, "FAIL %d\n", i);
      }
      else
      {
        /* other cmds not allowed. */
        strcpy(rep, "FAIL 1\n");
      }

      send(s, rep, strlen(rep), 0);
      continue;
    }


    /* data cmd */

    target = c.args[0];

    l = 0;

#define GEN_OP(t) \
    if( strcmp(target, #t)==0 ) \
    { \
      if( strcmp(op, "get")==0 ) \
      { \
        m_get(t, &c); \
        send(s, "EOF\n", 4, 0); \
      } \
      else if( strcmp(op, "add")==0 ) \
        m_add(t, &c);  \
      else if( strcmp(op, "update")==0 ) \
        m_update(t, &c); \
      else if( strcmp(op, "delete")==0 ) \
        m_delete(t, &c); \
      else  \
        send(s, "FAIL 1\n", 7, 0);  \
    } else

    GEN_OP(group)
    GEN_OP(user)
    GEN_OP(device)
    GEN_OP(tag)
    GEN_OP(discuss)
    GEN_OP(vote)
    GEN_OP(video)
    GEN_OP(file)

    if( strcmp(target, "all")==0 )
    {
      if( strcmp(op, "get")==0 )
      {
        m_get_all(&c);
        send(s, "EOF\n", 4, 0);
      }
    }
    else
    {
      send(s, "FAIL 1\n", 7, 0);
    }

  }

  close(s);
  trace_info("manager client closed\n");
  return NULL;
}

int handle_cmd_manage(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  int s = cmd->sock;

  /* disable the normal reply  */
  cmd->rl = 0;
  /* don't close the connection */
  cmd->sock = 0;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("login")
  {
    char *u, *p;
    iter it;
    struct db_user *du = NULL;
    pthread_t thread;

    /* authenticate user */

    NEXT_ARG(u);
    NEXT_ARG(p);

    md_iterate_user_begin(&it);
    while( (du = md_iterate_user_next(&it)) )
    {
      if( strcmp(du->name, u)==0 &&
          strcmp(du->password, p)==0 )
      {
        break;
      }
    }
    if( !du )
    {
      return ERR_REJECTED;
    }

    send(s, "OK\n", 3, 0);

    pthread_create(&thread, NULL, serv_manage, (void *)s);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}
