"""Read-only package dependency export. Does not load or delete candidate assets."""
import json
from pathlib import Path
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=True, include_hard_management_references=True)
assets = registry.get_assets_by_path('/Game', recursive=True)
packages = {}
for asset in assets:
    package = str(asset.package_name)
    if package in packages: continue
    packages[package] = {'class': str(asset.asset_class_path.asset_name),
        'dependencies': [str(p) for p in registry.get_dependencies(package, options) if str(p).startswith('/Game/')],
        'referencers': [str(p) for p in registry.get_referencers(package, options) if str(p).startswith('/Game/')]}
path = Path(unreal.Paths.project_dir(), 'Saved/Presentation/asset_dependency_graph.json')
path.write_text(json.dumps(packages, ensure_ascii=False), encoding='utf-8')
print(json.dumps({'success': True, 'packages': len(packages), 'export': str(path)}))
