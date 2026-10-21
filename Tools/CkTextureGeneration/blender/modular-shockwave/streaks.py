"""Editable native Blender dusty artillery streaks. No external texture inputs."""
import bpy, math, random, json, hashlib, time, sys, argparse
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
from node_helpers import node,m,vec,split,noise
started=time.time()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
s=bpy.context.scene;s.render.engine='BLENDER_EEVEE'
s.render.resolution_x=s.render.resolution_y=512;s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA';s.render.image_settings.color_depth='8'
s.render.film_transparent=True;s.render.dither_intensity=0;s.eevee.taa_render_samples=16
s.view_settings.view_transform='Raw';s.view_settings.look='None';s.view_settings.exposure=0;s.view_settings.gamma=1
s.render.fps=16;s.frame_start=0;s.frame_end=31
bpy.ops.object.camera_add(location=(0,0,20));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=20;s.camera=cam
s.world.color=(0,0,0)
rng=random.Random(cfg['seed']+1);records=[]
# Clusters deliberately leave wide gaps and unequal mass around the impact.
clusters=[(.12,6),(.91,4),(2.18,7),(3.53,3),(4.28,5),(5.37,6)]
for ci,(heading,count) in enumerate(clusters):
    for j in range(count):
        i=len(records);angle=heading+rng.gauss(0,math.radians(cfg['streak_spread_degrees'])*(1.7 if ci==2 else 1))
        reach=rng.uniform(5.3,7.7)*cfg['radius_scale'];length=rng.uniform(2.8,5.2)*cfg['radius_scale'];width=rng.uniform(.15,.34)*cfg['streak_width_multiplier']
        mat=bpy.data.materials.new('Dust_jet_%02d'%i);mat.use_nodes=True
        mat.surface_render_method='BLENDED';t=mat.node_tree;t.nodes.clear()
        uv=node(t,'ShaderNodeTexCoord');x,y,z=split(t,uv.outputs['UV'])
        # Native anisotropic turbulence tears the long tapered envelope into filaments.
        n,c=noise(t,vec(t,m(t,'MULTIPLY',x,1.2),m(t,'MULTIPLY',y,2.3)),5.2,3,i*.731)
        nx,ny,nz=split(t,c)
        fine,_=noise(t,vec(t,m(t,'MULTIPLY',x,2.5),m(t,'MULTIPLY',y,4)),15,2,i+3)
        meander=m(t,'MULTIPLY',m(t,'SINE',m(t,'MULTIPLY_ADD',x,10,i*1.7)),.12)
        yy=m(t,'ADD',m(t,'ADD',m(t,'SUBTRACT',y,.5),meander),m(t,'MULTIPLY',m(t,'SUBTRACT',nx,.5),.35))
        taper=m(t,'POWER',m(t,'MAXIMUM',m(t,'SINE',m(t,'MULTIPLY',x,math.pi)),0),.65)
        # Width narrows toward the advancing head; both card ends fade to zero.
        taper=m(t,'MULTIPLY',taper,m(t,'MULTIPLY_ADD',x,-.55,1))
        lateral=m(t,'DIVIDE',m(t,'ABSOLUTE',yy),m(t,'ADD',m(t,'MULTIPLY',taper,.42),.001))
        shape=m(t,'POWER',m(t,'MAXIMUM',m(t,'SUBTRACT',1,lateral),0),1.4)
        ends=m(t,'MINIMUM',m(t,'MULTIPLY',x,9),m(t,'MULTIPLY',m(t,'SUBTRACT',1,x),12))
        ends=m(t,'MINIMUM',m(t,'MAXIMUM',ends,0),1)
        breakup=m(t,'POWER',m(t,'MAXIMUM',m(t,'MULTIPLY_ADD',n,5.5,-2.2),0),1.1)
        density=m(t,'MULTIPLY',m(t,'MULTIPLY',shape,ends),m(t,'MULTIPLY',breakup,m(t,'MULTIPLY_ADD',fine,1.0,.25)))
        gain=node(t,'ShaderNodeValue','Animated dust extinction');gain.outputs[0].default_value=0
        alpha=m(t,'MINIMUM',m(t,'MULTIPLY',density,gain.outputs[0]),1)
        tr=node(t,'ShaderNodeBsdfTransparent');em=node(t,'ShaderNodeEmission');em.inputs['Color'].default_value=(1,1,1,1)
        mix=node(t,'ShaderNodeMixShader');t.links.new(alpha,mix.inputs[0]);t.links.new(tr.outputs[0],mix.inputs[1]);t.links.new(em.outputs[0],mix.inputs[2])
        out=node(t,'ShaderNodeOutputMaterial');t.links.new(mix.outputs[0],out.inputs['Surface'])
        mesh=bpy.data.meshes.new('Tapered_jet_card');mesh.from_pydata([(0,-1,0),(1,-1,0),(1,1,0),(0,1,0)],[],[(0,1,2,3)])
        uvmap=mesh.uv_layers.new(name='UVMap')
        for p in mesh.polygons:
            for li,uvco in zip(p.loop_indices,[(0,0),(1,0),(1,1),(0,1)]):uvmap.data[li].uv=uvco
        ob=bpy.data.objects.new('Cluster_%02d_jet_%02d'%(ci,i),mesh);s.collection.objects.link(ob);mesh.materials.append(mat);ob.rotation_euler.z=angle
        gainbase=rng.uniform(.55,.95)
        for f in range(32):
            sec=f/16*cfg['motion_speed'];expansion=1-math.exp(-sec*9)
            head=.35+(reach-.35)*expansion;tail=max(.05,head-length*(1-math.exp(-sec*15)))
            opacity=(1-math.exp(-sec*26))*math.exp(-max(sec-.16,0)*5.5)*max(0,1-sec/.94)
            ob.location=(tail*math.cos(angle),tail*math.sin(angle),i*.002)
            ob.scale=(max(.001,head-tail),width*(.5+expansion*.8),1)
            ob.keyframe_insert('location',frame=f);ob.keyframe_insert('scale',frame=f)
            gain.outputs[0].default_value=opacity*gainbase;gain.outputs[0].keyframe_insert('default_value',frame=f)
        records.append(dict(index=i,cluster=ci,angle=angle,max_reach=reach,length=length,width=width))
s.frame_set(12);s.render.filepath=str(OUT/'frame_03.png')
bpy.context.preferences.filepaths.file_preview_type='NONE'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'source.blend'))
for f in range(32):
    s.frame_set(f);s.render.filepath=str(OUT/('frame_%02d.png'%f));bpy.ops.render.render(write_still=True)
s.frame_set(12);s.render.resolution_x=s.render.resolution_y=1024;s.render.filepath=str(OUT/'static_1024.png');bpy.ops.render.render(write_still=True)
files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('*.png')}
files['source.blend']=hashlib.sha256((OUT/'source.blend').read_bytes()).hexdigest()
(OUT/'receipt.json').write_text(json.dumps(dict(blender=bpy.app.version_string,native=True,renderer='BLENDER_EEVEE',seed=cfg['seed']+1,frames=32,fps=16,resolution=512,static_frame=12,static_resolution=1024,ortho_scale=20,camera=[0,0,20],duration_seconds=time.time()-started,external_images=0,jets=records,sha256=files),indent=2))
print('STREAKS_COMPLETE',OUT)
