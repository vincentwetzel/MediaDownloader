import sys
import logging
import os
import platform
import subprocess
from PyQt6.QtWidgets import QApplication
from ui.main_window import MediaDownloaderApp
from core.logger_config import setup_logging, get_log_dir
import threading
from core.config_manager import ConfigManager
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
        log_dir = get_log_dir()
        # Open a file for fault logging
        # We keep the file object referenced globally so it doesn't get garbage collected immediately
        _fault_log_file = open(os.path.join(log_dir, "crash_faults.log"), "a")
        faulthandler.enable(file=_fault_log_file)
    except Exception:
        pass

def _get_system_theme():
    """
    Detects if the system is in dark mode.
    Returns 'dark' or 'light'. Defaults to 'dark' on error or non-Windows.
    """
    try:
        if sys.platform == "win32":
            import winreg
            # Check AppsUseLightTheme in registry
            key_path = r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path) as key:
                value, _ = winreg.QueryValueEx(key, "AppsUseLightTheme")
                return "light" if value == 1 else "dark"
    except Exception:
        pass
    
    # Default fallback if detection fails
    return "dark"

def main():
    setup_logging()
    logging.info("Starting Media Downloader...")

    try:
        config_manager = ConfigManager()

        # --- Create and run the application ---
        app = QApplication(sys.argv)
        
        # Apply theme based on settings
        theme = config_manager.get("General", "theme", fallback="auto")
        if theme == "system":
            theme = "auto"
            
        try:
            import qdarktheme
            if hasattr(qdarktheme, 'setup_theme'):
                qdarktheme.setup_theme(theme)
            elif hasattr(qdarktheme, 'load_stylesheet'):
                if theme == 'auto':
                    theme_to_load = _get_system_theme()
                else:
                    theme_to_load = theme
                app.setStyleSheet(qdarktheme.load_stylesheet(theme_to_load))
            else:
                logging.error("qdarktheme module found but has no setup_theme or load_stylesheet")
                app.setStyle("Fusion")
        except ImportError:
            logging.warning("pyqtdarktheme not found. Theme will default to system style.")
            app.setStyle("Fusion")
        except Exception as e:
            logging.error(f"Failed to apply theme '{theme}': {e}")
            app.setStyle("Fusion")
        
        # Get the yt-dlp version
        # We do this after theme setup but before window show.
        # Note: The heavy lifting of updates is now handled in background by main_window.
        from core.yt_dlp_worker import get_yt_dlp_version
        initial_yt_dlp_version = get_yt_dlp_version(force_check=True)
        config_manager.set("General", "yt_dlp_version", initial_yt_dlp_version)

        wnd = MediaDownloaderApp(config_manager=config_manager, initial_yt_dlp_version=initial_yt_dlp_version)
        wnd.show()

        sys.exit(app.exec())
    except Exception as e:
        logging.error(f"Fatal error in main: {e}", exc_info=True)
        raise


if __name__ == "__main__":
    main()
