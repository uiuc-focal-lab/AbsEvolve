"""
This file contains the code to compare the results of two runs of the tool on the same set of benchmarks.
The comparison is done by comparing the invariants and the assertion checks obtained from the two runs.
Invariants are compared by checking if one invariant is contained in the other using z3. Assertion checks are 
compared by comparing the number of checks that were proved, disproved and timed out in the two runs.
The results of the comparison are stored in a json file in the output folder of the second run.
"""
import os
import json
import z3

from enum import Enum
from utils import update_dict

class InvariantsComparator():
    # Helper function to get invariants from the file.
    def _parse_invariants_from_inv_json_file(self, filename):
        with open(filename, "r") as f:
            data = json.load(f)
        
        return data["all_invs"], data["timed_out"] if "timed_out" in data else False

    ##
    # Code to compute containment results.
    ##
    class InvContainmentResult(Enum):
        """
        Possible results of invariant comparison.
        """
        SYNTACTIC_SAME = 0 # Both invariant strings are same syntactically
        SAME = 1           # Both invariants represent the same set, but are not same syntactically
        STRONGER = 2       # Invariant 1 is stronger than Invariant 2.
        WEAKER = 3         # Invariant 1 is weaker than Invariant 2.
        INCOM = 4          # Both the invariants are incomparable.

    def _check_containment(self, smt1: str, smt2: str, vars_set: set[str]) -> bool:
        """
        Checks if the invariant represented by smt1 is contained in smt2.

        That is:  smt1 ⇒ smt2  ?
        """
        decls = {v: z3.Real(v) for v in vars_set}

        expr1 = z3.And(z3.parse_smt2_string(smt1, decls=decls))
        expr2 = z3.And(z3.parse_smt2_string(smt2, decls=decls))

        solver = z3.Solver()
        solver.add(expr1)
        solver.add(z3.Not(expr2))

        result = solver.check()
        if result == z3.unsat:
            return True
        elif result == z3.sat:
            return False
        else:
            raise RuntimeError("Z3 returned 'unknown' during containment check.")

    def _compare_invariants_containment(self, i1, i2):
        """
        Compare i1 and i2 by first checking if one is inside the another
        and then using the results.
        """
        if i1["str"] == i2["str"]:
            return InvariantsComparator.InvContainmentResult.SYNTACTIC_SAME
        
        i1_smt = i1["smt"]
        i2_smt = i2["smt"]

        comb_vars_set = set(i1["vars"]) | set(i2["vars"])
        one_in_two = self._check_containment(i1_smt, i2_smt, comb_vars_set)
        two_in_one = self._check_containment(i2_smt, i1_smt, comb_vars_set)

        if one_in_two and two_in_one:
            return InvariantsComparator.InvContainmentResult.SAME
        elif one_in_two:
            return InvariantsComparator.InvContainmentResult.STRONGER
        elif two_in_one:
            return InvariantsComparator.InvContainmentResult.WEAKER

        return InvariantsComparator.InvContainmentResult.INCOM

    def compare_invariants_in_files(self, file1, file2):
        # Parse the invariants
        inv_lis1, is_timed_out1 = self._parse_invariants_from_inv_json_file(file1)
        inv_lis2, is_timed_out2 = self._parse_invariants_from_inv_json_file(file2)

        result = {}

        if is_timed_out1 or is_timed_out2:
            if is_timed_out1:
                result["containment_checks"] = {"timed_out_benchs_1": [file1]}

            if is_timed_out2:
                result["containment_checks"] = {"timed_out_benchs_2": [file2]}
            
            return result

        assert(len(inv_lis1) == len(inv_lis2))

        # Go over the invariants, compare them pairwise and store the
        # results.
        incl_results = {}
        for i1, i2 in zip(inv_lis1, inv_lis2):
            if i1["str"] == "T" and i2["str"] == "T":
                continue

            if i1["str"] == "_|_":
                print(f"Bottom in {file1}")
                assert(False)

            if i2["str"] == "_|_":
                print(f"Bottom in {file2}")
                assert(False)

            containment_res = self._compare_invariants_containment(i1, i2)
            update_dict(incl_results, {containment_res.value:1})

        result["containment_checks"] = incl_results

        return result

##
# Class to compare assert numbers.
##
class AssertChecker():
    def compare_asserts_in_files(self, file1, file2):
        with open(file1, "r") as f1:
            data1 = json.load(f1)
        with open(file2, "r") as f2:
            data2 = json.load(f2)

        result = {}

        a1, is_timed_out1 = data1["all_checks"], data1["timed_out"]
        a2, is_timed_out2 = data2["all_checks"], data2["timed_out"]

        if is_timed_out1 or is_timed_out2:
            if is_timed_out1:
                result["timed_out_benchs_1"] = [file1]

            if is_timed_out2:
                result["timed_out_benchs_2"] = [file2]
            
            return result

        # Collect all possible keys
        all_keys = set(a1.keys()) | set(a2.keys())

        for k in all_keys:
            v1 = a1.get(k, 0)
            v2 = a2.get(k, 0)
            result[k] = v2 - v1

        return result

class ResultsChecker:
    def __init__(self):
        self.inv_checker = InvariantsComparator()
        self.assert_checker = AssertChecker()

    def _get_inv_file_in_folder(self, folder, bench_name):
        return  f"{folder}/{bench_name}/all_invs.json"

    def _get_checks_file_in_folder(self, folder, bench_name):
        return  f"{folder}/{bench_name}/all_checks.json"

    def compare(self, cmp_folder1, cmp_folder2, filename = None, probname=None):
        all_result_dicts = {}

        if probname is None:
            probs = [name for name in os.listdir(cmp_folder2) if os.path.isdir(os.path.join(cmp_folder2, name))]
        else:
            probs = [probname]

        for pro in probs:
            ##
            # Compare invariants
            ##
            file1 = self._get_inv_file_in_folder(cmp_folder1, pro)
            file2 = self._get_inv_file_in_folder(cmp_folder2, pro)

            inv_res = self.inv_checker.compare_invariants_in_files(file1, file2)

            if not inv_res:
                print(f"check {pro}!")
                continue
            
            # Merge inv_res result into global result
            for (dic_name, dic) in inv_res.items():
                if dic_name not in all_result_dicts:
                    all_result_dicts[dic_name] = {}
                
                update_dict(all_result_dicts[dic_name], dic)
            
            ##
            # Compare assertions checks
            ##
            file1 = self._get_checks_file_in_folder(cmp_folder1, pro)
            file2 = self._get_checks_file_in_folder(cmp_folder2, pro)

            assert_res = self.assert_checker.compare_asserts_in_files(file1, file2)

            if "assert_checks" not in all_result_dicts:
                all_result_dicts["assert_checks"] = {}
            update_dict(all_result_dicts["assert_checks"], assert_res)

        # Write merged output if filename is not None, else return
        if filename:
            with open(f"{cmp_folder2}/{filename}", "w") as f:
                f.write(json.dumps(all_result_dicts, indent=2))
        else:
            return all_result_dicts