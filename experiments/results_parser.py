"""
Code to parse the invariant and assertion check results from the log files of the tool.
The invariants are parsed from the lines containing the "INVARIANTS" keyword in the log file. 
The assertion checks are parsed from the block of lines containing the "ANALYSIS RESULTS" keyword in the log file. 
"""
import json
import re
import z3

from pathlib import Path

class ResultsParser:
    def _parse_constraints_as_z3(self, constraint_str, domain):
        """
        Take a constraint string, something like d.0-p.0 <= 0 and get
        the z3 expression and fills the specified map with string to z3 variable map.
        """
        cmp_ops = {
            "<=": lambda a, b: a <= b,
            "<":  lambda a, b: a < b,
            ">=": lambda a, b: a >= b,
            ">":  lambda a, b: a > b,
            "==": lambda a, b: a == b,
            "=":  lambda a, b: a == b,
        }

        cmp_regex = re.compile(r"(<=|>=|==|=|<|>)")
        constraints = []
        var_set = set()
        z3_var_set = set()
        z3_var_map = {}

        for raw in constraint_str.strip().split(";"):
            raw = raw.strip()
            if not raw:
                continue

            match = cmp_regex.search(raw)
            if not match:
                raise ValueError(f"No valid comparison operator in: {raw}")

            op = match.group(1)
            lhs_raw = raw[:match.start()].strip()
            rhs_raw = raw[match.end():].strip()

            try:
                rhs = float(rhs_raw)
            except ValueError:
                raise ValueError(f"Invalid RHS number in constraint: {raw}")

            lhs_expr = 0
            for m in re.finditer(r'([+\-])?\s*([^+\-]+)', lhs_raw):
                sign_str, term_body = m.groups()
                sign = -1 if sign_str == '-' else 1  # default is '+'

                # Parse coefficient and variable
                coef_match = re.fullmatch(r'(\d*\.?\d*)\*?([@\w.\-]+)', term_body.strip())
                if not coef_match:
                    raise ValueError(f"Could not parse term: {m.group(0)}")
                
                coef_str, var_name = coef_match.groups()
                if coef_str == ".":
                    coef_str = None
                    var_name = f".{var_name}"
                coef = float(coef_str) if coef_str else 1.0
                total_coef = sign * coef

                if var_name not in z3_var_map:
                    z3_var_map[var_name] = z3.Real(var_name)
                    var_set.add(var_name)
                    z3_var_set.add(z3_var_map[var_name])

                lhs_expr += total_coef * z3_var_map[var_name]

            constraint = cmp_ops[op](lhs_expr, rhs)
            constraints.append(z3.simplify(constraint))
        
        if len(constraints) != 0:
            return {
                "str": constraint_str,
                "smt": "\n".join(f"(assert {a.sexpr()})" for a in constraints),
                "dom": domain,
                "vars":list(var_set),
                "len":len(constraints)
            }
        else:
            return {
                "str": "T",
                "smt":"\n".join(f"(assert {a.sexpr()})" for a in [z3.BoolVal(True)]),
                "dom": domain,
                "vars":{},
                "len":0
            }

    def _parse_invariants_from_txt_file(self, filename, domain):
        """
        Parses invariants present in the specified text log file.
        """ 
        ##
        # Define patterns for different invariant formats.
        ##
        pattern1 = re.compile(
            r'/\*\*\s*INVARIANTS:\s*\{([^{}]*)\}\s*\*\*/'
        )

        pattern2 = re.compile(
            r'/\*\*\s*INVARIANTS:\s*\(\s*\{[^{}]*\}\s*,\s*\{([^{}]*)\}\s*\)\s*\*\*/'
        )

        pattern3 = re.compile(
            r'/\*\*\s*INVARIANTS:\s*_\|_\s*\*\*/'
        )

        invariants_lis = []
        with open(filename, 'r', encoding='utf-8') as file:
            for line_number, line in enumerate(file, 1):
                line = line.strip()

                # Check for Pattern 3 first, as it has a specific match
                match3 = pattern3.search(line)
                if match3:
                    invariants_lis.append({{"str": '_|_',
                                            "smt":"\n".join(f"(assert {a.sexpr()})" for a in [z3.BoolVal(False)]),
                                            "dom": domain,
                                            "vars":{},
                                            "len":0
                                            }})
                    continue  # Skip to next line after a successful match

                # Check for Pattern 2 next
                match2 = pattern2.search(line)
                if match2:
                    invariants = match2.group(1).strip()
                    invariants_lis.append(self._parse_constraints_as_z3(invariants, domain))
                    continue  # Skip to next line after a successful match

                # Finally, check for Pattern 1
                match1 = pattern1.search(line)
                if match1:
                    invariants = match1.group(1).strip()
                    invariants_lis.append(self._parse_constraints_as_z3(invariants, domain))

        return invariants_lis

    def _parse_invariants_from_json_file(self, filename, domain):
        """
        Parses invariants present in the specified json log file.
        """
        def clean_invariants(inv_str: str):
            # remove surrounding { }
            inv_str = inv_str.strip("{}")
            # split by ';'
            parts = [p.strip() for p in inv_str.split(";")]
            # filter out true/false/cmp*
            cleaned = [
                p for p in parts
                # Hacky way to remove the "boolean part" of the invariants.
                if p not in ("true", "false") and not p.startswith("cmp") and not ("or.cond" in p)
            ]
            return ";".join(cleaned)

        # read JSON file
        with open(filename, "r") as f:
            data = json.load(f)

        # extract invariants → second element of each pair
        invariants = [inv[1] for inv in data["main"]["invariants"]]

        # prune boolean things at start of invariants
        invariants_list = []
        for inv in invariants:
            inv = clean_invariants(inv)
            invariants_list.append(self._parse_constraints_as_z3(inv, domain))

        return invariants_list

    def parse_invariants_from_file(self, filename, domain):
        if Path(filename).suffix.lower() == ".json":
            return self._parse_invariants_from_json_file(filename, domain)
        else:
            return self._parse_invariants_from_txt_file(filename, domain)

    def _parse_checks_from_json_file(self, filename):
        # read JSON file
        with open(filename, "r") as f:
            data = json.load(f)

        return data["main"]["checks"]

    def _parse_checks_from_text_file(self, filename):
        result = {"safe": 0, "error": 0, "warning": 0}
        in_block = False

        # Read the file line by line and look for the lines containing the checks information. 
        # The checks information is present in the block between the lines 
        # containing "************** ANALYSIS RESULTS" and "************** ANALYSIS RESULTS END".
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                if "************** ANALYSIS RESULTS" in line:
                    in_block = True
                    continue
                if "************** ANALYSIS RESULTS END" in line:
                    break

                if in_block:
                    if "Number of total safe checks" in line:
                        result["safe"] = int(line.split()[0])
                    elif "Number of total error checks" in line:
                        result["error"] = int(line.split()[0])
                    elif "Number of total warning checks" in line:
                        result["warning"] = int(line.split()[0])

        return result

    def parse_checks_from_file(self, filename):
        if Path(filename).suffix.lower() == ".json":
            return self._parse_checks_from_json_file(filename)
        else:
            return self._parse_checks_from_text_file(filename)