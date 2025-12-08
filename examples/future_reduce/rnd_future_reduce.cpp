//  Copyright (c) 2014 John Biddiscombe
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/chrono.hpp>
#include <hpx/future.hpp>
#include <hpx/init.hpp>
#include <hpx/runtime.hpp>
#include <hpx/modules/async.hpp>
#include <hpx/modules/futures.hpp>

#include <iostream>
#include <random>
#include <vector>

constexpr int TEST_SUCCESS = 1;
constexpr int TEST_FAIL = 0;
constexpr int FAILURE_RATE_PERCENT = 5;
constexpr int SAMPLES_PER_LOOP = 10;
constexpr int TEST_LOOPS = 1000;

std::random_device rseed;
std::mt19937 gen(rseed());
std::uniform_int_distribution<int> dist(0, 99);

constexpr bool USE_LAMBDA = true;

//----------------------------------------------------------------------------

int reduce(hpx::future<std::vector<hpx::future<int>>>&& futvec)
{
    int res = TEST_SUCCESS;
    auto vfs = futvec.get();

    for (auto& f : vfs)
    {
        if (f.get() == TEST_FAIL)
        {
            return TEST_FAIL;
        }
    }
    return res;
}

//----------------------------------------------------------------------------

int generate_one()
{
    // returns fail with configured percentage probability
    return (dist(gen) >= (100 - FAILURE_RATE_PERCENT)) ? TEST_FAIL : TEST_SUCCESS;
}

//----------------------------------------------------------------------------

hpx::future<int> test_reduce()
{
    std::vector<hpx::future<int>> req_futures;
    req_futures.reserve(SAMPLES_PER_LOOP);

    for (int i = 0; i < SAMPLES_PER_LOOP; ++i)
    {
        req_futures.push_back(hpx::async(generate_one));
    }

    auto all_ready = hpx::when_all(req_futures);

    if constexpr (USE_LAMBDA)
    {
        return all_ready.then(
            hpx::unwrapping([](std::vector<hpx::future<int>> vfs) {
                int res = TEST_SUCCESS;

                hpx::wait_each(
                    [&res](hpx::future<int> f) {
                        if (f.get() == TEST_FAIL)
                        {
                            res = TEST_FAIL;
                        }
                    },
                    vfs);

                return res;
            }));
    }
    else
    {
        return all_ready.then(reduce);
    }
}

//----------------------------------------------------------------------------

int hpx_main()
{
    hpx::chrono::high_resolution_timer timer;
    int count = 0;

    for (int i = 0; i < TEST_LOOPS; ++i)
    {
        count += test_reduce().get();
    }

    double pr_pass =
        std::pow(1.0 - FAILURE_RATE_PERCENT / 100.0, SAMPLES_PER_LOOP);
    double exp_pass = TEST_LOOPS * pr_pass;

    std::cout << "From " << TEST_LOOPS << " tests, we got\n"
              << " " << count << " passes\n"
              << " " << exp_pass << " expected\n\n"
              << timer.elapsed() << " seconds\n"
              << std::flush;

    return hpx::local::finalize();
}

//----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    return hpx::local::init(hpx_main, argc, argv);
}
