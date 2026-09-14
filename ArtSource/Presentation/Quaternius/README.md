# Complete defeat animation source

Author: Quaternius. License: CC0 1.0 (see LICENSE).

- Author's pack page: https://quaternius.com/packs/universalanimationlibrary.html
- Author's distribution: https://quaternius.itch.io/universal-animation-library
- Standard-library glTF mirror: https://github.com/J-Ponzo/gltf-universal-animation-library
- Downloaded revision: `e24c23cf2a1323488a3faa226ea7ea21f644b73e`, 2026-09-14.

`QuaterniusDeath.fbx` contains the standard library's `Death01` action and its source mannequin. `QuaterniusCast.fbx` and `QuaterniusHeal.fbx` contain `Spell_Simple_Shoot` and `Spell_Simple_Enter`. They were converted with Blender 3.6 by `Tools/Presentation/export_quaternius_death.py`, then imported by `import_quaternius_death.py` and `author_cast_animations.py`. `author_complete_defeat.py` retargets the collapse to each playable skeleton, disabling standing foot IK. The generated character clips depend on local character assets and are separately excluded from public backup pending redistribution evidence.

To reproduce the FBX, clone the revision above into `Saved/Presentation/Quaternius`, then run the export script with Blender. The FBX checked in here is sufficient for the Unreal import; the mirror checkout is not needed at runtime.
