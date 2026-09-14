"""Sample adapter memory during the explicit standalone route benchmark."""
import csv,json,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[2]/'Saved/Presentation/Performance'
root.mkdir(parents=True,exist_ok=True)
rows=[]
for index in range(100):
 result=subprocess.run(['nvidia-smi','--query-gpu=timestamp,name,memory.used,memory.total,utilization.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,check=True)
 for line in result.stdout.strip().splitlines():
  fields=[x.strip() for x in next(csv.reader([line]))]
  rows.append({'timestamp':fields[0],'gpu':fields[1],'used_mib':float(fields[2]),'total_mib':float(fields[3]),'utilization_percent':float(fields[4])})
 (root/'adapter_memory.json').write_text(json.dumps({'scope':'whole GPU adapter including OS and other applications; not process-exclusive VRAM','samples':rows},indent=2),encoding='utf-8')
 time.sleep(1)
print(json.dumps({'samples':len(rows),'peak_adapter_mib':max(r['used_mib'] for r in rows)}))
