#!/bin/sh

# This program tests features of GET DATA /TYPE=TXT input program that
# it has in common with DATA LIST, using tests drawn from
# data-list.sh.

TEMPDIR=/tmp/pspp-tst-$$
TESTFILE=$TEMPDIR/`basename $0`.sps

# ensure that top_builddir  are absolute
if [ -z "$top_builddir" ] ; then top_builddir=. ; fi
if [ -z "$top_srcdir" ] ; then top_srcdir=. ; fi
top_builddir=`cd $top_builddir; pwd`
PSPP=$top_builddir/src/ui/terminal/pspp

# ensure that top_srcdir is absolute
top_srcdir=`cd $top_srcdir; pwd`

STAT_CONFIG_PATH=$top_srcdir/config
export STAT_CONFIG_PATH


cleanup()
{
     cd /
     rm -rf $TEMPDIR
}


fail()
{
    echo $activity
    echo FAILED
    cleanup;
    exit 1;
}


no_result()
{
    echo $activity
    echo NO RESULT;
    cleanup;
    exit 2;
}

pass()
{
    cleanup;
    exit 0;
}

mkdir -p $TEMPDIR

cd $TEMPDIR

# Create command file.
activity="create program"
cat > $TESTFILE << EOF
get data /type=txt /file=inline /delimiters="|X"
 /variables=A f7.2 B f7.2 C f7.2 D f7.2.
begin data.
1|23X45|2.03
2X22|34|23|
3|34|34X34
end data.

list.

get data /type=txt /file=inline /delimiters=', ' /delcase=variables 4
 /firstcase=2 /variables=A f7.2 B f7.2 C f7.2 D f7.2.
begin data.
# This record is ignored.
,1,2,3
,4,,5
6
7,
8 9
0,1,,,
,,,,
2

3
4
5
end data.
list.

get data /type=txt /file=inline /delimiters='\t' /delcase=variables 4
 /firstcase=3 /variables=A f7.2 B f7.2 C f7.2 D f7.2.
begin data.
# These records
# are skipped.
1	2	3	4
1	2	3	
1	2		4
1	2		
1		3	4
1		3	
1			4
1			
	2	3	4
	2	3	
	2		4
	2		
		3	4
		3	
			4
			
end data.
list.

get data /type=txt /file=inline /arrangement=fixed /fixcase=3 /variables=
	/1 start 0-19 adate
	/2 end 0-19 adate
	/3 count 0-2 f.
begin data.
07-22-2007
10-06-2007
321
07-14-1789
08-26-1789
4
01-01-1972
12-31-1999
682
end data.
list.

get data /type=txt /file=inline /arrangement=fixed /fixcase=2 /variables=
	/1 x 0 f 
           y 1 f.
begin data.
12

34

56

78

90

end data.
list.
EOF
if [ $? -ne 0 ] ; then no_result ; fi


activity="run program"
$SUPERVISOR $PSPP --testing-mode $TESTFILE
if [ $? -ne 0 ] ; then fail ; fi

activity="compare output"
perl -pi -e 's/^\s*$//g' $TEMPDIR/pspp.list
diff -b  $TEMPDIR/pspp.list - << EOF
       A        B        C        D
-------- -------- -------- --------
    1.00    23.00    45.00     2.03
    2.00    22.00    34.00    23.00
    3.00    34.00    34.00    34.00
       A        B        C        D
-------- -------- -------- --------
     .       1.00     2.00     3.00
     .       4.00      .       5.00
    6.00     7.00      .       8.00
    9.00      .00     1.00      .
     .        .        .        .
     .        .        .       2.00
     .       3.00     4.00     5.00
       A        B        C        D
-------- -------- -------- --------
    1.00     2.00     3.00     4.00
    1.00     2.00     3.00      .
    1.00     2.00      .       4.00
    1.00     2.00      .        .
    1.00      .       3.00     4.00
    1.00      .       3.00      .
    1.00      .        .       4.00
    1.00      .        .        .
     .       2.00     3.00     4.00
     .       2.00     3.00      .
     .       2.00      .       4.00
     .       2.00      .        .
     .        .       3.00     4.00
     .        .       3.00      .
     .        .        .       4.00
     .        .        .        .
               start                  end count
-------------------- -------------------- -----
          07/22/2007           10/06/2007   321
          07/14/1789           08/26/1789     4
          01/01/1972           12/31/1999   682
x y
- -
1 2
3 4
5 6
7 8
9 0
EOF
if [ $? -ne 0 ] ; then fail ; fi

pass;