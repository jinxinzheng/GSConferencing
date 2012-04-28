#ifndef  __CMDERR_H__
#define  __CMDERR_H__


/* cmd error codes */
enum {
  ERR_PARSE = 1,
  ERR_BAD_CMD,
  ERR_NOT_REG,
  ERR_REJECTED,
  ERR_INVL_ARG,
  ERR_OTHER,

  /* cmd specific errors */
  ERR_REGIST_ALREADY = 0x101,

  ERR_DEV_DISABLED = 0x201,
};


#endif  /*__CMDERR_H__*/
