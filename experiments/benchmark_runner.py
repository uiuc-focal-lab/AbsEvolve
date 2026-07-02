import csv
import json
import os
import psutil
import re
import signal
import shutil
import subprocess
import sys
import time

from collections import defaultdict
from dataclasses import dataclass
from itertools import filterfalse
from pathlib import Path
from tqdm import tqdm
from typing import Dict, List

from results_checker import ResultsChecker
from results_parser import ResultsParser
from utils import get_project_root, printer, update_dict, get_logger

##
# Constants
##
PROJECT_ROOT     = get_project_root()
CLAM_YAML_PY     = f"{PROJECT_ROOT}/src/clam/build/install/bin/clam-yaml.py"
CONFIG_YAML_FILE = f"{PROJECT_ROOT}/experiments/yaml/clam.svcomp19.yaml"
ALL_PROCESSES    = []

def cleanup(signum, frame):
    """
    Terminate all spawned processes on Ctrl+C.
    """
    print("Stopping all child processes...")

    for proc in ALL_PROCESSES:
        if proc.poll() is None:  # If still running
            try:
                parent = psutil.Process(proc.pid)
                for child in parent.children(recursive=True):  # Kill all descendants
                    child.terminate()
                parent.terminate()  # Kill the parent process
            except psutil.NoSuchProcess:
                pass  # Process is already gone

    sys.exit(1)

# Ensure Ctrl+C triggers cleanup
signal.signal(signal.SIGINT, cleanup)

@dataclass
class BenchmarkRunConfig:
    ##
    # Required parameters
    ##

    # abstract domain to use
    abs_dom: str

    ##
    # Optional parameters
    ##
    config_yaml_file: str = CONFIG_YAML_FILE
    specific_bench_names_list: List[str] = None
    inv_json_file: str = None
    cpu_time: int = -1
    mem: int = -1
    
    # Crab and elina parameters
    aff_prec_level: str = "default"
    quad_prec_level: str = "default"
    crab_only_cfg: bool = False
    max_collate_count: int = -1
    lin_solver_config: Dict[str, str] = None
    quad_solver_config: Dict[str, str] = None

