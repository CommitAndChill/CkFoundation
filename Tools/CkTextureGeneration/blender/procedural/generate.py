"""Native Blender shader texture renderer. Run with Blender --background --python.

No external Python modules; all texture signals are evaluated by Shader Nodes.
"""
import bpy, math, json, sys, time, argparse
from pathlib import Path
P=argparse.ArgumentParser(); P.add_argument('--spec',required=True); P.add_argument('--output',required=True)
args=P.parse_args(sys.argv[sys.argv.index('--')+1:]); spec=json.loads(Path(args.spec).read_text())
density_curve=spec.setdefault('density_curve','perceptual')
if density_curve not in ('perceptual','linear'): raise ValueError('density_curve must be perceptual or linear')
def shape_opacity(value):
 # Explicit artistic tone shaping, not physical density or a color-space read.
 value=min(1.0,max(0.0,value))
 if density_curve=='linear': return value
 return value/12.92 if value<=.04045 else ((value+.055)/1.055)**2.4
opacity_policy={'read_color_space':'Non-Color','density_curve':density_curve,'formula':'a/12.92 for a<=0.04045, else ((a+0.055)/1.055)^2.4' if density_curve=='perceptual' else 'identity','meaning':'deliberate artistic opacity tone shaping; not physical density'}
out=Path(args.output).resolve()
tool_root=Path(__file__).resolve().parents[2]
if out.is_relative_to(tool_root.parents[1]): raise ValueError('Output must be outside the plugin source tree')
if out.exists(): raise ValueError('Existing output preserved; choose a new directory')
if spec.get('family') not in ('erosion','smoke2d','smoke3d','shockwave','flipbook','metal','flow','normal_probe'): raise ValueError('Unsupported family')
if type(spec.get('resolution',1024)) is not int or not 1 <= spec.get('resolution',1024) <= 8192: raise ValueError('Invalid resolution')
if not isinstance(spec.get('seeds',[11]),list) or not spec.get('seeds',[11]) or any(type(s) is not int for s in spec.get('seeds',[11])): raise ValueError('Invalid seeds')
if type(spec.get('frames',1)) is not int or not 1 <= spec.get('frames',1) <= 1024: raise ValueError('Invalid frame count')
spec['output_dir']=str(out)
out.mkdir(parents=True,exist_ok=False)
family=spec['family']; res=spec.get('resolution',1024); seeds=spec.get('seeds',[11])
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=1; scene.cycles.use_denoising=False
scene.render.threads_mode='FIXED'; scene.render.threads=6
scene.render.resolution_x=res; scene.render.resolution_y=res; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGBA'; scene.render.image_settings.color_depth='16'
scene.render.dither_intensity=0; scene.render.film_transparent=True; scene.world.color=(0,0,0)
scene.view_settings.view_transform='Standard'; scene.view_settings.look='None'; scene.view_settings.exposure=0; scene.view_settings.gamma=1
bpy.ops.object.camera_add(location=(0,0,3)); cam=bpy.context.object; cam.data.type='ORTHO'; cam.data.ortho_scale=2; scene.camera=cam
bpy.ops.mesh.primitive_plane_add(size=2); plane=bpy.context.object; plane.name='TextureCard'
if family in ('flow','metal','erosion'):
 # Geometry extends past the camera crop. Expanded UVs preserve the camera's
 # exact 0..1 domain while periodic texels continue beyond each frame edge.
 plane.scale=(1.02,1.02,1)
 for corner in plane.data.uv_layers.active.data:
  corner.uv=((corner.uv.x-.5)*1.02+.5,(corner.uv.y-.5)*1.02+.5)
mat=bpy.data.materials.new('CK_'+family); mat.use_nodes=True; plane.data.materials.append(mat)
nt=mat.node_tree; nt.nodes.clear()
def node(kind,name=None):
 n=nt.nodes.new(kind)
 if name: n.label=name; n.name=name
 return n
