#include "stable_api.h"
#include "opencl_common.h"
#include "benchmarking.h"

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef max
#define max(a,b) (a < b ? b : a)
#endif

#define KERNIDX_INTEGRATE 0
#define KERNIDX_QUANTILE 2
#define KERN_POINTS_NAME "stable_points"
#define KERN_QUANTILE_NAME "stable_quantile"

#define MIN_POINTS_PER_QUEUE 200

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

    if (err) return err;

    cli->gauss = clCreateBuffer(cli->env.context, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR,
                                sizeof(cl_precision) * num_points, NULL, &err);

    if (err) return err;

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
#ifndef SIMULATOR_BUILD
    int err;
    err = clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->gauss, cli->h_gauss, 0, NULL, NULL);

    if (err) return err;
    cli->h_gauss = NULL;

    err = clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->kronrod, cli->h_kronrod, 0, NULL, NULL);

    if (err) return err;
    cli->h_kronrod = NULL;
#endif

    return 0;
}

int stable_clinteg_init(struct stable_clinteg *cli, size_t platform_index)
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

    if (opencl_initenv(&cli->env, platform_index))
    {
        stablecl_log(log_message, "OpenCL environment failure.");
        return -1;
    }

    if (opencl_load_kernel(&cli->env, "opencl/stable.cl", KERN_POINTS_NAME, KERNIDX_INTEGRATE))
    {
        stablecl_log(log_message, "OpenCL kernel %s load failure.", KERN_POINTS_NAME);
        return -1;
    }

    if (opencl_load_kernel(&cli->env, "opencl/stable.cl", KERN_QUANTILE_NAME, KERNIDX_QUANTILE))
    {
        stablecl_log(log_message, "OpenCL kernel %s load failure.", KERN_QUANTILE_NAME);
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

static void _stable_clinteg_prepare_kernel_data(struct stable_info* info, StableDist* dist)
{
    info->k1 = dist->k1;
    info->alfa = dist->alfa;
    info->alfainvalfa1 = dist->alfainvalfa1;
    info->beta = dist->beta;
    info->THETA_TH = stable_get_THETA_TH();
    info->theta0 = dist->theta0;
    info->xi = dist->xi;
    info->mu_0 = dist->mu_0;
    info->sigma = dist->sigma;
    info->xxi_th = stable_get_XXI_TH();
    info->c2_part = dist->c2_part;
    info->xi_coef = (exp(lgamma(1 + 1 / dist->alfa))) / (M_PI * pow(1 + dist->xi * dist->xi, 1 / (2 * dist->alfa)));
    info->c1 = dist->c1;
    info->final_pdf_factor = dist->c2_part / dist->sigma;
    info->final_cdf_factor = dist->alfa < 1 ? M_1_PI : - M_1_PI;
    info->final_cdf_addition = dist->c1;

    if(dist->cli.mode_bits == MODEMARKER_PDF)
    {
        info->max_reevaluations = dist->alfa > 1 ? 2 : 1;
    }
    else
    {
        info->max_reevaluations = 1;
    }

    short alfa_marker = dist->ZONE == ALFA_1 ? MODEMARKER_EQ1 : MODEMARKER_NEQ1;
    info->integrand = alfa_marker | dist->cli.mode_bits;

    if (dist->ZONE == ALFA_1)
        info->beta = fabs(dist->beta);
}

cl_precision* _stable_check_precision_type(double* values, size_t num_points)
{
    cl_precision* points = values;

#ifdef CL_PRECISION_IS_FLOAT
    stablecl_log(log_message, "Using floats, forcing cast.");

    points = (cl_precision*) calloc(num_points, sizeof(cl_precision));

    if (!points)
    {
        stablecl_log(log_err, "Couldn't allocate memory.");
        return NULL;
    }

    for (size_t i = 0; i < num_points; i++)
        points[i] = (cl_precision) values[i];
#endif

    return points;
}

short stable_clinteg_points_async(struct stable_clinteg *cli, double *x, size_t num_points, struct StableDistStruct *dist, cl_event* event)
{
    cl_int err = 0;
    size_t work_threads[2] = { KRONROD_EVAL_POINTS * num_points, MAX_WORKGROUPS };
    size_t workgroup_size[2] = { KRONROD_EVAL_POINTS, MAX_WORKGROUPS };
    cl_precision* points = NULL;

    _stable_clinteg_prepare_kernel_data(cli->h_args, dist);
    points = _stable_check_precision_type(x, num_points);

    if(!points)
        goto cleanup;

    err |= clEnqueueWriteBuffer(opencl_get_queue(&cli->env), cli->args, CL_FALSE, 0, sizeof(struct stable_info), cli->h_args, 0, NULL, NULL);
    err |= _stable_create_points_array(cli, points, num_points);

    if (err)
    {
        stablecl_log(log_err, "Couldn't set buffers: %d (%s)", err, opencl_strerr(err));
        goto cleanup;
    }

    opencl_set_current_kernel(&cli->env, cli->kern_index);

    bench_begin(cli->profiling.argset, cli->profile_enabled);
    int argc = 0;
    err |= clSetKernelArg(opencl_get_current_kernel(&cli->env), argc++, sizeof(cl_mem), &cli->args);
    err |= clSetKernelArg(opencl_get_current_kernel(&cli->env), argc++, sizeof(cl_mem), &cli->points);
    err |= clSetKernelArg(opencl_get_current_kernel(&cli->env), argc++, sizeof(cl_mem), &cli->gauss);
    err |= clSetKernelArg(opencl_get_current_kernel(&cli->env), argc++, sizeof(cl_mem), &cli->kronrod);
    bench_end(cli->profiling.argset, cli->profile_enabled);

    if (err)
    {
        stablecl_log(log_err, "Couldn't set kernel arguments: error %d", err);
        goto cleanup;
    }

    stablecl_log(log_message, "Enqueing kernel %d - %zu × %zu work threads, %zu × %zu workgroup size",
                 cli->kern_index, work_threads[0], work_threads[1], workgroup_size[0], workgroup_size[1], cli->points_rule);

    bench_begin(cli->profiling.enqueue, cli->profile_enabled);
    err = clEnqueueNDRangeKernel(opencl_get_queue(&cli->env), opencl_get_current_kernel(&cli->env),
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
    if (points) free(points);
#endif

    return err;
}

short stable_clinteg_points(struct stable_clinteg *cli, double *x, double *results_1, double *results_2, double *errs, size_t num_points, struct StableDistStruct *dist)
{
    cl_event event;
    cl_int err;

    err = stable_clinteg_points_async(cli, x, num_points, dist, &event);

    if (err)
    {
        stablecl_log(log_err, "Couldn't issue evaluation command to the GPU.");
        return err;
    }

    return stable_clinteg_points_end(cli, results_1, results_2, errs, num_points, dist, &event);
}

short stable_clinteg_points_end(struct stable_clinteg *cli, double *results_1, double *results_2, double* errs, size_t num_points, struct StableDistStruct *dist, cl_event* event)
{
    cl_int err = 0;

    if (event)
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
        if(results_1)
            results_1[i] = cli->h_kronrod[i];

        if(results_2 && cli->copy_gauss_array)
            results_2[i] = cli->h_gauss[i];

#if STABLE_MIN_LOG <= 0
        char msg[500];
        snprintf(msg, 500, "Results set P%zu: gauss_sum = %.3g, kronrod_sum = %.3g", i, cli->h_gauss[i], cli->h_kronrod[i]);
#endif

        if (errs)
        {
            if (cli->error_mode == error_from_results)
            {
                if(cli->h_kronrod[i] != 0)
                    errs[i] = fabs(cli->h_kronrod[i] - cli->h_gauss[i]) / cli->h_kronrod[i];
                else
                    errs[i] = fabs(cli->h_kronrod[i] - cli->h_gauss[i]);
            }
            else if(cli->error_mode == error_is_gauss_array)
                errs[i] = cli->h_gauss[i];

#if STABLE_MIN_LOG <= 0
            snprintf(msg + strlen(msg), 500 - strlen(msg), ", relerr = %.3g", errs[i]);
#endif
        }

#if STABLE_MIN_LOG <= 0
        stablecl_log(log_message, msg);
#endif
    }

    bench_end(cli->profiling.set_results, cli->profile_enabled);

    cl_int retval = _stable_unmap_gk_buffers(cli);
    if (retval)
        stablecl_log(log_warning, "Error unmapping buffers: %s (%d)", opencl_strerr(retval), retval);

    return err;
}

short stable_clinteg_points_parallel(struct stable_clinteg *cli, double *x, double *results_1, double* results_2, double *errs, size_t num_points, struct StableDistStruct *dist, size_t queues)
{
    cl_int err = 0;
    size_t i;
    size_t points_per_queue, processed_points, required_queues, points_for_current_queue;

    // Ensure we have enough queues
    if (cli->env.queue_count < queues)
        opencl_set_queues(&cli->env, queues);

    required_queues = num_points / MIN_POINTS_PER_QUEUE + 1;
    required_queues = required_queues > queues ? queues : required_queues;

    points_per_queue = num_points / required_queues;
    processed_points = 0;

    for (i = 0; i < required_queues; i++)
    {
        if (i == required_queues - 1)
            points_for_current_queue = num_points - processed_points; // Process remaining points. We don't want to leave anything because of rounding errors.
        else
            points_for_current_queue = points_per_queue;

        opencl_set_current_queue(&cli->env, i);
        err = stable_clinteg_points_async(cli, x + processed_points, points_for_current_queue, dist, NULL);

        if (err)
            goto cleanup;

        processed_points += points_for_current_queue;
    }

    if (processed_points != num_points)
        stablecl_log(log_err, "QQQQQ");

    processed_points = 0;

    for (i = 0; i < required_queues; i++)
    {
        if (i == required_queues - 1)
            points_for_current_queue = num_points - processed_points; // Process remaining points. We don't want to leave anything because of rounding errors.
        else
            points_for_current_queue = points_per_queue;

        opencl_set_current_queue(&cli->env, i);
        err = stable_clinteg_points_end(cli, results_1 + processed_points, results_2 + processed_points, errs ? errs + processed_points : NULL, points_for_current_queue, dist, NULL);

        if (err)
            goto cleanup;

        processed_points += points_for_current_queue;
    }

cleanup:
    if (err)
        stablecl_log(log_err, "Error on parallel evaluation.");

    return err;
}

void stable_clinteg_teardown(struct stable_clinteg *cli)
{
#ifndef SIMULATOR_BUILD
    if (cli->h_gauss)
        clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->gauss, cli->h_gauss, 0, NULL, NULL);

    if (cli->h_kronrod)
        clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->kronrod, cli->h_kronrod, 0, NULL, NULL);

    clEnqueueUnmapMemObject(opencl_get_queue(&cli->env), cli->args, cli->h_args, 0, NULL, NULL);
#endif

    clReleaseMemObject(cli->gauss);
    clReleaseMemObject(cli->kronrod);
    clReleaseMemObject(cli->args);

    opencl_teardown(&cli->env);
}

void stable_clinteg_printinfo()
{
    printf("Libstable - OpenCL parallel integration details:\n");
    printf(" %d points Gauss - Kronrod quadrature.\n", GK_POINTS);
    printf(" %d subdivisions, %d points per thread.\n", GK_SUBDIVISIONS, POINTS_EVAL);
    printf(" Precision used: %s.\n\n", cl_precision_type);
}

void stable_clinteg_set_mode(struct stable_clinteg* cli, clinteg_mode mode)
{
    if(mode == mode_pdf)
        cli->mode_bits = MODEMARKER_PDF;
    else if(mode == mode_cdf)
        cli->mode_bits = MODEMARKER_CDF;
    else // pcdf or quantile
        cli->mode_bits = MODEMARKER_PCDF;

    cli->kern_index = mode == mode_quantile ? KERNIDX_QUANTILE : KERNIDX_INTEGRATE;
    cli->copy_gauss_array = mode == mode_pcdf;
    cli->error_mode = mode == mode_quantile ? error_is_gauss_array : error_from_results;
}
