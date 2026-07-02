import argparse
import json

from benchmark_runner import BenchmarkRunConfig, BenchmarkRunner
from results_checker import ResultsChecker
from plot import create_exp1_plots, create_exp2_plots, create_app_exp2_no_collate_plots, create_solver_time_comparison_plots
from utils import get_project_root, get_logger

##
# Constants
##
PROJECT_ROOT   = get_project_root()
DATASET_FOLDER = f"{PROJECT_ROOT}/data/sv-benchmarks/"
PK_CONFIG_FILE = f"{PROJECT_ROOT}/experiments/yaml/clam-poly.svcomp19.yaml"

def run_linear_tradeoff_experiments(
    dataset_name,
    output_logs_folder,
    abstract_domain,
    logger,
    should_track_mem,
    benchmark_csv_file = "all_benchmarks.csv",
):
    exp_name = "7.1_linear" if benchmark_csv_file == "all_benchmarks.csv" else "7.1_linear_subset"
    runner = BenchmarkRunner()
    config = BenchmarkRunConfig(
        abs_dom  = abstract_domain,
        cpu_time = 200,
        max_collate_count = -1,
        inv_json_file = "inv.json"
    )
    if abstract_domain == "pk":
        config.config_yaml_file = PK_CONFIG_FILE

    input_folder = f"{DATASET_FOLDER}/{dataset_name}"
    output_base_folder = f"{PROJECT_ROOT}/{output_logs_folder}/{exp_name}/{dataset_name}/{abstract_domain}"
    logger.info(f"All detailed logs for this experiment are present at {output_base_folder}")
    result_checker = ResultsChecker()

    ##
    # Baseline with the specified domain
    ##
    logger.info(f"Analyzing benchmarks with baseline ELINA in domain: {abstract_domain}")
    bl_output_folder = f"{output_base_folder}/baseline"
    runner.run_using_config(config, input_folder, bl_output_folder, logger, benchmark_csv_file, should_log_time_mem_stats=should_track_mem)
    logger.info("-----------------------------------------------")

    config.aff_prec_level  = "affine-full"
    config.quad_prec_level = "default"

    ##
    # Gurobi LP baseline with the specified domain
    ##
    gb_output_folder = f"{output_base_folder}/aff-gb"
    logger.info(f"Analyzing benchmarks with Gurobi based LP solver transformer in domain: {abstract_domain}")
    runner.run_using_config(config, input_folder, gb_output_folder, logger,benchmark_csv_file, 
                            bl_outp_folder=bl_output_folder, # to compare with the baseline results
                            should_log_time_mem_stats=should_track_mem)
    logger.info("-----------------------------------------------")
    

    ##
    # Differing the number of epochs and collecting the results
    ##
    num_epochs = [0, 1, 2, 3, 4, 5]
    prev_name  = "baseline"
    for ep in num_epochs:
        config.lin_solver_config = {
            "name": "dual",
            "num_epochs" : str(ep)
        }
        this_name = f"aff-dual-{ep}"
        output_folder = f"{output_base_folder}/{this_name}"
        prev_output_folder = f"{output_base_folder}/{prev_name}" # to compare with the previous run
        prev_name = this_name
        logger.info(f"Analyzing benchmarks with AbsEvolve Transformer with {ep} gradient steps in domain: {abstract_domain}")
        runner.run_using_config(config, input_folder, output_folder, logger, benchmark_csv_file, 
                                bl_outp_folder=bl_output_folder, # to compare with the baseline results, 
                                should_log_time_mem_stats=should_track_mem)
        result_checker.compare(prev_output_folder, output_folder, "prev_epoch_cmp.json")
        result_checker.compare(gb_output_folder, output_folder, "gurobi_cmp.json")
        logger.info("-----------------------------------------------")

