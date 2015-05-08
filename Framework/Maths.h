#pragma once 

#include "./NeoHelper/neohelper_global.h"

template <class T>
T Mean(vector<T>& x) 
{
	int n = x.size();
    T sum = 0;
    for(int i=0; i<n; i++)
        sum += x[i];
    return n ? sum/n : 0;
}
 
template <class T>
T Median(vector<T>& x) 
{
	size_t n = x.size();
	if(n < 1)
		return T();

    // the following two loops sort the array x in ascending order
    for(size_t i=0; i<n-1; i++) 
	{
        for(size_t j=i+1; j<n; j++) 
		{
            if(x[j] < x[i]) 
			{
                // swap elements
                T temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }
 
    if(n%2==0) // if there is an even number of elements, return mean of the two elements in the middle
        return((x[n/2 - 1] + x[n/2]) / 2);
    else       // else return the element in the middle
        return x[n/2];
}

NEOHELPER_EXPORT unsigned int isqrt(unsigned int i);