cd Desktop
rm -vf /srvr/z5242677/postgresql-12.5/src/tutorial/intset.c
rm -vf /srvr/z5242677/postgresql-12.5/src/tutorial/intset.source
rm -vf /srvr/z5242677/postgresql-12.5/src/tutorial/intset.so
rm -vf /srvr/z5242677/postgresql-12.5/src/tutorial/intset.o
rm -vf /srvr/z5242677/postgresql-12.5/src/tutorial/Makefile

cp -rf intset.c /srvr/z5242677/postgresql-12.5/src/tutorial/intset.c
cp -rf intset.source /srvr/z5242677/postgresql-12.5/src/tutorial/intset.source
cp -rf test.sql /srvr/z5242677/postgresql-12.5/src/tutorial/test.sql
cp -rf Makefile /srvr/z5242677/postgresql-12.5/src/tutorial/Makefile
cd /srvr/z5242677/postgresql-12.5/src/tutorial
make
pgs start
dropdb test
createdb test
psql test -f intset.sql
psql test -f test.sql
