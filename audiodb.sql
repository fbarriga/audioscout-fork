CREATE TABLE trcks (
	uid INTEGER PRIMARY KEY AUTOINCREMENT,
	composer varchar(30),
	title    varchar(30),
	performer varchar(30),
	date      varchar(10),
	album     varchar(30),
	genre     varchar(10),
	year      int,
	dur       int,
	part      int,
	time      datetime
);