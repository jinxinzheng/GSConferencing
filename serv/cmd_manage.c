#include "cmd_handler.h"
#include <unistd.h>
#include <string.h>
#include "db/md.h"
#include "include/debug.h"
#include <include/thread.h>
#include "manage.h"
#include "upgrade.h"

#define BUFLEN  (CMD_MAX+100)

#define _response(buf, len) \
  send(s, buf, len, 0)

/* response a single short formatted string to the cmd */
#define response(cmd, fmt, args...) \
do {  \
  char _buf[BUFLEN];  \
  int _l = sprintf(_buf, "%d " fmt, (cmd)->cmd_seq, ##args);  \
  _response(_buf, _l);  \
} while(0)


#define append(fmt, a...) l+=sprintf(buf+l,fmt,##a)
#define flush() send(s, buf, l, 0)


#define shift(c) \
({ \
  if( !(p = (c)->args[a++]) ) \
    break; \
  p; \
})
#define argis(arg) \
  (strcmp(p, (arg))==0)


#include "db/mng.h"


#define m_get(type, c) \
do \
{ \
  iter _it = NULL; \
  struct db_##type *_d; \
\
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
}while(0)

#define m_add(type, c) \
do \
{ \
  struct db_##type _d, *_p = &_d; \
  int _i; \
  read_##type(_p, c); \
  _i = md_add_##type(_p); \
  if( _i==0 ) \
    response(c, "OK\n"); \
  else \
    response(c, "FAIL %d\n", _i); \
}while(0)

#define m_update(type, c) \
do \
{ \
  int _id;  \
  int _i; \
  struct db_##type *_p; \
  char *_arg = (c)->args[1];  \
  if( !_arg ) \
  { \
    response(c, "FAIL %d\n", ERR_INVL_ARG); \
    break;  \
  } \
  _id = atoi( _arg ); \
  _p = md_find_##type(_id); \
  read_##type(_p, c); \
  _i = md_update_##type(_p); \
  if( _i==0 ) \
    response(c, "OK\n"); \
  else \
    response(c, "FAIL %d\n", _i); \
}while(0)

#define m_delete(type, c) \
do \
{ \
  int _id;  \
  int _i; \
  char *_arg = (c)->args[1];  \
  if( !_arg ) \
  { \
    response(c, "FAIL %d\n", ERR_INVL_ARG); \
    break;  \
  } \
  if( strcmp(_arg, "all")==0 ) {  \
    _i = md_clear_##type(); \
  } else {  \
    _id = atoi( _arg ); \
    _i = md_del_##type(_id); \
  } \
  if( _i==0 ) \
    response(c, "OK\n"); \
  else \
    response(c, "FAIL %d\n", _i); \
}while(0)

#define m_get_all(c) \
do \
{ \
  m_get(device, c); \
  m_get(tag, c); \
  m_get(regist, c);\
  m_get(discuss, c); \
  m_get(vote, c); \
}while(0)

static void *serv_manage(void *arg)
{
  int s = (int) arg;

  char buf[BUFLEN];
  int l;

  struct cmd c;
  int r;
  int a=0;
  char *p;
  int i;

  //c.sock = s;

  const char *op;
  const char *target;

  /* loop until the client closes */
  while( (l=recv(s, buf, BUFLEN, 0)) > 0 )
  {
    char rep[BUFLEN];
    int rl;

    buf[l]=0;
    trace_info("manager cmd: %s\n", buf);

    /* copy the command for (possible) reply use */
    strcpy(rep, buf);
    rl = l;
    if (rep[rl-1] == '\n')
      rep[--rl] = 0;

    memset(&c, 0, sizeof c);
    if( (r=parse_cmd(buf, &c)) != 0 )
    {
      response(&c, "FAIL %d\n", r);
      continue;
    }

    op = c.cmd;

    if( strcmp(op, "cmd")==0 )
    {
      char *cmd = c.args[0];

      if( !cmd )
      {
        strcpy(rep, "FAIL 1\n");
        goto end_cmd;
      }

      if( strcmp(cmd,"chair_ctl")==0 )
      {
        int allow = 0;

        if( c.args[1] )
          allow = atoi(c.args[1]);
        else
        {
          sprintf(rep, "FAIL 1\n");
          goto end_cmd;
        }

        manage_set_allow_chair(allow);
        manage_notify_all_chairs();
        sprintf(rep, "OK\n");
      }
      else if( strcmp(cmd, "upgrade")==0 )
      {
        if( upgrade() )
          sprintf(rep, "OK\n");
        else
          sprintf(rep, "FAIL 1\n");
      }
      else
      if( strcmp(cmd,"discctrl")==0 ||
          strcmp(cmd,"votectrl")==0 ||
          strcmp(cmd,"regist")==0   ||
          strcmp(cmd,"ptc")==0
        )
      {
        /* pass it to the normal cmd handler. */
        struct cmd newcmd;

        memset(&newcmd, 0, sizeof newcmd);
        newcmd.device_id = 0;
        newcmd.cmd = cmd;

        rl = sprintf(rep, "0 %s ", cmd);

        i = 1;
        while( c.args[i] )
        {
          newcmd.args[i-1] = c.args[i];

          rl += sprintf(rep+rl, "%s ", c.args[i]);

          i++;
        }

        newcmd.rep = rep;
        newcmd.rl = rl;

        i = handle_cmd(&newcmd);
        if( i != 0 )
        {
          sprintf(rep, "FAIL %d\n", i);
          goto end_cmd;
        }

        /* fix back the manager reply */
        if( rep[0]=='0' && rep[1]==' ' )
          i=2;
        else
          i=0;
        l = sprintf(buf, "%d %s %s", c.device_id, c.cmd, rep+i);

        _response(buf, l);
        continue;
      }
      else
      {
        /* other cmds not allowed. */
        strcpy(rep, "FAIL 1\n");
        goto end_cmd;
      }

end_cmd:
      response(&c, "%s", rep);
      continue;
    }


    /* data cmd */

    target = c.args[0];

    if( !target )
    {
      response(&c, "FAIL 1\n");
      continue;
    }

    l = 0;

#define GEN_OP(t) \
    if( strcmp(target, #t)==0 ) \
    { \
      if( strcmp(op, "get")==0 ) \
      { \
        append("%d OK\n", c.cmd_seq); \
        m_get(t, &c); \
        append("EOF\n"); \
        flush(); \
      } \
      else if( strcmp(op, "add")==0 ) \
        m_add(t, &c);  \
      else if( strcmp(op, "update")==0 ) \
        m_update(t, &c); \
      else if( strcmp(op, "delete")==0 ) \
        m_delete(t, &c); \
      else  \
        response(&c, "FAIL 1\n");  \
    } else

    GEN_OP(group)
    GEN_OP(user)
    GEN_OP(device)
    GEN_OP(tag)
    GEN_OP(auth_card)
    GEN_OP(regist)
    GEN_OP(discuss)
    GEN_OP(vote)
    GEN_OP(video)
    GEN_OP(file)

    if( strcmp(target, "all")==0 )
    {
      if( strcmp(op, "get")==0 )
      {
        append("%d OK\n", c.cmd_seq);
        m_get_all(&c);
        append("EOF\n");
        flush();
      }
    }
    else
    {
      response(&c, "FAIL 1\n");
    }

  }

  close(s);

  manage_set_active(0);
  manage_notify_all_chairs();

  trace_info("manager client closed\n");
  return NULL;
}

static int validate_user(const char *user, const char *pswd)
{
  iter it = NULL;
  struct db_user *du = NULL;

  while( (du = md_iterate_user_next(&it)) )
  {
    if( strcmp(du->name, user)==0 &&
        strcmp(du->password, pswd)==0 )
    {
      break;
    }
  }

  return (du != NULL);
}

static int change_passwd(const char *user, const char *new_pswd)
{
  iter it = NULL;
  struct db_user *du = NULL;

  while( (du = md_iterate_user_next(&it)) )
  {
    if( strcmp(du->name, user)==0 )
    {
      break;
    }
  }
  if( !du )
  {
    return 0;
  }

  strcpy(du->password, new_pswd);
  md_update_user(du);

  return 1;
}

static int cmd_manage(struct cmd *cmd)
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

    /* authenticate user */

    NEXT_ARG(u);
    NEXT_ARG(p);

    if( !validate_user(u, p) )
    {
      return ERR_REJECTED;
    }

    response(cmd, "OK\n");

    manage_set_active(1);
    manage_notify_all_chairs();

    start_thread(serv_manage, (void *)s);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}


static int cmd_server_user(struct cmd *cmd)
{
  char *subcmd;
  int a=0;

  NEXT_ARG(subcmd);

  if (0);

  SUBCMD("login")
  {
    char *u, *p;

    NEXT_ARG(u);
    NEXT_ARG(p);

    if( !validate_user(u, p) )
      return ERR_REJECTED;

    REP_OK(cmd);
  }

  SUBCMD("passwd")
  {
    char *u, *p;

    NEXT_ARG(u);
    NEXT_ARG(p);

    if( !change_passwd(u, p) )
      return ERR_OTHER;

    REP_OK(cmd);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}

CMD_HANDLER_SETUP(manage);
CMD_HANDLER_SETUP(server_user);