def link(a,b): nt.links.new(a,b)
def mathn(op,a,b=None):
 n=node('ShaderNodeMath'); n.operation=op
 for i,x in enumerate([a,b]):
  if x is None: continue
  if isinstance(x,(int,float)): n.inputs[i].default_value=x
  else: link(x,n.inputs[i])
 return n.outputs[0]
def add(a,b): return mathn('ADD',a,b)
def mul(a,b): return mathn('MULTIPLY',a,b)
def sub(a,b): return mathn('SUBTRACT',a,b)
def clamp(a): return mathn('MINIMUM',mathn('MAXIMUM',a,0),1)
def ramp(a,lo,hi): return clamp(mathn('DIVIDE',sub(a,lo),hi-lo))
def combine(x,y,z):
 n=node('ShaderNodeCombineXYZ')
 for i,v in enumerate([x,y,z]):
  if isinstance(v,(int,float)): n.inputs[i].default_value=v
  else: link(v,n.inputs[i])
 return n.outputs[0]
uv=node('ShaderNodeTexCoord'); sep=node('ShaderNodeSeparateXYZ'); link(uv.outputs['UV'],sep.inputs[0]); u,v=sep.outputs['X'],sep.outputs['Y']
seed=node('ShaderNodeValue','Seed'); seed.outputs[0].default_value=seeds[0]
clock=node('ShaderNodeValue','Evolution'); clock.outputs[0].default_value=0
def noise(vec,scale=5,detail=4,w=None):
 n=node('ShaderNodeTexNoise'); n.noise_dimensions='4D'; n.inputs['Scale'].default_value=scale; n.inputs['Detail'].default_value=detail; n.inputs['Roughness'].default_value=.7
 link(vec,n.inputs['Vector']); link(w or seed.outputs[0],n.inputs['W']); return n.outputs['Fac']
def periodic(scale,offset=0,anisotropy=1):
 # 4D hypertorus embedding guarantees matching opposite edges analytically.
 a=mul(u,2*math.pi); b=mul(v,2*math.pi)
 xyz=combine(mul(mathn('COSINE',a),scale*anisotropy),mul(mathn('SINE',a),scale*anisotropy),mul(mathn('COSINE',b),scale))
 return noise(xyz,1,5,add(mul(mathn('SINE',b),scale),add(seed.outputs[0],offset)))
def ellipse(x,y,sx,sy): return mathn('SQRT',add(mul(mathn('DIVIDE',x,sx),mathn('DIVIDE',x,sx)),mul(mathn('DIVIDE',y,sy),mathn('DIVIDE',y,sy))))
x=sub(u,.5); y=sub(v,.5)
output=node('ShaderNodeOutputMaterial'); emission=node('ShaderNodeEmission'); emission.inputs['Strength'].default_value=1; link(emission.outputs[0],output.inputs['Surface'])
signals={}; alpha=False
if family=='alpha_probe':
 constant=node('ShaderNodeValue','Half density diagnostic'); constant.outputs[0].default_value=.5; signals['smoke']=constant.outputs[0]; alpha=True
elif family=='erosion':
 a=periodic(2.2); b=periodic(8.5,2.3); signals['erosion']=ramp(add(mul(a,.78),mul(b,.22)),.29,.7)
