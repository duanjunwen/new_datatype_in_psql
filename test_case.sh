cd /srvr/z5242677/testing/
cp ../postgresql-12.5/src/tutorial/intset.c .
cp ../postgresql-12.5/src/tutorial/intset.source .
cd /srvr/z5242677/testing/
source /srvr/z5242677/env
./run_test.py > test_result_detail.txt
./run_test.py TRUE > test_result_sum.txt
