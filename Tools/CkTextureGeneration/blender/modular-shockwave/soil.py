"""Native chipped soil slabs: sparse readable chunks, no texture/noise/simulation."""
import bpy, math, random, json, hashlib, time, argparse, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parent
BASE=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
parser=argparse.ArgumentParser()
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--config',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
cfg=json.loads(args.config.read_text());OUT=args.out.resolve()
if not args.out.is_absolute() or OUT.is_relative_to(BASE) or OUT.exists():
    raise ValueError('Absolute new external output required; existing output preserved')
assert 0<cfg['seed']<2147483640
assert .8<=cfg['radius_scale']<=1.1
assert 3<=cfg['streak_spread_degrees']<=15
assert 1.2<=cfg['streak_width_multiplier']<=2.5
assert .5<=cfg['billow_weight']<=1.5
assert 5<=cfg['debris_count']<=35 and 0<=cfg['hero_debris_count']<=cfg['debris_count']
assert .8<=cfg['motion_speed']<=1.25
OUT.mkdir(parents=True)
out=OUT;started=time.time()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
s=bpy.context.scene;s.render.engine='BLENDER_EEVEE';s.render.film_transparent=True
s.view_settings.view_transform='Raw';s.view_settings.look='None';s.view_settings.exposure=0;s.view_settings.gamma=1;s.world.color=(0,0,0)
bpy.ops.object.camera_add(location=(0,0,20));s.camera=bpy.context.object;s.camera.data.type='ORTHO';s.camera.data.ortho_scale=20
rng=random.Random(cfg['seed']+2);records=[]
for i in range(cfg['debris_count']):
    hero=i<cfg['hero_debris_count'];radius=rng.uniform(.24,.31) if hero else rng.uniform(.10,.20)
    axis=rng.uniform(.8,1.25) if hero else rng.uniform(.6,1.8)
    count=rng.randint(5,7);verts=[]
    for k in range(count):
        a=math.tau*k/count+rng.uniform(-.16,.16);r=radius*rng.uniform(.72,1.15)
        verts.append((math.cos(a)*r*axis,math.sin(a)*r,radius*rng.uniform(.14,.30)))
    verts += [(x*.84,y*.84,-radius*rng.uniform(.15,.3)) for x,y,z in verts]
    # Off-center top vertex creates broad uneven chipped facets rather than spheres.
    verts.append((radius*.15,-radius*.13,radius*.35));center=2*count
    faces=[(center,k,(k+1)%count) for k in range(count)]
    faces += [(k,k+count,(k+1)%count+count,(k+1)%count) for k in range(count)]
    faces.append(tuple(range(count,2*count)))
    mesh=bpy.data.meshes.new('Chipped_slab_%02d'%i);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new('Mud_chunk_%02d%s'%(i,'_hero' if hero else ''),mesh);s.collection.objects.link(o)
    mats=[]
    for j in range(5):
        mat=bpy.data.materials.new('Chunk_%02d_facet_%d'%(i,j));mat.use_nodes=True;mat.surface_render_method='BLENDED'
        t=mat.node_tree;t.nodes.clear();em=t.nodes.new('ShaderNodeEmission');grey=rng.uniform(.22,.48);em.inputs['Color'].default_value=(grey,grey,grey,1)
        tr=t.nodes.new('ShaderNodeBsdfTransparent');mix=t.nodes.new('ShaderNodeMixShader');outnode=t.nodes.new('ShaderNodeOutputMaterial')
        t.links.new(tr.outputs[0],mix.inputs[1]);t.links.new(em.outputs[0],mix.inputs[2]);t.links.new(mix.outputs[0],outnode.inputs['Surface']);mats.append((mat,mix));mesh.materials.append(mat)
    for face in mesh.polygons:face.material_index=rng.randrange(5)
    angle=rng.choice([.12,.91,2.18,3.53,4.28,5.37])+rng.gauss(0,.22);reach=rng.uniform(2,6.8)*cfg['radius_scale']
    spin=[rng.uniform(-.8,.8),rng.uniform(-.8,.8),rng.uniform(-5,5)]
    initial=rng.uniform(0,math.tau);offset=rng.uniform(-.25,.25);height=rng.uniform(.6,1.2);flight=rng.uniform(.65,.95)
    fade_end=rng.uniform(1.25,1.6);fade_start=rng.uniform(.9,1.05)
    for f in range(32):
        sec=f/16*cfg['motion_speed'];travel=1-math.exp(-sec*4);r=reach*travel;side=offset*math.sin(sec*2)
        o.location=(r*math.cos(angle)-side*math.sin(angle),r*math.sin(angle)+side*math.cos(angle),height*math.sin(min(sec/flight,1)*math.pi))
        o.rotation_euler=(spin[0]*min(sec,flight),spin[1]*min(sec,flight),initial+spin[2]*min(sec,flight))
        alpha=0 if f==0 else max(0,min(1,(fade_end-f/16)/(fade_end-fade_start)))
        o.keyframe_insert('location',frame=f);o.keyframe_insert('rotation_euler',frame=f)
        for mat,mix in mats:mix.inputs[0].default_value=alpha;mix.inputs[0].keyframe_insert('default_value',frame=f)
    records.append(dict(name=o.name,hero=hero,radius=radius,elongation=axis,angle=angle,reach=reach,flight=flight,height=height,fade_start=fade_start,fade_end=fade_end))
s.eevee.taa_render_samples=32;s.render.resolution_x=s.render.resolution_y=512;s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.image_settings.color_depth='8';s.render.dither_intensity=0
s.render.fps=16;s.frame_start=0;s.frame_end=31;s.frame_set(12)
bpy.context.preferences.filepaths.file_preview_type='NONE';bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(out/'source.blend'))
for f in range(32):
    s.frame_set(f);s.render.filepath=str(out/('frame_%02d.png'%f));bpy.ops.render.render(write_still=True)
s.frame_set(12);s.render.resolution_x=s.render.resolution_y=1024;s.render.filepath=str(out/'static_1024.png');bpy.ops.render.render(write_still=True)
files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.png')};files['source.blend']=hashlib.sha256((out/'source.blend').read_bytes()).hexdigest()
(out/'receipt.json').write_text(json.dumps(dict(native=True,blender=bpy.app.version_string,seed=cfg['seed']+2,transparency='BLENDED',frames=32,fps=16,resolution=512,static_frame=12,ortho_scale=20,camera=[0,0,20],records=records,sha256=files,seconds=time.time()-started),indent=2))
print('CHUNKY_CLODS_COMPLETE')
