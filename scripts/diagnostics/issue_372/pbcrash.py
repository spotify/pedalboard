from PyQt6.QtCore import QCoreApplication, pyqtSignal, QObject
from pedalboard.io import AudioFile
import sys


class AudioLoader(QObject):
    audio_loaded = pyqtSignal()

    def __init__(self):
        super().__init__()
        with AudioFile('PN.flac', 'r') as f:
            f.read(44100)
        self.file = f

    def check_file(self):
        if self.file:
            self.audio_loaded.emit()


class MyApp(QCoreApplication):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.AudioLoader = AudioLoader()
        self.AudioLoader.audio_loaded.connect(self.on_success)

    def on_success(self):
        print('Audio loaded!')
        sys.exit()


if __name__ == '__main__':
    app = MyApp([])
    app.AudioLoader.check_file()
    app.exec()
