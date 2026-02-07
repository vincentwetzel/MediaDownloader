import configparser
import os
import logging

log = logging.getLogger(__name__)


class ConfigManager:
    def __init__(self, ini_path="settings.ini"):
        self.ini_path = ini_path
        # Use interpolation=None to prevent ConfigParser from trying to interpolate % values
        # This is crucial because yt-dlp templates use % extensively (e.g. %(title)s)
        self.config = configparser.ConfigParser(interpolation=None)
        self.load_config()

    def load_config(self):
        if not os.path.exists(self.ini_path):
            self._set_defaults()
            self.save()
        else:
            self.config.read(self.ini_path, encoding="utf-8")
            # Ensure required sections exist and add new ones if missing
            if not self.config.has_section("General"):
                self.config.add_section("General")
            if not self.config.has_section("Paths"):
                self.config.add_section("Paths")
            
            # Add new default settings if they don't exist in existing config
            self._add_missing_defaults()

            log.debug(f"Loaded configuration from {self.ini_path}")

    def _set_defaults(self):
        self.config["General"] = {
            "video_quality": "best",
            "video_ext": "mp4",
            "vcodec": "h264",
            "audio_quality": "best",
            "audio_ext": "mp3",
            "acodec": "aac",
            "max_threads": "2",
            "sponsorblock": "True",
            "restrict_filenames": "False",
            "exit_after": "False",
            "yt_dlp_update_channel": "stable",
            "yt_dlp_version": "Unknown",
            "cookies_from_browser": "None",
            "js_runtime_path": "",  # New setting for JavaScript runtime
            "output_template": "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s",
            "subtitles_embed": "False",
            "subtitles_write": "False",
            "subtitles_write_auto": "False",
            "subtitles_langs": "en",
            "subtitles_format": "None",
        }
        self.config["Paths"] = {
            "completed_downloads_directory": "",
            "temporary_downloads_directory": "",
        }
        log.debug("Default configuration created")

    def _add_missing_defaults(self):
        # Add any new default settings that might be missing from an older config file
        # General section defaults
        general_defaults = {
            "video_quality": "best",
            "video_ext": "mp4",
            "vcodec": "h264",
            "audio_quality": "best",
            "audio_ext": "mp3",
            "acodec": "aac",
            "max_threads": "2",
            "sponsorblock": "True",
            "restrict_filenames": "False",
            "exit_after": "False",
            "yt_dlp_update_channel": "stable",
            "yt_dlp_version": "Unknown",
            "cookies_from_browser": "None",
            "js_runtime_path": "",  # New setting for JavaScript runtime
            "output_template": "%(title)s [%(uploader)s][%(upload_date>%m-%d-%Y)s][%(id)s].%(ext)s",
            "subtitles_embed": "False",
            "subtitles_write": "False",
            "subtitles_write_auto": "False",
            "subtitles_langs": "en",
            "subtitles_format": "None",
        }
        for key, value in general_defaults.items():
            if key not in self.config["General"]:
                self.config["General"][key] = value
                log.debug(f"Added missing default General setting: {key}={value}")

        # Paths section defaults
        paths_defaults = {
            "completed_downloads_directory": "",
            "temporary_downloads_directory": "",
        }
        for key, value in paths_defaults.items():
            if key not in self.config["Paths"]:
                self.config["Paths"][key] = value
                log.debug(f"Added missing default Paths setting: {key}={value}")
        
        # Save config if any defaults were added
        if any(key not in self.config["General"] for key in general_defaults) or \
           any(key not in self.config["Paths"] for key in paths_defaults):
            self.save()


    def save(self):
        with open(self.ini_path, "w", encoding="utf-8") as f:
            self.config.write(f)
        log.debug("Configuration saved to disk")

    def get(self, section, option=None, fallback=None):
        if option:
            try:
                return self.config.get(section, option, fallback=fallback)
            except (configparser.NoSectionError, configparser.NoOptionError):
                return fallback
        try:
            return dict(self.config[section])
        except configparser.NoSectionError:
            return {}

    def set(self, section, option, value):
        if not self.config.has_section(section):
            self.config.add_section(section)
        self.config.set(section, option, value)
        self.save()