class BenchmarkRunner():
    def __init__(self):
        self.results_parser = ResultsParser()
        self.res_checker = ResultsChecker()

    ##
    # Helper methods
    ##
    def _get_all_c_files(self, folder_path: str, recursive: bool = False):
        """
        Get all .c files in the specified folder
        """
        folder = Path(folder_path)

        if not folder.exists():
            raise FileNotFoundError(f"The folder '{folder_path}' does not exist.")
        if not folder.is_dir():
            raise NotADirectoryError(f"The path '{folder_path}' is not a directory.")

        if recursive:
            c_files = list(folder.rglob('*.c'))
        else:
            c_files = list(folder.glob('*.c'))

        return c_files

    def _create_empty_folder(self, folder_name):
        """
        Create empty folder at the specified path
        """
        # Check if the folder exists
        if os.path.exists(folder_name):
            # Delete all files inside the folder
            for filename in os.listdir(folder_name):
                file_path = os.path.join(folder_name, filename)
                try:
                    if os.path.isfile(file_path) or os.path.islink(file_path):
                        os.unlink(file_path)  # Remove the file or link
                    elif os.path.isdir(file_path):
                        shutil.rmtree(file_path)  # Remove the directory and its contents
                except Exception as e:
                    printer(f'Failed to delete {file_path}. Reason: {e}', self.top_level_logfile)
        else:
            # Create the folder if it doesn't exist
            os.makedirs(folder_name)

    def _extract_json_lines(self, input_file_path, output_file_path):
        """
        Extract (and delete) json lines from the input file and write them in
        the output file
        """
        json_array = []
        pattern = re.compile(r".*\"solver_time\".*")

        with open(input_file_path, "r") as input_file:
            lines = input_file.readlines()

        with open(input_file_path, "w") as input_file:
            for line in lines:
                if pattern.search(line):
                    try:
                        json_obj = json.loads(line.strip())
                        json_array.append(json_obj)
                    except json.JSONDecodeError:
                        printer(f"Skipping invalid JSON line: {line.strip()}", self.top_level_logfile)
                else:
                    input_file.write(line)

        with open(output_file_path, "w") as output_file:
            json.dump(json_array, output_file, indent=4)

    def _merge_probs_jsons(self, parent_folder: str):
        """
        Merge all probs.json files in the subfolders of the parent folder into a single probs.json file in the parent folder.
        """
        parent = Path(parent_folder)
        combined = []

        for subdir, _, files in os.walk(parent):
            if subdir == str(parent):
                continue  # skip parent itself
            sub_path = Path(subdir)
            if "probs.json" in files:
                file_path = sub_path / "probs.json"
                try:
                    with open(file_path, "r") as f:
                        data = json.load(f)
                        if isinstance(data, list):
                            combined.extend(data)
                        else:
                            print(f"Warning: {file_path} is not a JSON array.")
                except Exception as e:
                    print(f"Error reading {file_path}: {e}")

        # Write to parent/probs.json
        output_path = parent / "probs.json"
        with open(output_path, "w") as f:
            json.dump(combined, f, indent=2)

    def _merge_baseline_comp_files(self, parent_folder: str):
        """
        Merge all baseline_cmp.json files in the subfolders of the parent folder into a single baseline_cmp.json file in the parent folder.
        """
        parent = Path(parent_folder)
        all_result_dicts = {}

        json_name = "baseline_cmp.json"

        for subdir, _, files in os.walk(parent):
            if subdir == str(parent):
                continue  # skip the parent itself

            sub_path = Path(subdir)
            if json_name not in files:
                continue

            file_path = sub_path / json_name
            try:
                with open(file_path, "r") as f:
                    data = json.load(f)

                if not data:
                    continue  # skip empty {}

                # Merge all dictionaries result into the global result
                for (dic_name, dic) in data.items():
                    if dic_name not in all_result_dicts:
                        all_result_dicts[dic_name] = {}
                    
                    update_dict(all_result_dicts[dic_name], dic)

            except Exception as e:
                print(f"Error reading {file_path}: {e}")

        # Write merged output
        output_path = parent / json_name
        with open(output_path, "w") as f:
            f.write(json.dumps(all_result_dicts, indent=2))

    def _merge_all_checks(self, parent_folder: str):
        """
        Merge all all_checks.json files in the subfolders of the parent folder into a single all_checks.json file in the parent folder.
        """
        parent = Path(parent_folder)
        merged = defaultdict(int)

        json_name = "all_checks.json"

        for subdir, _, files in os.walk(parent):
            if subdir == str(parent):
                continue  # skip the parent itself

            sub_path = Path(subdir)
            if json_name not in files:
                continue

            file_path = sub_path / json_name
            try:
                with open(file_path, "r") as f:
                    data = json.load(f)

                if not data or "all_checks" not in data:
                    continue

                for k, v in data["all_checks"].items():
                    merged[k] += v

            except Exception as e:
                print(f"Error reading {file_path}: {e}")

        # Write merged output
        output_path = parent / json_name
        with open(output_path, "w") as f:
            f.write(json.dumps({"all_checks": dict(merged)}, indent=2))

    def _get_loading_times_and_memory(self, log_file):
        """
        Extract the loading times and memory usage from the log file.
        """
        total_ms = 0
        total_mem = -1
        time_pattern = re.compile(r"Time for loading lib .*?: (\d+)\s*ms", re.IGNORECASE)
        mem_pattern = re.compile(r"Maximum resident set size \(kbytes\):\s+(\d+)")
        for line in log_file:
            time_match = time_pattern.search(line)
            mem_match = mem_pattern.search(line)
            if time_match:
                total_ms += int(time_match.group(1))
            if mem_match:
                total_mem = int(mem_match.group(1))
        return total_ms / 1000.0, total_mem

    def _extract_invariants(self, log_file, all_invs_file, domain, timed_out):
        """
        Extract the invariants from the log file and write them in the all_invs_file.
        It uses the results_parser to parse the invariants from the log file.
        """
        invariants = self.results_parser.parse_invariants_from_file(log_file, domain)
        with open(all_invs_file, "w") as inv_file:
            json.dump({"all_invs": invariants, "timed_out": timed_out}, inv_file, indent = 4)

    def _extract_checks(self, log_file, all_checks_file, timed_out):
        """
        Extract the assertion checks from the log file and write them in the all_checks_file.
        It uses the results_parser to parse the checks from the log file.
        """
        checks = self.results_parser.parse_checks_from_file(log_file)
        with open(all_checks_file, "w") as checks_file:
            json.dump({"all_checks": checks, "timed_out": timed_out}, checks_file, indent = 4)

    def _run_command_in_folder(self, command, outp_folder, domain, bl_outp_folder, should_parse_json):
        """
        1. Run the command in the specified output folder and log the output in a log file in that folder.
        2. Extract the invariants and checks from the log file and write them in the output folder.
        3. If bl_outp_folder is not None, compare the invariants and checks with the baseline output in bl_outp_folder 
        and write the comparison result in the output folder.
        """
        self._create_empty_folder(outp_folder)
        os.chdir(outp_folder)

        time_taken = 0
        timed_out = False
        local_log_file = f"{outp_folder}/{self.log_file_basename}"
        local_invs_file = f"{outp_folder}/all_invs.json"
        local_checks_file = f"{outp_folder}/all_checks.json"

        # Run the command and log the output in the local log file.
        with open(local_log_file, "w") as log_file:
            s = time.perf_counter()
            try:
                proc = subprocess.Popen(command, 
                                        shell=True,
                                        stdout=log_file,
                                        stderr=log_file,
                                        text=True,
                                        preexec_fn=os.setpgrp)
                ALL_PROCESSES.append(proc)

                ret_code = proc.wait()  # Wait for process to complete

                time_taken = time.perf_counter() - s

                if ret_code == 20 or ret_code == 26:
                    # Time out!
                    timed_out = True

            except Exception as e:
                printer(f"Error executing command: {e}", self.top_level_logfile)
                cleanup(None, None)
    
        with open(local_log_file, "r") as log_file:
            loading_time, total_mem = self._get_loading_times_and_memory(log_file)
            time_taken -= loading_time

        ##
        # Post command running tasks: 
        # Extracting invariants, extracting json logs, compare invariants from the baseline
        ##
        if Path(f"{outp_folder}/inv.main.json").exists():
            inv_outp_file = f"{outp_folder}/inv.main.json"
        else:
            inv_outp_file = local_log_file

        if Path(f"{outp_folder}/inv.main.json").exists():
            checks_outp_file = f"{outp_folder}/inv.main.json"
        else:
            checks_outp_file = local_log_file
        self._extract_invariants(inv_outp_file, local_invs_file, domain, timed_out)
        self._extract_checks(checks_outp_file, local_checks_file, timed_out)

        if should_parse_json:
            # Extract the json logs from the local log file and write them in the local probs.json file.
            # The probs.json are only logged if the solvers are called with log flag set to true.
            # In this setting, the probs.json file contains the linear and quadratic problems that
            # were solved by the tool along with their solving times.
            self._extract_json_lines(local_log_file, "probs.json")

        if bl_outp_folder is not None:
            # Compare the invariants and checks with the baseline output and write the comparison result in the output folder.
            bench_name = outp_folder.split('/')[-1]
            this_compare_result_file = f"{bench_name}/baseline_cmp.json"
            self.res_checker.compare(bl_outp_folder, str(Path(outp_folder).parent), this_compare_result_file, bench_name)

        return command, outp_folder, time_taken, total_mem, timed_out

    def _get_bench_file_names_to_run(self, bench_folder, benchmark_csv_file):
        """
        Get the benchmark file names to run from the benchmark csv file in the bench_folder.
        """
        bench_csv_file = f"{bench_folder}/{benchmark_csv_file}"
        if Path(bench_csv_file).is_file():
            with open(bench_csv_file, mode="r") as file:
                reader = csv.DictReader(filterfalse(
                    lambda ln: ln.lstrip().startswith("#"), file))

                values = []
                for row in reader:
                    values.append(row["Filename"])

            return values
        else:
            raise ValueError(f"{bench_csv_file} is not a valid file!")
    
    def _run_analysis_in_bench_folder(self,
                                      command_flags,
                                      outp_folder_name,
                                      bench_folder,
                                      logger,
                                      config_yaml_file,
                                      domain,
                                      specific_bench_names_list = None,
                                      benchmark_csv_file = "all_benchmarks.csv",
                                      bl_outp_folder = None,
                                      should_parse_json = False,
                                      should_log_time_mem_stats = False):
        """
        Run analysis with the specified command flags on the files in the bench_folder.
        """
        # If specific_bench_names_list is provided, run only on those benchmarks. 
        # Otherwise, get the benchmark file names to run from the benchmark csv file in the bench_folder.
        if specific_bench_names_list is None:
            bench_names = self._get_bench_file_names_to_run(bench_folder, benchmark_csv_file)
            msg = f"Running on {len(bench_names)} benchmarks from {benchmark_csv_file} in {bench_folder}"
            printer(msg, self.top_level_logfile)
            logger.info(msg)
        else:
            bench_names = specific_bench_names_list
            msg = f"Running on specified benchmarks: {bench_names}"
            printer(msg, self.top_level_logfile)
            logger.info(msg)
        
        timer = 0
        self.run_stats_json["benchmarks"] = {}
        pbar = tqdm(bench_names, desc="Benchmarks", dynamic_ncols=True)
        for bench_name in pbar:
            benchmark_file=f"{bench_folder}/{bench_name}"
            output_folder=f"{outp_folder_name}/{bench_name}"

            # Create the command to run.
            command = f"python {CLAM_YAML_PY} -y {config_yaml_file} {benchmark_file}" +  command_flags
            if should_log_time_mem_stats:
                command = f"/usr/bin/time -v {command}"
            
            # Run the command and get the time taken, memory used and whether it timed out or not.
            _, _, t, mem, timed_out = self._run_command_in_folder(command, output_folder, domain, bl_outp_folder, should_parse_json)
            
            # Print the time taken and memory used for the benchmark and update the stats json.
            printer(f"{bench_name} Time taken: {t} {'Mem: ' + str(mem) if mem != -1 else ''} {'Timed out!' if timed_out else ''}", self.top_level_logfile)
            self.run_stats_json["benchmarks"][bench_name] = {"time": t, "timed_out": timed_out}
            if mem != -1:
                self.run_stats_json["benchmarks"][bench_name]["mem"] = mem
            pbar.set_postfix(time=f"{t:.2f}s")
            timer += t
        
        printer(f"Total time taken: {timer}", self.top_level_logfile)
        self.run_stats_json["total_time"] = timer
        logger.info(f"Total time taken: {timer}")

        ##
        # Collate all the checks
        ##
        self._merge_all_checks(outp_folder_name)

        ##
        # Collate all the invariant results.
        # This is done only when bl_outp_folder is not None because the invariant results 
        # are only computed in this case and we want to collate them for the benchmarks that were run.
        ##
        if bl_outp_folder is not None:
            self._merge_baseline_comp_files(outp_folder_name)

        if should_parse_json:
            ##
            # Collate all probs json files
            ##
            self._merge_probs_jsons(outp_folder_name)

    def _solver_config_to_command_string(self, solver_config):
        """
        Take solver config and convert to str representation for the command to run
        """
        command_string = ""

        for k, v in solver_config.items():
            command_string = command_string + f"{k}->{v},"

        return command_string

    def _generate_command_flags(self, bench_config:BenchmarkRunConfig):
        """
        Generate the command flags to run the benchmark with the specified configuration.
        """
        command_flags = f" --crab-dom {bench_config.abs_dom}"

        if bench_config.inv_json_file:
            command_flags += f" --ojson {bench_config.inv_json_file}"

        if bench_config.cpu_time != -1:
            command_flags += f" --cpu {bench_config.cpu_time}"

        if bench_config.mem != -1:
            command_flags += f" --mem {bench_config.mem}"

        if bench_config.aff_prec_level != "default":
            command_flags += f" --crab-affine-precision-level {bench_config.aff_prec_level}"
        if bench_config.quad_prec_level != "default":
            command_flags += f" --crab-quad-precision-level {bench_config.quad_prec_level}"
        if bench_config.crab_only_cfg:
            command_flags += " --crab-only-cfg"

        if bench_config.max_collate_count != -1:
            command_flags += f" --crab-max-collate-count {bench_config.max_collate_count}"
        
        # Crab dom params
        crab_dom_params = []

        if bench_config.lin_solver_config:
            crab_dom_params.append(f"elina.linear_solver_config={self._solver_config_to_command_string(bench_config.lin_solver_config)}")
        if bench_config.quad_solver_config:
            crab_dom_params.append(f"elina.quad_solver_config={self._solver_config_to_command_string(bench_config.quad_solver_config)}")

        if len(crab_dom_params) != 0:
            command_flags += f" --crab-dom-params '{':'.join(crab_dom_params)}'"

        return command_flags

    ##
    # Public Methods
    ##
    def run_using_config(
        self,
        bench_config,  # Config to run the benchmark
        input_folder,  # Folder with the programs to analyze
        outp_folder,   # Folders where the analysis result has to stored
        logger,        # Logger to log the output of the benchmark run
        benchmark_csv_file = "all_benchmarks.csv", # CSV file in the input folder from which to read the files to analyze
        bl_outp_folder = None, # Folder where a baseline might exist for comparison
        should_parse_json = False, # Should the output files be parsed for JSON output (only needed if JSON logs),
        should_log_time_mem_stats = False # Should the commands log the time and memory stats as well
    ):
        global ALL_PROCESSES
        ALL_PROCESSES = []

        # Get the command flags and log them
        self.log_file_basename = "run_log.txt"
        self.top_level_logfile = f"{outp_folder}/{self.log_file_basename}"
        cmdflags = self._generate_command_flags(bench_config)
        printer(f"Running with {cmdflags}", self.top_level_logfile)

        # Stats json
        self.run_stats_json = {}

        # Run the analysis in the benchmark folder
        self._run_analysis_in_bench_folder(cmdflags, 
                                           outp_folder, 
                                           input_folder,
                                           logger,
                                           bench_config.config_yaml_file,
                                           bench_config.abs_dom,
                                           bench_config.specific_bench_names_list,
                                           benchmark_csv_file,
                                           bl_outp_folder,
                                           should_parse_json,
                                           should_log_time_mem_stats)

        with open(f"{outp_folder}/run_log.json", "w") as f:
            json.dump(self.run_stats_json, f, indent = 4)

