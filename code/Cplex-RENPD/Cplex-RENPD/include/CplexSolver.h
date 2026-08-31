#pragma once

#include "Instance.h"
#include "SolveResult.h"

#include <string>

class CplexSolver {
public:
    explicit CplexSolver(double time_limit = 300.0);

    SolveResult solve(
        const Instance& instance,
        int instance_id = 0,
        const std::string& instance_name = "") const;

    double timeLimit() const;
    void setTimeLimit(double time_limit);

private:
    double time_limit_ = 300.0;
};
