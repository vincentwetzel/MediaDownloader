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

        def log_binary_sources():
            from core.binary_manager import get_bundled_binary_path, get_system_binary_path, get_ffmpeg_location
            names = ["yt-dlp", "gallery-dl", "ffmpeg", "ffprobe", "aria2c", "deno"]
            for name in names:
                bundled = get_bundled_binary_path(name)
                system = get_system_binary_path(name)
                if bundled:
                    logging.info("Bundled %s: %s", name, bundled)
                else:
                    logging.warning("Bundled %s not found.", name)
                if system:
                    logging.info("System %s: %s", name, system)
                else:
                    logging.info("System %s not found.", name)
            ffmpeg_location = get_ffmpeg_location(prefer_system=True)
            if ffmpeg_location:
                logging.info("yt-dlp ffmpeg location resolved to: %s", ffmpeg_location)
            else:
                logging.warning("yt-dlp ffmpeg location could not be resolved.")

        def auto_update_yt_dlp():
            """
            Synchronously updates yt-dlp on startup before the GUI loads.
            """
            channel = config_manager.get("General", "yt_dlp_update_channel", fallback="nightly")
            if channel == "none":
                logging.info("yt-dlp auto-update is disabled.")
                return

            logging.info(f"Starting automatic yt-dlp update for '{channel}' channel.")
            
            import core.yt_dlp_worker
            import shutil

            target_exe = core.yt_dlp_worker._YT_DLP_PATH
            if not target_exe:
                core.yt_dlp_worker.check_yt_dlp_available()
                target_exe = core.yt_dlp_worker._YT_DLP_PATH

            if not target_exe:
                target_exe = shutil.which("yt-dlp")

            if not target_exe:
                logging.error("Could not locate yt-dlp executable for auto-update.")
                return

            cmd = [target_exe]
            if channel == "nightly":
                cmd.extend(["--update-to", "nightly"])
            else:
                cmd.append("-U")

            try:
                # Since the GUI is not running, a synchronous call is safe on all platforms.
                # For frozen Windows apps, hide the console.
                creation_flags = 0
                if sys.platform == "win32" and getattr(sys, "frozen", False):
                    creation_flags = subprocess.CREATE_NO_WINDOW

                proc = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    creationflags=creation_flags,
                    stdin=subprocess.DEVNULL,
                    timeout=120
                )

                output = proc.stdout + "\n" + proc.stderr
                if proc.returncode == 0:
                    if "is up to date" in output:
                        logging.info(f"yt-dlp is already up to date on the '{channel}' channel.")
                    else:
                        logging.info(f"yt-dlp successfully updated on the '{channel}' channel.")
                else:
                    logging.error(f"yt-dlp auto-update failed. Output:\n{output}")

            except subprocess.TimeoutExpired:
                logging.error("yt-dlp auto-update timed out.")
            except Exception as e:
                logging.exception("yt-dlp auto-update failed with an exception.")

        def auto_update_gallery_dl():
            """
            Synchronously updates gallery-dl on startup.
            """
            from core.update_manager import download_gallery_dl_update
            logging.info("Starting automatic gallery-dl update.")
            try:
                status, message = download_gallery_dl_update()
                if status == 'success':
                    logging.info(f"gallery-dl update status: {status}, message: {message}")
                elif status == 'up_to_date':
                    logging.info(message)
                else:
                    logging.warning(f"gallery-dl update status: {status}, message: {message}")
            except Exception as e:
                logging.exception("gallery-dl auto-update failed with an exception.")

        # --- Run startup tasks synchronously before creating the GUI ---
        log_binary_sources()
        auto_update_yt_dlp()
        auto_update_gallery_dl()
        
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
        
        # Get the yt-dlp version *after* the update has run
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
