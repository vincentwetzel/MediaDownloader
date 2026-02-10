
import os
import sys
import time
import shutil
import logging
import subprocess
import argparse

# Basic logging setup
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    stream=sys.stdout
)


def _is_windows():
    return sys.platform == "win32"


def main(
    pid_to_wait_for: int,
    file_to_unpack: str,
    unpack_destination: str,
    launch_target: str,
    self_destruct_script_path: str,
):
    """
    Main function for the update runner.

    Args:
        pid_to_wait_for: The process ID of the main application to wait for.
        file_to_unpack: The path to the update archive to unpack.
        unpack_destination: The directory to unpack the update to.
        launch_target: The path to the executable to launch after the update.
        self_destruct_script_path: Path to the script that will delete the runner.
    """
    logging.info(f"Update runner started. PID: {os.getpid()}")
    logging.info(f"Waiting for main application (PID: {pid_to_wait_for}) to exit...")

    if _is_windows():
        try:
            import psutil
            main_proc = psutil.Process(pid_to_wait_for)
            main_proc.wait(timeout=60)
        except psutil.NoSuchProcess:
            logging.info(f"Main application (PID: {pid_to_wait_for}) already exited.")
        except psutil.TimeoutExpired:
            logging.error("Timeout expired while waiting for main application to exit. Exiting.")
            sys.exit(1)
        except ImportError:
            # Fallback if psutil is not available
            time.sleep(5)
    else:
        # On non-Windows, psutil should be available, but as a fallback:
        time.sleep(5)

    logging.info("Main application exited. Proceeding with update.")

    try:
        logging.info(f"Unpacking '{file_to_unpack}' to '{unpack_destination}'...")
        shutil.unpack_archive(file_to_unpack, unpack_destination)
        logging.info("Unpacking completed.")
    except Exception as e:
        logging.error(f"Failed to unpack archive: {e}")
        sys.exit(1)

    logging.info(f"Update applied. Relaunching application: '{launch_target}'")
    try:
        if _is_windows():
            subprocess.Popen([launch_target], creationflags=subprocess.CREATE_NEW_CONSOLE)
        else:
            subprocess.Popen([launch_target])
    except Exception as e:
        logging.error(f"Failed to relaunch application: {e}")
        sys.exit(1)

    if self_destruct_script_path:
        logging.info("Executing self-destruct script and exiting.")
        if _is_windows():
            subprocess.Popen(["cmd.exe", "/c", self_destruct_script_path], shell=True)
        else:
            subprocess.Popen(["/bin/bash", self_destruct_script_path])

    sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Application Update Runner")
    parser.add_argument("--pid", required=True, type=int, help="PID of the main process to wait for.")
    parser.add_argument("--file", required=True, help="Path to the update archive.")
    parser.add_argument("--destination", required=True, help="Path to the unpack destination directory.")
    parser.add_argument("--launch", required=True, help="Path of the executable to launch after update.")
    parser.add_argument("--self-destruct-script", default="", help="Path to the self-destruct script.")
    
    args = parser.parse_args()

    main(
        pid_to_wait_for=args.pid,
        file_to_unpack=args.file,
        unpack_destination=args.destination,
        launch_target=args.launch,
        self_destruct_script_path=args.self_destruct_script,
    )
