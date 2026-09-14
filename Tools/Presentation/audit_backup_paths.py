"""Prepare an exact Git path allowlist; does not stage, commit, or push."""
import json,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
def git(*args):return subprocess.check_output(['git','-c','core.quotePath=false',*args],cwd=root,stderr=subprocess.DEVNULL).decode('utf-8')
tracked=git('diff','--name-only','-z').split('\0')
untracked=git('ls-files','--others','--exclude-standard','-z').split('\0')
rows=[];included=[];issues=[]
for rel in sorted(set(tracked+untracked)-{''}):
    path=root/rel
    size=path.stat().st_size if path.is_file() else 0
    reason=''
    if rel.startswith('Plugins/VRM4U/') and path.suffix.lower() in {'.dll','.pdb','.lib'}:reason='pre-existing local plugin binary; source provenance not verified'
    elif (rel.startswith('Content/Presentation/Animation/') and not rel.startswith('Content/Presentation/Animation/CC0/')) or rel.startswith('ArtSource/Presentation/Portraits/') or '/T_Portrait_' in rel:reason='derived from local third-party character / animation assets; public redistribution evidence unavailable'
    elif rel.startswith('Content/') and rel.count('/')==1 and any(x in path.name for x in ['LIVE_','FINAL_','PLAY_','FIX_']):reason='intermediate retarget output / redirector; not a distributable source asset'
    elif path.suffix.lower() in {'.blend1','.bak'}:reason='local recovery copy'
    elif size>=100*1024*1024:reason='exceeds GitHub individual file limit'
    if not reason and path.is_file() and path.suffix.lower() in {'.py','.cpp','.h','.md','.ini','.json','.toml','.ps1'}:
        text=path.read_text(encoding='utf-8-sig',errors='replace')
        for line_no,line in enumerate(text.splitlines(),1):
            if re.search(r'(?:sk-[A-Za-z0-9]{24,}|gh[pousr]_[A-Za-z0-9]{20,}|BEGIN (?:RSA |OPENSSH )?PRIVATE KEY)',line):
                issues.append({'path':rel,'line':line_no,'kind':'credential-like literal (redacted)'})
    rows.append({'path':rel,'bytes':size,'git':'modified tracked' if rel in tracked else 'untracked','decision':'exclude' if reason else 'include','reason':reason})
    if not reason:included.append(rel)
out=root/'Saved/Presentation';out.mkdir(exist_ok=True)
(out/'git_backup_audit.json').write_text(json.dumps({'remote':git('remote','get-url','origin').strip(),'branch':git('branch','--show-current').strip(),'ahead':git('log','--oneline','origin/main..HEAD').splitlines(),'files':rows,'secret_findings':issues},ensure_ascii=False,indent=2),encoding='utf-8')
(out/'git_backup_paths.txt').write_text('\n'.join(included)+'\n',encoding='utf-8')
print(json.dumps({'included':len(included),'excluded':len(rows)-len(included),'secret_findings':issues},ensure_ascii=False))
