#include "Instance.h"
#include "MinCostFlowSolver.h"
#include "ShipPlanGenerate.h"
#include "SolveResult.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

namespace {
double elapsedSeconds(chrono::steady_clock::time_point start_time) {
    const auto end_time = chrono::steady_clock::now();
    return chrono::duration<double>(end_time - start_time).count();
}

filesystem::path instanceFilePath(int instance_id, const string& instance_directory) {
    filesystem::path filename = "instance" + to_string(instance_id) + ".txt";
    if (!instance_directory.empty()) {
        filename = filesystem::path(instance_directory) / filename;
    }

    return filename;
}

bool checkInstanceFiles(int start_instance, int end_instance, const string& instance_directory) {
    bool all_files_exist = true;

    for (int instance_id = start_instance; instance_id <= end_instance; instance_id++) {
        const filesystem::path filename = instanceFilePath(instance_id, instance_directory);
        if (!filesystem::exists(filename)) {
            cerr << "Missing instance file: " << filename.string() << endl;
            all_files_exist = false;
        }
    }

    return all_files_exist;
}

SolveResult solveH1(
    const Instance& instance,
    int instance_id,
    const string& instance_name,
    double time_limit) {

    SolveResult result(instance_id);
    result.instance_name = instance_name;
    const auto start_time = chrono::steady_clock::now();

    try {
        ShipPlanGenerate ship_plan_generator;
        const ShippingSchedule shipping_schedule = ship_plan_generator.generate(instance);
        if (!shipping_schedule.feasible) {
            result.status = SolveStatus::Infeasible;
            result.message = shipping_schedule.message;
            result.solve_time = elapsedSeconds(start_time);
            return result;
        }

        MinCostFlowSolver mcf_solver(time_limit);
        const ProductionSchedule production_schedule = mcf_solver.solve(instance, shipping_schedule.y);
        if (!production_schedule.feasible) {
            result.status = SolveStatus::Error;
            result.message = production_schedule.message;
            result.solve_time = elapsedSeconds(start_time);
            return result;
        }

        result.shipping_cost = shipping_schedule.shipping_cost;
        result.production_cost = production_schedule.objective;
        result.inventory_cost = production_schedule.inventory_cost;
        result.disruption_cost = production_schedule.disruption_cost;
        result.upper_bound = result.shipping_cost + result.production_cost;
        result.lower_bound = 0.0;
        result.status = SolveStatus::Feasible;
        result.solve_time = elapsedSeconds(start_time);
    }
    catch (exception& e) {
        result.status = SolveStatus::Error;
        result.message = e.what();
        result.solve_time = elapsedSeconds(start_time);
    }
    catch (...) {
        result.status = SolveStatus::Error;
        result.message = "Unknown H1 heuristic error.";
        result.solve_time = elapsedSeconds(start_time);
    }

    return result;
}

void writeResult(ofstream& out, const SolveResult& result) {
    out << result.instance_id << '\t' << fixed << setprecision(2);
    if (!result.hasFeasibleSolution()) {
        out << "-\t-\t-\t-\t-\t" << result.solve_time << '\n';
    }
    else {
        out << result.upper_bound << '\t'
            << result.shipping_cost << '\t'
            << result.production_cost << '\t'
            << result.inventory_cost << '\t'
            << result.disruption_cost << '\t'
            << result.solve_time << '\n';
    }
    out.flush();
}

void printUsage(const char* program_name) {
    cerr << "Usage: " << program_name
        << " [start_instance end_instance [instance_directory [output_file [time_limit_seconds]]]]"
        << endl;
}
}

int main(int argc, char* argv[]) {
    int start_instance = 1;
    int end_instance = 10;
    string instance_directory;
    string output_file = "Result-H1.txt";
    double time_limit = 300.0;

    if (argc != 1 && argc != 3 && argc != 4 && argc != 5 && argc != 6) {
        printUsage(argv[0]);
        return 1;
    }

    if (argc >= 3) {
        start_instance = atoi(argv[1]);
        end_instance = atoi(argv[2]);
    }
    if (argc >= 4) {
        instance_directory = argv[3];
    }
    if (argc >= 5) {
        output_file = argv[4];
    }
    if (argc >= 6) {
        time_limit = atof(argv[5]);
    }

    if (start_instance > end_instance || time_limit <= 0.0) {
        printUsage(argv[0]);
        return 1;
    }

    if (!checkInstanceFiles(start_instance, end_instance, instance_directory)) {
        return 1;
    }

    ofstream result_file(output_file);
    if (!result_file.is_open()) {
        cerr << "Error: cannot open result file " << output_file << endl;
        return 1;
    }

    try {
        for (int instance_id = start_instance; instance_id <= end_instance; instance_id++) {
            cout << "instance = " << instance_id << endl;

            Instance instance;
            filesystem::path instance_path = instanceFilePath(instance_id, instance_directory);
            instance.readFromFile(instance_path.string());

            const SolveResult result = solveH1(
                instance,
                instance_id,
                instance_path.filename().string(),
                time_limit);

            cout << fixed << setprecision(2)
                << "upper bound = " << result.upper_bound << ' '
                << "shipping = " << result.shipping_cost << ' '
                << "phaseIII = " << result.production_cost << ' '
                << "time = " << result.solve_time << ' '
                << "status = " << result.statusName();
            if (!result.hasFeasibleSolution() && !result.message.empty()) {
                cout << ' ' << result.message;
            }
            cout << endl;

            writeResult(result_file, result);

            if (result.status == SolveStatus::Error) {
                cerr << "Solver error on instance " << instance_id
                    << ": " << result.message << endl;
                return 1;
            }
        }
    }
    catch (exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    cout << "procedure is over!" << endl;
    return 0;
}
