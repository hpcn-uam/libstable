#ifndef OPENCL_INTEG_H
#define OPENCL_INTEG_H

#include "openclenv.h"

struct stable_clinteg {
	double interv_begin;
	double interv_end;
	unsigned int subdivisions;
	int points_rule; // Points for GK rule.
	struct openclenv env;

	double* h_gauss;
	double* h_kronrod;
	double result;
	double abs_error;
	double* subinterval_errors;
};

int stable_clinteg_init(struct stable_clinteg* cli);
double stable_clinteg_integrate(struct stable_clinteg* cli, double a, double b, double epsabs, double epsrel, unsigned short limit,
                   double *result, double *abserr, double beta_, double k1, double xxipow);
void stable_clinteg_teardown(struct stable_clinteg* cli);


#endif

