import logging
import os

from pathlib import Path
from tqdm import tqdm

def get_project_root():
    """
    Returns the root of the project.
    Note: This function depends on the location of this file.
    """
    return Path(__file__).resolve().parent.parent

def printer(text: str, file=None):
    """
    If file is specified, write to that, else write to stdout.
    """
    if file is not None:
        os.makedirs(os.path.dirname(file), exist_ok=True)
        with open(file, 'a') as f:
            f.write(str(text) + '\n')
    else:
        print(text)

def update_dict(left: dict, right: dict):
    """
    Update values in `left` dict using `right` as an updater.
    - If a key exists in both and values are numeric, add them.
    - If a key exists in both and values are lists, append them.
    - If key exists only in `right`, insert it.
    """
    for k, v in right.items():
        if k in left:
            if isinstance(left[k], (int, float)) and isinstance(v, (int, float)):
                left[k] += v
            elif isinstance(left[k], list) and isinstance(v, list):
                left[k].extend(v)
            else:
                raise TypeError(f"Unsupported value type for key '{k}': {type(left[k])}")
        else:
            left[k] = v
    return left

class TqdmLoggingHandler(logging.Handler):
    """
    Logging handler that plays nicely with tqdm progress bars.
    """
    def emit(self, record):
        try:
            msg = self.format(record)
            tqdm.write(msg)
            self.flush()
        except Exception:
            self.handleError(record)


class ColorFormatter(logging.Formatter):

    COLORS = {
        "DEBUG": "\033[36m",
        "INFO": "\033[94m",
        "WARNING": "\033[93m",
        "ERROR": "\033[91m",
        "CRITICAL": "\033[41m",
    }

    RESET = "\033[0m"

    def format(self, record):
        levelname = record.levelname
        color = self.COLORS.get(levelname, "")
        record.levelname = f"{color}{levelname}{self.RESET}"
        try:
            return super().format(record)
        finally:
            record.levelname = levelname


def get_logger(name="experiment", log_file=None):
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    logger.handlers.clear()
    logger.propagate = False

    fmt = "%(asctime)s | %(levelname)s | %(message)s"
    datefmt = "%H:%M:%S"

    # terminal handler
    term_handler = TqdmLoggingHandler()
    term_handler.setFormatter(ColorFormatter(fmt, datefmt))
    logger.addHandler(term_handler)

    # file handler
    if log_file:
        Path(log_file).parent.mkdir(parents=True, exist_ok=True)

        file_handler = logging.FileHandler(log_file)
        file_handler.setFormatter(logging.Formatter(fmt, datefmt))
        logger.addHandler(file_handler)

    return logger

if __name__ == "__main__":
    print(get_project_root())