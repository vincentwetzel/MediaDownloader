import logging
import os
import sys

class StreamToLogger:
    """
    Fake file-like stream object that redirects writes to a logger instance.
    """
    def __init__(self, logger, level):
        self.logger = logger
        self.level = level
        self.linebuf = ''

    def write(self, buf):
        for line in buf.rstrip().splitlines():
            self.logger.log(self.level, line.rstrip())

    def flush(self):
        pass

def setup_logging():
    log_dir = "logs"
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "MediaDownloader.log")

    # Get the root logger and set it to capture everything
    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)

    # Create a formatter
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s")

    # Remove any existing handlers to avoid duplicates
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)

    # --- File Handler ---
    # Logs everything (DEBUG and above) to the log file
    file_handler = logging.FileHandler(log_path, encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    root_logger.addHandler(file_handler)

    # --- Console (Stream) Handler ---
    # Only logs INFO and above to the console
    if sys.stderr is not None:
        stream_handler = logging.StreamHandler()
        stream_handler.setLevel(logging.DEBUG)
        stream_handler.setFormatter(formatter)
        root_logger.addHandler(stream_handler)

    # Redirect stderr to the logger so uncaught exceptions from sub-processes
    # or other libraries are captured in our log file.
    sys.stderr = StreamToLogger(logging.getLogger('STDERR'), logging.ERROR)

