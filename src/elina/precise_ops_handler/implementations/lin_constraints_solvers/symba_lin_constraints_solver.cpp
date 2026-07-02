#include "symba_lin_constraints_solver.hpp"
#include "utils.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <libgen.h>
#include <limits>
#include <omp.h>
#include <regex>
#include <string>
#include <unordered_map>
#include <z3++.h>

double INF = std::numeric_limits<double>::infinity();

std::map<int, z3::expr> create_z3_vars_map(z3::context& ctx, const std::set<int>& variables)
{
    std::map<int, z3::expr> vars_to_z3_vars_map;
    std::string var_name;

    for (auto it = variables.begin(); it != variables.end(); ++it)
    {
        int var_id = *it;
        var_name = "v_" + std::to_string(var_id);
        z3::expr var = ctx.real_const(var_name.c_str());
        vars_to_z3_vars_map.emplace(var_id, var);
    }

    return vars_to_z3_vars_map;
}

z3::expr make_real(z3::context& ctx, double value)
{
    std::ostringstream os;
    os << std::fixed               // no scientific notation
       << std::setprecision(15)    // enough for 53-bit double
       << value;

    std::string s = os.str();
    // trim trailing zeros and the dot
    if (auto p = s.find('.'); p != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.')   s.pop_back();
    }
    if (s.empty()) s = "0";
    return ctx.real_val(s.c_str());          // valid SMT-LIB numeral
}

z3::expr generate_term_from_constraint_list(z3::context& ctx,
                                            const std::map<int, z3::expr>& variables_map,
                                            const Utils::CoeffVarSet& cons_set)
{
    z3::expr term = ctx.real_val(0);

    for (const auto& coeff_var : cons_set) {
        double coeff = coeff_var.coeff;
        int var = coeff_var.var;

        if (var == -1) {
            term = term + make_real(ctx, coeff);
        } else {
            auto it = variables_map.find(var);
            term = term + make_real(ctx, coeff) * it->second;
        }
    }

    return term;
}

z3::expr generate_bounds_constraint(z3::context& ctx,
                                    const std::map<int, z3::expr>& variables_map,
                                    int var,
                                    std::pair<double, double> bounds)
{
    z3::expr_vector bounds_constraints(ctx);

    // Lower bound
    if (bounds.first != -INF){
        auto it = variables_map.find(var);
        bounds_constraints.push_back(it->second >= make_real(ctx, bounds.first));
    }

    // Upper bound
    if (bounds.second != INF){
        auto it = variables_map.find(var);
        bounds_constraints.push_back(it->second <= make_real(ctx, bounds.second));
    }
    
    return z3::mk_and(bounds_constraints);
}

z3::expr generate_linear_constraint(z3::context& ctx,
                                    const std::map<int, z3::expr>& variables_map,
                                    const Utils::CoeffVarSet& cons_set,
                                    double constant)
{
    return generate_term_from_constraint_list(ctx, variables_map, cons_set) <= constant;
}

z3::expr_vector get_init_constraints(z3::context& ctx,
                                     std::map<int, z3::expr>& vars_to_z3_vars_map,
                                     const std::map<int, std::pair<double, double>>& var_bounds_map,
                                     const std::map<Utils::CoeffVarSet, double>& constraints_map)
{
    z3::expr_vector init_constraints(ctx);

    // Generate constraints for variable bounds
    for (const auto& pair : var_bounds_map) {
        init_constraints.push_back(generate_bounds_constraint(ctx, vars_to_z3_vars_map, pair.first, pair.second));
    }

    // Generate constraints for the linear constraints
    for (const auto& pair : constraints_map) {
        init_constraints.push_back(generate_linear_constraint(ctx, vars_to_z3_vars_map, pair.first, pair.second));
    }

    return init_constraints;
}

