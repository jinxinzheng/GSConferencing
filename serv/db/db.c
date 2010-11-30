#include <stdio.h>
#include <string.h>
#include <sqlite.h>
#include "db.h"


sqlite *db = NULL;		//pointer to sqlite database.


int db_init()
{
	char *errmsg;
	db = sqlite_open("test.db", 0777, &errmsg);
	if (db == 0)
	{
		fprintf(stderr, "Could not open database: %s\n", errmsg);
		sqlite_freemem(errmsg);
		return SQLITE_ERROR;
	}
	else
	{
		printf("Successfully connected to database.\n");
		return SQLITE_OK;
	}
}


void db_close()
{
	sqlite_close(db);
}


/*
int callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct device **pdev = (struct device **)pArg;
	int i;
	(*pdev)->id = atoi(argv[0]);
	strcpy((*pdev)->ip, argv[1]);
	(*pdev)->port = atoi(argv[2]);
	(*pdev)->tagid = atoi(argv[3]);
	for (i = 0; i < argc; i++)
	{
		printf("%s = %s; ", columnNames[i], argv[i]);
	}
	(*pdev)++;
	printf("\n");
	return 0;
}
*/


int db_get_device(struct db_device *dev)
{
	const char **values, **columnNames;
	char *errmsg;
	int ret, cols, rows, i;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from device;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		dev->id = atoi(values[0]);
		strcpy(dev->ip, values[1]);
		dev->port = atoi(values[2]);
		dev->tagid = atoi(values[3]);
		dev++;
/*
		for (i = 0; i < cols; i++) {
			printf("%s: %s; ", columnNames[i], values[i]);
		}
		printf("\n");
*/	}

	ret = sqlite_finalize(vm, &errmsg);
/*
	ret = sqlite_exec(db, "PRAGMA SHOW_DATATYPES=ON", NULL, NULL, NULL);
	ret = sqlite_exec(db, sqlcmd, callback, &dev, &errmsg);
*/
	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}

	return rows;
}


int db_update_device(struct db_device *dev)
{
	char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "UPDATE device SET ip = '%s', port = %d, tagid = %d "
         "WHERE id = %d;", dev->ip, dev->port, dev->tagid, dev->id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}


int db_add_device(struct db_device *dev)
{
	char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "insert into device(id, ip, port, tagid) values(%d, '%s', %d, %d);",
         dev->id, dev->ip, dev->port, dev->tagid);

//	printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
  	{
		fprintf(stderr, "SQL error: %s\n%s\n", errmsg, sqlcmd);
  	}
  	else
  	{
		printf("%d was inserted as ID %d\n", dev->id, sqlite_last_insert_rowid(db));
	}

	return ret;
}


int db_del_device(int id)
{
	char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from device where id = %d;", id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}


int db_get_tag(struct db_tag *tag)
{
  const char **values, **columnNames;
	char *errmsg;
	int ret, cols, rows, i;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from tag;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		tag->id = atoi(values[0]);
		strcpy(tag->name, values[1]);
		tag++;
	}

	ret = sqlite_finalize(vm, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}

	return rows;
}

int db_update_tag(struct db_tag *tag)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "UPDATE tag SET name = '%s' WHERE id = %d;", tag->name, tag->id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}

int db_add_tag(struct db_tag *tag)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "insert into tag(name) values('%s');", tag->name);

//	printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
  	{
		fprintf(stderr, "SQL error: %s\n%s\n", errmsg, sqlcmd);
  	}
  	else
  	{
		printf("%s was inserted as ID %d\n", tag->name, sqlite_last_insert_rowid(db));
	}

	return ret;
}

int db_del_tag(int id)
{
    char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from tag where id = %d;", id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}


int db_get_vote(struct db_vote *vote)
{
  const char **values, **columnNames;
	char *errmsg;
	int ret, cols, rows, i;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from vote;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		vote->id = atoi(values[0]);
		strcpy(vote->name, values[1]);
		vote->type = atoi(values[2]);
		vote->options_count = atoi(values[3]);
		strcpy(vote->members, values[4]);
		vote++;
	}

	ret = sqlite_finalize(vm, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}

	return rows;
}

int db_update_vote(struct db_vote *vote)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "UPDATE vote SET name = '%s', type = %d, options_count = %d, "
         "members = '%s' WHERE id = %d;", vote->name, vote->type, vote->options_count, vote->members, vote->id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}

int db_add_vote(struct db_vote *vote)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "insert into vote(name, type, options_count, members) "
         "values('%s', %d, %d, '%s');", vote->name, vote->type, vote->options_count, vote->members);

//	printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
  	{
		fprintf(stderr, "SQL error: %s\n%s\n", errmsg, sqlcmd);
  	}
  	else
  	{
		printf("%s was inserted as ID %d\n", vote->name, sqlite_last_insert_rowid(db));
	}

	return ret;
}

int db_del_vote(int id)
{
    char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from vote where id = %d;", id);

//  printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}
	else
	{
		printf("%d row(s) were changed\n", sqlite_changes(db));
	}

	return ret;
}


/*
 * just for testing...
 */
int __main(void)
{
	struct db_device dev[10];

	int ret;
	ret = db_init();
//	printf("%d\n", ret);
/*
	ret = db_get_device(dev);
	printf("%d\n", ret);

	int i;
	for (i = 0; i < ret; i++)
	{
		printf("%d; %s; %d; %d\n", dev[i].id, dev[i].ip, dev[i].port, dev[i].tagid);
	}       */
/*
	struct db_device dev2;

	dev2.id = 201;
	strcpy(dev2.ip, "192.168.150.110");
	dev2.port = 33333;
	dev2.tagid = 2;

	ret = db_add_device(&dev2);
	printf("%d\n", ret);
*/
/*
	struct db_device dev3;

	dev3.id = 201;
	strcpy(dev3.ip, "10.100.150.1");
	dev3.port = 5555;
	dev3.tagid = 2;

	ret = db_update_device(&dev3);
	printf("%d\n", ret);
*/
/*    int devid = 201;
    ret = db_del_device(devid);
    printf("%d\n", ret);
*/

    struct db_tag tag[10];

/*    ret = db_get_tag(tag);

    int i;
	for (i = 0; i < ret; i++)
	{
		printf("%d; %s\n", tag[i].id, tag[i].name);
	}
*/
/*    struct db_tag tag2;

    strcpy(tag2.name, "abcdefg");

    ret = db_add_tag(&tag2);
    printf("%d\n",ret);
*/
/*    struct db_tag tag3;

    tag3.id = 7;
    strcpy(tag3.name, "qwertyui");

    ret = db_update_tag(&tag3);
    printf("%d\n",ret);
*/
/*    int tagid = 7;

    ret = db_del_tag(7);
    printf("%d\n",ret);
*/

    struct db_vote vote[10];

/*    ret = db_get_vote(vote);

    int i;
	for (i = 0; i < ret; i++)
	{
		printf("%d; %s; %d; %d; %s\n", vote[i].id, vote[i].name,
            vote[i].type, vote[i].options_count, vote[i].members);
	}
*/
/*    struct db_vote vote2 = {0, "test_vote", 2, 2, "lisi;wangwu;dingliu"};

    ret = db_add_vote(&vote2);
    printf("%d\n",ret);
*/
/*    struct db_vote vote3 = {5, "test_vote2", 1, 2, "lisi;wangwu"};

    ret = db_update_vote(&vote3);
    printf("%d\n",ret);
*/
    int voteid = 5;
    ret = db_del_vote(voteid);
    printf("%d\n",ret);

	db_close();

	return 0;
}
