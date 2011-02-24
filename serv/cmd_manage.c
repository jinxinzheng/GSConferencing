#include "cmd_handler.h"
#include <pthread.h>
#include <string.h>
#include "db/md.h"

#define BUFLEN 2048

#define append(fmt, a...) l+=sprintf(buf+l,fmt,##a)
#define flush() send(s, buf, l, 0)


#define shift(c) \
  (p = (c)->args[a++])
#define argis(arg) \
  (strcmp(p, (arg))==0)


#define write_device(d) \
  append("%d:%s:%d:%d:%d:%d:%s:%d\n", \
    (int)d->id, d->ip, d->port, d->tagid, d->online, \
    d->user_card, d->user_name, d->user_gender)

#define read_device(d, c) \
do \
{ \
  a = 1; \
  d->id = atoi( shift(c) ); \
  strcpy( d->ip, shift(c) ); \
  d->port = atoi( shift(c) ); \
  d->tagid = atoi( shift(c) ); \
  d->user_card = atoi( shift(c) ); \
  strcpy( d->user_name, shift(c) ); \
  d->user_gender = atoi( shift(c) ); \
}while(0)


#define write_tag(t) \
  append("%d:%s\n", (int)t->id, t->name)

#define read_tag(t, c) \
do \
{ \
  a = 1; \
  t->id = atoi( shift(c) ); \
  strcpy( t->name, shift(c) ); \
}while(0)


#define write_discuss(d) \
  append("%d:%s:%s\n", (int)d->id, d->name, d->members)

#define read_discuss(d, c) \
do \
{ \
  a = 1; \
  d->id = atoi( shift(c) ); \
  strcpy( d->name, shift(c) ); \
  strcpy( d->members, shift(c) ); \
}while(0)


#define write_vote(v) \
  append("%d:%s:%d:%s\n", (int)v->id, v->name, v->type, v->members)

#define read_vote(v, c) \
do \
{ \
  a = 1; \
  v->id = atoi( shift(c) ); \
  strcpy( v->name, shift(c) ); \
  v->type = atoi( shift(c) ); \
  strcpy( v->members, shift(c) ); \
}while(0)


#define m_get(type, c) \
do \
{ \
  iter _it; \
  struct db_##type *_d; \
\
  l = 0; \
  md_iterate_##type##_begin(&_it); \
  append("table " #type "\n"); \
  while( _d = md_iterate_##type##_next(&_it) ) \
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

  struct cmd c;
  int a=0;
  char *p;

  //c.sock = s;

  const char *op;
  const char *target;

  /* loop until the client closes */
  while( (l=recv(s, buf, 2048, 0)) > 0 )
  {
    buf[l]=0;
    fprintf(stderr, "manager cmd: %s\n", buf);
    parse_cmd(buf, &c);

    op = c.cmd;
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
    } else

    GEN_OP(device)
    GEN_OP(tag)
    GEN_OP(discuss)
    GEN_OP(vote)

    if( strcmp(target, "all")==0 )
    {
      if( strcmp(op, "get")==0 )
      {
        m_get_all(&c);
        send(s, "EOF\n", 4, 0);
      }
    }

  }

  close(s);
  fprintf(stderr, "manager client closed\n");
  return NULL;
}

static struct {
  const char *user;
  const char *pass;
} userdb[] = {
  { "admin", "admin" }
};

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
    int i,l;
    pthread_t thread;

    /* authenticate user */

    NEXT_ARG(u);
    NEXT_ARG(p);

    l = sizeof userdb/sizeof userdb[0];
    for( i=0 ; i<l ; i++ )
    {
      if( strcmp(userdb[i].user, u)==0 &&
          strcmp(userdb[i].pass, p)==0 )
      {
        break;
      }
    }
    if( i>=l )
    {
      return ERR_REJECTED;
    }

    send(s, "OK\n", 3, 0);

    pthread_create(&thread, NULL, serv_manage, (void *)s);
  }

  else return 2; /*sub cmd not found*/

  return 0;
}
