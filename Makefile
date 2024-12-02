CC = gcc

ifeq ($(CC),clang)
  STACK_FLAGS = -fno-stack-protector -Wl,-allow_stack_execute
else
  STACK_FLAGS = -fno-stack-protector -z execstack
endif

CFLAGS = ${STACK_FLAGS} -Wall -Iutil -Iatm -Ibank -Irouter -I. -I/usr/include/openssl
LDFLAGS = -lssl -lcrypto

all: bin bin/atm bin/bank bin/router bin/init

bin:
	mkdir -p bin

bin/atm : atm/atm-main.c atm/atm.c
	${CC} ${CFLAGS} atm/atm.c atm/atm-main.c encryption/enc.c -o bin/atm ${LDFLAGS}

bin/bank : bank/bank-main.c bank/bank.c
	${CC} ${CFLAGS} bank/bank.c bank/bank-main.c util/hash_table.c util/list.c encryption/enc.c -o bin/bank ${LDFLAGS}

bin/router : router/router-main.c router/router.c
	${CC} ${CFLAGS} router/router.c router/router-main.c -o bin/router ${LDFLAGS}

bin/init : init.c
	${CC} ${CFLAGS} init.c encryption/enc.c -o bin/init ${LDFLAGS}

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} ${CFLAGS} util/list.c util/list_example.c -o bin/list-test ${LDFLAGS}
	${CC} ${CFLAGS} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test ${LDFLAGS}

clean:
	rm -f bin/*
