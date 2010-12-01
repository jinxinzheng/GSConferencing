create table tag
(
	id integer primary key,		--autoincrement field
	name varchar(10)
);

create table device
(
       id int primary key,
       ip varchar(15),
       port int,
       tagid int,
       foreign key (tagid) references tag(id)
);

create table vote
(
	id integer primary key,		--autoincrement field
	name varchar(32),
	type int,
	options_count int,
	members varchar(1024)
);


--some test data.

insert into tag(name) values('Original');
insert into tag(name) values('Chinese');
insert into tag(name) values('English');
insert into tag(name) values('French');
insert into tag(name) values('Japanese');
insert into tag(name) values('Spanish');

insert into device(id, ip, port, tagid) values(101, '192.168.1.100', 12345, 1);
insert into device(id, ip, port, tagid) values(102, '192.168.1.101', 12345, 1);
insert into device(id, ip, port, tagid) values(103, '192.168.1.102', 12345, 2);
insert into device(id, ip, port, tagid) values(104, '192.168.1.103', 12345, 3);

insert into vote(name, type, options_count, members) values('vote_1', 1, 2, 'zhangsan;lisi');
insert into vote(name, type, options_count, members) values('vote_2', 2, 2, 'zhangsan;wangwu');
insert into vote(name, type, options_count, members) values('vote_3', 2, 2, 'zhangsan');
insert into vote(name, type, options_count, members) values('vote_4', 3, 2, 'zhangsan');
