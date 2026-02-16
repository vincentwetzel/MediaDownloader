import logging
import os
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QTableWidget,
    QTableWidgetItem, QLineEdit, QFileDialog, QMessageBox, QInputDialog,
    QFormLayout, QDialog, QDialogButtonBox, QTextEdit, QCheckBox, QComboBox,
    QHeaderView, QAbstractItemView, QScrollArea, QFrame, QSizePolicy
)
from PyQt6.QtCore import Qt
from core.sorting_manager import SortingManager

log = logging.getLogger(__name__)

class SortingTab(QWidget):
    def __init__(self, parent, config_manager):
        super().__init__(parent)
        self.config_manager = config_manager
        self.sorting_manager = SortingManager(config_manager)
        self.init_ui()

    def init_ui(self):
        layout = QVBoxLayout(self)

        # Header
        header = QLabel("Sorting Rules")
        header.setStyleSheet("font-size: 16px; font-weight: bold;")
        layout.addWidget(header)

        description = QLabel(
            "Create rules to automatically move downloaded files to specific folders based on metadata like uploader, title, or tags.\n"
            "You can also define dynamic subfolder patterns using metadata tokens like {uploader}, {upload_year}, {album_year}, or {album}."
        )
        description.setWordWrap(True)
        layout.addWidget(description)

        # Table of rules
        self.rules_table = QTableWidget()
        self.rules_table.setColumnCount(5)
        self.rules_table.setHorizontalHeaderLabels(["Name", "Type", "Condition", "Target Path", "Subfolder"])
        self.rules_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.rules_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch) # Condition stretches
        self.rules_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch) # Path stretches
        self.rules_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.rules_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.rules_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.rules_table.itemDoubleClicked.connect(self.edit_selected_rule)
        layout.addWidget(self.rules_table)

        # Buttons
        btn_layout = QHBoxLayout()
        
        add_btn = QPushButton("Add Rule")
        add_btn.clicked.connect(self.add_rule)
        btn_layout.addWidget(add_btn)

        edit_btn = QPushButton("Edit Rule")
        edit_btn.clicked.connect(self.edit_selected_rule)
        btn_layout.addWidget(edit_btn)

        del_btn = QPushButton("Delete Rule")
        del_btn.clicked.connect(self.delete_rule)
        btn_layout.addWidget(del_btn)

        # Spacer
        btn_layout.addStretch()

        # Reordering Buttons
        up_btn = QPushButton("Move Up")
        up_btn.clicked.connect(self.move_rule_up)
        btn_layout.addWidget(up_btn)

        down_btn = QPushButton("Move Down")
        down_btn.clicked.connect(self.move_rule_down)
        btn_layout.addWidget(down_btn)

        layout.addLayout(btn_layout)

        self.refresh_rules_list()

    def refresh_rules_list(self):
        # Preserve selection if possible
        selected_row = -1
        selected_items = self.rules_table.selectedItems()
        if selected_items:
            selected_row = selected_items[0].row()

        self.rules_table.setRowCount(0)
        for rule in self.sorting_manager.rules:
            try:
                row = self.rules_table.rowCount()
                self.rules_table.insertRow(row)

                name = str(rule.get('name') or 'Unnamed Rule')
                path = str(rule.get('target_path') or 'No Path')
                
                # Type
                dtype = str(rule.get('download_type') or 'All')
                if rule.get('audio_only', False) and dtype == 'All':
                    dtype = 'Audio'
                
                # Condition
                conditions = []
                if 'conditions' in rule:
                    conditions = rule.get('conditions', [])
                elif 'uploaders' in rule:
                    conditions = [{
                        "field": 'uploader',
                        "operator": 'is_one_of',
                        "values": rule.get('uploaders', [])
                    }]
                else:
                    conditions = [{
                        "field": rule.get('filter_field', 'uploader'),
                        "operator": rule.get('filter_operator', 'is_one_of'),
                        "values": rule.get('filter_values', [])
                    }]

                cond_strs = []
                for cond in conditions:
                    field = str(cond.get('field', 'uploader')).replace('_', ' ').title()
                    op = str(cond.get('operator', 'is_one_of')).replace('_', ' ')
                    values = cond.get('values', [])
                    if not isinstance(values, list): values = []
                    
                    if len(values) == 1 and op == 'is one of': op = 'is'
                    
                    val_str = f"'{values[0]}'" if len(values) == 1 else f"[{len(values)} values]"
                    if not values: val_str = "[any]"
                    cond_strs.append(f"{field} {op} {val_str}")
                
                condition_text = " AND ".join(cond_strs)

                # Subfolder
                subfolder = ""
                if rule.get('subfolder_pattern'):
                    subfolder = str(rule['subfolder_pattern'])
                elif rule.get('date_subfolders'):
                    subfolder = "YYYY - MM"

                # Set items
                self.rules_table.setItem(row, 0, QTableWidgetItem(name))
                self.rules_table.setItem(row, 1, QTableWidgetItem(dtype))
                self.rules_table.setItem(row, 2, QTableWidgetItem(condition_text))
                self.rules_table.setItem(row, 3, QTableWidgetItem(os.path.normpath(path)))
                self.rules_table.setItem(row, 4, QTableWidgetItem(subfolder))

                # Store ID in the first item
                self.rules_table.item(row, 0).setData(Qt.ItemDataRole.UserRole, rule['id'])
            except Exception:
                log.exception("Error displaying sorting rule")

        # Restore selection
        if selected_row >= 0 and selected_row < self.rules_table.rowCount():
            self.rules_table.selectRow(selected_row)

    def add_rule(self):
        try:
            dialog = RuleDialog(self)
            if dialog.exec():
                data = dialog.get_data()
                if data['name'] and data['target_path']:
                    self.sorting_manager.add_rule(**data)
                    self.refresh_rules_list()
        except Exception:
            log.exception("Error adding rule")
            QMessageBox.critical(self, "Error", "An error occurred while opening the rule dialog.")

    def edit_selected_rule(self):
        selected_items = self.rules_table.selectedItems()
        if not selected_items:
            return
        
        # Get ID from the first column of the selected row
        row = selected_items[0].row()
        rule_id = self.rules_table.item(row, 0).data(Qt.ItemDataRole.UserRole)
        
        rule = next((r for r in self.sorting_manager.rules if r['id'] == rule_id), None)
        if rule:
            try:
                dialog = RuleDialog(self, rule)
                if dialog.exec():
                    data = dialog.get_data()
                    if data['name'] and data['target_path']:
                        self.sorting_manager.update_rule(rule_id, **data)
                        self.refresh_rules_list()
            except Exception:
                log.exception("Error editing rule")
                QMessageBox.critical(self, "Error", "An error occurred while opening the rule dialog.")

    def delete_rule(self):
        selected_items = self.rules_table.selectedItems()
        if not selected_items:
            return

        row = selected_items[0].row()
        rule_id = self.rules_table.item(row, 0).data(Qt.ItemDataRole.UserRole)
        
        confirm = QMessageBox.question(
            self, "Delete Rule",
            "Are you sure you want to delete this sorting rule?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if confirm == QMessageBox.StandardButton.Yes:
            self.sorting_manager.delete_rule(rule_id)
            self.refresh_rules_list()

    def move_rule_up(self):
        selected_items = self.rules_table.selectedItems()
        if not selected_items:
            return
        
        row = selected_items[0].row()
        if row == 0: return # Already at top

        rule_id = self.rules_table.item(row, 0).data(Qt.ItemDataRole.UserRole)
        self.sorting_manager.move_rule(rule_id, "up")
        self.refresh_rules_list()
        self.rules_table.selectRow(row - 1)

    def move_rule_down(self):
        selected_items = self.rules_table.selectedItems()
        if not selected_items:
            return
        
        row = selected_items[0].row()
        if row == self.rules_table.rowCount() - 1: return # Already at bottom

        rule_id = self.rules_table.item(row, 0).data(Qt.ItemDataRole.UserRole)
        self.sorting_manager.move_rule(rule_id, "down")
        self.refresh_rules_list()
        self.rules_table.selectRow(row + 1)

class ConditionWidget(QWidget):
    METADATA_FIELDS = {
        "Uploader": "uploader",
        "Title": "title",
        "Playlist Title": "playlist_title",
        "Tags": "tags",
        "Description": "description",
        "Duration (seconds)": "duration"
    }
    OPERATORS = {
        "Is one of": "is_one_of",
        "Contains": "contains",
        "Equals": "equals",
        "Greater than": "greater_than",
        "Less than": "less_than"
    }

    def __init__(self, parent=None, condition=None):
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Fixed)
        self.layout = QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0) # Zero margins everywhere
        self.layout.setSpacing(2)
        
        # Top row: Field, Operator, Remove Button
        top_row = QHBoxLayout()
        top_row.setContentsMargins(0, 0, 0, 0)
        
        self.field_combo = QComboBox()
        self.field_combo.addItems(self.METADATA_FIELDS.keys())
        
        self.operator_combo = QComboBox()
        
        self.remove_btn = QPushButton("Remove")
        self.remove_btn.setStyleSheet("color: red;")
        self.remove_btn.setFixedWidth(60)
        
        top_row.addWidget(QLabel("If"))
        top_row.addWidget(self.field_combo)
        top_row.addWidget(self.operator_combo)
        top_row.addWidget(self.remove_btn)
        
        self.layout.addLayout(top_row)
        
        # Bottom row: Values
        self.values_edit = QTextEdit()
        self.values_edit.setFixedHeight(60)
        self.layout.addWidget(self.values_edit)
        
        # Connect signals
        self.field_combo.currentTextChanged.connect(self.update_operators)
        
        # Initial setup
        if condition:
            field_key = condition.get('field', 'uploader')
            op_key = condition.get('operator', 'is_one_of')
            values = condition.get('values', [])
            
            field_text = next((k for k, v in self.METADATA_FIELDS.items() if v == field_key), "Uploader")
            op_text = next((k for k, v in self.OPERATORS.items() if v == op_key), "Is one of")
            
            self.field_combo.setCurrentText(field_text)
            self.update_operators(field_text)
            self.operator_combo.setCurrentText(op_text)
            
            if isinstance(values, list):
                safe_values = [str(v) for v in values if v is not None]
                self.values_edit.setPlainText('\n'.join(safe_values))
        else:
            self.update_operators(self.field_combo.currentText())

    def update_operators(self, field_text):
        self.operator_combo.clear()
        if field_text == "Duration (seconds)":
            self.operator_combo.addItems(["Greater than", "Less than", "Equals"])
            self.values_edit.setPlaceholderText("Enter duration in seconds (e.g., 300).")
        else:
            self.operator_combo.addItems(["Is one of", "Contains", "Equals"])
            self.values_edit.setPlaceholderText("Enter values, one per line.")

    def get_data(self):
        field_text = self.field_combo.currentText()
        op_text = self.operator_combo.currentText()
        
        return {
            "field": self.METADATA_FIELDS.get(field_text, "uploader"),
            "operator": self.OPERATORS.get(op_text, "is_one_of"),
            "values": [v.strip() for v in self.values_edit.toPlainText().split('\n') if v.strip()]
        }