elif family in ('smoke2d','flipbook') and spec.get('smoke_style')=='billowy':
 # Add connected density volumes rather than thresholding stretched surface noise.
 progress=clock.outputs[0]
 def softnoise(vec,scale,w):
  n=node('ShaderNodeTexNoise'); n.noise_dimensions='4D'
  n.inputs['Scale'].default_value=scale; n.inputs['Detail'].default_value=1.7; n.inputs['Roughness'].default_value=.45
  link(vec,n.inputs['Vector']); link(w,n.inputs['W']); return n.outputs['Fac']
 p=combine(x,y,mul(progress,.1)); phase=mul(seed.outputs[0],.731)
 qx=add(x,mul(sub(softnoise(p,3.6,seed.outputs[0]),.5),.14))
 qy=add(sub(y,mul(progress,.08)),mul(sub(softnoise(p,3.6,add(seed.outputs[0],19.7)),.5),.14))
 qx=add(qx,mul(sub(softnoise(p,8,add(seed.outputs[0],31.2)),.5),.06))
 qy=add(qy,mul(sub(softnoise(p,8,add(seed.outputs[0],47.8)),.5),.06))
 curl=add(mul(mathn('SINE',add(mul(qy,10),phase)),.047),mul(qy,.13))
 def gaussian(dx,dy,sx,sy):
  return mathn('EXPONENT',mul(add(mathn('POWER',mathn('DIVIDE',dx,sx),2),mathn('POWER',mathn('DIVIDE',dy,sy),2)),-.5))
 density=mul(gaussian(sub(qx,curl),sub(qy,-.03),.041,.235),.17)
 for i,(cy,sx,sy,weight) in enumerate([(-.24,.055,.09,.34),(-.14,.08,.11,.48),(-.035,.115,.12,.55),(.08,.10,.13,.48),(.19,.075,.105,.34)]):
  shift=mul(mathn('SINE',add(phase,i*2.1)),.045)
  center=add(add(mul(mathn('SINE',add(phase,cy*10)),.047),cy*.13),shift)
  density=add(density,mul(gaussian(sub(qx,center),sub(qy,cy),sx,sy),weight))
 # Sparse long arcs grow from opposite shoulders and fade before their tips.
 for side,base in [(-1,-.035),(1,.07)]:
  t=mathn('DIVIDE',sub(qy,base),.28)
  arc=add(curl,mul(mathn('SINE',mul(clamp(t),2.2)),side*.18))
  width=add(.009,mul(sub(1,clamp(t)),.014))
  ribbon=mathn('EXPONENT',mul(mathn('POWER',mathn('DIVIDE',sub(qx,arc),width),2),-.5))
  taper=mul(ramp(t,0,.18),sub(1,ramp(t,.65,1)))
  density=add(density,mul(mul(ribbon,taper),.19))
 cloud=ramp(softnoise(combine(qx,qy,progress),10,add(seed.outputs[0],7.3)),.34,.66)
 cloud=mul(mul(cloud,cloud),sub(3,mul(cloud,2)))
 modulation=add(.45,mul(cloud,1.1))
 density=mul(density,modulation)
 edge=mul(sub(1,ramp(mathn('ABSOLUTE',x),.36,.48)),sub(1,ramp(mathn('ABSOLUTE',y),.36,.48)))
 signals['smoke']=mul(sub(1,mathn('EXPONENT',mul(density,-spec.get('density_gain',1.0)))),edge); alpha=True
elif family in ('smoke2d','flipbook'):
 # Domain-warped elongated billows with finer branching filaments.
 progress=clock.outputs[0]; lift=sub(y,mul(progress,.11))
 broad=noise(combine(mul(x,2.7),mul(lift,1.35),mul(progress,.16)),3.1,5)
 warp=mul(sub(broad,.5),.24)
 qx=add(x,warp); radial=ellipse(qx,lift,.31,.43)
 core=sub(1,ramp(radial,.18,1.1))
 strands=noise(combine(mul(qx,9),mul(lift,1.6),progress),4,6)
 detail=noise(combine(mul(qx,4),mul(lift,2.2),progress),10,3)
 density=mul(mathn('POWER',core,1.45),mul(ramp(strands,.26,.7),ramp(broad,.24,.66)))
 density=mul(density,add(.55,mul(detail,.65)))
 signals['smoke']=clamp(mul(density,spec.get('density_gain',1.9))); alpha=True
