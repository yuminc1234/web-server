XX=g++

SRCS=main.cpp\
     timer.cpp\
     command.cpp\
     server.cpp\
     request.cpp\
     log.cpp\
     epoll.cpp\
     threadpool.cpp\
     connectionpool.cpp\
     connectionraii.cpp

OBJS=$(SRCS:.cpp=.o)

LIBS=`mysql_config --libs`\
     -lpthread

EXEC=server

CXXFLAGS+=`mysql_config --cflags --libs`

server:$(OBJS)
	$(XX) -o $(EXEC) $(OBJS) $(LIBS)

%.o:%.cpp
	$(XX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(OBJS)
