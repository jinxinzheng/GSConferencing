#ifndef DB_H_INCLUDED
#define DB_H_INCLUDED

/*
 * just for testing...
 */
struct db_device
{
	long id;
	char ip[32];
	int port;
	int tagid;

  struct db_device *hash_next;
  struct db_device **hash_pprev;
};

struct db_tag
{
	long id;
	char name[10];

  struct db_tag *hash_next;
  struct db_tag **hash_pprev;
};

struct db_vote
{
	long id;
	char name[32];
	int type;
	int options_count;
	char members[1024];

  struct db_vote *hash_next;
  struct db_vote **hash_pprev;
};


/*
 * init sqlite database.
 *
 * the return value indicates the database status.
 */
int db_init();

/*
 * close sqlite database.
 *
 * no return value.
 */
void db_close();


/*
 * get all device from database.
 *
 * the argument point to a db_device array which stored devices info.
 * the return value indicates how many device in database.
 */
int db_get_device(struct db_device *);

/*
 * update a device info in database.
 *
 * the argument point to a db_device variable which you want to update.
 * the return value indicates the operation result.
 */
int db_update_device(struct db_device *);

/*
 * add a device info in database.
 *
 * the argument point to a db_device variable which you want to add.
 * the return value indicates the operation result.
 * 
 * note: the device id is not an auto increment field, so you have to specify it.
 */
int db_add_device(struct db_device *);

/*
 * delete a device info in database.
 *
 * the argument is the device id which you want to delete.
 * the return value indicates the operation result.
 */
int db_del_device(int id);


/*
 * get all tag from database.
 *
 * the argument point to a db_tag array which stored tag info.
 * the return value indicates how many tag in database.
 */
int db_get_tag(struct db_tag *);

/*
 * update a tag info in database.
 *
 * the argument point to a db_tag variable which you want to update.
 * the return value indicates the operation result.
 */
int db_update_tag(struct db_tag *);

/*
 * add a tag info in database.
 *
 * the argument point to a db_tag variable which you want to add.
 * the return value indicates the operation result.
 *
 * note: the tag id is an auto increment field, so it's unnecessary.
 */
int db_add_tag(struct db_tag *);

/*
 * delete a tag info in database.
 *
 * the argument is the tag id which you want to delete.
 * the return value indicates the operation result.
 */
int db_del_tag(int id);



/*
 * get all vote from database.
 *
 * the argument point to a db_vote array which stored vote info.
 * the return value indicates how many vote in database.
 */
int db_get_vote(struct db_vote *);

/*
 * update a vote info in database.
 *
 * the argument point to a db_vote variable which you want to update.
 * the return value indicates the operation result.
 */
int db_update_vote(struct db_vote *);

/*
 * add a vote info in database.
 *
 * the argument point to a db_vote variable which you want to add.
 * the return value indicates the operation result.
 *
 * note: the tag id is an auto increment field, so it's unnecessary.
 */
int db_add_vote(struct db_vote *);

/*
 * delete a vote info in database.
 *
 * the argument is the vote id which you want to delete.
 * the return value indicates the operation result.
 */
int db_del_vote(int id);



#endif // DB_H_INCLUDED
