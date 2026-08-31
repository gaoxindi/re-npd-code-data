#include "CGheuristic.h"
#include "Instance.h"
#include "SolveResult.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

namespace {
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

void writeResult(ofstream& out, const SolveResult& result) {
    out << result.instance_id << '\t'
        << fixed << setprecision(2)
        << result.upper_bound << '\t'
        << result.lower_bound << '\t'
        << result.solve_time << '\n';
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
    int end_instance = 150;
    string instance_directory;
    string output_file = "Result-RENPD-CG.txt";
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

    CGheuristic solver(time_limit);

    try {
        for (int instance_id = start_instance; instance_id <= end_instance; instance_id++) {
            cout << "instance = " << instance_id << endl;

            Instance instance;
            filesystem::path instance_path = instanceFilePath(instance_id, instance_directory);
            instance.readFromFile(instance_path.string());

            SolveResult result = solver.solve(
                instance,
                instance_id,
                instance_path.filename().string());

            cout << fixed << setprecision(2)
                << "upper bound = " << result.upper_bound << ' '
                << "lower bound = " << result.lower_bound << ' '
                << "time = " << result.solve_time << ' '
                << "status = " << result.statusName();
            if (!result.hasFeasibleSolution() && !result.message.empty()) {
                cout << ' ' << result.message;
            }
            cout << endl;

            if (result.status == SolveStatus::Error) {
                cerr << "Solver error on instance " << instance_id
                    << ": " << result.message << endl;
                return 1;
            }

            writeResult(result_file, result);
        }
    }
    catch (exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    cout << "procedure is over!" << endl;
    return 0;
}
