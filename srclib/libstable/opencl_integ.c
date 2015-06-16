#include "stable_api.h"
#include "opencl_common.h"
#include "benchmarking.h"

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef max
#define max(a,b) (a < b ? b : a)
#endif


static int _stable_set_results(struct stable_clinteg *cli);

static int _stable_can_overflow(struct stable_clinteg *cli)
{
    cl_uint work_threads = cli->points_rule * cli->subdivisions;

    return work_threads / cli->subdivisions != cli->points_rule;
}

static int _stable_create_points_array(struct stable_clinteg *cli, cl_precision *points, size_t num_points)
{
    int err;

    if (cli->points)
        clReleaseMemObject(cli->points);
    if (cli->gauss)
        clReleaseMemObject(cli->gauss);
    if (cli->kronrod)
        clReleaseMemObject(cli->kronrod);

    cli->points = clCreateBuffer(cli->env.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                 sizeof(cl_precision) * num_points, points, &err);

    if(err) return err;

    cli->gauss = clCreateBuffer(cli->env.context, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR,
                                 sizeof(cl_precision) * num_points, NULL, &err);

    if(err) return err;

    cli->kronrod = clCreateBuffer(cli->env.context, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR,
                                 sizeof(cl_precision) * num_points, NULL, &err);

    return err;
}

static int _stable_map_gk_buffers(struct stable_clinteg *cli, size_t points)
{
    int err = 0;

    cli->h_gauss = clEnqueueMapBuffer(opencl_get_queue(&cli->env), cli->gauss, CL_FALSE, CL_MAP_READ, 0, points * sizeof(cl_precision), 0, NULL, NULL, &err);
    cli->h_kronrod = clEnqueueMapBuffer(opencl_get_queue(&cli->env), cli->kronrod, CL_TRUE, CL_MAP_READ, 0, points * sizeof(cl_precision), 0, NULL, NULL, &err);

    return err;
}

static int  _stable_unmap_gk_buffers(struct stable_clinteg* cli)
{
	int err;
    err = clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->gauss, cli->h_gauss, 0, NULL, NULL);

	if(err) return err;
    cli->h_gauss = NULL;

    err = clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->kronrod, cli->h_kronrod, 0, NULL, NULL);

	if(err) return err;
    cli->h_kronrod = NULL;

    return 0;
}

int stable_clinteg_init(struct stable_clinteg *cli)
{
    int err;

    cli->points_rule = GK_POINTS;
    cli->subdivisions = GK_SUBDIVISIONS;

#ifdef BENCHMARK
    cli->profile_enabled = 1;
#else
    cli->profile_enabled = 0;
#endif

    if (_stable_can_overflow(cli))
    {
        stablecl_log(log_warning, "Warning: possible overflow in work dimension (%d x %d)."
                     , cli->points_rule, cli->subdivisions);
        return -1;
    }

    if (opencl_initenv(&cli->env, "opencl/stable_pdf.cl", "stable_pdf_points"))
    {
        stablecl_log(log_message, "OpenCL environment failure.");
        return -1;
    }

    cli->subinterval_errors = (cl_precision *) calloc(cli->subdivisions, sizeof(cl_precision));

    if (!cli->subdivisions)
    {
        perror("Host memory allocation failed.");
        return -1;
    }

    cli->gauss = NULL;
    cli->kronrod = NULL;
    cli->points = NULL;
    cli->args = clCreateBuffer(cli->env.context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR,
                               sizeof(struct stable_info), NULL, &err);

    if (err)
    {
        stablecl_log(log_err, "Buffer creation failed: %s", opencl_strerr(err));
        return -1;
    }

    cli->h_args = clEnqueueMapBuffer(opencl_get_queue(&cli->env), cli->args, CL_TRUE, CL_MAP_WRITE, 0, sizeof(struct stable_info), 0, NULL, NULL, &err);

    if (err)
    {
        stablecl_log(log_err, "Buffer mapping failed: %s. "
                     "Host pointers (gauss, kronrod, args): (%p, %p, %p)",
                     opencl_strerr(err), cli->h_gauss, cli->h_kronrod, cli->h_args);
        return -1;
    }

    return 0;
}

