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

    # When running as a frozen .exe with noconsole=True (pythonw), sys.stderr might be None.
    # We need to ensure we don't try to write to a None stream in the StreamHandler.
    handlers = [logging.FileHandler(log_path, encoding="utf-8")]
    
    # Only add StreamHandler if sys.stderr is available (i.e., we have a console)
    if sys.stderr is not None:
        handlers.append(logging.StreamHandler())

    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        handlers=handlers
    )

    # Redirect stderr to the logger
    # Even if sys.stderr was None (no console), we can replace it with our logger
    # so that any code writing to stderr gets captured in the log file.
    sys.stderr = StreamToLogger(logging.getLogger('STDERR'), logging.ERROR)
