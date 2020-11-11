CPPOBJS = $(wildcard *.cpp)
COBJS = $(wildcard *.c)
LIBS = -lpthread 
TARGET = animal
%.o:%.cpp
	g++ $(LIBS) -c $< -o $@
%.o:%.c
	g++ $(LIBS) -c $< -o $@

all:
	g++ $(COBJS) $(CPPOBJS) $(LIBS) -g -o $(TARGET)

clean:
	-rm $(TARGET)
