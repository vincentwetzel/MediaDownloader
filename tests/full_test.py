"""
Full integration test with longer timeout for complete download + move to complete.
Tests the entire pipeline: download -> embed -> move.
"""
import sys
import logging
import os
import time
from pathlib import Path

# Ensure project root is on sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import QTimer
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging

setup_logging()
log = logging.getLogger(__name__)

app = QApplication(sys.argv)
wnd = MediaDownloaderApp()
wnd.show()

# Use short URLs for faster testing
urls = [
    "https://www.youtube.com/shorts/W8mQ0PyIOc4",
    "https://www.youtube.com/shorts/hHu6eQPyvwg",
]

temp_dir = Path("J:\\yt-dlp\\temp_downloads")
completed_dir = Path("J:\\yt-dlp")

def check_files():
    """Check if files were moved."""
    temp_files = list(temp_dir.glob("*.mp4")) if temp_dir.exists() else []
    completed_files = list(completed_dir.glob("*.mp4")) if completed_dir.exists() else []
    
    print(f"\n[STATUS] Temp downloads: {len(temp_files)} files")
    for f in temp_files[:5]:
        print(f"  - {f.name}")
    
    print(f"[STATUS] Completed downloads: {len(completed_files)} files")
    # Only show recently modified files
    recent = [f for f in completed_files if time.time() - f.stat().st_mtime < 120]
    for f in recent[:5]:
        print(f"  - {f.name} ({time.time() - f.stat().st_mtime:.0f}s old)")
    
    if temp_files:
        print("\n[ERROR] Files still in temp directory! Move job may have failed.")
    elif recent:
        print("\n[SUCCESS] Files were moved to completed directory!")
    else:
        print("\n[UNKNOWN] No recent files found. Check logs for details.")

# Start downloads
QTimer.singleShot(2000, lambda: wnd.start_downloads(urls, {}))

# Check files after 60 seconds (give plenty of time for download + embed + move)
QTimer.singleShot(60000, check_files)

# Quit after 65 seconds
QTimer.singleShot(65000, app.quit)

print("[TEST] Starting full integration test with 65-second timeout...")
print("[TEST] This test downloads shorts and verifies they move from temp to completed directory.")
sys.exit(app.exec())