class RuleDialog(QDialog):
    def __init__(self, parent=None, rule=None):
        super().__init__(parent)
        self.setWindowTitle("Sorting Rule")
        self.resize(600, 700)
        
        self.layout = QVBoxLayout(self)
        
        form_layout = QFormLayout()

        # --- Create widgets ---
        self.name_edit = QLineEdit()
        
        self.path_edit = QLineEdit()
        browse_btn = QPushButton("Browse")
        
        self.subfolder_pattern_edit = QLineEdit()
        self.tokens_combo = QComboBox()
        
        self.type_combo = QComboBox()
        
        # --- Layout Path Widget ---
        path_widget = QWidget()
        path_layout = QHBoxLayout(path_widget)
        path_layout.setContentsMargins(0, 0, 0, 0)
        path_layout.addWidget(self.path_edit)
        path_layout.addWidget(browse_btn)
        
        # --- Layout Subfolder Pattern Widget ---
        pattern_widget = QWidget()
        pattern_layout = QHBoxLayout(pattern_widget)
        pattern_layout.setContentsMargins(0, 0, 0, 0)
        pattern_layout.addWidget(self.subfolder_pattern_edit)
        pattern_layout.addWidget(self.tokens_combo)
        
        # --- Add rows to form ---
        form_layout.addRow("Rule Name:", self.name_edit)
        form_layout.addRow("Target Folder:", path_widget)
        form_layout.addRow("Subfolder Pattern (Optional):", pattern_widget)
        form_layout.addRow("Rule Applies To:", self.type_combo)
        
        self.layout.addLayout(form_layout)

        # --- Conditions Area ---
        conditions_group = QWidget()
        conditions_group_layout = QVBoxLayout(conditions_group)
        conditions_group_layout.setContentsMargins(0, 0, 0, 0)
        conditions_group_layout.setSpacing(0) # Zero spacing between label and scroll area
        
        conditions_group_layout.addWidget(QLabel("Conditions (All must match):"))
        
        scroll = QScrollArea()
        scroll.setFrameShape(QFrame.Shape.NoFrame) # Remove frame for seamless look
        scroll.setWidgetResizable(True)
        
        self.conditions_container = QWidget()
        # Use VBox layout but force it to pack tightly
        self.conditions_layout = QVBoxLayout(self.conditions_container)
        self.conditions_layout.setContentsMargins(0, 0, 0, 0) # Remove margins to tighten spacing
        self.conditions_layout.setSpacing(0) # Zero spacing between conditions
        self.conditions_layout.setAlignment(Qt.AlignmentFlag.AlignTop) # Force top alignment
        
        # Add button to layout
        self.add_cond_btn = QPushButton("Add Condition")
        self.add_cond_btn.clicked.connect(self.add_condition)
        self.conditions_layout.addWidget(self.add_cond_btn)
        
        # Add stretch to force top alignment
        self.conditions_layout.addStretch()
        
        scroll.setWidget(self.conditions_container)
        conditions_group_layout.addWidget(scroll)
        
        self.layout.addWidget(conditions_group)

        # --- Buttons ---
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self.layout.addWidget(buttons)

        # --- Populate and Connect ---
        browse_btn.clicked.connect(self.browse_path)
        
        self.subfolder_pattern_edit.setPlaceholderText("e.g., {upload_year}/{uploader} or {album_year}/{album}")
        self.subfolder_pattern_edit.setToolTip(
            "Create dynamic subfolders using tokens like {upload_year}, {upload_month}, {album_year}, {uploader}, or {album}."
        )
        
        self.tokens_combo.addItem("Insert...")
        self.tokens_combo.addItem("Year {upload_year}", "{upload_year}")
        self.tokens_combo.addItem("Month {upload_month}", "{upload_month}")
        self.tokens_combo.addItem("Day {upload_day}", "{upload_day}")
        self.tokens_combo.addItem("Uploader {uploader}", "{uploader}")
        self.tokens_combo.addItem("Title {title}", "{title}")
        self.tokens_combo.addItem("ID {id}", "{id}")
        self.tokens_combo.addItem("Album Year {album_year}", "{album_year}")
        self.tokens_combo.addItem("Album {album}", "{album}")
        self.tokens_combo.activated.connect(self.insert_token)
        
        self.type_combo.addItems([
            "All Downloads",
            "Video Downloads",
            "Audio Downloads",
            "Gallery Downloads",
            "Video Playlist Downloads",
            "Audio Playlist Downloads",
        ])
        
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)

        # --- Populate with existing rule data if provided ---
        if rule:
            self.name_edit.setText(rule.get('name') or '')
            self.path_edit.setText(rule.get('target_path') or '')
            
            if rule.get('date_subfolders', False):
                self.subfolder_pattern_edit.setText("{upload_year} - {upload_month}")
            else:
                self.subfolder_pattern_edit.setText(rule.get('subfolder_pattern') or '')

            saved_type = rule.get('download_type') or 'All'
            if rule.get('audio_only', False) and saved_type == 'All': saved_type = 'Audio'
            type_map = {
                "Video": 1,
                "Audio": 2,
                "Gallery": 3,
                "Video Playlist": 4,
                "Audio Playlist": 5,
            }
            self.type_combo.setCurrentIndex(type_map.get(saved_type, 0))

            # Populate conditions
            conditions = []
            if 'conditions' in rule:
                conditions = rule.get('conditions', [])
            elif 'uploaders' in rule: # Legacy
                conditions = [{
                    "field": 'uploader',
                    "operator": 'is_one_of',
                    "values": rule.get('uploaders', [])
                }]
            else: # Legacy single filter
                conditions = [{
                    "field": rule.get('filter_field', 'uploader'),
                    "operator": rule.get('filter_operator', 'is_one_of'),
                    "values": rule.get('filter_values', [])
                }]
            
            for cond in conditions:
                self.add_condition(cond)
        else:
            # Add one empty condition by default
            self.add_condition()

    def add_condition(self, condition_data=None):
        widget = ConditionWidget(self, condition_data)
        widget.remove_btn.clicked.connect(lambda: self.remove_condition(widget))
        # Insert before the button (which is before the stretch)
        index = self.conditions_layout.indexOf(self.add_cond_btn)
        self.conditions_layout.insertWidget(index, widget)

    def remove_condition(self, widget):
        widget.deleteLater()
        self.conditions_layout.removeWidget(widget)

    def browse_path(self):
        path = QFileDialog.getExistingDirectory(self, "Select Target Folder")
        if path:
            self.path_edit.setText(os.path.normpath(path))

    def insert_token(self, index):
        if index == 0: return
        token = self.tokens_combo.currentData()
        if token:
            self.subfolder_pattern_edit.insert(token)
            self.subfolder_pattern_edit.setFocus()
        self.tokens_combo.setCurrentIndex(0)

    def get_data(self):
        # Type
        type_map = {
            0: "All",
            1: "Video",
            2: "Audio",
            3: "Gallery",
            4: "Video Playlist",
            5: "Audio Playlist",
        }
        download_type = type_map.get(self.type_combo.currentIndex(), "All")
        
        # Collect conditions
        conditions = []
        for i in range(self.conditions_layout.count()):
            item = self.conditions_layout.itemAt(i)
            widget = item.widget()
            if isinstance(widget, ConditionWidget):
                conditions.append(widget.get_data())
        
        return {
            "name": self.name_edit.text().strip(),
            "target_path": os.path.normpath(self.path_edit.text().strip()),
            "subfolder_pattern": self.subfolder_pattern_edit.text().strip(),
            "download_type": download_type,
            "conditions": conditions
        }
