import logging
import os
import sys
import tempfile

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

def get_log_dir():
    candidates = []

    # For source runs, prefer a local logs folder for easy access during development.
    if not getattr(sys, 'frozen', False):
        try:
            cwd_logs = os.path.join(os.getcwd(), "logs")
            candidates.append(cwd_logs)
        except OSError:
            pass

    # Windows: prefer LocalAppData to avoid Program Files permission issues
    if sys.platform == "win32":
        base = os.getenv("LOCALAPPDATA") or os.getenv("APPDATA")
        if base:
            candidates.append(os.path.join(base, "MediaDownloader", "logs"))
    else:
        # Cross-platform fallback: ~/.local/share/MediaDownloader/logs
        base = os.path.join(os.path.expanduser("~"), ".local", "share")
        candidates.append(os.path.join(base, "MediaDownloader", "logs"))

    # Last resort: temp directory
    candidates.append(os.path.join(tempfile.gettempdir(), "MediaDownloader", "logs"))

    last_error = None
    for path in candidates:
        try:
            os.makedirs(path, exist_ok=True)
            # Test for write permissions, as os.makedirs(exist_ok=True) doesn't fail
            # if the directory exists but is not writable.
            test_file = os.path.join(path, f".writable.{os.getpid()}")
            with open(test_file, "w") as f:
                pass
            os.remove(test_file)
            return path
        except Exception as e:
            last_error = e
            continue

    # If all else fails, re-raise the last error
    if last_error:
        raise last_error
    return "logs" # Should not be reached

def setup_logging():
    log_dir = get_log_dir()
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
