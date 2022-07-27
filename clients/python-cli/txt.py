from enum import Enum


class Font(Enum):
    SIXCAPS_120 = 'sixcaps_120'
    ROBOTO_MONO_100 = 'roboto_mono_100'
    DIALOG_16 = 'dialog_16'
    SPECIAL_ELITE_30 = 'special_elite_30'
    OPEN_SANS_8 = 'open_sans_8'
    OPEN_SANS_BOLD_8 = 'open_sans_bold_8'
    OPEN_SANS_14 = 'open_sans_14'
    OPEN_SANS_BOLD_14 = 'open_sans_bold_14'
    DSEG14_90 = 'dseg14_90'
    PRESS_START_50 = 'press_start_50'

    def __str__(self):
        return self.value
