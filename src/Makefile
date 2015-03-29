CC := gcc
SRC :=  dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c test_assign2_1.c
OBJ :=  dberror.o storage_mgr.o buffer_mgr_stat.o buffer_mgr.o test_assign2_1.o

assignment2: $(OBJ)
	$(CC) -w -o test_assign2 $?
	$(CC) -w -o test_assign2_2 dberror.c storage_mgr.c buffer_mgr_stat.c buffer_mgr.c test_assign2_2.c

%.o: %.c
	$(CC) -w -g -c $<

clean:
	rm -rf test_assign2 test_assign2_2 *.o
