CFLAG= -g -Wall -O0 -rdynamic
gcc=g++ -std=c++11 $(CFLAG) 

default:main

main:
	$(gcc) -I xlib -I libev libev/.libs/libev.a main.cc ps.cc pc.cc xlib/xlib.cc -o px
