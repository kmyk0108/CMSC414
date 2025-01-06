CC = gcc

ifeq ($(CC),clang)
  STACK_FLAGS = -fno-stack-protector -Wl,-allow_stack_execute
else
  STACK_FLAGS = -fno-stack-protector
endif

CFLAGS = ${STACK_FLAGS} -Wall -Iutil -Iatm -Ibank -Irouter -I. -I/usr/include/openssl
LDFLAGS = -lssl -lcrypto

all: bin bin/atm bin/bank bin/router bin/init atm bank init 

bin:
	mkdir -p bin

bin/atm : atm-side/atm-main.c atm-side/atm.c
	${CC} ${CFLAGS} atm-side/atm.c atm-side/atm-main.c encryption/enc.c -o bin/atm ${LDFLAGS}

bin/bank : bank-side/bank-main.c bank-side/bank.c
	${CC} ${CFLAGS} bank-side/bank.c bank-side/bank-main.c util/hash_table.c util/list.c encryption/enc.c -o bin/bank ${LDFLAGS}

bin/router : router/router-main.c router/router.c
	${CC} ${CFLAGS} router/router.c router/router-main.c -o bin/router ${LDFLAGS}

bin/init : init.c
	${CC} ${CFLAGS} init.c encryption/enc.c -o bin/init ${LDFLAGS}

atm : bin/atm 
	cp bin/atm atm 

bank : bin/bank 
	cp bin/bank bank 

init : bin/init 
	cp bin/init init 

test : util/list.c util/list_example.c util/hash_table.c util/hash_table_example.c
	${CC} ${CFLAGS} util/list.c util/list_example.c -o bin/list-test ${LDFLAGS}
	${CC} ${CFLAGS} util/list.c util/hash_table.c util/hash_table_example.c -o bin/hash-table-test ${LDFLAGS}

clean:
	rm -f bin/* atm bank init *.bank *.card *.atm
