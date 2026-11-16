"""Render an isolated fresh recipe cache without rerunning the solver."""
import bpy,addon_utils,json,sys,argparse,time,hashlib
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from preflight import validate_destination

N='bl_ext.user_default.continuum_flow'
ROOT=Path(__file__).resolve().parent
EXISTING=None

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(out,mode,selected,extinction=2.3,skip_static=False):
    out=validate_destination(out)
    out.mkdir(parents=True,exist_ok=False)
    module=addon_utils.enable(N,default_set=False,persistent=False)
    assert module is not None
    source=EXISTING/'output/candidate.blend';settings=json.loads((EXISTING/'output/settings.json').read_text())
    cache=(EXISTING/settings['bake_directory']).resolve()
    assert cache.is_relative_to(EXISTING/'cache') and (cache/'frame_000001.vdb').is_file()
    before={'source':digest(source),'settings':digest(EXISTING/'output/settings.json'),
            'cache_manifest':digest(EXISTING/'output/cache_manifest.json')}
    bpy.ops.wm.open_mainfile(filepath=str(source))
    s=bpy.context.scene
    volumes=[o for o in s.objects if o.type=='VOLUME'];assert len(volumes)==1
    vol=volumes[0];vol.data.filepath=str(cache/'frame_000001.vdb')
    assert vol.data.frame_offset==1 and vol.data.frame_duration==64
    # Editable copied graph must not retain an output/free-cache destination in
    # the accepted experiment. Reading the VDB sequence is the sole dependency.
    for group in bpy.data.node_groups:
        if group.bl_idname=='CONTINUUM_FLOW_NODE_TREE':
            for n in group.nodes:
                if n.bl_idname=='CONTINUUM_FLOW_OUTPUT_NODE':
                    n.output_path='//future_rebakes';n.last_bake_directory=''
    s.render.engine='BLENDER_EEVEE';s.render.film_transparent=True
    s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.image_settings.color_depth='16'
    s.render.resolution_percentage=100;s.render.fps=24;s.render.fps_base=1
    bpy.context.preferences.filepaths.save_version=0;bpy.context.preferences.filepaths.file_preview_type='NONE'
    mat=vol.data.materials[0];tree=mat.node_tree
    principled=next(n for n in tree.nodes if n.type=='PRINCIPLED_VOLUME')
    density_node=principled.inputs['Density'].links[0].from_node
    assert density_node.type=='MATH' and density_node.operation=='MULTIPLY'
    density_node.inputs[1].default_value=extinction
    density_node.label='Editable projected extinction scale'
    if mode=='opacity':
        # Extinction-only data master. No lights affect alpha; raw RGB is black.
        principled.inputs['Color'].default_value=(0,0,0,1)
        principled.inputs['Emission Strength'].default_value=0
        for o in s.objects:
            if o.type=='LIGHT':o.hide_render=True
        s.view_settings.view_transform='Raw';s.world.color=(0,0,0)
    # Lit-neutral variant retains the reviewed existing material/rig; its shading
    # is explicitly distinguished from the lighting-free opacity texture.
    s.frame_set(36);s.render.resolution_x=s.render.resolution_y=256
    vol.data.filepath=bpy.path.relpath(str(cache/'frame_000001.vdb'),start=str(out))
    bpy.ops.wm.save_as_mainfile(filepath=str(out/'source.blend'),relative_remap=False)
    (out/'frames').mkdir();renders=[];start=time.perf_counter()
    for index,frame in enumerate(selected):
        s.frame_set(frame);s.render.resolution_x=s.render.resolution_y=256
        s.render.filepath=str(out/'frames'/f'frame_{index:02d}.png')
        t=time.perf_counter();bpy.ops.render.render(write_still=True)
        renders.append({'index':index,'source_frame':frame,'source_time_seconds':(frame-1)/24,'seconds':time.perf_counter()-t})
        print('CFD_RENDER',mode,index,frame,flush=True)
    for frame in (() if skip_static else (20,36,47)):
        s.frame_set(frame);s.render.resolution_x=s.render.resolution_y=1024
        s.render.filepath=str(out/f'static_{frame:02d}.png');bpy.ops.render.render(write_still=True)
    after={'source':digest(source),'settings':digest(EXISTING/'output/settings.json'),
           'cache_manifest':digest(EXISTING/'output/cache_manifest.json')}
    assert before==after,'Existing accepted source changed during read-only render'
    receipt={'success':True,'blender':bpy.app.version_string,'mode':mode,'extinction':extinction,'source':str(source),'source_cache':str(cache),
             'source_settings':settings,'original_file_hashes':before,'originals_unchanged':True,'selected_source_frames':selected,
             'output_fps':8,'duration_seconds':2,'time_mapping':'Every third24fps source frame; contiguous2second window, no independent reseeding or arbitrary retiming.',
             'renders':renders,'render_seconds':time.perf_counter()-start,'seed':settings['seed'],
             'method':'Read-only existing Continuum Flow0.4 OpenCL advected density; freshly rendered in Blender. Fresh simulation receipt is stored with the run; rendering does not rebake it.',
             'limitations':['One-shot2second window ends with residual smoke; not a seamless loop or complete decay.','Lit variant retains baked neutral lighting; opacity variant contains no authored lighting.','Fresh seeded variation; artistic acceptance remains pending.']}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2))
    (out/'render_source.py').write_bytes(Path(__file__).read_bytes())
    print('CONTINUUM_RENDER_PASS',mode,flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--mode',choices=['lit','opacity'],default='lit')
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--start',type=int,default=8)
    p.add_argument('--extinction',type=float,default=2.3)
    p.add_argument('--skip-static',action='store_true')
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);EXISTING=a.run.resolve();frames=[a.start+3*i for i in range(16)]
    assert 1<=frames[0] and frames[-1]<=64
    assert .1<=a.extinction<=8
    run(a.out,a.mode,frames,a.extinction,a.skip_static)
