objects = main.o fileparse.o Daemon.o the_thing.o
app = minicron

edit : $(objects)
	gcc -o $(app) $(objects) -Wall -lexplain

main.o: fileparse.h Daemon.h the_thing.h
fileparse.o: fileparse.h taskstruct.h

.PHONY : clean
clean :
	rm $(app) $(objects)
