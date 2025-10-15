import os
import shutil
import sys


def is_browser_installed(browser_key: str) -> bool:
    """Return True if the given browser (one of yt-dlp supported browsers)
    appears installed on this machine.  browser_key should be lowercase like:
    'chrome','chromium','brave','edge','firefox','opera','safari','vivaldi','whale'."""
    browser_key = (browser_key or "").lower()
    names = {
        "chrome": ["chrome", "google-chrome", "chrome.exe"],
        "chromium": ["chromium", "chromium-browser", "chromium.exe"],
        "brave": ["brave", "brave-browser", "brave.exe"],
        "edge": ["msedge", "edge", "msedge.exe"],
        "firefox": ["firefox", "firefox.exe"],
        "opera": ["opera", "opera.exe"],
        "safari": ["Safari.app"],
        "vivaldi": ["vivaldi", "vivaldi.exe"],
        "whale": ["whale", "whale.exe"],
    }

    # quick check with shutil.which
    if shutil.which(browser_key):
        return True

    # platform specific path guesses
    if sys.platform == "win32":
        win_paths = {
            "chrome": [
                r"C:\Program Files\Google\Chrome\Application\chrome.exe",
                r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
            ],
            "chromium": [
                r"C:\Program Files\Chromium\Application\chrome.exe",
                r"C:\Program Files (x86)\Chromium\Application\chrome.exe",
            ],
            "brave": [
                r"C:\Program Files\BraveSoftware\Brave-Browser\Application\brave.exe",
                r"C:\Program Files (x86)\BraveSoftware\Brave-Browser\Application\brave.exe",
            ],
            "edge": [
                r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
                r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
            ],
            "firefox": [
                r"C:\Program Files\Mozilla Firefox\firefox.exe",
                r"C:\Program Files (x86)\Mozilla Firefox\firefox.exe",
            ],
            "opera": [
                r"C:\Program Files\Opera\launcher.exe",
                r"C:\Program Files (x86)\Opera\launcher.exe",
            ],
            "vivaldi": [
                r"C:\Program Files\Vivaldi\Application\vivaldi.exe",
                r"C:\Program Files (x86)\Vivaldi\Application\vivaldi.exe",
            ],
            "whale": [
                r"C:\Program Files\Naver\Naver Whale\Application\whale.exe",
                r"C:\Program Files (x86)\Naver\Naver Whale\Application\whale.exe",
            ],
            "safari": [
                r"C:\Program Files\Safari\Safari.exe",
                r"C:\Program Files (x86)\Safari\Safari.exe",
            ],
        }
        paths = win_paths.get(browser_key, [])
        for p in paths:
            if p and os.path.exists(p):
                return True

    if sys.platform == "darwin":
        mac_map = {
            "safari": "/Applications/Safari.app",
            "chrome": "/Applications/Google Chrome.app",
            "edge": "/Applications/Microsoft Edge.app",
            "firefox": "/Applications/Firefox.app",
            "opera": "/Applications/Opera.app",
            "brave": "/Applications/Brave Browser.app",
            "vivaldi": "/Applications/Vivaldi.app",
            "whale": "/Applications/Naver Whale.app",
            "chromium": "/Applications/Chromium.app",
        }
        p = mac_map.get(browser_key)
        if p and os.path.exists(p):
            return True

    # fallback to scanning common names with which
    for candidate in names.get(browser_key, []):
        if shutil.which(candidate):
            return True

    return False
