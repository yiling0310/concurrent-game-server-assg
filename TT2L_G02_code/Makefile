CXX = g++
CXXFLAGS = -Wall -pthread -lrt

all: server client

server: server.cpp
	$(CXX) server.cpp -o server $(CXXFLAGS)

client: client.cpp
	$(CXX) client.cpp -o client $(CXXFLAGS)

clean:
	rm -f server client game.log /dev/shm/*game_shm* 