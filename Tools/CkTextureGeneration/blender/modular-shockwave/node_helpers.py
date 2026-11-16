"""Accepted shader helpers only; importing does not create scenes or render."""
import bpy

def node(t,kind,label=None,**kw):
    n=t.nodes.new(kind)
    if label:n.label=label;n.name=label
    for k,v in kw.items():setattr(n,k,v)
    return n

def setv(t,s,v):
    if isinstance(v,(float,int)):s.default_value=v
    else:t.links.new(v,s)

def m(t,op,a,b=None,c=None):
    n=node(t,'ShaderNodeMath',operation=op)
    for i,v in enumerate((a,b,c)):
        if v is not None:setv(t,n.inputs[i],v)
    return n.outputs[0]

def vec(t,x,y,z=0):
    n=node(t,'ShaderNodeCombineXYZ')
    for i,v in enumerate((x,y,z)):setv(t,n.inputs[i],v)
    return n.outputs[0]

def split(t,v):
    n=node(t,'ShaderNodeSeparateXYZ');t.links.new(v,n.inputs[0]);return tuple(n.outputs)

def noise(t,v,scale=4,detail=3,w=0,label='Multiscale turbulence'):
    name='CKEval_Multiscale_Turbulence'
    group=bpy.data.node_groups.get(name)
    if group is None:
        group=bpy.data.node_groups.new(name,'ShaderNodeTree')
        for nm,typ in [('Vector','NodeSocketVector'),('Scale','NodeSocketFloat'),('Detail','NodeSocketFloat'),('W','NodeSocketFloat')]:
            group.interface.new_socket(name=nm,in_out='INPUT',socket_type=typ)
        for nm,typ in [('Fac','NodeSocketFloat'),('Color','NodeSocketColor')]:
            group.interface.new_socket(name=nm,in_out='OUTPUT',socket_type=typ)
        inp=group.nodes.new('NodeGroupInput');inp.location=(-240,0)
        out=group.nodes.new('NodeGroupOutput');out.location=(240,0)
        tex=group.nodes.new('ShaderNodeTexNoise');tex.noise_dimensions='4D';tex.inputs['Roughness'].default_value=.66
        for nm in ('Vector','Scale','Detail','W'):group.links.new(inp.outputs[nm],tex.inputs[nm])
        for nm in ('Fac','Color'):group.links.new(tex.outputs[nm],out.inputs[nm])
    n=node(t,'ShaderNodeGroup',label);n.node_tree=group
    t.links.new(v,n.inputs['Vector']);n.inputs['Scale'].default_value=scale
    n.inputs['Detail'].default_value=detail
    setv(t,n.inputs['W'],w)
    return n.outputs['Fac'],n.outputs['Color']
