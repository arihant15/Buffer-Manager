SRC := \
	dberror.c \
	storage_mgr.c \
	buffer_mgr_stat.c \
	buffer_mgr.c \
	test_assign2_1.c 
	
OBJ := \
	dberror.o \
	storage_mgr.o \
	buffer_mgr_stat.o \
	buffer_mgr.o \
	test_assign2_1.o 
	

assign2: $(OBJ)
	gcc -w -o assign2 $?
	

%.o: %.c
	gcc -g -c $<

clean:
	rm -rf assign2 *.o
