/* ************************************************************************
 * Copyright (C) 2018-2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ************************************************************************ */

#pragma once

#include "testing_common.hpp"

template <typename T>
void testing_set_get_vector(const Arguments& arg)
{
    auto rocblas_set_vector_fn    = rocblas_set_vector;
    auto rocblas_set_vector_fn_64 = rocblas_set_vector_64;
    auto rocblas_get_vector_fn    = rocblas_get_vector;
    auto rocblas_get_vector_fn_64 = rocblas_get_vector_64;

    int64_t N    = arg.N;
    int64_t incx = arg.incx;
    int64_t incy = arg.incy;
    int64_t ldd  = arg.ldd;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N < 0 || incx <= 0 || incy <= 0 || ldd <= 0)
    {
        DAPI_EXPECT(rocblas_status_invalid_size,
                    rocblas_set_vector_fn,
                    (N, sizeof(T), nullptr, incx, nullptr, ldd));
        DAPI_EXPECT(rocblas_status_invalid_size,
                    rocblas_get_vector_fn,
                    (N, sizeof(T), nullptr, ldd, nullptr, incy));
        return;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    HOST_MEMCHECK(host_vector<T>, hx, (N, incx));
    HOST_MEMCHECK(host_vector<T>, hy, (N, incy));
    HOST_MEMCHECK(host_vector<T>, hy_gold, (N, incy));

    double cpu_time_used;
    cpu_time_used        = 0.0;
    double rocblas_error = 0.0;

    // allocate memory on device
    DEVICE_MEMCHECK(device_vector<T>, db, (N, ldd));

    // Initial Data on CPU
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true);
    rocblas_init_vector(hy, arg, rocblas_client_alpha_sets_nan, false);

    if(arg.unit_check || arg.norm_check)
    {
        // set GPU memory to zero
        CHECK_HIP_ERROR(hipMemset(db, 0, sizeof(T) * (1 + ldd * (N - 1))));

        DAPI_CHECK(rocblas_set_vector_fn, (N, sizeof(T), hx, incx, db, ldd));
        DAPI_CHECK(rocblas_get_vector_fn, (N, sizeof(T), db, ldd, hy, incy));

        cpu_time_used = get_time_us_no_sync();

        // reference calculation
        for(size_t i = 0; i < N; i++)
        {
            hy_gold[i * incy] = hx[i * incx];
        }

        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, incy, hy, hy_gold);
        }

        if(arg.norm_check)
        {
            rocblas_error = norm_check_general<T>('F', 1, N, incy, hy, hy_gold);
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_no_sync();

            DAPI_DISPATCH(rocblas_set_vector_fn, (N, sizeof(T), hx, incx, db, ldd));
            DAPI_DISPATCH(rocblas_get_vector_fn, (N, sizeof(T), db, ldd, hy, incy));
        }

        gpu_time_used = get_time_us_no_sync() - gpu_time_used;

        ArgumentModel<e_N, e_incx, e_incy, e_ldd>{}.log_args<T>(rocblas_cout,
                                                                arg,
                                                                gpu_time_used,
                                                                ArgumentLogging::NA_value,
                                                                set_get_vector_gbyte_count<T>(N),
                                                                cpu_time_used,
                                                                rocblas_error);
    }
}
