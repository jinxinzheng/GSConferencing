#include "cmd_handler.h"
#include <string.h>
#include "sys.h"
#include "devctl.h"
#include "cast.h"
#include <include/util.h>
#include <include/types.h>
#include <cmd/t2cmd.h>
#include "interp.h"

static int cmd_sub(struct cmd *cmd)
{
  char *p;

  p = cmd->args[0];

  if (!p)
    return 1;
  else
  {
    struct device *d;
    struct tag *t;
    int tid, gid;
    int unsub = 0;
    int repid = 0;

    THIS_DEVICE(cmd, d);

    gid = d->group->id;
    tid = atoi(p);

    if( tid == 0 )
    {
      return ERR_OTHER;
    }

    if( tid < 0 )
    {
      tid = -tid;
      unsub = 1;
    }

    t = request_tag(gid, tid);

    if( unsub )
    {
      dev_unsubscribe(d, t);
    }
    else
    {
      dev_subscribe(d, t);
      if( t->interp.rep )
      {
        repid = t->interp.rep->id;
      }
    }

    /* send cmd to the ucast-dev. */
    if( t->ucast )
    {
      char buf[1024];
      struct type2_cmd *c = (struct type2_cmd *) buf;
      struct t2cmd_sub *args = (struct t2cmd_sub *) c->data;
      c->type = 2;
      c->cmd = T2CMD_SUB;
      c->len = sizeof(*args);
      args->id = d->id;
      args->addr = d->addr.sin_addr.s_addr;
      args->port = d->addr.sin_port;
      args->flag = 0;
      args->flag |= d->active? 1:0;
      args->sub = !unsub;
      args->tag = tid;
      device_cmd(t->ucast, (char *)c, T2CMD_SIZE(c));
    }

    /* check for interp dev in RE mode. */
    if( d->type == DEVTYPE_INTERP &&
        d->tag->interp.mode == INTERP_RE )
    {
      if( !d->discuss.open )
      {
        if( unsub )
        {
          interp_del_rep(d->tag);
        }
        else
        {
          apply_interp_mode(d->tag, d);
        }
      }
    }

    device_save(d);

    REP_ADD(cmd, "OK");
    REP_ADD_NUM(cmd, repid);
    REP_END(cmd);
  }

  return 0;
}

CMD_HANDLER_SETUP(sub);
