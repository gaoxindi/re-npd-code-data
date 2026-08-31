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
                var_z[i][t] = IloNumVar(env, 0, 1, ILOINT);
            }
        }

        IloNumVarArray var_x(env, instance.m + 1);
        for (int t = 1; t <= instance.m; t++) {
            var_x[t] = IloNumVar(env, 0, IloInfinity, ILOINT);
        }

        IloNumVarArray var_phi(env, instance.m + 1);
        for (int t = 1; t <= instance.m; t++) {
            var_phi[t] = IloNumVar(env, 0, IloInfinity, ILOINT);
        }

        for (int t = 1; t <= instance.m; t++) {
            model.add(var_x[t] <= instance.capacity);
        }

        IloExpr before_recovery_shipments(env);
        for (int i = 1; i <= instance.n; i++) {
            for (int t = 1; t <= instance.T - 1; t++) {
                before_recovery_shipments += var_z[i][t];
            }
        }
        model.add(before_recovery_shipments == 0);
        before_recovery_shipments.end();

        for (int i = 1; i <= instance.n; i++) {
            IloExpr assigned_once(env);
            for (int t = instance.T; t <= instance.m; t++) {
                assigned_once += var_z[i][t];
            }
            model.add(assigned_once == 1);
            assigned_once.end();
        }

        for (int t = 1; t <= instance.m; t++) {
            IloExpr shipped_quantity(env);
            IloExpr produced_quantity(env);
            for (int s = 1; s <= t; s++) {
                produced_quantity += var_x[s];
                for (int i = 1; i <= instance.n; i++) {
                    shipped_quantity += var_z[i][s] * instance.quantity[i];
                }
            }
            model.add(shipped_quantity <= produced_quantity);
            shipped_quantity.end();
            produced_quantity.end();
        }

        for (int t = 1; t <= instance.m; t++) {
            model.add(instance.xt_NPD[t] - var_x[t] <= var_phi[t]);
            model.add(var_x[t] - instance.xt_NPD[t] <= var_phi[t]);
        }

        IloExpr obj(env);
        for (int i = 1; i <= instance.n; i++) {
            for (int t = 1; t <= instance.m; t++) {
                obj += instance.quantity[i] * instance.mo_ship_cost[i][t] * var_z[i][t];
            }
        }

        for (int t = 1; t <= instance.m; t++) {
            for (int s = 1; s <= t; s++) {
                IloExpr shipped_on_day_s(env);
                for (int i = 1; i <= instance.n; i++) {
                    shipped_on_day_s += var_z[i][s] * instance.quantity[i];
                }
                obj += instance.inven_cost[t] * (var_x[s] - shipped_on_day_s);
                shipped_on_day_s.end();
            }
        }

        for (int t = 1; t <= instance.m; t++) {
            obj += instance.disru_cost * var_phi[t];
        }

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
