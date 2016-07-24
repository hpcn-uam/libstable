/*
 * Copyright (C) 2015 - Naudit High Performance Computing and Networking
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include "stable_api.h"
#include "benchmarking.h"
#include "opencl_integ.h"
#include "kde.h"

#define MAX_POINTS 5000

int main(int argc, char **argv)
{
	size_t num_points = 5000;
	size_t num_components = 3;
	size_t i;

	double alphas[] = { 1.2, 0.8, 2 };
	double betas[] = { -0.5, 0.5, 0 };
	double mus[] = { -2, 0, 2 };
	double sigmas[] = { 0.5, 0.8, 0.2 };
	double weights[] = { 0.2, 0.5, 0.3 };
	/*
	double alphas[] = { 0.35, 0.6 };
	double betas[] = { 0.8, 0 };
	double mus[] = { 1.5, 1.65 };
	double sigmas[] = { 0.05, 0.05 };
	double weights[] = { 0.7, 0.3 };
	*/
	double rnd[MAX_POINTS];
	double pdf[MAX_POINTS];
	double pdf_predicted[MAX_POINTS];
	double x[MAX_POINTS];
	double epdf[MAX_POINTS];
	double mn = -5, mx = 5;

	FILE* infile = NULL;
	FILE* outfile;
	int retval = EXIT_SUCCESS;

	StableDist* dist;

	if (argc == 2) {
		infile = fopen(argv[1], "r");

		if (!infile) {
			perror("fopen");
			return EXIT_FAILURE;
		}
	}

	dist = stable_create(alphas[0], betas[0], sigmas[0], mus[0], 0);

	if (!dist) {
		fprintf(stderr, "StableDist creation failure. Aborting.\n");
		return 1;
	}

	if (infile == NULL) {
		stable_set_mixture_components(dist, num_components);

		for (i = 0; i < dist->num_mixture_components; i++) {
			dist->mixture_weights[i] = weights[i];
			stable_setparams(dist->mixture_components[i], alphas[i], betas[i], sigmas[i], mus[i], 0);
		}

		stable_rnd(dist, rnd, num_points);
		gsl_sort(rnd, 1, num_points);
	} else {
		printf("Reading from file %s... ", argv[1]);

		for (i = 0; fscanf(infile, "%lf,%lf", x + i, rnd + i) == 2 && i < MAX_POINTS; i++);

		num_points = i;
		printf("%zu records\n", i);

		stable_set_mixture_components(dist, 2);

		for (i = 0; i < dist->num_mixture_components; i++)
			dist->mixture_weights[i] = 0.5;

		dist->mixture_components[0]->mu_0 = 1.45;
		dist->mixture_components[1]->mu_0 = 1.65;
	}

	outfile = fopen("mixtures_rnd.dat", "w");

	if (!outfile) {
		perror("fopen");
		retval = EXIT_FAILURE;
		goto out;
	}

	for (i = 0; i < num_points; i++)
		fprintf(outfile, "%lf\n", rnd[i]);

	fclose(outfile);
	outfile = fopen("mixtures_dat.dat", "w");

	for (i = 0; i < num_points; i++) {
		x[i] = mn + i * (mx - mn) / num_points;
		epdf[i] = kerneldensity(rnd, x[i], num_points, 0.5);
	}

	stable_activate_gpu(dist);

	stable_pdf(dist, x, num_points, pdf, NULL);

	stable_fit_mixture(dist, rnd, num_points);

	stable_pdf(dist, x, num_points, pdf_predicted, NULL);

	for (i = 0; i < num_points; i++)
		fprintf(outfile, "%lf %lf %lf %lf\n", x[i], pdf[i], pdf_predicted[i], epdf[i]);

	fclose(outfile);

	printf("Done\n");
out:
	stable_free(dist);

	return retval;
}
