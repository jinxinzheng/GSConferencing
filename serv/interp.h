#ifndef __INTERP_H__
#define __INTERP_H__


void interp_add_dup_tag(struct tag *t, struct tag *dup);

void interp_del_dup_tag(struct tag *t);

void interp_dup_pack(struct tag *dup, struct packet *pack);


#endif  /*__INTERP_H__*/
