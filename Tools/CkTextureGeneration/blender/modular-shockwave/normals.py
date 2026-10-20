"""Blender entrypoint: derive native normal/white data from fresh recipe sources.

blender --background --python normals.py -- --run PATH
Never opens accepted sources; only RUN/<layer>/source.blend is consumed.
All outputs are new and an occupied RUN/normals directory is rejected.
"""
import argparse
import hashlib
import json
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector

LAYERS = ('streaks', 'billows', 'soil')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def encode(tree, normal):
    """Same explicit world-to-billboard basis as accepted normals-r04."""
    basis = tree.nodes.new('ShaderNodeCombineXYZ')
    basis.name = 'Billboard_right_up_toward_viewer'
    rotation = bpy.context.scene.camera.matrix_world.to_quaternion()
    for i, axis in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))):
        dot = tree.nodes.new('ShaderNodeVectorMath')
        dot.operation = 'DOT_PRODUCT'
        tree.links.new(normal, dot.inputs[0])
        dot.inputs[1].default_value = rotation @ Vector(axis)
        tree.links.new(dot.outputs['Value'], basis.inputs[i])
    norm = tree.nodes.new('ShaderNodeVectorMath')
    norm.operation = 'NORMALIZE'
    tree.links.new(basis.outputs[0], norm.inputs[0])
    sep = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(norm.outputs[0], sep.inputs[0])
    lt = tree.nodes.new('ShaderNodeMath')
    lt.operation = 'LESS_THAN'
    tree.links.new(sep.outputs['Z'], lt.inputs[0])
    lt.inputs[1].default_value = 0
    sign = tree.nodes.new('ShaderNodeMath')
    sign.operation = 'MULTIPLY_ADD'
    tree.links.new(lt.outputs[0], sign.inputs[0])
    sign.inputs[1].default_value = -2
    sign.inputs[2].default_value = 1
    front = tree.nodes.new('ShaderNodeVectorMath')
    front.operation = 'SCALE'
    tree.links.new(norm.outputs[0], front.inputs[0])
    tree.links.new(sign.outputs[0], front.inputs[3])
    scale = tree.nodes.new('ShaderNodeVectorMath')
    scale.operation = 'MULTIPLY_ADD'
    tree.links.new(front.outputs[0], scale.inputs[0])
    scale.inputs[1].default_value = (.5, .5, .5)
    scale.inputs[2].default_value = (.5, .5, .5)
    return scale.outputs[0]


def emissions():
    for mat in bpy.data.materials:
        if not mat.use_nodes or not mat.users:
            continue
        tree = mat.node_tree
        mixes = [n for n in tree.nodes if n.type == 'MIX_SHADER']
        if not mixes:
            continue
        if len(mixes) != 1 or not mixes[0].inputs[2].links:
            raise ValueError('Expected one transparent/emission Mix Shader: ' + mat.name)
        mix = mixes[0]
        surface = mix.inputs[2].links[0].from_node
        if surface.type != 'EMISSION':
            raise ValueError('Expected emission surface: ' + mat.name)
        yield tree, mix, surface


def configure():
    scene = bpy.context.scene
    if scene.camera is None:
        raise ValueError('Source has no camera')
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
    bpy.context.preferences.filepaths.save_version = 0
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_depth = '16'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.dither_intensity = 0
    scene.view_settings.view_transform = 'Raw'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    scene.render.use_compositing = False
    scene.render.use_sequencer = False
    scene.render.resolution_percentage = 100
    return scene


def adapt(name):
    count = 0
    for tree, mix, surface in emissions():
        if name == 'billows':
            height = tree.nodes['One_lifetime_density_envelope'].inputs[0].links[0].from_socket
            bump = tree.nodes.new('ShaderNodeBump')
            bump.name = 'Approximate_billows_relief_from_unlit_density'
            bump.invert = False
            bump.inputs['Strength'].default_value = .65
            bump.inputs['Distance'].default_value = .45
            tree.links.new(height, bump.inputs['Height'])
            normal = bump.outputs['Normal']
        elif name == 'streaks':
            clamp = mix.inputs[0].links[0].from_node
            gain_mul = clamp.inputs[0].links[0].from_node
            height = gain_mul.inputs[0].links[0].from_socket
            bump = tree.nodes.new('ShaderNodeBump')
            bump.name = 'Approximate_shallow_dust_relief'
            bump.invert = False
            bump.inputs['Strength'].default_value = .45
            bump.inputs['Distance'].default_value = .055
            tree.links.new(height, bump.inputs['Height'])
            normal = bump.outputs['Normal']
        else:
            geometry = tree.nodes.new('ShaderNodeNewGeometry')
            geometry.name = 'Actual_soil_mesh_facet_normal'
            normal = geometry.outputs['True Normal']
        tree.links.new(encode(tree, normal), surface.inputs['Color'])
        count += 1
    if count == 0:
        raise ValueError('No supported materials for ' + name)
    return count


