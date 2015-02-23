/* tests/fittest
 *
 * Example program that test Libstable parameter estimation methods.
 *
 * Copyright (C) 2013. Javier Royuela del Val
 *                     Federico Simmross Wattenberg
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
 *
 *
 *  Javier Royuela del Val.
 *  E.T.S.I. Telecomunicación
 *  Universidad de Valladolid
 *  Paseo de Belén 15, 47002 Valladolid, Spain.
 *  jroyval@lpi.tel.uva.es
 */
#include "stable_api.h"
#include "benchmarking.h"
#include <time.h>
#include <stdlib.h>

typedef int (*fitter)(StableDist *, const double *, const unsigned int);

struct fittest
{
	fitter func;
	short gpu_enabled;
	const char *name;
};

int main (int argc, char *argv[])
{
	double alfa, beta, sigma, mu;
	double *data;
	int i = 1, iexp, N, Nexp;
	int seed;
	double total_duration, start, end;
	struct fittest tests[] =
	{
		{ stable_fit_mle, 0, "MLE" },
		{ stable_fit_mle2d, 0, "M2D"},
		{ stable_fit_koutrouvelis, 0, "KTR"},
		{ stable_fit_mle, 1, "MLE" },
		{ stable_fit_mle2d, 1, "M2D"},
		{ stable_fit_koutrouvelis, 1, "KTR"},
	};
	struct fittest *test;
	size_t num_tests = sizeof tests / sizeof(struct fittest);

	StableDist *dist = NULL;

	alfa = 1.5;
	beta = 0.75;
	sigma = 5.0;
	mu = 15.0;
	N = 100;
	Nexp = 5;
	seed = -1;

	printf("%f %f %f %f %d %d\n", alfa, beta, sigma, mu, N, Nexp);
	if ((dist = stable_create(alfa, beta, sigma, mu, 0)) == NULL)
	{
		printf("Error when creating the distribution");
		exit(1);
	}

	stable_set_THREADS(1);
	stable_set_absTOL(1e-16);
	stable_set_relTOL(1e-8);
	stable_set_FLOG("errlog.txt");

	if (seed < 0)
		stable_rnd_seed(dist, time(NULL));
	else
		stable_rnd_seed(dist, seed);

	/* Random sample generation */
	data = (double *)malloc(N * Nexp * sizeof(double));

	stable_rnd(dist, data, N * Nexp);

	for (i = 0; i < num_tests; i++)
	{
		test = tests + i;
		total_duration = 0;
		for (iexp = 0; iexp < Nexp; iexp++)
		{
			stable_fit_init(dist, data + iexp * N, N, NULL, NULL);

			if (test->gpu_enabled)
				stable_activate_gpu(dist);
			else
				stable_deactivate_gpu(dist);

			start = get_ms_time();
			test->func(dist, data + iexp * N, N);
			end = get_ms_time();

			total_duration += end - start;
		}

		printf("%s", test->name);

		if (test->gpu_enabled)
			printf("_GPU");

		printf("\t%lf\n", total_duration / Nexp);
	}


	free(data);
	stable_free(dist);

	fclose(stable_get_FLOG());

	return 0;
}
