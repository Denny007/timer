#PRO1 := con_timeout
PRO2 := list_timer
PRO3 := stress_client
PRO4 := wheel_timer

.PHONY:all
all: $(PRO2) $(PRO3) $(PRO4)

CC = gcc

OBJ1 = connect_timeout.o

OBJ2 += list_timer.o 
OBJ2 += noactive_conn.o

OBJ3 += stress_client.o

OBJ4 += wheel_timer.o

CFLAGS = -g -Wall

$(PRO1):$(OBJ1)
	$(CC) -o $@ $(OBJ1)

$(PRO2):$(OBJ2)
	$(CC) -o $@ $(OBJ2)

$(PRO3):$(OBJ3)
	$(CC) -o $@ $(OBJ3)

$(PRO4):$(OBJ4)
	$(CC) -o $@ $(OBJ4)


%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY:clean
clean:
	rm -rf *.o $(PRO1) $(PRO2) $(PRO3) $(PRO4)
