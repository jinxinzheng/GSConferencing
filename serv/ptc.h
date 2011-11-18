#ifndef __PTC_H__
#define __PTC_H__


void add_ptc(struct device *ptc);

/* put the dev to ptc stack but not send cmd */
void ptc_put(struct device *d);

void ptc_push(struct device *d);

void ptc_remove(struct device *d);

void ptc_go_current();

int is_ptc(struct device *d);


#endif  /*__PTC_H__*/
