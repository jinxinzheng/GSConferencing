#include <stdio.h>
#include <stdlib.h>
#include <sqlite.h>
#include <string.h>
#include "db.h"


sqlite *db = NULL;		//pointer to sqlite database.


int db_init()
{
	char *errmsg;
	db = sqlite_open("sys.db", 0777, &errmsg);
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
	int ret, cols, rows;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from device;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		dev->id = atol(values[0]);
		strcpy(dev->ip, values[1]);
		dev->port = atoi(values[2]);
		dev->tagid = atoi(values[3]);
		dev++;
/*
        int i;
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
         "WHERE id = %ld;", dev->ip, dev->port, dev->tagid, dev->id);

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

	sprintf(sqlcmd, "insert into device(id, ip, port, tagid) values(%ld, '%s', %d, %d);",
         dev->id, dev->ip, dev->port, dev->tagid);

//	printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
  	{
		fprintf(stderr, "SQL error: %s\n%s\n", errmsg, sqlcmd);
  	}
  	else
  	{
		printf("%ld was inserted as ID %d\n", dev->id, sqlite_last_insert_rowid(db));
	}

	return ret;
}


int db_del_device(long id)
{
	char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from device where id = %ld;", id);

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
	int ret, cols, rows;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from tag;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		tag->id = atol(values[0]);
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

	sprintf(sqlcmd, "UPDATE tag SET name = '%s' WHERE id = %ld;", tag->name, tag->id);

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

int db_del_tag(long id)
{
    char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from tag where id = %ld;", id);

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
	int ret, cols, rows;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from vote;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		vote->id = atol(values[0]);
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
         "members = '%s' WHERE id = %ld;", vote->name, vote->type, vote->options_count, vote->members, vote->id);

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

int db_del_vote(long id)
{
    char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from vote where id = %ld;", id);

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

int db_get_discuss(struct db_discuss *discuss)
{
  const char **values, **columnNames;
	char *errmsg;
	int ret, cols, rows;
	char sqlcmd[64];
	sqlite_vm *vm;

	rows = 0;
	strcpy(sqlcmd, "select * from discuss;");
	ret = sqlite_compile(db, sqlcmd, NULL, &vm, &errmsg);

	while (sqlite_step(vm, &cols, &values, &columnNames) == SQLITE_ROW)
	{
		rows++;
		discuss->id = atol(values[0]);
		strcpy(discuss->name, values[1]);
		strcpy(discuss->members, values[2]);
		discuss++;
	}

	ret = sqlite_finalize(vm, &errmsg);

	if (ret != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", errmsg);
	}

	return rows;
}

int db_update_discuss(struct db_discuss *discuss)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "UPDATE discuss SET name = '%s', "
         "members = '%s' WHERE id = %ld;", discuss->name, discuss->members, discuss->id);

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

int db_add_discuss(struct db_discuss *discuss)
{
    char *errmsg;
	int ret;
	char sqlcmd[256];

	sprintf(sqlcmd, "insert into discuss(name, members) "
         "values('%s', '%s');", discuss->name, discuss->members);

//	printf("%s\n", sqlcmd);

	ret = sqlite_exec(db, sqlcmd, NULL, NULL, &errmsg);

	if (ret != SQLITE_OK)
  	{
		fprintf(stderr, "SQL error: %s\n%s\n", errmsg, sqlcmd);
  	}
  	else
  	{
		printf("%s was inserted as ID %d\n", discuss->name, sqlite_last_insert_rowid(db));
	}

	return ret;
}

int db_del_discuss(long id)
{
    char *errmsg;
	int ret;
	char sqlcmd[64];

	sprintf(sqlcmd, "delete from discuss where id = %ld;", id);

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