std::tuple<z3::expr, z3::expr, std::string> generate_comb_block(
    z3::context& ctx,
    const std::map<int, z3::expr>& vars,
    const Utils::CoeffVarSet& comb,
    int index)
{
    std::string comb_name = "comb_var_" + std::to_string(index);
    std::string bound_name = "bound_" + std::to_string(index);

    z3::expr term = generate_term_from_constraint_list(ctx, vars, comb);
    z3::expr comb_var = ctx.real_const(comb_name.c_str());
    z3::expr bound_var = ctx.real_const(bound_name.c_str());

    // We need to get lower bound for term and tool gives upper bounds
    // So, use -1*term
    return {-1 * term == comb_var, comb_var <= bound_var, comb_name};
}

std::string write_solver_to_temp_file(z3::solver& solver)
{
    // Template for the temporary file (must end with "XXXXXX")
    char tempFileName[] = "/tmp/sybma_fileXXXXXX";

    // Create the temporary file and get its file descriptor
    int fd = mkstemp(tempFileName);
    if (fd == -1) {
        perror("Error creating temporary file");
        throw std::runtime_error("Failed to create temporary file");
    }

    // Write the solver's SMT-LIB content to the file
    std::string smt2_content = solver.to_smt2();
    if (write(fd, smt2_content.c_str(), smt2_content.size()) == -1) {
        perror("Error writing to temporary file");
        close(fd);
        throw std::runtime_error("Failed to write to temporary file");
    }

    // Close the file descriptor (file persists on disk)
    close(fd);

    // Return the temporary file's name
    return std::string(tempFileName);
}

std::string get_symba_binary()
{
    // Retrieve the environment variable
    const char* env_var = std::getenv("SYMBA_BINARY");

    if (env_var) {
        std::string binary_path = env_var;

        // If the binary file is not executable (or not there), print an error message.
        if (access(binary_path.c_str(), X_OK) != 0) {
            std::cerr << "No valid executable binary found at:" << binary_path << std::endl;
            exit(0);
        }
        
        return binary_path;
    } else {
        // Environment variable is not set
        std::cerr << "Environment variable SYMBA_BINARY is not set!" << std::endl;
        exit(0);
    }
}

std::string get_result_from_symba(const std::string& input_smt_file)
{
    // Get the Symba binary path
    std::string symbaBinary = get_symba_binary();
    std::string command = symbaBinary + " -b=" + input_smt_file;

    // Run the command and capture output
    try {
        std::string result = Utils::run_command(command);
        return result;
    } catch (const std::runtime_error& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return "";
    }
}

std::unordered_map<std::string, double> process_symba_output(const std::string output)
{
    std::unordered_map<std::string, double> combVarMap;
    // Matches lines like: RESULT: comb_var_3 : [-1/2,5/3]
    // Captures: (1) variable name, (2) lower bound, (3) upper bound (as integers or rationals; ±1/0 = ±inf)
    std::regex pattern(R"(RESULT: (comb_var_\d+) : \[(-?\d+(?:/\d+)?|-?1/0),(-?\d+(?:/\d+)?|-?1/0)\])");
    std::smatch match;

    // Split the output into lines
    std::istringstream outputStream(output);
    std::string line;

    while (std::getline(outputStream, line)) {
        if (std::regex_search(line, match, pattern)) {
            std::string varName = match[1];
            std::string upperBoundStr = match[3];

            double upperBound;
            if (upperBoundStr == "1/0") {
                upperBound = INF;
            } else if (upperBoundStr == "-1/0") {
                upperBound = -INF;
            } else if (upperBoundStr.find('/') != std::string::npos) {
                // Handle rational numbers
                size_t slashPos = upperBoundStr.find('/');
                double num = std::stod(upperBoundStr.substr(0, slashPos));
                double denom = std::stod(upperBoundStr.substr(slashPos + 1));
                upperBound = num / denom;
            } else {
                upperBound = std::stod(upperBoundStr);
            }

            combVarMap[varName] = upperBound;
        }
    }

    return combVarMap;
}

std::unordered_map<std::string, double> run_symba_and_parse(z3::solver& solver)
{
    std::string tmp = write_solver_to_temp_file(solver);
    std::string out = get_result_from_symba(tmp);
    Utils::delete_file(tmp);
    return process_symba_output(out);
}

