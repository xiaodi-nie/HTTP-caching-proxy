SOURCES=cache.h proxy_server.h
OBJS=$(patsubst %.cpp, %.o, $(SOURCES))
CPPFLAGS=-ggdb3 -std=c++11 -Wall -pedantic -pthread

all: proxy

proxy: $(OBJS)
	g++ $(CPPFLAGS) -o proxy $(SOURCES) main.cpp


clean:
	rm -f *~ *.o proxy *.log