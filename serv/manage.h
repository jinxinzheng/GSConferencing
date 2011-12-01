#ifndef __MANAGE_H__
#define __MANAGE_H__


struct device;

void manage_add_chair(struct device *d);
void manage_set_active(int a);
void manage_set_allow_chair(int a);
void manage_notify_chair(struct device *d);
void manage_notify_all_chairs();


#endif  /*__MANAGE_H__*/
