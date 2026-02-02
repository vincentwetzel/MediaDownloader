import sys
import logging
import os
from PyQt6.QtWidgets import QApplication
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging
# Import file operations monitor early to capture deletes/moves during runtime
try:
    from core import file_ops_monitor  # noqa: F401
except Exception:
    pass

import faulthandler

# Enable faulthandler to catch segfaults
# In noconsole mode (e.g. .pyw or frozen exe), sys.stderr is None.
if sys.stderr is not None:
    faulthandler.enable()
else:
    try:
        # Ensure logs directory exists
        os.makedirs("logs", exist_ok=True)
        # Open a file for fault logging
        # We keep the file object referenced globally so it doesn't get garbage collected immediately
        _fault_log_file = open(os.path.join("logs", "crash_faults.log"), "a")
        faulthandler.enable(file=_fault_log_file)
    except Exception:
        pass

def main():
    setup_logging()
    logging.info("Starting Media Downloader...")

    try:
        app = QApplication(sys.argv)
        wnd = MediaDownloaderApp()
        wnd.show()
        sys.exit(app.exec())
    except Exception as e:
        logging.error(f"Fatal error in main: {e}", exc_info=True)
        raise


if __name__ == "__main__":
    main()
