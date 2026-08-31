#include "CplexSolver.h"

#include <ilcplex/ilocplex.h>

#include <chrono>
#include <exception>
#include <sstream>
#include <string>

using namespace std;

typedef IloArray<IloNumVarArray> NumVarMatrix;

namespace {
SolveStatus toSolveStatus(IloAlgorithm::Status status) {
    if (status == IloAlgorithm::Optimal) {
        return SolveStatus::Optimal;
    }
    if (status == IloAlgorithm::Feasible) {
        return SolveStatus::Feasible;
    }
    if (status == IloAlgorithm::Infeasible) {
        return SolveStatus::Infeasible;
    }
    if (status == IloAlgorithm::Unbounded) {
        return SolveStatus::Unbounded;
    }
    return SolveStatus::Unknown;
}

double elapsedSeconds(chrono::steady_clock::time_point start_time) {
    const auto end_time = chrono::steady_clock::now();
    return chrono::duration<double>(end_time - start_time).count();
}

bool isEligibleShippingDay(const Instance& instance, int order, int day) {
    return day >= instance.T && instance.hasEligibleShippingMode(order, day);
}
}

CplexSolver::CplexSolver(double time_limit)
    : time_limit_(time_limit) {
}

SolveResult CplexSolver::solve(
    const Instance& instance,
    int instance_id,
    const string& instance_name) const {

    SolveResult result(instance_id);
    result.instance_name = instance_name;

    const auto start_time = chrono::steady_clock::now();
    IloEnv env;

    try {
        IloModel model(env);
        IloObjective objective = IloAdd(model, IloMinimize(env));

        NumVarMatrix var_z(env, instance.n + 1);
        for (int i = 1; i <= instance.n; i++) {
            var_z[i] = IloNumVarArray(env, instance.m + 1);
            for (int t = 1; t <= instance.m; t++) {
                const double upper_bound = isEligibleShippingDay(instance, i, t) ? 1.0 : 0.0;
                var_z[i][t] = IloNumVar(env, 0.0, upper_bound, ILOINT);
            }
        }

        IloNumVarArray var_x(env, instance.m + 1);
        IloNumVarArray var_y(env, instance.m + 1);
        IloNumVarArray var_phi(env, instance.m + 1);
        for (int t = 1; t <= instance.m; t++) {
            var_x[t] = IloNumVar(env, 0.0, IloInfinity, ILOINT);
            var_y[t] = IloNumVar(env, 0.0, IloInfinity, ILOFLOAT);
            var_phi[t] = IloNumVar(env, 0.0, IloInfinity, ILOINT);
        }

        for (int t = 1; t <= instance.m; t++) {
            model.add(var_x[t] <= instance.capacity);
        }

        for (int i = 1; i <= instance.n; i++) {
            IloExpr assigned_once(env);
            for (int t = instance.T; t <= instance.m; t++) {
                if (isEligibleShippingDay(instance, i, t)) {
                    assigned_once += var_z[i][t];
                }
            }
            model.add(assigned_once == 1);
            assigned_once.end();
        }

        for (int t = 1; t <= instance.m; t++) {
            IloExpr shipped_on_day(env);
            for (int i = 1; i <= instance.n; i++) {
                shipped_on_day += instance.quantity[i] * var_z[i][t];
            }
            model.add(var_y[t] == shipped_on_day);
            shipped_on_day.end();
        }

        IloExpr cumulative_produced(env);
        IloExpr cumulative_shipped(env);
        for (int t = 1; t <= instance.m; t++) {
            cumulative_produced += var_x[t];
            cumulative_shipped += var_y[t];
            model.add(cumulative_shipped <= cumulative_produced);
        }

        for (int t = 1; t <= instance.m; t++) {
            model.add(instance.xt_NPD[t] - var_x[t] <= var_phi[t]);
            model.add(var_x[t] - instance.xt_NPD[t] <= var_phi[t]);
        }

        IloExpr obj(env);
        for (int i = 1; i <= instance.n; i++) {
            for (int t = instance.T; t <= instance.m; t++) {
                if (isEligibleShippingDay(instance, i, t)) {
                    obj += instance.quantity[i] * instance.mo_ship_cost[i][t] * var_z[i][t];
                }
            }
        }

        cumulative_produced.clear();
        cumulative_shipped.clear();
        for (int t = 1; t <= instance.m; t++) {
            cumulative_produced += var_x[t];
            cumulative_shipped += var_y[t];
            obj += instance.inven_cost[t] * (cumulative_produced - cumulative_shipped);
            obj += instance.disru_cost * var_phi[t];
        }
        cumulative_produced.end();
        cumulative_shipped.end();

        objective.setExpr(obj);
        obj.end();

        IloCplex solver(model);
        solver.setOut(env.getNullStream());
        solver.setParam(IloCplex::TiLim, time_limit_);
        solver.solve();

        result.status = toSolveStatus(solver.getStatus());
        if (result.hasFeasibleSolution()) {
            result.upper_bound = solver.getObjValue();
            result.lower_bound = solver.getBestObjValue();
        }
        else {
            result.upper_bound = 0.0;
            try {
                result.lower_bound = solver.getBestObjValue();
            }
            catch (...) {
                result.lower_bound = 0.0;
            }
        }

        result.solve_time = elapsedSeconds(start_time);
        env.end();
    }
    catch (IloException& e) {
        result.status = SolveStatus::Error;
        ostringstream message;
        message << e;
        result.message = message.str();
        result.solve_time = elapsedSeconds(start_time);
        env.end();
    }
    catch (exception& e) {
        result.status = SolveStatus::Error;
        result.message = e.what();
        result.solve_time = elapsedSeconds(start_time);
        env.end();
    }
    catch (...) {
        result.status = SolveStatus::Error;
        result.message = "Unknown CPLEX solver error.";
        result.solve_time = elapsedSeconds(start_time);
        env.end();
    }

    return result;
}

double CplexSolver::timeLimit() const {
    return time_limit_;
}

void CplexSolver::setTimeLimit(double time_limit) {
    time_limit_ = time_limit;
}
