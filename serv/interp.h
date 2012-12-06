#ifndef __INTERP_H__
#define __INTERP_H__


void interp_set_rep_tag(struct tag *t, struct tag *rep);

void interp_del_rep(struct tag *t);

/* in cmd_interp.c */
void apply_interp_mode(struct tag *t, struct device *d);


#endif  /*__INTERP_H__*/
