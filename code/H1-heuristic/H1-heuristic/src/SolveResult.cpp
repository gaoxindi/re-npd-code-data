#include "SolveResult.h"

using namespace std;

SolveResult::SolveResult(int instance_id)
    : instance_id(instance_id) {
}

bool SolveResult::hasFeasibleSolution() const {
    return status == SolveStatus::Optimal || status == SolveStatus::Feasible;
}

string SolveResult::statusName() const {
    return statusName(status);
}

string SolveResult::statusName(SolveStatus status) {
    switch (status) {
    case SolveStatus::NotSolved:
        return "NotSolved";
    case SolveStatus::Optimal:
        return "Optimal";
    case SolveStatus::Feasible:
        return "Feasible";
    case SolveStatus::Infeasible:
        return "Infeasible";
    case SolveStatus::Error:
        return "Error";
    case SolveStatus::Unknown:
        return "Unknown";
    default:
        return "Unknown";
    }
}
