calc :
	cc calc.c -o calc -Og -g -lpthread

test : calc
	./calc

run : test

sample : calc
	./calc | head -20

debug : calc
	gdb calc

opt :
	cc calc.c -o calc -O3 -lpthread

clean :
	rm -f calc
