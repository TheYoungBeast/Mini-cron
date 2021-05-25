objects = main.o fileparse.o signals.o sort.o misc.o
app = minicron

all: $(objects)
	gcc -std=c18 -o $(app) $(objects) -Wall -Wextra -Wpedantic -lexplain

debug: $(objects)
	gcc -g -ggdb -std=c18 $(objects) -Wall -Wextra -Wpedantic -lexplain

main.o: fileparse.h signals.h
fileparse.o: fileparse.h taskstruct.h
signals.o: signals.h
sort.o: sort.h taskstruct.h
misc.o: misc.h

.PHONY : clean
clean :
	rm $(app) $(objects)

clean-debug:
	rm $(objects) a.out
