"""Reusable fresh GPU smoke simulation; all writes stay in a new run directory."""
import json,sys,argparse
sys.dont_write_bytecode=True
sys.path.insert(0,str(__import__("pathlib").Path(__file__).resolve().parent))
from preflight import validate_config, validate_destination
from select_gpu import select_gpu_slot
import bpy,addon_utils,importlib,math,time,hashlib
from pathlib import Path
from mathutils import Vector
parser=argparse.ArgumentParser()
parser.add_argument('--config',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--gpu',required=True,help='Case-insensitive OpenCL device-name substring; exactly one GPU must match')
parser.add_argument('--platform-index',type=int,help='Explicit OpenCL platform index for duplicate device names')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
recipe=validate_config(args.config)
P=validate_destination(args.out); ROOT=Path(__file__).resolve().parent
O=P/'output'
assert 1 <= recipe['seed'] < 2147483647
assert .04 <= recipe['cell_size'] <= .15
assert 0 <= recipe['turbulence_amplitude'] <= 40
assert .5 <= recipe['source_width'] <= 1.5
assert .5 <= recipe['upward_speed'] <= 1.5
N='bl_ext.user_default.continuum_flow'
assert addon_utils.enable(N,default_set=False,persistent=False) is not None
ec=importlib.import_module(N+'.Internal.Core.export.export_config')
ap=importlib.import_module(N+'.Internal.UI.animation_proxy')
ss=importlib.import_module(N+'.Internal.Core.solver.solver_status')
runner=importlib.import_module(N+'.Solver.General.main')
kernel=importlib.import_module(N+'.Solver.Kernel.solver')
import pyopencl as cl
SEED=recipe['seed']
s=bpy.context.scene;s.render.fps=24;s.render.fps_base=1;s.frame_start=1;s.frame_end=64
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
for group in list(bpy.data.node_groups):
    if group.bl_idname=='CONTINUUM_FLOW_NODE_TREE':bpy.data.node_groups.remove(group)
tree=bpy.data.node_groups.new('Smoke 02 - editable GPU gas recipe','CONTINUUM_FLOW_NODE_TREE')
def make(kind,name=None):
    n=tree.nodes.new('CONTINUUM_FLOW_'+kind+'_NODE')
    if name:n.name=name;n.label=name
    return n
def link(a,ao,b,bi):tree.links.new(a.outputs[ao],b.inputs[bi])
def prop(n,key,value,frame=None):
    meta=n.bl_rna.properties[key]
    if meta.type=='FLOAT':value=tuple(float(x) for x in value) if getattr(meta,'is_array',False) else float(value)
    proxy=ap.ensure_scene_proxy(s,n,key);s[proxy]=value;setattr(n,key,value)
    if frame is not None:s.keyframe_insert(data_path=f'["{proxy}"]',frame=frame)
domain=make('DOMAIN');physics=make('PHYSICS');sim=make('SIMULATION');out=make('OUTPUT')
domain.lx=12;domain.ly=10;domain.lz=14;domain.resolution=recipe['cell_size']
link(domain,'Domain',sim,'Domain');link(physics,'Physics',sim,'Physics');link(sim,'Result',out,'Result')
sim.start_frame=1;sim.end_frame=65;sim.solver_backend='GPU';sim.cfl=1.5;sim.advection_substeps=2;sim.simulate_sparsely=False
devices=ss.refresh_opencl_devices()['GPU']
gpu_index=select_gpu_slot(devices,args.gpu,args.platform_index)
expected_descriptor=devices[gpu_index]
expected_gpu_name=devices[gpu_index]['device_name']
assert gpu_index < ss.MAX_SELECTABLE_DEVICES, 'Matched GPU exceeds add-on selectable device count'
P.mkdir(parents=True,exist_ok=False); O.mkdir()
(P/'recipe.json').write_text(json.dumps(recipe,indent=2))
sim.gpu_devices=tuple(i==gpu_index for i in range(ss.MAX_SELECTABLE_DEVICES))
out.output_path=str(P/'cache');out.fps=24;out.export_smoke=True;out.export_flame=False;out.export_t=False;out.output_precision='float16'
for key,value in {'temperature_dissipation':8,'temperature_production_rate':0,'buoyancy':20,'expansion_rate':0,'smoke_dissipation':2,'smoke_production_rate':0,'fuel_dissipation':0,'fuel_burn_rate':0,'vorticity':70}.items():prop(physics,key,value)
for f,val in [(1,2),(40,2),(56,12),(64,45)]:prop(physics,'smoke_dissipation',val,f)
src=make('SOURCE','Warm smoke pulse - no fuel or combustion');link(src,'Source',sim,'Source')
src.randomness_scale=.28;src.randomness_seed=SEED;src.smoke_randomness=55;src.temperature_randomness=12;src.velocity_randomness=35
prop(src,'temperature',550);prop(src,'fuel',0);prop(src,'extra_pressure',0)
for f,val in [(1,16),(4,28),(8,15),(12,27),(15,16),(17,0)]:prop(src,'smoke',val,f)
for f,vel in [(1,(.8,.2,10.0)),(6,(-.9,-.25,8.5)),(11,(.65,.35,10.5)),(15,(-.35,0,8.0)),(17,(0,0,0))]:prop(src,'velocity',(vel[0],vel[1],vel[2]*recipe['upward_speed']),f)
# A connected, irregular source mouth supplies successive dense folds. It stops
# locally instead of teleporting or resetting the advected gas to cold air.
for i,(loc,radius) in enumerate([((-.34,0,2.0),.57),((.30,.10,2.04),.53),((0,-.12,2.18),.60)]):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=radius*recipe['source_width'],location=loc)
    obj=bpy.context.object;obj.name=f'Smoke_source_{i}';obj.hide_render=True
    tex=bpy.data.textures.new(obj.name+'_irregularity',type='CLOUDS');tex.noise_scale=.25;tex.noise_depth=2
    mod=obj.modifiers.new('Irregular emitting mouth','DISPLACE');mod.texture=tex;mod.strength=.16
    obj.keyframe_insert('scale',frame=1);obj.keyframe_insert('scale',frame=17)
    obj.scale=(.001,.001,.001);obj.keyframe_insert('scale',frame=18)
    geo=make('GEOMETRY',obj.name+' geometry');geo.source_object=obj;link(geo,'Geometry',src,'Geometry')
