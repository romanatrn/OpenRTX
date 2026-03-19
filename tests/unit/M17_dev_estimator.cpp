/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_test_macros.hpp>

#include "protocols/M17/DevEstimator.hpp"

TEST_CASE("DevEstimator tracks positive and negative outer deviations",
          "[m17][dev-estimator]")
{
    DevEstimator estimator;
    estimator.init({3000, -3000});

    estimator.sample(3300);
    estimator.sample(3600);
    estimator.sample(-3200);
    estimator.sample(-3400);
    estimator.update();

    const auto deviation = estimator.outerDeviation();
    REQUIRE(deviation.first == 3375);
    REQUIRE(deviation.second == -3375);
    REQUIRE(estimator.zeroOffset() == 0);
}

TEST_CASE("DevEstimator keeps prior values until both polarities are seen",
          "[m17][dev-estimator]")
{
    DevEstimator estimator;
    estimator.init({2500, -2500});

    estimator.sample(3000);
    estimator.sample(3200);
    estimator.update();

    const auto deviation = estimator.outerDeviation();
    REQUIRE(deviation.first == 2500);
    REQUIRE(deviation.second == -2500);
    REQUIRE(estimator.zeroOffset() == 0);
}