def run_full_tradeoff_experiments(
    dataset_name,
    output_logs_folder,
    abstract_domain, 
    logger,
    should_track_mem,
    should_collate_instructions,
    benchmark_csv_file = "all_benchmarks.csv",
):
    exp_name = "7.2_full" if should_collate_instructions else "appendix_d.3_full_no_collation"
    runner = BenchmarkRunner()
    config = BenchmarkRunConfig(
        abs_dom  = abstract_domain,
        cpu_time = 200,
        max_collate_count = -1 if should_collate_instructions else 1,
        inv_json_file = "inv.json"
    )
    if abstract_domain == "pk":
        config.config_yaml_file = PK_CONFIG_FILE

    input_folder = f"{DATASET_FOLDER}/{dataset_name}"
    output_base_folder = f"{PROJECT_ROOT}/{output_logs_folder}/{exp_name}/{dataset_name}/{abstract_domain}"
    logger.info(f"All detailed logs for this experiment are present at {output_base_folder}")
    result_checker = ResultsChecker()

    ##
    # Baseline with the specified domain
    ##
    logger.info(f"Analyzing benchmarks with baseline ELINA in domain: {abstract_domain}")
    bl_output_folder = f"{output_base_folder}/baseline"
    runner.run_using_config(config, input_folder, bl_output_folder, logger, benchmark_csv_file, should_log_time_mem_stats=should_track_mem)
    logger.info("-----------------------------------------------")

    ##
    # Differing the number of epochs and collecting the results
    ##
    if abstract_domain != "pk":
        config.aff_prec_level  = "affine-full"
    config.quad_prec_level = "quad-full"
    num_epochs = [0, 1, 2, 3, 4, 5]
    prev_name  = "baseline"
    for ep in num_epochs:
        config.lin_solver_config = {
            "name": "dual",
            "num_epochs" : str(ep)
        }
        if abstract_domain == "pk":
            config.lin_solver_config["num_adaptive_learning_epochs"] = 0

        config.quad_solver_config = {
            "name": "dual",
            "num_epochs" : str(ep)
        }
        if abstract_domain == "pk":
            config.quad_solver_config["quad_initialization"] = 0
            config.quad_solver_config["split_lams_lr"] = 0.5

        this_name = f"aff-quad-dual-{ep}"
        output_folder = f"{output_base_folder}/{this_name}"
        prev_output_folder = f"{output_base_folder}/{prev_name}" # to compare with the previous run
        prev_name = this_name
        logger.info(f"Analyzing benchmarks with AbsEvolve Transformer with {ep} gradient steps in domain: {abstract_domain}")
        runner.run_using_config(config, input_folder, output_folder, logger, benchmark_csv_file, 
                                bl_outp_folder=bl_output_folder, # to compare with the baseline results 
                                should_log_time_mem_stats=should_track_mem)
        result_checker.compare(prev_output_folder, output_folder, "prev_epoch_cmp.json")
        logger.info("-----------------------------------------------")

def run_log_linear_problems_experiment(
    dataset_name,
    output_logs_folder,
    abstract_domain,
    logger,
    benchmark_csv_file = "all_benchmarks.csv",
):
    exp_name = "7.1_solver_comp"
    runner = BenchmarkRunner()
    config = BenchmarkRunConfig(
        abs_dom  = abstract_domain,
        cpu_time = 200,
        max_collate_count = -1,
        inv_json_file = "inv.json"
    )
    if abstract_domain == "pk":
        config.config_yaml_file = PK_CONFIG_FILE

    input_folder = f"{DATASET_FOLDER}/{dataset_name}"
    output_base_folder = f"{PROJECT_ROOT}/{output_logs_folder}/{exp_name}/{dataset_name}/{abstract_domain}"
    result_checker = ResultsChecker()

    ##
    # Baseline with the specified domain
    ##
    bl_output_folder = f"{output_base_folder}/baseline"
    logger.info(f"Analyzing benchmarks with baseline ELINA in domain: {abstract_domain}")
    runner.run_using_config(config, input_folder, bl_output_folder, logger, benchmark_csv_file)
    logger.info("-----------------------------------------------")

    config.aff_prec_level  = "affine-full"
    config.quad_prec_level = "default"

    ##
    # Gurobi LP baseline with the specified domain and log enabled
    ##
    config.lin_solver_config = {
                "name": "gb",
                "log" : "1"
            }
    gb_output_folder = f"{output_base_folder}/aff-gb"
    logger.info(f"Analyzing benchmarks with Gurobi based LP solver transformer (problem logging on) in domain: {abstract_domain}")
    runner.run_using_config(config, input_folder, gb_output_folder, logger, benchmark_csv_file,
                            bl_outp_folder=bl_output_folder,
                            should_parse_json=True) # Option to parse the logged problems.
    logger.info("-----------------------------------------------")

    ##
    # Results with dual and epochs 5 with log enabled
    ##
    config.lin_solver_config = {
        "name": "dual",
        "num_epochs" : "5",
        "log" : "1"
    }
    output_folder = f"{output_base_folder}/aff-dual-5"
    logger.info(f"Analyzing benchmarks with AbsEvolve Transformer with 5 gradient steps (problem logging on) in domain: {abstract_domain}")
    runner.run_using_config(config, input_folder, output_folder, logger, benchmark_csv_file, 
                            bl_outp_folder=bl_output_folder,
                            should_parse_json=True) # Option to parse the logged problems.
    logger.info("-----------------------------------------------")
    result_checker.compare(gb_output_folder, output_folder, "gurobi_cmp.json")

