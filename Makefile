CFLAG= -g -Wall -O0 -rdynamic
gcc=g++ -std=c++11 $(CFLAG) 

default:main

main:
	$(gcc) -I. main.cc server/*.cc xlib/*.cc -o px
check:
	find ./ -regex ".*\.cc\|.*\.h" | xargs ./tools/cpplint.py
