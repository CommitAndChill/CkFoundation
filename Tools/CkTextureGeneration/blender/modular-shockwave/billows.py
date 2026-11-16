"""Native Blender billboard composition of accepted CFD smoke; no new fluid bake."""
import bpy,sys,json,math,random,hashlib,time,argparse
from pathlib import Path
from mathutils import Vector

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
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=scene.render.resolution_y=512;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.image_settings.color_depth='16'
    scene.render.film_transparent=True;scene.render.dither_intensity=0;scene.eevee.taa_render_samples=8
    scene.view_settings.view_transform='Raw';scene.view_settings.look='None';scene.world.color=(0,0,0)
    scene.render.fps=16;scene.frame_start=1;scene.frame_end=32
    bpy.context.preferences.filepaths.save_version=0
    bpy.context.preferences.filepaths.file_preview_type='NONE'
    bpy.ops.object.camera_add(location=(0,0,20));scene.camera=bpy.context.object
    scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=20
    rng=random.Random(cfg['seed']);records=[];switches=[]
    # Uneven, overlapping outward lobes; every card has its own lifetime and curl.
    angles=[.12,.39,.72,1.16,1.40,1.87,2.15,2.44,2.81,3.02,3.51,3.85,4.21,4.48,4.78,5.17,5.43,5.91]
    for i,a in enumerate(angles):
        a+=rng.uniform(-.10,.10);birth=rng.uniform(.04,.17);speed=rng.uniform(3.1,4.8)*cfg['radius_scale']
        width=rng.uniform(6.0,9.0);height=rng.uniform(6.6,10.4);curl=rng.uniform(-.25,.25)
        alpha=rng.uniform(.95,1.65)*cfg['billow_weight'];phase=rng.randint(2,7)
        mat=bpy.data.materials.new(f'CFD_billows_{i:02d}');mat.use_nodes=True;t=mat.node_tree;t.nodes.clear()
        bpy.ops.mesh.primitive_plane_add(size=2);o=bpy.context.object;o.name=f'Perimeter_billows_{i:02d}';o.data.materials.append(mat)
        records.append(dict(index=i,angle=a,birth=birth,speed=speed,width=width,height=height,curl=curl,alpha=alpha,phase=phase))
        for frame in range(1,33):
            seconds=(frame-1)/16*cfg['motion_speed'];age=max(0,seconds-birth)
            progress=1-math.exp(-age*1.7);radius=.5+speed*progress
            theta=a+curl*progress
            o.location=(radius*math.cos(theta),radius*math.sin(theta),i*.002)
            grow=.20+.86*(1-math.exp(-age*3.3))
            o.scale=(width*.5*grow*(-1 if i%3==0 else 1),height*.5*grow,1)
            o.rotation_euler=(0,0,theta-math.pi/2+curl*age*.5)
            for prop in ('location','scale','rotation_euler'):o.keyframe_insert(data_path=prop,frame=frame)

    s=scene;s.eevee.taa_render_samples=16
    unlit=bpy.data.images.load(str(BASE/'inputs/unlit_atlas_4x4.png'),check_existing=False)
    lit=bpy.data.images.load(str(BASE/'inputs/lit_atlas_4x4.png'),check_existing=False)
    for im in (unlit,lit):im.colorspace_settings.name='Non-Color'
    switches=[];addressing=[]
    for record in records:
        mat=bpy.data.materials[f'CFD_billows_{record["index"]:02d}'];mat.surface_render_method='BLENDED'
        t=mat.node_tree;t.animation_data_clear();t.nodes.clear()
        uv=t.nodes.new('ShaderNodeTexCoord');scale=t.nodes.new('ShaderNodeVectorMath');scale.operation='SCALE'
        t.links.new(uv.outputs['UV'],scale.inputs[0]);scale.inputs[3].default_value=.25
        coords=[];opacity=[];colors=[]
        for label in ('A','B'):
            offset=t.nodes.new('ShaderNodeCombineXYZ');offset.name=f'Atlas_tile_origin_{label}'
            add=t.nodes.new('ShaderNodeVectorMath');add.operation='ADD'
            t.links.new(scale.outputs[0],add.inputs[0]);t.links.new(offset.outputs[0],add.inputs[1])
            coords.append(offset)
            for image,collection in ((unlit,opacity),(lit,colors)):
                tex=t.nodes.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Linear';tex.extension='CLIP'
                t.links.new(add.outputs[0],tex.inputs['Vector']);collection.append(tex)
        blend=t.nodes.new('ShaderNodeValue');blend.name='Continuous_source_frame_crossfade'
        a_mix=t.nodes.new('ShaderNodeMixRGB');a_mix.blend_type='MIX'
        t.links.new(blend.outputs[0],a_mix.inputs[0]);t.links.new(opacity[0].outputs['Alpha'],a_mix.inputs[1]);t.links.new(opacity[1].outputs['Alpha'],a_mix.inputs[2])
        color_mix=t.nodes.new('ShaderNodeMixRGB');color_mix.blend_type='MIX'
        t.links.new(blend.outputs[0],color_mix.inputs[0]);t.links.new(colors[0].outputs['Color'],color_mix.inputs[1]);t.links.new(colors[1].outputs['Color'],color_mix.inputs[2])
        strength=t.nodes.new('ShaderNodeMath');strength.operation='MULTIPLY';strength.name='One_lifetime_density_envelope'
        t.links.new(a_mix.outputs[0],strength.inputs[0])
        em=t.nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(1,1,1,1)
        trans=t.nodes.new('ShaderNodeBsdfTransparent');mix=t.nodes.new('ShaderNodeMixShader')
        t.links.new(strength.outputs[0],mix.inputs[0]);t.links.new(trans.outputs[0],mix.inputs[1]);t.links.new(em.outputs[0],mix.inputs[2])
        out=t.nodes.new('ShaderNodeOutputMaterial');t.links.new(mix.outputs[0],out.inputs['Surface'])
        switches.append((t,color_mix,em))
        frames=[]
        for frame in range(1,33):
            sec=(frame-1)/16*cfg['motion_speed'];age=max(0,sec-record['birth'])
            index=min(15.,record['phase']+age*8);lo=int(index);hi=min(15,lo+1);fraction=index-lo
            for tile,n in zip((lo,hi),coords):
                n.inputs[0].default_value=(tile%4)/4;n.inputs[1].default_value=(3-tile//4)/4
                n.inputs[0].keyframe_insert('default_value',frame=frame);n.inputs[1].keyframe_insert('default_value',frame=frame)
            blend.outputs[0].default_value=fraction;blend.outputs[0].keyframe_insert('default_value',frame=frame)
            # Decay begins while the front is still expanding. No delayed second emission.
            fade=min(1,age/.12)*math.exp(-max(0,sec-.68)*1.15)*max(0,min(1,(31/16-(frame-1)/16)/.38))
            strength.inputs[1].default_value=record['alpha']*fade;strength.inputs[1].keyframe_insert('default_value',frame=frame)
            frames.append(dict(frame=frame-1,source_a=lo,source_b=hi,crossfade=fraction,opacity_multiplier=record['alpha']*fade))
        addressing.append(dict(card=record['index'],frames=frames))
    scene['method']='Fresh seeded native radial transforms and blended accepted CFD imagery; no new radial fluid solve'
    scene['seed']=cfg['seed']
    scene.frame_set(13);scene.render.resolution_x=scene.render.resolution_y=512
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'source.blend'))
    started=time.time()
    for mode in ('opacity','lit'):
        (OUT/mode).mkdir()
        for t,col,em in switches:
            if mode=='lit':t.links.new(col.outputs[0],em.inputs['Color'])
        for f in range(1,33):
            scene.frame_set(f);scene.render.filepath=str(OUT/mode/f'frame_{f-1:02d}.png');bpy.ops.render.render(write_still=True)
        scene.frame_set(13);scene.render.resolution_x=scene.render.resolution_y=1024
        scene.render.filepath=str(OUT/f'static_{mode}_1024.png');bpy.ops.render.render(write_still=True)
        scene.render.resolution_x=scene.render.resolution_y=512
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'source_lit.blend'))
    (OUT/'receipt.json').write_text(json.dumps(dict(success=True,native=True,blender=bpy.app.version_string,seed=cfg['seed'],frames=32,fps=16,static_frame=12,frame_offset=1,ortho_scale=20,cards=records,addressing=addressing,seconds=time.time()-started,method=scene['method']),indent=2))
    print('FRESH_BILLOWS_COMPLETE',flush=True)
main()
