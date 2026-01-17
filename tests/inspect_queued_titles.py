import os
import sys
_sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import logging
from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QTimer
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging

setup_logging()
app = QApplication([])
wnd = MediaDownloaderApp()

# Force all downloads to be queued by setting max_threads to 0
try:
    wnd.config_manager.set("General", "max_threads", "0")
except Exception:
    pass

urls = [
    "https://www.youtube.com/watch?v=5qap5aO4i9A",
    "https://www.youtube.com/watch?v=3JZ_D3ELwOQ",
    "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
    "https://www.youtube.com/watch?v=Zi_XLOBDo_Y",
]

# Start downloads shortly after initialization
QTimer.singleShot(500, lambda: wnd.start_downloads(urls, {}))

# Inspect titles after 5 seconds and print them
def inspect_and_exit():
    print("--- Active items and titles ---")
    for url, w in wnd.tab_active.active_items.items():
        try:
            t = w.title_label.text()
        except Exception:
            t = "<no widget title>"
        print(f"{url} -> {t}")
    print("--- end ---")
    app.quit()

QTimer.singleShot(5000, inspect_and_exit)
sys.exit(app.exec())
