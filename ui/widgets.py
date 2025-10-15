from PyQt6.QtWidgets import QTextEdit
import logging
log = logging.getLogger(__name__)

class UrlTextEdit(QTextEdit):
    def focusInEvent(self, ev):
        super().focusInEvent(ev)
        try:
            from PyQt6.QtGui import QGuiApplication
            cb = QGuiApplication.clipboard()
            txt = cb.text().strip()
            if txt.startswith("http://") or txt.startswith("https://"):
                log.debug("Auto-paste from clipboard: %s", txt)
                self.setPlainText(txt)
        except Exception:
            pass