LinConstraintsSolution solve_combinations_parallel(const LinConstraintsProblem& problem, int num_threads)
{
    std::vector<Utils::CoeffVarSet> comb_vec(problem.combinations_to_solve.begin(), problem.combinations_to_solve.end());
    std::vector<double> results(comb_vec.size(), std::numeric_limits<double>::infinity());

    omp_set_num_threads(num_threads);

    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(comb_vec.size()); ++i) {
        try {
            z3::context ctx;

            //
            // Create variables and initial constraints
            //
            std::map<int, z3::expr> vars = create_z3_vars_map(ctx, problem.variables);
            z3::expr_vector init_constraints = get_init_constraints(ctx, vars, problem.var_bounds_map, problem.constraints_map);

            //
            // Generate combination-specific constraints
            //
            auto [new_cs, bound_cs, name] = generate_comb_block(ctx, vars, comb_vec[i], i);
            init_constraints.push_back(new_cs);
            z3::expr_vector bounds_constraints(ctx);
            bounds_constraints.push_back(bound_cs);

            //
            // Add implication to solver and run Symba
            //
            z3::solver solver(ctx);
            solver.add(z3::implies(z3::mk_and(init_constraints), z3::mk_and(bounds_constraints)));

            auto parsed = run_symba_and_parse(solver);

            // Use result if available
            if (!parsed.empty())
                results[i] = parsed.begin()->second;

        } catch (...) {
            // In case of any exception, default to +inf
            results[i] = std::numeric_limits<double>::infinity();
        }
    }

    //
    // Populate final solution
    //
    LinConstraintsSolution sol;
    for (size_t i = 0; i < comb_vec.size(); ++i) {
        sol.lower_bounds_map[comb_vec[i]] = -1*results[i];
    }

    return sol;
}

LinConstraintsSolution solve_combinations_sequential(const LinConstraintsProblem& problem)
{
    z3::context ctx;

    //
    // Create the Z3 variables
    //
    std::map<int, z3::expr> vars = create_z3_vars_map(ctx, problem.variables);

    //
    // Generate the initial constraints
    //
    z3::expr_vector init_constraints = get_init_constraints(ctx, vars, problem.var_bounds_map, problem.constraints_map);

    //
    // Generate the constraints for bound computation
    //
    z3::expr_vector bounds_constraints(ctx);
    std::map<std::string, Utils::CoeffVarSet> comb_names;

    int idx = 0;
    for (const auto& comb : problem.combinations_to_solve) {
        auto [new_cs, bound_cs, name] = generate_comb_block(ctx, vars, comb, idx++);
        init_constraints.push_back(new_cs);
        bounds_constraints.push_back(bound_cs);
        comb_names[name] = comb;
    }

    //
    // Create the solver and add "initial constraints => bounds constraints"
    //
    z3::solver solver(ctx);
    solver.add(z3::implies(z3::mk_and(init_constraints), z3::mk_and(bounds_constraints)));

    //
    // Solve using Symba and parse the answer
    //
    std::unordered_map<std::string, double> parsed = run_symba_and_parse(solver);

    //
    // Use the answer to populate the solution to be returned
    //
    LinConstraintsSolution sol;
    for (const auto& [name, val] : parsed)
        sol.lower_bounds_map[comb_names.at(name)] = -1*val;

    return sol;
}

LinConstraintsSolution SymbaLinConstraintsSolver::get_lower_bounds_for_combinations(const LinConstraintsProblem& problem)
{
    if (this->use_parallel_mode)
        return solve_combinations_parallel(problem, this->num_threads);
    else
        return solve_combinations_sequential(problem);
}

void SymbaLinConstraintsSolver::set_params(const std::unordered_map<std::string, std::string>& parameters)
{
    for (const auto& pair : parameters) {
        const std::string& key = pair.first;
        const std::string& value = pair.second;

        std::string lower_val = value;
        std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

        // Check if the key is one of the expected ones
        if (key == "use_parallel_mode") {
            this->use_parallel_mode = (lower_val == "true" || lower_val == "1");
        } else if (key == "num_threads") {
            this->num_threads = Utils::parse_string_to_int(value);
        } else {
            // Throw error if an unexpected key is found
            throw std::invalid_argument("Unexpected key: " + key);
        }
    }
}