def render_sequence(scene, folder, offset, selected):
    folder.mkdir()
    scene.render.resolution_x = scene.render.resolution_y = 512
    for i in range(32):
        scene.frame_set(i + offset)
        scene.render.filepath = str(folder / f'frame_{i:02d}.png')
        bpy.ops.render.render(write_still=True)
    scene.frame_set(selected + offset)
    scene.render.resolution_x = scene.render.resolution_y = 1024
    scene.render.filepath = str(folder.parent / f'{folder.name}_1024.png')
    bpy.ops.render.render(write_still=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True, type=Path)
    parser.add_argument('--offsets-json', type=Path,
                        help='JSON object of streaks/billows/soil frame offsets; defaults 0/1/0')
    parser.add_argument('--selected-frame', type=int, default=12)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    run = args.run.resolve()
    offsets = {'streaks': 0, 'billows': 1, 'soil': 0}
    if args.offsets_json:
        supplied = json.loads(args.offsets_json.read_text())
        if set(supplied) != set(LAYERS) or any(type(v) is not int for v in supplied.values()):
            raise ValueError('Offsets must contain exactly three integer layer offsets')
        offsets = supplied
    if not 0 <= args.selected_frame < 32:
        raise ValueError('Selected frame must be in 0..31')
    sources = {name: run / name / 'source.blend' for name in LAYERS}
    if not all(path.is_file() for path in sources.values()):
        raise FileNotFoundError('Expected fresh RUN/<layer>/source.blend for all three layers')
    out = run / 'normals'
    out.mkdir()  # Fail before scene mutation if occupied.
    before = {name: sha(path) for name, path in sources.items()}
    records = []
    for name, source in sources.items():
        started = time.time()
        bpy.ops.wm.open_mainfile(filepath=str(source))
        scene = configure()
        count = adapt(name)
        folder = out / name
        folder.mkdir()
        scene['normal_method'] = ('Dust: native bump from unlit density; soil: actual mesh facet normal. '
                                  '+X image right, +Y image up, +Z toward viewer; native GL.')
        scene.frame_set(args.selected_frame + offsets[name])
        scene.render.resolution_x = scene.render.resolution_y = 512
        bpy.ops.wm.save_as_mainfile(filepath=str(folder / 'normal_source.blend'))
        render_sequence(scene, folder / 'normal', offsets[name], args.selected_frame)
        for tree, _mix, surface in emissions():
            for link in list(surface.inputs['Color'].links):
                tree.links.remove(link)
            surface.inputs['Color'].default_value = (1, 1, 1, 1)
        scene.frame_set(args.selected_frame + offsets[name])
        scene.render.resolution_x = scene.render.resolution_y = 512
        bpy.ops.wm.save_as_mainfile(filepath=str(folder / 'white_source.blend'))
        render_sequence(scene, folder / 'white', offsets[name], args.selected_frame)
        records.append(dict(name=name, source=str(source), sha256=before[name],
                            offset=offsets[name], materials=count, seconds=time.time() - started))
    after = {name: sha(path) for name, path in sources.items()}
    if before != after:
        raise AssertionError('Fresh beauty sources were modified')
    receipt = dict(blender=bpy.app.version_string, frames=32, fps=16,
                   selected_frame=args.selected_frame, cases=records,
                   input_sources_unchanged=True, native_precision_bits=16,
                   alpha_reference='Exact native alpha channels of matched normal and white RGBA PNGs',
                   basis='+X image right,+Y image up,+Z toward viewer; native OpenGL',
                   reconstruction='normalize(2*Cnormal-Cwhite)',
                   limits=['Dust is density-height relief, not physical fluid surface.',
                           'Billboard basis; no Unreal runtime validation.'])
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
