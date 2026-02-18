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

    def get_config_dir(self):
        """Returns the directory where the config file is stored."""
        return os.path.dirname(os.path.abspath(self.ini_path))

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

        self._enforce_required_settings()
        log.debug(f"Loaded configuration from {self.ini_path}")

    def _set_defaults(self):
        self.config["General"] = {
            "video_quality": "best",
            "video_ext": "mp4",
            "vcodec": "h264",
            "audio_quality": "best",
            "audio_ext": "opus",
            "acodec": "opus",
            "max_threads": "2",
            "sponsorblock": "True",
            "restrict_filenames": "False",
            "exit_after": "False",
            "yt_dlp_update_channel": "stable",
            "yt_dlp_version": "Unknown",
            "cookies_from_browser": "None",
            "gallery_cookies_from_browser": "None",
            "output_template": "%(title)s [%(uploader)s][%(release_date>%m-%d-%Y)s][%(id)s].%(ext)s",
            "embed_chapters": "True",
            "subtitles_embed": "False",
            "subtitles_write": "False",
            "subtitles_write_auto": "False",
            "subtitles_langs": "en",
            "subtitles_format": "None",
            "download_archive": "True",
        }
        self.config["Paths"] = {
            "completed_downloads_directory": "",
            "temporary_downloads_directory": "",
        }
        log.debug("Default configuration created")

    def _add_missing_defaults(self):
        modified = False
        # Add any new default settings that might be missing from an older config file
        # General section defaults
        general_defaults = {
            "video_quality": "best",
            "video_ext": "mp4",
            "vcodec": "h264",
            "audio_quality": "best",
            "audio_ext": "opus",
            "acodec": "opus",
            "max_threads": "2",
            "sponsorblock": "True",
            "restrict_filenames": "False",
            "exit_after": "False",
            "yt_dlp_update_channel": "stable",
            "yt_dlp_version": "Unknown",
            "cookies_from_browser": "None",
            "gallery_cookies_from_browser": "None",
            "output_template": "%(title)s [%(uploader)s][%(release_date>%m-%d-%Y)s][%(id)s].%(ext)s",
            "embed_chapters": "True",
            "subtitles_embed": "False",
            "subtitles_write": "False",
            "subtitles_write_auto": "False",
            "subtitles_langs": "en",
            "subtitles_format": "None",
            "download_archive": "True",
        }
        for key, value in general_defaults.items():
            if not self.config.has_option("General", key):
                self.config.set("General", key, value)
                log.debug(f"Added missing default General setting: {key}={value}")
                modified = True

        # Paths section defaults
        paths_defaults = {
            "completed_downloads_directory": "",
            "temporary_downloads_directory": "",
        }
        for key, value in paths_defaults.items():
            if not self.config.has_option("Paths", key):
                self.config.set("Paths", key, value)
                log.debug(f"Added missing default Paths setting: {key}={value}")
                modified = True
        
        # Save config if any defaults were added
        if modified:
            self.save()

    def _enforce_required_settings(self):
        """Enforce settings that are intentionally always enabled."""
        modified = False
        if self.get("General", "download_archive", fallback="True") != "True":
            self.config.set("General", "download_archive", "True")
            modified = True
            log.info("Forced setting: General.download_archive=True")
        if modified:
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
        
        # Check if value is actually different to avoid unnecessary disk writes
        if self.config.has_option(section, option):
            current_value = self.config.get(section, option)
            if current_value == str(value):
                return

        self.config.set(section, option, str(value))
        self.save()