void stable_retrieve_profileinfo(struct stable_clinteg *cli, cl_event event)
{
    stablecl_profileinfo(&cli->profiling, event);
}

static void _stable_print_profileinfo(struct opencl_profile *prof)
{
    printf("OpenCL Profile: %3.3g ms submit, %3.3g ms start, %3.3g ms finish.\n", prof->submit_acum, prof->start_acum, prof->finish_acum);
    printf("\tKernel exec time: %3.3g.\n", prof->exec_time);
}

short stable_clinteg_points_async(struct stable_clinteg *cli, double *x, size_t num_points, struct StableDistStruct *dist, cl_event* event)
{
    cl_int err = 0;
    size_t work_threads[2] = { KRONROD_EVAL_POINTS * num_points, GK_SUBDIVISIONS / 2 };
    size_t workgroup_size[2] = { KRONROD_EVAL_POINTS, GK_SUBDIVISIONS / 2 };
    cl_precision* points = NULL;

    cli->h_args->k1 = dist->k1;
    cli->h_args->alfa = dist->alfa;
    cli->h_args->alfainvalfa1 = dist->alfainvalfa1;
    cli->h_args->beta = dist->beta;
    cli->h_args->THETA_TH = stable_get_THETA_TH();
    cli->h_args->theta0 = dist->theta0;
    cli->h_args->xi = dist->xi;
    cli->h_args->mu_0 = dist->mu_0;
    cli->h_args->sigma = dist->sigma;
    cli->h_args->xxi_th = stable_get_XXI_TH();
    cli->h_args->c2_part = dist->c2_part;
    cli->h_args->S = dist->S;
    cli->h_args->xi_coef = (exp(lgamma(1 + 1 / dist->alfa))) / (M_PI * pow(1 + dist->xi * dist->xi, 1/(2*dist->alfa)));

    if (dist->ZONE == GPU_TEST_INTEGRAND)
        cli->h_args->integrand = GPU_TEST_INTEGRAND;
    else if (dist->ZONE == GPU_TEST_INTEGRAND_SIMPLE)
        cli->h_args->integrand = GPU_TEST_INTEGRAND_SIMPLE;
    else if (dist->ZONE == ALFA_1)
        cli->h_args->integrand = PDF_ALPHA_EQ1;
    else
        cli->h_args->integrand = PDF_ALPHA_NEQ1;

#ifdef CL_PRECISION_IS_FLOAT
    stablecl_log(log_message, "Using floats, forcing cast.");

    points = (cl_precision*) calloc(num_points, sizeof(cl_precision));

    if(!points)
    {
        stablecl_log(log_err, "Couldn't allocate memory.");
        goto cleanup;
    }

    for(size_t i = 0; i < num_points; i++)
        points[i] = (cl_precision) x[i];
#else
    points = x;
#endif

    err |= clEnqueueWriteBuffer(opencl_get_queue(&cli->env), cli->args, CL_FALSE, 0, sizeof(struct stable_info), cli->h_args, 0, NULL, NULL);
    err |= _stable_create_points_array(cli, points, num_points);

    if (err)
    {
        stablecl_log(log_err, "Couldn't set buffers: %d (%s)", err, opencl_strerr(err));
        goto cleanup;
    }

    bench_begin(cli->profiling.argset, cli->profile_enabled);
    int argc = 0;
    err |= clSetKernelArg(cli->env.kernel, argc++, sizeof(cl_mem), &cli->args);
    err |= clSetKernelArg(cli->env.kernel, argc++, sizeof(cl_mem), &cli->points);
    err |= clSetKernelArg(cli->env.kernel, argc++, sizeof(cl_mem), &cli->gauss);
    err |= clSetKernelArg(cli->env.kernel, argc++, sizeof(cl_mem), &cli->kronrod);
    bench_end(cli->profiling.argset, cli->profile_enabled);

    if (err)
    {
        stablecl_log(log_err, "Couldn't set kernel arguments: error %d", err);
        goto cleanup;
    }

    stablecl_log(log_message, "Enqueing kernel - %zu × %zu work threads, %zu × %zu workgroup size", work_threads[0], work_threads[1], workgroup_size[0], workgroup_size[1], cli->points_rule);

    bench_begin(cli->profiling.enqueue, cli->profile_enabled);
    err = clEnqueueNDRangeKernel(opencl_get_queue(&cli->env), cli->env.kernel,
                                 2, NULL, work_threads, workgroup_size, 0, NULL, event);
    bench_end(cli->profiling.enqueue, cli->profile_enabled);

    if (err)
    {
        stablecl_log(log_err, "Error enqueueing the kernel command: %s (%d)", opencl_strerr(err), err);
        goto cleanup;
    }

cleanup:
    stablecl_log(log_message, "Async command issued.");

#ifdef CL_PRECISION_IS_FLOAT
    if(points) free(points);
#endif

    return err;
}

