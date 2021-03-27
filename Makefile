calc :
	cc calc.c -o calc -Og -g -lpthread

test : calc
	./calc

sample : calc
	./calc | head -40

debug : calc
	gdb calc

clean :
	rm -f calc
