#ifndef _CONF_H_
#define _CONF_H_

int conf_load();
int conf_save();

const char *conf_get_val(const char *name);
void conf_set_val(const char *name, const char *val);

#endif
