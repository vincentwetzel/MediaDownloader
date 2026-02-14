import json
import os
import logging
import uuid
import datetime
import re

log = logging.getLogger(__name__)

class SortingManager:
    def __init__(self, config_manager):
        self.config_manager = config_manager
        self.rules_path = os.path.join(self.config_manager.get_config_dir(), "sorting_rules.json")
        self.rules = []
        self.load_rules()

    def load_rules(self):
        if os.path.exists(self.rules_path):
            try:
                with open(self.rules_path, 'r', encoding='utf-8') as f:
                    self.rules = json.load(f)
            except Exception:
                log.exception("Failed to load sorting rules")
                self.rules = []
        else:
            self.rules = []

    def save_rules(self):
        try:
            with open(self.rules_path, 'w', encoding='utf-8') as f:
                json.dump(self.rules, f, indent=4)
        except Exception:
            log.exception("Failed to save sorting rules")

    def _normalize_subfolder_pattern(self, pattern):
        """Normalize subfolder pattern: forward slashes, no duplicates, no leading/trailing."""
        if not pattern:
            return None
        # Replace backslashes with forward slashes
        pattern = pattern.replace('\\', '/')
        # Collapse multiple slashes
        pattern = re.sub(r'/+', '/', pattern)
        # Strip leading/trailing slashes
        pattern = pattern.strip('/')
        return pattern if pattern else None

    def add_rule(self, name, target_path, subfolder_pattern=None, download_type="All", conditions=None):
        if conditions is None:
            conditions = []
        
        subfolder_pattern = self._normalize_subfolder_pattern(subfolder_pattern)
            
        rule = {
            "id": str(uuid.uuid4()),
            "name": name,
            "target_path": os.path.normpath(target_path),
            "subfolder_pattern": subfolder_pattern,
            "download_type": download_type,
            "conditions": conditions
        }
        self.rules.append(rule)
        self.save_rules()

    def update_rule(self, rule_id, name, target_path, subfolder_pattern=None, download_type="All", conditions=None):
        if conditions is None:
            conditions = []
            
        subfolder_pattern = self._normalize_subfolder_pattern(subfolder_pattern)
            
        for rule in self.rules:
            if rule.get("id") == rule_id:
                rule["name"] = name
                rule["target_path"] = os.path.normpath(target_path)
                rule["subfolder_pattern"] = subfolder_pattern
                rule["download_type"] = download_type
                rule["conditions"] = conditions
                # Remove legacy fields
                rule.pop('uploaders', None)
                rule.pop('audio_only', None)
                rule.pop('date_subfolders', None)
                rule.pop('filter_field', None)
                rule.pop('filter_operator', None)
                rule.pop('filter_values', None)
                break
        self.save_rules()

    def delete_rule(self, rule_id):
        self.rules = [r for r in self.rules if r.get("id") != rule_id]
        self.save_rules()

    def move_rule(self, rule_id, direction):
        """Move a rule up or down in the list."""
        try:
            index = next(i for i, r in enumerate(self.rules) if r['id'] == rule_id)
        except StopIteration:
            return

        if direction == "up" and index > 0:
            self.rules.insert(index - 1, self.rules.pop(index))
            self.save_rules()
        elif direction == "down" and index < len(self.rules) - 1:
            self.rules.insert(index + 1, self.rules.pop(index))
            self.save_rules()

    def get_target_path(self, metadata, current_download_type="Video"):
        if not metadata:
            return None

        for rule in self.rules:
            rule_type = rule.get('download_type', 'All')
            # Backwards compatibility for legacy audio_only flag
            if rule.get('audio_only', False) and rule_type == 'All':
                rule_type = 'Audio'

            if not self._download_type_matches(rule_type, current_download_type, metadata):
                continue

            # --- Backwards compatibility for single-filter rules ---
            if 'filter_field' in rule:
                conditions = [{
                    "field": rule.get('filter_field', 'uploader'),
                    "operator": rule.get('filter_operator', 'is_one_of'),
                    "values": rule.get('filter_values', [])
                }]
            elif 'uploaders' in rule:
                conditions = [{
                    "field": 'uploader',
                    "operator": 'is_one_of',
                    "values": rule.get('uploaders', [])
                }]
            else:
                conditions = rule.get('conditions', [])

            if not conditions:
                return self._get_final_path(rule, metadata)

            all_conditions_met = True
            for cond in conditions:
                if not self._check_condition(cond, metadata):
                    all_conditions_met = False
                    break
            
            if all_conditions_met:
                log.info(f"Sorting rule '{rule.get('name')}' matched.")
                return self._get_final_path(rule, metadata)
        
        return None

    def _is_playlist_item(self, metadata):
        """Detect whether metadata belongs to an item downloaded from a playlist."""
        playlist_keys = ('playlist', 'playlist_title', 'playlist_id', 'playlist_index')
        for key in playlist_keys:
            value = metadata.get(key)
            if value not in (None, "", "NA"):
                return True
        return False

    def _download_type_matches(self, rule_type, current_download_type, metadata):
        """Check if a rule type applies to the current download and metadata context."""
        normalized_rule_type = str(rule_type or 'All').strip()

        if normalized_rule_type == 'All':
            return True

        if normalized_rule_type == 'Video Playlist':
            return current_download_type == 'Video' and self._is_playlist_item(metadata)

        if normalized_rule_type == 'Audio Playlist':
            return current_download_type == 'Audio' and self._is_playlist_item(metadata)

        return normalized_rule_type == current_download_type

    def _check_condition(self, condition, metadata):
        field_key = condition.get('field', 'uploader')
        operator = condition.get('operator', 'is_one_of')
        # Strip whitespace and lowercase rule values
        values_to_check = [str(v).strip().lower() for v in condition.get('values', [])]

        if not values_to_check: # A condition with no values is considered invalid/unmatchable
            return False

        metadata_value = metadata.get(field_key)
        
        # Fallback: if uploader is missing, try channel
        if metadata_value is None and field_key == 'uploader':
            metadata_value = metadata.get('channel')

        if metadata_value is None:
            return False

        # Numeric comparison for duration
        if field_key == 'duration':
            try:
                meta_num = float(metadata_value)
                target_num = float(values_to_check[0])
                if operator == 'greater_than': return meta_num > target_num
                if operator == 'less_than': return meta_num < target_num
                if operator == 'equals': return meta_num == target_num
            except (ValueError, TypeError):
                return False
        else:
            # String comparisons
            metadata_items = []
            if isinstance(metadata_value, list):
                metadata_items = [str(item).strip().lower() for item in metadata_value]
            elif isinstance(metadata_value, str):
                metadata_items = [metadata_value.strip().lower()]
            else:
                metadata_items = [str(metadata_value).strip().lower()]

            if operator == 'is_one_of':
                return any(item in values_to_check for item in metadata_items)
            elif operator == 'contains':
                return any(val in item for val in values_to_check for item in metadata_items)
            elif operator == 'equals':
                return any(item == val for val in values_to_check for item in metadata_items)
        
        return False

    def _get_final_path(self, rule, metadata):
        """Helper to construct the final path, including dynamic subfolders."""
        base_path = rule.get('target_path')
        
        # Handle legacy date_subfolders flag
        if rule.get('date_subfolders', False):
            upload_date = metadata.get('upload_date')  # YYYYMMDD
            if upload_date and len(str(upload_date)) == 8:
                try:
                    upload_date_str = str(upload_date)
                    year = upload_date_str[:4]
                    month = upload_date_str[4:6]
                    subfolder = f"{year} - {month}"
                    return os.path.join(base_path, subfolder)
                except Exception:
                    log.warning(f"Could not parse upload date '{upload_date}' for legacy subfolder creation")
            return base_path

        # Handle new generic subfolder pattern
        pattern = rule.get('subfolder_pattern')
        if pattern:
            try:
                # Prepare metadata for formatting
                safe_metadata = {}
                for k, v in metadata.items():
                    safe_metadata[k] = str(v) if v is not None else "NA"

                if safe_metadata.get('playlist', "NA") == "NA":
                    playlist_fallback = metadata.get('playlist') or metadata.get('playlist_title')
                    if playlist_fallback is not None:
                        safe_metadata['playlist'] = str(playlist_fallback)
                 
                # Add some helper keys for dates if upload_date exists
                upload_date = str(metadata.get('upload_date', ''))
                if len(upload_date) == 8:
                    safe_metadata['upload_year'] = upload_date[:4]
                    safe_metadata['upload_month'] = upload_date[4:6]
                    safe_metadata['upload_day'] = upload_date[6:8]
                else:
                    safe_metadata['upload_year'] = "NA"
                    safe_metadata['upload_month'] = "NA"
                    safe_metadata['upload_day'] = "NA"

                # Replace tokens like {uploader} or {upload_year}
                def replace_token(match):
                    key = match.group(1)
                    return safe_metadata.get(key, "NA")

                # Support both {token} and yt-dlp-style %(token)s placeholders.
                subfolder = re.sub(r'%\(([^)]+)\)s', replace_token, pattern)
                subfolder = re.sub(r'\{(\w+)\}', replace_token, subfolder)
                
                # Sanitize the subfolder path to remove illegal characters
                # We normalize the pattern first to handle mixed slashes
                subfolder = subfolder.replace('\\', '/')
                parts = subfolder.split('/')
                
                sanitized_parts = []
                for part in parts:
                    # Remove illegal chars from filename component
                    sanitized = re.sub(r'[<>:"|?*]', '', part).strip()
                    if sanitized:
                        sanitized_parts.append(sanitized)
                
                if sanitized_parts:
                    return os.path.join(base_path, *sanitized_parts)
                    
            except Exception:
                log.exception(f"Error formatting subfolder pattern '{pattern}'")

        return base_path
