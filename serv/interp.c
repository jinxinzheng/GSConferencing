#include "include/pack.h"
#include "tag.h"
#include "network.h"

/* notify all clients that the tag
 * is replicated. */
static void notify_interp_rep(struct tag *t)
{
  static int sock = 0;
  struct pack_ucmd ucmd;

  ucmd.type = PACKET_UCMD;
  ucmd.cmd = UCMD_INTERP_REP_TAG;
  ucmd.u.interp.tag = t->id;
  ucmd.u.interp.rep = t->interp.rep? t->interp.rep->id:0;

  /* don't use the tag's cast sock.
   * we are not in the casting thread.. */
  if( !sock )
    sock = open_broadcast_sock();

  broadcast_local(sock, &ucmd, sizeof(ucmd));
}

void interp_set_rep_tag(struct tag *t, struct tag *rep)
{
  t->interp.rep = rep;
  notify_interp_rep(t);
}

void interp_del_rep(struct tag *t)
{
  t->interp.rep = NULL;
  notify_interp_rep(t);
}
