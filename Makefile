objects = main.o fileparse.o
app = minicron

edit : $(objects)
	gcc -o $(app) $(objects) -Wall -lexplain

main.o: fileparse.h
fileparse.o: fileparse.h taskstruct.h

.PHONY : clean
clean :
	rm $(app) $(objects)