elif family=='shockwave':
 radius=ellipse(x,y,1,1); angle=node('ShaderNodeMath'); angle.operation='ARCTAN2'; link(y,angle.inputs[0]); link(x,angle.inputs[1])
 rays=noise(combine(mul(angle.outputs[0],16),mul(radius,1.5),0),3,4)
 ring=mathn('EXPONENT',mul(mathn('POWER',mathn('DIVIDE',sub(radius,.29),.075),2),-1))
 puff=noise(combine(mul(x,5),mul(y,5),0),5,5)
 streak=mul(ramp(rays,.44,.7),mul(ramp(radius,.08,.24),sub(1,ramp(radius,.28,.49))))
 signals['shockwave']=clamp(add(mul(ring,mul(puff,.95)),mul(streak,.9)))
 if spec.get('puffy',False):
  ca=mathn('COSINE',angle.outputs[0]); sa=mathn('SINE',angle.outputs[0])
  rimnoise=noise(combine(mul(ca,3),mul(sa,3),0),1.6,2)
  rimradius=add(.28,mul(sub(rimnoise,.5),.13))
  rimwidth=add(.047,mul(rimnoise,.038))
  cloudring=mathn('EXPONENT',mul(mathn('POWER',mathn('DIVIDE',sub(radius,rimradius),rimwidth),2),-1))
  cloud=noise(combine(mul(x,4),mul(y,4),0),2.3,3)
  puffdensity=mul(cloudring,add(.32,mul(ramp(cloud,.28,.69),.9)))
  broad_rays=noise(combine(mul(ca,8),mul(sa,8),mul(radius,1.5)),1.2,2)
  raymask=mul(ramp(broad_rays,.48,.67),mul(ramp(radius,.06,.22),sub(1,ramp(radius,.26,.48))))
  signals['shockwave']=clamp(add(mul(puffdensity,1.15),mul(raymask,.3)))
elif family=='normal_probe':
 probeheight=add(mul(u,.2),mul(v,.4))
 bump=node('ShaderNodeBump'); bump.inputs['Strength'].default_value=1; bump.inputs['Distance'].default_value=1; link(probeheight,bump.inputs['Height'])
 normal=node('ShaderNodeVectorMath'); normal.operation='MULTIPLY_ADD'; link(bump.outputs['Normal'],normal.inputs[0]); normal.inputs[1].default_value=(.5,-.5,.5); normal.inputs[2].default_value=(.5,.5,.5)
 signals['normal_dx']=normal.outputs[0]
elif family=='flow':
 # Analytic divergence-free vector field from a periodic stream function.
 a=mul(u,2*math.pi); b=mul(v,2*math.pi)
 aa=add(mul(a,3),seed.outputs[0]); bb=mul(b,2)
 fx=add(mul(mathn('SINE',a),mathn('COSINE',b)),mul(mul(mathn('SINE',aa),mathn('COSINE',bb)),.35))
 fy=add(mul(mul(mathn('COSINE',a),mathn('SINE',b)),-1),mul(mul(mathn('COSINE',aa),mathn('SINE',bb)),-.525))
 signals['flow_xy']=combine(add(.5,mul(fx,.3)),add(.5,mul(fy,.3)),0)
elif family=='metal':
 coarse=periodic(3); grain=periodic(45,7); chips=ramp(coarse,.57,.66)
 scratch=ramp(periodic(2,13,35),.62,.74)
 chips=clamp(add(chips,mul(scratch,.65)))
 height=add(mul(coarse,.08),add(mul(grain,.014),add(mul(chips,-.065),mul(scratch,-.025))))
 color=node('ShaderNodeMixRGB'); color.blend_type='MIX'; link(chips,color.inputs[0]); color.inputs[1].default_value=(.08,.15,.18,1); color.inputs[2].default_value=(.31,.33,.35,1)
 modulation=node('ShaderNodeMixRGB'); modulation.blend_type='MULTIPLY'; modulation.inputs[0].default_value=.4; link(color.outputs[0],modulation.inputs[1]); link(add(.55,mul(grain,.7)),modulation.inputs[2])
 bump=node('ShaderNodeBump'); bump.inputs['Strength'].default_value=.7; bump.inputs['Distance'].default_value=.09; link(height,bump.inputs['Height'])
 normal=node('ShaderNodeVectorMath'); normal.operation='MULTIPLY_ADD'; link(bump.outputs['Normal'],normal.inputs[0]); normal.inputs[1].default_value=(.5,-.5,.5); normal.inputs[2].default_value=(.5,.5,.5)
 signals={'albedo':modulation.outputs[0],'height':height,'roughness':clamp(add(.5,add(mul(grain,.18),mul(chips,-.24)))),'metallic':chips,'normal_dx':normal.outputs[0]}
