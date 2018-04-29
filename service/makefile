service : threadpool.o main.o 
	gcc threadpool.o main.o -o service -lpthread

main.o :MIPSServ.c
	gcc -c MIPSServ.c -o main.o -lpthread

threadpool.o:threadpool.c
	gcc -c threadpool.c -o threadpool.o -lpthread

clean:
	rm *.o
