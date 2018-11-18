all:
	make clean
	make lib
	make build
	#make test
	
clean:
	rm -f *.o *.so *.out

lib: libOrderManager.so

libOrderManager.so: WriteFirstRWLock.h challenge.h OrderManager.h OrderManager.cpp
	g++ -fPIC -shared WriteFirstRWLock.h challenge.h OrderManager.h OrderManager.cpp -o libOrderManager.so
	
TestMain.o: OrderManager.h TestMain.cpp
	g++ TestMain.cpp  -c 

TestMain.out:TestMain.o libOrderManager.so
	g++ TestMain.o libOrderManager.so -o TestMain.out -lpthread
	
build: TestMain.out
	
test: 
	LD_LIBRARY_PATH=. ./TestMain.out
	