elif family=='smoke3d':
 plane.hide_render=True; scene.cycles.samples=spec.get('samples',12); scene.cycles.use_denoising=spec.get('denoise',False)
 bpy.ops.mesh.primitive_cube_add(size=2); volume=bpy.context.object; volume.scale=(.7,.9,.55)
 volume.data.materials.append(mat); stretch=node('ShaderNodeVectorMath'); stretch.operation='MULTIPLY'; link(uv.outputs['Generated'],stretch.inputs[0]); stretch.inputs[1].default_value=(2,.65,1.3); coord=noise(stretch.outputs[0],5,5)
 gsep=node('ShaderNodeSeparateXYZ'); link(uv.outputs['Generated'],gsep.inputs[0])
 rr=ellipse(sub(gsep.outputs['X'],.5),sub(gsep.outputs['Y'],.5),.48,.48)
 zfade=sub(1,ramp(mathn('ABSOLUTE',sub(gsep.outputs['Z'],.5)),.1,.49))
 density=mul(mul(sub(1,ramp(rr,.1,1)),zfade),mul(ramp(coord,.42,.66),9))
 vol=node('ShaderNodeVolumePrincipled'); vol.inputs['Color'].default_value=(.8,.8,.8,1); link(density,vol.inputs['Density']); link(vol.outputs[0],output.inputs['Volume']); nt.links.remove(output.inputs['Surface'].links[0])
 bpy.ops.object.light_add(type='AREA',location=(-1,1,2)); bpy.context.object.data.energy=500; bpy.context.object.data.shape='DISK'; bpy.context.object.data.size=3
 signals={'smoke3d':None}
else: raise ValueError(family)
if alpha:
 transparent=node('ShaderNodeBsdfTransparent'); mix=node('ShaderNodeMixShader'); link(next(iter(signals.values())),mix.inputs[0]); link(transparent.outputs[0],mix.inputs[1]); link(emission.outputs[0],mix.inputs[2]); link(mix.outputs[0],output.inputs['Surface']); emission.inputs['Color'].default_value=(1,1,1,1)
# Encapsulate the complete procedural signal network as an editable reusable shader group.
group=bpy.data.node_groups.new('CK_'+family+'_Generator','ShaderNodeTree')
for name in signals: group.interface.new_socket(name=name,in_out='OUTPUT',socket_type='NodeSocketColor')
# Clone the evaluated signal network into a reusable group without context operators.
exclude={output,emission}
if alpha: exclude.update({transparent,mix})
copy={}
for original in list(nt.nodes):
 if original in exclude: continue
 clone=group.nodes.new(original.bl_idname); clone.name=original.name; clone.label=original.label
 for prop in ('operation','noise_dimensions','blend_type','normalize'):
  if hasattr(original,prop) and hasattr(clone,prop):
   try: setattr(clone,prop,getattr(original,prop))
   except (TypeError,AttributeError): pass
 for i,sock in enumerate(original.inputs):
  if i<len(clone.inputs) and hasattr(sock,'default_value'):
   try: clone.inputs[i].default_value=sock.default_value
   except (TypeError,ValueError): pass
 for i,sock in enumerate(original.outputs):
  if hasattr(sock,'default_value') and i<len(clone.outputs):
   try: clone.outputs[i].default_value=sock.default_value
   except (TypeError,ValueError): pass
 copy[original]=clone
for edge in list(nt.links):
 if edge.from_node in copy and edge.to_node in copy:
  oi=list(edge.from_node.outputs).index(edge.from_socket); ii=list(edge.to_node.inputs).index(edge.to_socket)
  group.links.new(copy[edge.from_node].outputs[oi],copy[edge.to_node].inputs[ii])
gout=group.nodes.new('NodeGroupOutput')
for name,sig in signals.items():
 if sig is not None:
  group.links.new(copy[sig.node].outputs[list(sig.node.outputs).index(sig)],gout.inputs[name])
