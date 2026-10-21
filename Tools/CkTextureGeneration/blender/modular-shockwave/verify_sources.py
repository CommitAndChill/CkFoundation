"""Reopen owned sources and rerender selected early/late samples unchanged."""
from pathlib import Path
import bpy,sys,argparse,json
HERE=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--run',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);run=a.run.resolve()
if not a.run.is_absolute() or run.is_relative_to(HERE):
    raise ValueError('Absolute external run required')
out=run/'verification';out.mkdir(exist_ok=False)
records=[]
for name,source,offset in [('streaks','streaks/source.blend',0),('soil','soil/source.blend',0),('billows','billows/source.blend',1),('billows_lit','billows/source_lit.blend',1)]:
    bpy.ops.wm.open_mainfile(filepath=str(run/source));s=bpy.context.scene
    s.render.resolution_x=s.render.resolution_y=512;s.render.resolution_percentage=100
    for index in (12,27):
        s.frame_set(index+offset);s.render.filepath=str(out/f'{name}_{index:02d}.png');bpy.ops.render.render(write_still=True)
        records.append(dict(layer=name,index=index,source=source,source_frame=index+offset,output=s.render.filepath))
(out/'receipt.json').write_text(json.dumps(records,indent=2))
print('RECIPE_SOURCE_REPLAY_COMPLETE',flush=True)
