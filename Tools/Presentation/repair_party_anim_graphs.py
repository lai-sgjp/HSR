"""Compatibility entry point for the current self-contained animation authoring flow."""
from pathlib import Path
import unreal
folder=Path(unreal.Paths.project_dir(),'Tools/Presentation')
for name in ['author_combat_animations.py','rebuild_locomotion_from_source.py']:
    path=folder/name
    exec(compile(path.read_text(encoding='utf-8-sig'),str(path),'exec'),{'__name__':'__main__'})