gcontrols=group.nodes.new('NodeGroupInput'); gcontrols.name='Exposed_Controls'
for controlname in ('Seed','Evolution'):
 original=group.nodes.get(controlname)
 if original is not None:
  socket=group.interface.new_socket(name=controlname,in_out='INPUT',socket_type='NodeSocketFloat'); socket.default_value=original.outputs[0].default_value
  for edge in list(original.outputs[0].links): group.links.new(gcontrols.outputs[controlname],edge.to_socket)
group.use_fake_user=True; group.asset_mark(); group.asset_data.description='Reusable native '+family+' shader signal generator'
# Keep seed/evolution controls in the material for explicit batch edits; this material
# and the equivalent reusable group both contain the complete editable signal graph.
mat.asset_mark(); mat.asset_data.description='Native procedural '+family+'; Seed and Evolution controls in Shader Editor'
geo=bpy.data.node_groups.new('CK_TextureCard_Geometry','GeometryNodeTree'); geo.interface.new_socket(name='Geometry',in_out='INPUT',socket_type='NodeSocketGeometry'); geo.interface.new_socket(name='Geometry',in_out='OUTPUT',socket_type='NodeSocketGeometry')
gi=geo.nodes.new('NodeGroupInput'); go=geo.nodes.new('NodeGroupOutput'); trans=geo.nodes.new('GeometryNodeTransform'); geo.links.new(gi.outputs['Geometry'],trans.inputs['Geometry']); geo.links.new(trans.outputs['Geometry'],go.inputs['Geometry'])
modifier=plane.modifiers.new('Reusable texture card geometry','NODES'); modifier.node_group=geo
# Deterministic alpha uses rendered grayscale emission rather than Monte Carlo
# transparent-surface coverage. This keeps single-sample CPU exports smooth.
if alpha:
 link(emission.outputs[0],output.inputs['Surface']); link(signals['smoke'],emission.inputs['Color'])
# A second object demonstrates genuine Geometry Nodes surface construction.
bpy.ops.mesh.primitive_plane_add(size=2,location=(3,0,0)); demo=bpy.context.object; demo.name='GeometryNodes_DisplacedSurface_Demo'; demo.hide_render=True
show=bpy.data.node_groups.new('CK_GeometryNodes_DisplacedSurface','GeometryNodeTree'); show.interface.new_socket(name='Geometry',in_out='INPUT',socket_type='NodeSocketGeometry'); show.interface.new_socket(name='Geometry',in_out='OUTPUT',socket_type='NodeSocketGeometry')
gin=show.nodes.new('NodeGroupInput'); gout2=show.nodes.new('NodeGroupOutput'); subdiv=show.nodes.new('GeometryNodeSubdivideMesh'); subdiv.inputs['Level'].default_value=6
pos=show.nodes.new('GeometryNodeInputPosition'); ntex=show.nodes.new('ShaderNodeTexNoise'); ntex.inputs['Scale'].default_value=5; vec=show.nodes.new('ShaderNodeCombineXYZ'); amp=show.nodes.new('ShaderNodeMath'); amp.operation='MULTIPLY'; amp.inputs[1].default_value=.12
setpos=show.nodes.new('GeometryNodeSetPosition'); show.links.new(gin.outputs['Geometry'],subdiv.inputs['Mesh']); show.links.new(subdiv.outputs['Mesh'],setpos.inputs['Geometry']); show.links.new(pos.outputs[0],ntex.inputs['Vector']); show.links.new(ntex.outputs['Fac'],amp.inputs[0]); show.links.new(amp.outputs[0],vec.inputs['Z']); show.links.new(vec.outputs[0],setpos.inputs['Offset']); show.links.new(setpos.outputs[0],gout2.inputs[0]); show.asset_mark()
mod=demo.modifiers.new('Procedural surface demonstration','NODES'); mod.node_group=show
records=[]; started=time.perf_counter()
if 'channels' in spec:
 if not isinstance(spec['channels'],list) or not spec['channels'] or any(channel not in signals for channel in spec['channels']):
  raise ValueError('channels must be a nonempty subset of '+str(list(signals)))
