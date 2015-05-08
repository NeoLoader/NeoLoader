#include "GlobalHeader.h"
#include "Maths.h"

unsigned int isqrt(unsigned int i) // http://www.finesse.demon.co.uk/steven/sqrt.html
{
	int max;
	int j;
	unsigned int a, c, d, s;
	a=0; c=0;
	if (i>=0x10000)
		max=15; 
	else 
		max=7;
	s = 1<<(max*2);
	j=max;
	do {
		d = c + (a<<(j+1)) + s;
		if (d<=i) {
			c=d; 
			a |= 1<<j; 
		};
		if (d!=i) { 
			s>>=2; 
			j--; 
		} 
	} while ((j>=0)&&(d!=i));
	return a;
}