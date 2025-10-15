import configparser
import os
import logging

log = logging.getLogger(__name__)


class ConfigManager:
    def __init__(self, ini_path="settings.ini"):
        self.ini_path = ini_path
        self.config = configparser.ConfigParser()
        self.load_config()

    def load_config(self):
        if not os.path.exists(self.ini_path):
            self._set_defaults()
            self.save()
        else:
            self.config.read(self.ini_path, encoding="utf-8")
            log.debug(f"Loaded configuration from {self.ini_path}")

    def _set_defaults(self):
        self.config["General"] = {
            "video_quality": "best",
            "audio_quality": "best",
            "max_threads": "2",
            "sponsorblock": "True",
            "restrict_filenames": "False",
            "exit_after": "False",
        }
        self.config["Paths"] = {
            "completed_downloads_directory": "",
            "temporary_downloads_directory": "",
        }
        log.debug("Default configuration created")

    def save(self):
        with open(self.ini_path, "w", encoding="utf-8") as f:
            self.config.write(f)
        log.debug("Configuration saved to disk")

    def get(self, section, option=None, fallback=None):
        if option:
            return self.config.get(section, option, fallback=fallback)
        return dict(self.config[section])

    def set(self, section, option, value):
        self.config.set(section, option, value)
        self.save()
