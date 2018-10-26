CFLAG= -g -Wall -O0 -rdynamic
gcc=g++ -std=c++11 $(CFLAG) 

default:main

main:
	$(gcc) -I. main.cc server/ps.cc server/pc.cc xlib/xlib.cc xlib/net_util.cc -o px
check:
	find ./ -regex ".*\.cc\|.*\.h" | xargs ./tools/cpplint.py
