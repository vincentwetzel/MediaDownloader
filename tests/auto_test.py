import sys
import logging
import os
import sys as _sys
# Ensure project root is on sys.path so package imports like `ui.*` work
_sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QTimer
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging

# Configure logging to console and file
setup_logging()

app = QApplication(sys.argv)
wnd = MediaDownloaderApp()
wnd.show()

# Reduce max_threads to 2 to force queuing
try:
    wnd.config_manager.set("General", "max_threads", "2")
except Exception:
    pass

urls = [
    "https://www.youtube.com/watch?v=5qap5aO4i9A",
    "https://www.youtube.com/watch?v=3JZ_D3ELwOQ",
    "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
    "https://www.youtube.com/watch?v=Zi_XLOBDo_Y",
    "https://www.youtube.com/watch?v=9bZkp7q19f0",
    "https://www.youtube.com/watch?v=1wYNFfgrXTI",
]

# Start downloads after a short delay so UI is ready
QTimer.singleShot(1000, lambda: wnd.start_downloads(urls, {}))

# Quit after 30 seconds to allow metadata fetches to run
QTimer.singleShot(30000, app.quit)

sys.exit(app.exec())