def run_symba_baseline_for_linear_tradeoffs(
    dataset_name,
    output_logs_folder,
    abstract_domain,
    logger,
    benchmark_csv_file = "all_benchmarks.csv",
):
    exp_name = "7.1_symba_baseline"
    runner = BenchmarkRunner()
    config = BenchmarkRunConfig(
        abs_dom  = abstract_domain,
        cpu_time = 200,
        max_collate_count = -1,
        inv_json_file = "inv.json"
    )
    if abstract_domain == "pk":
        config.config_yaml_file = PK_CONFIG_FILE

    input_folder = f"{DATASET_FOLDER}/{dataset_name}"
    output_base_folder = f"{PROJECT_ROOT}/{output_logs_folder}/{exp_name}/{dataset_name}/{abstract_domain}"

    config.aff_prec_level  = "affine-full"
    config.quad_prec_level = "default"

    ##
    # Symba baseline
    ##
    config.lin_solver_config = {
        "name": "symba",
        "use_parallel_mode": "0"
    }
    symba_output_folder = f"{output_base_folder}/aff-symba"
    runner.run_using_config(config, input_folder, symba_output_folder, logger, benchmark_csv_file)


##
# Main experiments
##
def run_exp_7_1_linear(dataset_name, output_logs_folder, plots_folder, benchmark_csv_file = "all_benchmarks.csv", should_track_mem = False):
    """
    Experiment 7.1: Run trade-off experiments with only linear collation.
    """
    domains = ["elina-zones", "oct"]

    is_subset = benchmark_csv_file != "all_benchmarks.csv"
    if is_subset:
        logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/7.1_linear_subset.log")
        logger.info(f"Running linear trade-off experiments on a subset of benchmarks (Sec 7.1) for domains: {domains}")
        logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/7.1_linear_subset.log")
    else:
        logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/7.1_linear.log")
        logger.info(f"Running linear trade-off experiments (Sec 7.1) for domains: {domains}")
        logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/7.1_linear.log")

    for domain in domains:
        if is_subset:
            logger.info(f"Running linear trade-off experiments on a subset of benchmarks (Sec 7.1) for domain: {domain}")
        else:
            logger.info(f"Running linear trade-off experiments (Sec 7.1) for domain: {domain}")

        run_linear_tradeoff_experiments(dataset_name, output_logs_folder, domain, logger, should_track_mem, benchmark_csv_file=benchmark_csv_file)
        logger.info(f"Completed runs for domain {domain} in experiment for 7.1. Now creating the plot in {PROJECT_ROOT}/{plots_folder}.")

        create_exp1_plots(f"{PROJECT_ROOT}/{output_logs_folder}", f"{PROJECT_ROOT}/{plots_folder}", is_subset, [domain])
        logger.info("==================================================")

def run_exp_7_2_full(dataset_name, output_logs_folder, plots_folder, should_collate, oct_zones_benchmarks, poly_benchmarks, should_track_mem = False):
    """
    Experiment 7.2: Run trade-off experiments with both linear and quadratic operators handled.
    should_collate_instructions is a boolean that indicates whether to run the experiments with instruction collation enabled or not. 
    If True, the experiments are run with instruction collation enabled, otherwise they are run with instruction collation disabled.
    """
    domains = ["elina-zones", "oct", "pk"]

    if should_collate:
        logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/7.2_full.log")
        logger.info(f"Running full trade-off experiments (Sec 7.2) for domains: {domains}")
        logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/7.2_full.log")
    else:
        logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/appendix_d.3_full_no_collation.log")
        logger.info(f"Running full trade-off experiments without collation (Appendix D.3) for domains: {domains}")
        logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/appendix_d.3_full_no_collation.log")

    for domain in domains:
        if should_collate:
            logger.info(f"Running full trade-off experiments (Sec 7.2) for domain: {domain}")
        else:
            logger.info(f"Running full trade-off experiments without collation (Appendix D.3) for domain: {domain}")
        
        if domain in ["elina-zones", "oct"]:
             run_full_tradeoff_experiments(dataset_name, output_logs_folder, domain, logger, should_track_mem, should_collate, oct_zones_benchmarks)
        else:
            run_full_tradeoff_experiments(dataset_name, output_logs_folder, domain, logger, should_track_mem, should_collate, poly_benchmarks)
        
        if should_collate:
            logger.info(f"Completed runs for domain {domain} in experiment for 7.2. Now creating the plots at {PROJECT_ROOT}/{plots_folder}.")
            create_exp2_plots(f"{PROJECT_ROOT}/{output_logs_folder}", f"{PROJECT_ROOT}/{plots_folder}", [domain])
            logger.info("==================================================")
        else:
            logger.info(f"Completed runs for domain {domain} in experiment for Appendix D.3. Now creating the plots at {PROJECT_ROOT}/{plots_folder}.")
            create_app_exp2_no_collate_plots(f"{PROJECT_ROOT}/{output_logs_folder}", f"{PROJECT_ROOT}/{plots_folder}", [domain])
            logger.info("==================================================")

