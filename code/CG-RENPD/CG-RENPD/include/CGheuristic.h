#pragma once

#include "Instance.h"
#include "SolveResult.h"

#include <string>

class CGheuristic {
public:
    explicit CGheuristic(
        double time_limit = 300.0,
        double cg_gap_tolerance = 0.005,
        double mip_gap_tolerance = 0.005,
        double reduced_cost_tolerance = 1.0e-5,
        int max_iterations = 1000);

    SolveResult solve(
        const Instance& instance,
        int instance_id = 0,
        const std::string& instance_name = "") const;

    double timeLimit() const;
    void setTimeLimit(double time_limit);

    double cgGapTolerance() const;
    void setCgGapTolerance(double tolerance);

    double mipGapTolerance() const;
    void setMipGapTolerance(double tolerance);

    int maxIterations() const;
    void setMaxIterations(int max_iterations);

private:
    double time_limit_ = 300.0;
    double cg_gap_tolerance_ = 0.005;
    double mip_gap_tolerance_ = 0.005;
    double reduced_cost_tolerance_ = 1.0e-5;
    int max_iterations_ = 1000;
};