turb=make('FORCE_TURBULENCE');turb.scale=.55;turb.seed=SEED;turb.frequency=1.3;prop(turb,'amplitude',recipe['turbulence_amplitude']);link(turb,'Force',sim,'Forces')
# Native same-direction circulation avoids the previous pair's reinforced
# central updraft. Offset centers provide overlapping rolling regions.
for i,(origin,axis,radius,strength) in enumerate([((-.8,0,3.5),(0,-1,0),2.5,8),((.9,.3,4.0),(0,-1,0),2.2,6)]):
    swirl=make('FORCE_SWIRL',f'Localized air vortex {i}')
    prop(swirl,'axis',axis);prop(swirl,'radius',radius)
    for frame,height in [(1,3.5),(20,3.5),(35,5.4),(47,6.8),(64,6.8)]:
        prop(swirl,'origin',(origin[0],origin[1],height+.5*i),frame)
    for frame,factor in [(1,0),(17,0),(20,1),(35,1),(44,.5),(56,0),(64,0)]:prop(swirl,'strength',strength*factor,frame)
    link(swirl,'Force',sim,'Forces')
for action in bpy.data.actions:
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'
s.frame_set(1)
cfg=ec.build_config_dict(bpy.context,sim)
bake,cfg=ec.export_config_dict(cfg)
assert Path(bake).resolve().is_relative_to(P/'cache')
cfg['bake_directory']=str(bake);cfg['simulation']['preview']['enabled']=False
(O/'simulation_config.json').write_text(json.dumps(cfg,indent=2))
selected=[];original=kernel.select_opencl_device
def verify_gpu(settings):
    exported=settings.get('compute_devices') or []
    assert len(exported)==1 and exported[0]==expected_descriptor, exported
    d=original(settings)
    expected_platform=cl.get_platforms()[expected_descriptor['platform_index']]
    expected_device=expected_platform.get_devices(device_type=cl.device_type.GPU)[expected_descriptor['device_index']]
    assert d==expected_device and d.platform==expected_platform, 'Solver selected a different OpenCL device'
    assert d.type & cl.device_type.GPU and d.name==expected_gpu_name,(d.name,expected_gpu_name)
    board=d.name
    if 'AMD' in d.vendor.upper() or 'ADVANCED MICRO DEVICES' in d.vendor.upper():
        board=d.get_info(0x4038)
    record={'name':d.name,'board':board,'vendor':d.vendor,'type':int(d.type),'driver':d.driver_version,'memory':int(d.global_mem_size),'selected_descriptor':expected_descriptor,'flattened_gpu_slot':gpu_index}
    selected.append(record);print('ACTUAL_SMOKE_GPU',json.dumps(record),flush=True);return d
