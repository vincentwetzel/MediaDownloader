import sys
import logging
from PyQt6.QtWidgets import QApplication
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging
# Import file operations monitor early to capture deletes/moves during runtime
try:
    from core import file_ops_monitor  # noqa: F401
except Exception:
    pass

import faulthandler
faulthandler.enable()

def main():
    setup_logging()
    logging.info("Starting Media Downloader...")

    app = QApplication(sys.argv)
    wnd = MediaDownloaderApp()
    wnd.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