def run_exp_7_1_solver_comparison(dataset_name, output_logs_folder, plots_folder):
    """
    Run the experiment comparing the solvers (Sec 7.1).
    """
    logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/7.1_solver_comparison.log")
    logger.info(f"Running solver comparison experiment (Sec 7.1) for domains: elina-zones and oct")
    logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/7.1_solver_comparison.log")

    logger.info(f"Collecting solver problems for comparison experiments (Sec 7.1) for domain: elina-zones")
    run_log_linear_problems_experiment(dataset_name, args.logs_folder, "elina-zones", logger)

    logger.info("==================================================")

    logger.info(f"Collecting solver problems for comparison experiments (Sec 7.1) for domain: oct")
    run_log_linear_problems_experiment(dataset_name, args.logs_folder, "oct", logger)

    logger.info("==================================================")

    logger.info(f"Creating solver time comparison plots as in Sec 7.1 at {PROJECT_ROOT}/{plots_folder}")
    create_solver_time_comparison_plots(f"{PROJECT_ROOT}/{output_logs_folder}", f"{PROJECT_ROOT}/{plots_folder}")

def run_exp_7_1_symba_baseline(dataset_name, output_logs_folder):
    """
    Run the experiment computing the time taken by Symba baseline (Sec 7.1).
    """
    logger = get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/7.1_symba_baseline.log")
    logger.info(f"Running Symba baseline experiment (Sec 7.1) for domains: elina-zones and oct")
    logger.info(f"Experiment logs are present at {PROJECT_ROOT}/{output_logs_folder}/7.1_symba_baseline.log")

    logger.info(f"Running Symba baseline for linear trade-offs in domain: elina-zones")
    run_symba_baseline_for_linear_tradeoffs(dataset_name, args.logs_folder, "elina-zones", logger)

    # Calculate the number of time outs and log it.
    time_file = f"{PROJECT_ROOT}/{output_logs_folder}/7.1_symba_baseline/nla-digbench/elina-zones/aff-symba/run_log.json"
    with open(time_file, "r") as f:
        time_data = json.load(f)
    timeouts = sum(
        1 for b in time_data["benchmarks"].values() if b["timed_out"]
    )

    logger.info(f"Number of time outs for elina-zones: {timeouts}")

    logger.info("==================================================")

    logger.info(f"Running Symba baseline for linear trade-offs in domain: oct")
    run_symba_baseline_for_linear_tradeoffs(dataset_name, args.logs_folder, "oct", logger)

    # Calculate the number of time outs and log it.
    time_file = f"{PROJECT_ROOT}/{output_logs_folder}/7.1_symba_baseline/nla-digbench/oct/aff-symba/run_log.json"
    with open(time_file, "r") as f:
        time_data = json.load(f)
    timeouts = sum(
        1 for b in time_data["benchmarks"].values() if b["timed_out"]
    )
    logger.info(f"Number of time outs for oct: {timeouts}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--exp_name",
        type=str,
        required=True,
        choices=["7.1-Linear-Subset", "7.1-Linear", "7.2-Full", "7.1-Solver-Comp", "7.1-Symba-Baseline", "Appendix-D.3-No-Collation"],
        help="Name of the experiment configuration"
    )
    parser.add_argument(
        "--logs_folder",
        default="logs",
        help="Output logs directory relative to project root"
    )
    parser.add_argument(
        "--plots_folder",
        default="plots",
        help="Plots directory relative to project root"
    )
    args = parser.parse_args()

    DATASET_NAME = "nla-digbench"
    OCT_ZONES_BENCHMARKS = "oct_zones_tradeoff_benchmarks.csv"
    POLY_BENCHMARKS = "poly_tradeoff_benchmarks.csv"
    SUBSET_BENCHMARKS = "all_benchmarks_subset.csv"

    if args.exp_name == "7.1-Linear":
        run_exp_7_1_linear(DATASET_NAME, args.logs_folder, args.plots_folder)
    elif args.exp_name == "7.1-Linear-Subset":
        run_exp_7_1_linear(DATASET_NAME, args.logs_folder, args.plots_folder, benchmark_csv_file=SUBSET_BENCHMARKS)
    elif args.exp_name == "7.2-Full":
        run_exp_7_2_full(DATASET_NAME, args.logs_folder, args.plots_folder, True, OCT_ZONES_BENCHMARKS, POLY_BENCHMARKS)
    elif args.exp_name == "7.1-Solver-Comp":
        run_exp_7_1_solver_comparison(DATASET_NAME, args.logs_folder, args.plots_folder)
    elif args.exp_name == "7.1-Symba-Baseline":
        run_exp_7_1_symba_baseline(DATASET_NAME, args.logs_folder)
    elif args.exp_name == "Appendix-D.3-No-Collation":
        run_exp_7_2_full(DATASET_NAME, args.logs_folder, args.plots_folder, False, OCT_ZONES_BENCHMARKS, POLY_BENCHMARKS)