kernel.select_opencl_device=verify_gpu
runner.set_message_callback(lambda msg:print('BAKE_PROGRESS',json.dumps(msg),flush=True) if msg.get('type')=='stats' and int(msg.get('frame',0))%16==0 else None)
start=time.perf_counter();runner.main(cfg);seconds=time.perf_counter()-start
files=sorted(bake.glob('frame_*.vdb'));assert selected and len(files)>=65
out.last_bake_directory=str(bake)
(O/'cache_manifest.json').write_text(json.dumps({'files':[{'path':f.relative_to(P).as_posix(),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in files]},indent=2))
settings={'continuum':{'version':'0.4.0','solver_backend':'GPU','actual_solver_device':selected[0]},'frames_baked':64,'cache_samples':len(files),'bake_directory':bake.relative_to(P).as_posix(),'bake_seconds':seconds,'fps':24,'preview_frames':[8,20,36,56],'seed':SEED,'cell_size':recipe['cell_size'],'domain':[12,10,14],'method':'Pure advected smoke, warm pulsed upward emission, buoyancy, vorticity, noise force and localized overlapping same-direction air vortices; no fuel, flame, extra pressure or static smoke image. Restrained 3D shader detail modulates actual density.','limitations':['Ported recipe; historical acceptance does not prove this run visually accepted.','GPU and Blender/add-on versions recorded per run; cross-device repeatability unverified.']}
(O/'settings.json').write_text(json.dumps(settings,indent=2))
v=bpy.data.volumes.new('Simulated smoke density');v.filepath=str(files[0]);v.is_sequence=True;v.frame_start=1;v.frame_duration=64;v.frame_offset=1
vo=bpy.data.objects.new('Smoke_CFD',v);s.collection.objects.link(vo)
s.frame_set(20);bpy.context.view_layer.update();v.grids.load();assert 'density' in {g.name for g in v.grids}
mat=bpy.data.materials.new('Neutral smoke - actual advected density');mat.use_nodes=True;t=mat.node_tree;t.nodes.clear()
def mathnode(op,a,b):
    n=t.nodes.new('ShaderNodeMath');n.operation=op
    for i,x in enumerate([a,b]):
        if isinstance(x,(int,float)):n.inputs[i].default_value=x
        else:t.links.new(x,n.inputs[i])
    return n.outputs[0]
d=t.nodes.new('ShaderNodeAttribute');d.attribute_name='density'
coords=t.nodes.new('ShaderNodeTexCoord');noise=t.nodes.new('ShaderNodeTexNoise');noise.noise_dimensions='4D';noise.inputs['Scale'].default_value=3.2;noise.inputs['Detail'].default_value=3;noise.inputs['Roughness'].default_value=.65;t.links.new(coords.outputs['Object'],noise.inputs['Vector'])
for f,w in [(1,0),(64,.5)]:noise.inputs['W'].default_value=w;noise.inputs['W'].keyframe_insert('default_value',frame=f)
# Small extinction variation, not aggressive carving; CFD controls the silhouette.
detail=mathnode('ADD',mathnode('MULTIPLY',noise.outputs['Fac'],1.2),.35)
density=mathnode('MULTIPLY',mathnode('MULTIPLY',d.outputs['Fac'],detail),2.3)
p=t.nodes.new('ShaderNodeVolumePrincipled');p.inputs['Color'].default_value=(.24,.24,.24,1);p.inputs['Anisotropy'].default_value=.12;p.inputs['Emission Strength'].default_value=0
t.links.new(density,p.inputs['Density']);outmat=t.nodes.new('ShaderNodeOutputMaterial');t.links.new(p.outputs['Volume'],outmat.inputs['Volume']);v.materials.append(mat)
bpy.ops.object.camera_add(location=(0,-24,8));cam=bpy.context.object;cam.name='Fixed_Smoke_Camera';cam.rotation_euler=(Vector((0,0,5.4))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=12.4;s.camera=cam
for name,loc,power,size in [('Key',(-5,-6,10),3500,6),('Rim',(5,3,9),4500,5),('Fill',(3,-5,4),600,6)]:
    bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.name=name;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((0,0,5))-l.location).to_track_quat('-Z','Y').to_euler()
s.world.color=(.06,.06,.06);s.render.engine='BLENDER_EEVEE';s.eevee.volumetric_tile_size='2';s.eevee.volumetric_samples=256;s.eevee.use_volumetric_shadows=True;s.eevee.volumetric_shadow_samples=32;s.eevee.use_volume_custom_range=True;s.eevee.volumetric_start=17;s.eevee.volumetric_end=31;s.eevee.taa_render_samples=64
s.render.resolution_x=s.render.resolution_y=256;s.render.resolution_percentage=100;s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.film_transparent=True;s.view_settings.view_transform='AgX'
s.frame_set(20);bpy.context.preferences.filepaths.save_version=0;bpy.context.preferences.filepaths.file_preview_type='NONE'
v.filepath=bpy.path.relpath(str(files[0]),start=str(O));bpy.ops.wm.save_as_mainfile(filepath=str(O/'candidate.blend'),relative_remap=False)
(P/'recipe_source.py').write_bytes(Path(__file__).read_bytes())
print('SMOKE_GPU_BAKE_PASS',len(files),seconds,flush=True)
