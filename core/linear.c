#include <stdio.h>
#include <stdlib.h>

#include "earthworm_simple_funcs.h"

void calloc_error(char error_text[])
{
        logit("e","Memory allocation error in linear codes...\n");
        logit("e","%s\n",error_text);
        logit("e","...now exiting to system...\n");
        exit(1);
}


float *float_vector(int nl, int nh)
{
        float *v;

        v=(float *)calloc((unsigned) (nh-nl+1),sizeof(float));
        if (!v) calloc_error("allocation failure in float_vector()");
        return v-nl;
}

int *int_vector(int nl, int nh)
{
        int *v;

        v=(int *)calloc((unsigned) (nh-nl+1),sizeof(int));
        if (!v) calloc_error("allocation failure in int_vector()");
        return v-nl;
}

float **float_matrix(int n1, int n2, int ncl, int nch)
{
        int i;
        float **m;

        m=(float **) calloc((unsigned) (n2-n1+1),sizeof(float*));
        if (!m) calloc_error("allocation failure 1 in float_matrix()");
        m -= n1;

        for(i=n1;i<=n2;i++) {
                m[i]=(float *) calloc((unsigned) (nch-ncl+1),sizeof(float));
                if (!m[i]) calloc_error("allocation failure 2 in float_matrix()");
                m[i] -= ncl;
        }
        return m;
}

void free_float_vector(float *v, int nl)
{
        free((char*) (v+nl));
}


void free_int_vector(int *v, int nl)
{
        free((char*) (v+nl));
}



void free_float_matrix(float **m, int n1, int n2, int ncl)
{
        int i;

        for(i=n2;i>=n1;i--) 
        {
               free((char*) (m[i]+ncl));
        }
        free((char*) (m+n1));
}
