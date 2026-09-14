"""Copy and verify only the user's confirmed inventory; never delete files."""
import csv,hashlib,json,shutil,subprocess
from pathlib import Path
from datetime import datetime
root=Path(__file__).resolve().parents[2]
review=root/'docs/asset-cleanup-review/files.csv'
rows=[r for r in csv.DictReader(review.open(encoding='utf-8-sig')) if r['verdict']=='建议删除']
assert len(rows)==2093, 'Confirmed inventory size changed'
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()
for r in rows:
    p=(root/r['path']).resolve()
    assert p.is_relative_to(root/'Content') and p.suffix.lower() in {'.uasset','.umap','.fbx','.png'},r['path']
    assert p.is_file() and p.stat().st_size==int(r['bytes']) and sha(p)==r['sha256'], 'Changed file: '+r['path']
backup=root.parent/'HSR_asset_backups'/datetime.now().strftime('%Y%m%d_%H%M%S_approved_cleanup')
assert not backup.is_relative_to(root)
backup.mkdir(parents=True,exist_ok=False)
shutil.copy2(review,backup/'confirmed_inventory.csv')
print('BACKUP '+str(backup),flush=True)
for i,r in enumerate(rows,1):
    dst=backup/r['path'];dst.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/r['path'],dst)
    assert sha(dst)==r['sha256'],'Backup verification failed: '+r['path']
    if i%200==0:print(f'VERIFIED {i}/{len(rows)}',flush=True)
manifest={'authorized_by':'User confirmed 2093 suggested-delete rows on 2026-09-14','backup_dir':str(backup),'inventory_sha256':sha(review),'pre_cleanup_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'verified':True,'count':len(rows),'bytes':sum(int(r['bytes']) for r in rows),'files':rows}
(backup/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
(root/'Saved/Presentation/approved_cleanup_backup.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in manifest.items() if k!='files'},ensure_ascii=True),flush=True)
