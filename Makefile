all: lab1.c
	gcc -Wall -Wextra -g -o simpsh lab1.c
dist:
	tar -czvf lab1.tar.gz Makefile lab1.c check.sh pg98.txt
check:
	chmod +x ./check.sh
	./check.sh
clean:
	rm -rf lab1.tar.gz
