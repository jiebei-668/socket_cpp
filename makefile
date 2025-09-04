# 在make前需要单独制作动态库和静态库 libsocket.so libsocket.a 命令在最后
all: demo04 demo15
demo04: demo04.c
	g++ -g -o demo04 demo04.c
demo15: demo15.cpp
	g++ -g -o demo15 demo15.cpp 
clean:
	rm -rf bin/*
	rm -rf demo04 demo15 


