# Presentation source assets

These assets were created locally for the authorized September 2026 HSR Demo rebuild.

- `NightCityKit.blend`: editable modular city/arena/hub prop collection. The 15 `SM_*.fbx` files are its UE exports; regenerate with `Tools/Presentation/build_models.py` using Blender.
- `ItemIcons/EquipmentIcons.blend`: original equipment icon sculptures and rendered PNGs. Regenerate with `Tools/Presentation/build_item_icons.py`.
- `Portraits/`: captures of the project's existing character models. Character ownership/licensing remains that of the original project assets.
- `layouts.json`: generated scene placement manifest in meters; authored by `Tools/Presentation/scene_layout.py` and consumed by the editor scene authoring workflow.

The modular meshes, simple procedural materials and item icon sculptures were generated in this project, without downloaded or paid asset dependencies. Existing characters, their animations and engine/plugin assets were reused; their original licenses are unchanged. Reference screenshots guide composition only and are not packaged as game textures. `*.blend1` files are Blender recovery backups, not required for import.

UE assets reside under `/Game/Presentation`, with formal map content under `/Game/Maps/VerticalSlice`. Preserve the gameplay IDs in existing definitions when reauthoring. Editor scripts can modify many binary assets: retain backups and run only the script appropriate to the intended change.
