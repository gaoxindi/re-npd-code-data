# RE-NPD Code and Data

This repository provides the code and data used in the computational experiments for the paper on integrated production and transportation rescheduling under transportation disruptions.

## Repository Structure

### Code

- `CG-RENPD`: Column-generation-based heuristic for Problem RE-NPD.
- `Cplex-RENPD`: Direct CPLEX benchmark for Problem RE-NPD.
- `CG-RENPD-non-consecutive`: Column-generation-based heuristic for instances with finite and non-consecutive shipping modes.
- `H1-heuristic`: Heuristic H1 used as a practical benchmark.

### Data

- `data-small-scale.zip`: Small-scale instances generated according to Online Appendix C.1.
- `data-large-scale.zip`: Large-scale instances generated according to Online Appendix C.1.
- `data-sensitivity-analysis.zip`: Sensitivity-analysis instances generated according to Online Appendix C.2.
- `data-practical-instance.zip`: Practical instances generated according to Section 6.3.1 of the manuscript.

## Matching Between Code and Data

The data files should be used with the corresponding code projects as follows.

| Data file | Instance source | Compatible code |
|---|---|---|
| `data-small-scale.zip` | Online Appendix C.1 | `CG-RENPD`, `Cplex-RENPD` |
| `data-large-scale.zip` | Online Appendix C.1 | `CG-RENPD`, `Cplex-RENPD` |
| `data-sensitivity-analysis.zip` | Online Appendix C.2 | `CG-RENPD-non-consecutive` |
| `data-practical-instance.zip` | Section 6.3.1 | `CG-RENPD-non-consecutive`, `Cplex-RENPD`, `H1-heuristic` |

After downloading a data file, unzip it and use the folder containing the `instance*.txt` files as the input instance directory.

## Computational Environment

All algorithms were implemented in C++20 and compiled in Release mode. IBM ILOG CPLEX 22.1.0 was used as the optimization solver.

The time limit was set to 300 seconds for each instance. In the CG-based heuristic, the CG relative-gap tolerance was set to `0.005`, the reduced-cost tolerance for adding new columns was set to `1.0e-5`, and the maximum number of CG iterations was set to `1000`.

The restricted integer program solved after CG was solved by CPLEX with `MIPEmphasis = 4` and `EpGap = 0.005`. For the direct CPLEX benchmark, only the time limit was specified, and all other CPLEX parameters, including optimality, feasibility, and integrality tolerances, were kept at their default values.

The minimum cost flow problem in Heuristic H1 was solved by CPLEX with `RootAlg = Network`.

## How to Run

Each project can be opened and compiled in Visual Studio. The CPLEX include and library directories may need to be adjusted according to the local CPLEX installation path.

The executable programs use the following command-line interface:

```text
program.exe start_instance end_instance instance_directory output_file time_limit_seconds
```

For example:

```text
CG-RENPD.exe 1 150 data-small-scale Result-RENPD-CG.txt 300
```

where `data-small-scale` is the folder obtained after unzipping `data-small-scale.zip`.

## Reproducibility Notes

No random choices are made during the solution procedures. The reported computational results are based on the fixed generated instance files provided in this repository.

The data sets were generated following the procedures described in Online Appendix C.1, Online Appendix C.2, and Section 6.3.1 of the manuscript.

## License

This repository is released under the MIT License.
