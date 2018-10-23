CFLAG= -g -Wall -O0 -rdynamic
gcc=g++ -std=c++11 $(CFLAG) 

default:main

main:
	$(gcc) -I xlib -lev main.cc ps.cc pc.cc xlib/xlib.cc xlib/net_util.cc -o px
