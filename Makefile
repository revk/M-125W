LIBMQTT=-DLIBMQTT -lmosquitto

SQLINC=$(shell mysql_config --include)
SQLLIB=$(shell mysql_config --libs)
SQLVER=$(shell mysql_config --version | sed 'sx\..*xx')
CCOPTS=${SQLINC} -I. -I/usr/local/ssl/include -D_GNU_SOURCE -g -Wall -funsigned-char -lm
OPTS=-L/usr/local/ssl/lib ${SQLLIB} ${CCOPTS}

all: git daikinac

SQLlib/sqllib.o: SQLlib/sqllib.c
	make -C SQLlib

mqttweigh: mqttweigh.c SQLlib/sqllib.o
	cc -O -o $@ $< ${OPTS} -lpopt ${LIBMQTT} -ISQLlib SQLlib/sqllib.o -lcurl -DSQLLIB

git:
	git submodule update --init

update:
	git submodule update --remote --merge