for seedval in seeds:
 seed.outputs[0].default_value=seedval
 for frame in range(spec.get('frames',1)):
  clock.outputs[0].default_value=frame/max(1,spec.get('frames',1)-1)
  for channel,signal in signals.items():
   if spec.get('channels') and channel not in spec['channels']: continue
   scene.cycles.pixel_filter_type='BOX' if channel=='normal_dx' or family=='flow' else 'BLACKMAN_HARRIS'
   scene.render.filter_size=.01 if channel=='normal_dx' or family=='flow' else 1.5
   if signal is not None and not alpha: link(signal,emission.inputs['Color'])
   # Data channels must bypass the display transfer function.
   scene.view_settings.view_transform='Standard' if channel=='albedo' else 'Raw'
   filename=f'{family}_s{seedval:03d}_{channel}'+(f'_f{frame:02d}' if spec.get('frames',1)>1 else '')+'.png'
   scene.render.filepath=str(out/filename); t=time.perf_counter(); bpy.ops.render.render(write_still=True)
   if alpha:
    img=bpy.data.images.load(str(out/filename),check_existing=False); img.colorspace_settings.name='Non-Color'
    px=list(img.pixels[:])
    for k in range(0,len(px),4):
     a=shape_opacity(px[k]); px[k]=1; px[k+1]=1; px[k+2]=1; px[k+3]=a
    img.pixels[:]=px; img.filepath_raw=str(out/filename); img.file_format='PNG'; img.save(); bpy.data.images.remove(img)
   if family in ('erosion','shockwave') or (family=='metal' and channel in ('roughness','metallic','height')):
    img=bpy.data.images.load(str(out/filename),check_existing=False); img.colorspace_settings.name='Non-Color'
    px=list(img.pixels[:])
    for k in range(0,len(px),4): px[k+1]=px[k]; px[k+2]=px[k]
    img.pixels[:]=px; img.filepath_raw=str(out/filename); img.file_format='PNG'; img.save(); bpy.data.images.remove(img)
   if family=='flow':
    img=bpy.data.images.load(str(out/filename),check_existing=False); img.colorspace_settings.name='Non-Color'; px=list(img.pixels[:])
    for k in range(0,len(px),4): px[k+2]=0; px[k+3]=1
    img.pixels[:]=px; img.filepath_raw=str(out/filename); img.file_format='PNG'; img.save(); bpy.data.images.remove(img)
   role='normal' if channel=='normal_dx' else channel if family=='metal' else 'flow' if family=='flow' else 'sprite' if family=='smoke3d' else 'mask'
   metadata={'expected_size':[res,res],'tileable':family in ('erosion','metal','flow'),'role':role,'color_space':'sRGB' if channel=='albedo' else 'linear','alpha_convention':'straight' if alpha or family=='smoke3d' else 'opaque','provenance':{'backend':'Blender','version':bpy.app.version_string,'family':family,'seed':seedval,'frame':frame,'method':'native shader nodes rendered with Cycles CPU'},'normal_convention':'DirectX' if channel=='normal_dx' else None,'opacity_policy':opacity_policy if alpha else None}
   if family=='flow': metadata['encoding']='RG=XY*.5+.5; neutral(.5,.5); +U right/+V up; Unreal decode RG*2-1; unitless UV-space; B0/A1'
   (out/(filename+'.json')).write_text(json.dumps(metadata,indent=2))
   records.append({'file':filename,'seed':seedval,'frame':frame,'channel':channel,'seconds':round(time.perf_counter()-t,3)})
scene['spec_json']=json.dumps(spec); scene['provenance']='All image signals evaluated by Blender Cycles Shader Nodes; CPU rendering'
bpy.ops.wm.save_as_mainfile(filepath=str(out/(family+'.blend')))
(out/'manifest.json').write_text(json.dumps({'spec':spec,'blender':bpy.app.version_string,'seconds':round(time.perf_counter()-started,3),'renders':records,'opacity_policy':opacity_policy if alpha else None,'limitations':['Emission channel export, not UV lightmap bake','Normal map uses DirectX inverted green convention','3D smoke has camera and lighting dependence']},indent=2))
print('CK_GENERATION_COMPLETE',family,len(records))