short stable_clinteg_points(struct stable_clinteg *cli, double *x, double *pdf_results, double *errs, size_t num_points, struct StableDistStruct *dist)
{
    cl_event event;
    cl_int err;

    err = stable_clinteg_points_async(cli, x, num_points, dist, &event);

    if(err)
    {
        stablecl_log(log_err, "Couldn't issue evaluation command to the GPU.");
        return err;
    }

    return stable_clinteg_points_end(cli, pdf_results, errs, num_points, dist, &event);
}

short stable_clinteg_points_end(struct stable_clinteg *cli, double *pdf_results, double* errs, size_t num_points, struct StableDistStruct *dist, cl_event* event)
{
    cl_int err = 0;

    if(event)
        clWaitForEvents(1, event);

    bench_begin(cli->profiling.buffer_read, cli->profile_enabled);
    err = _stable_map_gk_buffers(cli, num_points);
    bench_end(cli->profiling.buffer_read, cli->profile_enabled);

    if (err)
    {
        stablecl_log(log_err, "Error reading results from the GPU: %s (%d)", opencl_strerr(err), err);
        return err;
    }

    if (cli->profile_enabled)
        stable_retrieve_profileinfo(cli, *event);

    bench_begin(cli->profiling.set_results, cli->profile_enabled);

    for (size_t i = 0; i < num_points; i++)
    {
        pdf_results[i] = cli->h_kronrod[i];
        char msg[500];

        snprintf(msg, 500, "Results set P%zu: gauss_sum = %.3g, kronrod_sum = %.3g", i, cli->h_gauss[i], cli->h_kronrod[i]);
        if (errs)
        {
            errs[i] = cli->h_kronrod[i] - cli->h_gauss[i];
            snprintf(msg + strlen(msg), 500 - strlen(msg), ", abserr = %.3g", errs[i]);
        }

        stablecl_log(log_message, msg);
    }

    bench_end(cli->profiling.set_results, cli->profile_enabled);

	cl_int retval = _stable_unmap_gk_buffers(cli);
	if(retval)
		stablecl_log(log_warning, "Error unmapping buffers: %s (%d)", opencl_strerr(retval), retval);

    return err;
}


void stable_clinteg_teardown(struct stable_clinteg *cli)
{
    if(cli->h_gauss)
        clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->gauss, cli->h_gauss, 0, NULL, NULL);

    if(cli->h_kronrod)
        clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->kronrod, cli->h_kronrod, 0, NULL, NULL);

    clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->args, cli->h_args, 0, NULL, NULL);

    clReleaseMemObject(cli->gauss);
    clReleaseMemObject(cli->kronrod);
    clReleaseMemObject(cli->args);

    opencl_teardown(&cli->env);
}