def run_example():
    runner = BenchmarkRunner()
    output_logs_folder = "logs"
    dataset_folder = f"{PROJECT_ROOT}/data/custom/"
    logger =  get_logger(log_file=f"{PROJECT_ROOT}/{output_logs_folder}/custom.log")
    logger.info("The logs for this run will be stored in: " + f"{PROJECT_ROOT}/{output_logs_folder}/custom/")
    abstract_domain = "elina-zones"
    config = BenchmarkRunConfig(
        abs_dom  = abstract_domain,
        cpu_time = 200,
        max_collate_count = -1,
        inv_json_file = "inv.json"
    )

    output_base_folder = f"{PROJECT_ROOT}/{output_logs_folder}/custom/"

    ##
    # Baseline with the specified domain
    ##
    bl_output_folder = f"{output_base_folder}/baseline"
    logger.info(f"Running baseline with the specified domain: {abstract_domain}")
    runner.run_using_config(config, dataset_folder, bl_output_folder, logger)
    logger.info("-----------------------------------------------")


    ##
    # Gurobi LP baseline with the specified domain
    ##

    # This settings allow the analysis to collate affine instructions together and
    # handle them together.
    config.aff_prec_level  = "affine-full"

    gb_output_folder = f"{output_base_folder}/aff-gb"
    logger.info(f"Analyzing benchmarks with Gurobi based LP solver transformer in domain: {abstract_domain}")
    runner.run_using_config(config, dataset_folder, gb_output_folder, logger,
                            bl_outp_folder=bl_output_folder) # to compare with the baseline results
    logger.info("-----------------------------------------------")

    ##
    # Validate results against expected outputs
    ##
    expected = {
        "baseline": {"safe": 0, "warning": 3, "error": 0},
        "aff-gb":   {"safe": 3, "warning": 0, "error": 0},
    }
    folders = {"baseline": bl_output_folder, "aff-gb": gb_output_folder}

    all_validated = True
    for run_name, folder in folders.items():
        checks_file = f"{folder}/all_checks.json"
        print(f"Reading logs at {checks_file} ...")
        with open(checks_file, "r") as f:
            actual = json.load(f)["all_checks"]
        if actual == expected[run_name]:
            print(f"  [{run_name}] Validated!")
        else:
            print(f"  [{run_name}] Mismatch! Expected {expected[run_name]}, got {actual}")
            all_validated = False

    if all_validated:
        print("\nAll checks validated successfully!")
    else:
        print("\nSome checks did not match expected values. Please inspect the logs.")

if __name__ == "__main__":
    run_example()