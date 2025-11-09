import bpy
import os

# Clear existing objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Output directory
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)

def create_material(name, base_color):
    """Create a PBR material with the specified base color (RGBA)"""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    
    # Get the Principled BSDF node (created by default)
    bsdf = nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs['Base Color'].default_value = base_color
        bsdf.inputs['Roughness'].default_value = 0.5
        bsdf.inputs['Metallic'].default_value = 0.1
    
    return mat

# Create and export UV Sphere with GREEN material
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.5, location=(0, 0, 0))
sphere = bpy.context.active_object
sphere.name = "Sphere"
bpy.ops.object.shade_smooth()
# Apply green material
green_mat = create_material("SphereMaterial", (0.2, 1.0, 0.2, 1.0))  # Green
sphere.data.materials.append(green_mat)
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "sphere.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported sphere.glb (GREEN) to {output_dir}")

print(f"Exported sphere.glb (GREEN) to {output_dir}")

# Delete sphere and create cube with RED material
bpy.ops.object.delete()
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
cube = bpy.context.active_object
cube.name = "Cube"
bpy.ops.object.shade_smooth()
# Apply red material
red_mat = create_material("CubeMaterial", (1.0, 0.2, 0.2, 1.0))  # Red
cube.data.materials.append(red_mat)
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "cube.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported cube.glb (RED) to {output_dir}")

print(f"Exported cube.glb (RED) to {output_dir}")

# Delete cube and create ico sphere for the "model" placeholder with BLUE material
bpy.ops.object.delete()
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5, location=(0, 0, 0))
ico = bpy.context.active_object
ico.name = "IcoSphere"
bpy.ops.object.shade_smooth()
# Apply blue material
blue_mat = create_material("IcoSphereMaterial", (0.2, 0.2, 1.0, 1.0))  # Blue
ico.data.materials.append(blue_mat)
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "model.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported model.glb (BLUE) to {output_dir}")

print(f"Exported model.glb (BLUE) to {output_dir}")

print("\n✓ All primitive models with colors exported successfully!")
print(f"  - RED Cube: {os.path.join(output_dir, 'cube.glb')}")
print(f"  - GREEN Sphere: {os.path.join(output_dir, 'sphere.glb')}")  
print(f"  - BLUE IcoSphere: {os.path.join(output_dir, 'model.glb')}")
