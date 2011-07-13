-- 会议组
create table [group]
(
 id int,            -- id
 name varchar(32),  -- 名称
 discuss_mode int,  -- 讨论模式
 discuss_id int,    -- 议题id
 regist_start int,  -- 开始报到
 regist_mode int,   -- 报到模式
 regist_expect int, -- 报到应到人数
 regist_arrive int, -- 报到实到人数
 vote_id int,       -- 表决id
 vote_results varchar(256), --表决结果
 primary key (id)
);

-- 用户管理
create table [user]
(
 id integer,            -- id, autoincrement
 name varchar(64),      -- 用户名
 password varchar(64),  -- 密码
 primary key (id)
);

-- 语言通道
create table tag
(
 id integer primary key,  -- id, autoincrement
 name varchar(32)         -- 通道名称
);

-- 终端设备
create table device
(
 id int primary key,    -- id
 ip varchar(15),        -- ip地址
 port int,              -- 端口号
 tagid int,             -- 通道id
 user_id varchar(64),   -- 用户id
 user_card int,         -- 用户卡号
 user_name varchar(64), -- 用户名字
 user_gender int,       -- 用户性别
 online int,            -- 是否在线
 sub1 int,              -- 收听通道1
 sub2 int,              -- 收听通道2
 discuss_chair int,     -- 是否讨论主席
 discuss_open int,      -- 是否开启话筒
 regist_master int,     -- 是否报到主席
 regist_reg int,        -- 是否已报到
 vote_master int,       -- 是否表决主席
 vote_choice int,       -- 表决结果
 ptc_id int,            -- 云台设备id
 ptc_cmd varchar(256),  -- 云台控制命令
 foreign key (tagid) references tag(id)
);

-- 投票表决
create table vote
(
 id integer primary key,  -- id, autoincrement
 name varchar(32),     -- 表决名称
 type int,             -- 表决类型
 options_count int,    -- 表决项数量
 members varchar(1024) -- 表决代表
);

-- 讨论议题
create table discuss
(
 id integer primary key,  -- id, autoincrement
 name varchar(64),     -- 议题名称
 members varchar(1024) -- 讨论代表
);

-- 视频文件
create table video
(
 id integer primary key,  -- id, autoincrement
 name varchar(64),  -- 视频名称
 path varchar(260)  -- 视频路径
);

-- 文稿文件
create table [file]
(
 id integer primary key,  -- id, autoincrement
 path varchar(260)  -- 文稿路径
);

create table sysconfig
(
 id integer primary key,  -- id, autoincrement
 name varchar(64),   -- config name
 value varchar(64)   -- config value
);

-- prepared data

insert into [group] (
  id,
  name,
  discuss_mode,
  discuss_id,
  regist_start,
  regist_mode,
  regist_expect,
  regist_arrive,
  vote_id,
  vote_results)
values (
  1,
  'default',
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  '0');

insert into [user] (name,password) values('admin','admin');

insert into tag(name) values('Original');
insert into tag(name) values('Chinese');
insert into tag(name) values('English');
insert into tag(name) values('French');
insert into tag(name) values('Japanese');
insert into tag(name) values('Spanish');

insert into vote(name, type, options_count, members) values('vote_1', 1, 2, '101,102,103');
insert into vote(name, type, options_count, members) values('vote_2', 2, 2, '101,102');
insert into vote(name, type, options_count, members) values('vote_3', 2, 2, '101');
insert into vote(name, type, options_count, members) values('vote_4', 3, 2, '104');

insert into discuss(name, members) values('test_discuss_1', '101,102,103,104');
insert into discuss(name, members) values('test_discuss_2', '101,102,103');

insert into video(name, path) values('video1', '/video1.avi');
insert into video(name, path) values('video2', '/video2.avi');

insert into [file](path) values('file1.txt');
insert into [file](path) values('file2.txt');

insert into sysconfig (name, value) values("contrast","0");
insert into sysconfig (name, value) values("language","0");
