"""Summarize original engine CSV data; retain both reported and exclusive RT timings."""
import csv
import json
import statistics
from pathlib import Path

root = Path(__file__).resolve().parents[2]
summary = {'method': 'Each 8s CSV, omit first60/final3 rows. Means and P95 are per rendered frame, not per capture. '
           'RenderThreadTime near zero in this Editor setup; also sum exclusive RenderThread scopes excluding EventWait. '
           'Editor/Slate/Python overhead and pre-existing world rendering are included.', 'scenarios': {}}
for path in (root / 'Saved/Profiling/CSV').glob('CharacterPreview_*.csv'):
    if path.stat().st_size < 20: continue
    with path.open(encoding='utf-8-sig') as file:
        reader = csv.DictReader(file)
        columns = reader.fieldnames
        rows = list(reader)[60:-3]
    metrics = {}
    for key in ['FrameTime', 'GameThreadTime', 'RenderThreadTime', 'RHIThreadTime', 'GPUTime', 'RenderThreadExclusiveNoWait']:
        values = []
        for row in rows:
            try:
                value = sum(float(row[k]) for k in columns if k.startswith('Exclusive/RenderThread/') and k != 'Exclusive/RenderThread/EventWait') if key == 'RenderThreadExclusiveNoWait' else float(row[key])
                values.append(value)
            except (ValueError, KeyError, TypeError): pass
        if values:
            metrics[key] = {'mean_ms': statistics.mean(values), 'p95_ms': sorted(values)[int(.95*(len(values)-1))], 'frames': len(values)}
    summary['scenarios'][path.stem.removeprefix('CharacterPreview_')] = metrics
out = root / 'Saved/CharacterPreview/performance-summary.json'
out.write_text(json.dumps(summary, indent=2), encoding='utf8')
print(json.dumps(summary, indent=2))
