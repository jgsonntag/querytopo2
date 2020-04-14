#
#  Makefile for querytopo2 program
#
CFLAGS = -lm
SRC = querytopo2.cpp 
OBJ = querytopo2.o
ULIBS = /home/sonntag/Libcpp/libjohn2.a


/home/sonntag/bin/querytopo2 : querytopo2
	mv querytopo2 /home/sonntag/bin/querytopo2

querytopo2: $(OBJ) $(ULIBS)
	g++ $(CFLAGS) -L/home/sonntag/Libcpp -o querytopo2 $(OBJ) -ljohn2
	
querytopo2.o: querytopo2.cpp
	g++ -c querytopo2.cpp

$(ULIBS): FORCE
	cd /home/sonntag/Libcpp; $(MAKE)

FORCE:
