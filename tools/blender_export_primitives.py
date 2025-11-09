import bpy
import os

print("="*60)
print("EXPORTING COLORED PRIMITIVES FOR STARWORLD")
print("="*60)

# Clear existing objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

# Output directory
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)
print(f"Output directory: {output_dir}\n")

def create_material(name, base_color):
    """Create a PBR material with the specified base color (RGBA)"""
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    
    # Find the Principled BSDF node
    bsdf = None
    for node in nodes:
        if node.type == 'BSDF_PRINCIPLED':
            bsdf = node
            break
    
    if not bsdf:
        bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    
    # Set material properties
    bsdf.inputs['Base Color'].default_value = base_color
    bsdf.inputs['Roughness'].default_value = 0.5
    bsdf.inputs['Metallic'].default_value = 0.1
    
    return mat

# ========== SPHERE (GREEN) ==========
print("1. Creating GREEN sphere...")
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.5, location=(0, 0, 0))
sphere = bpy.context.active_object
sphere.name = "Sphere"
bpy.ops.object.shade_smooth()

green_mat = create_material("SphereMaterial", (0.2, 1.0, 0.2, 1.0))
sphere.data.materials.append(green_mat)

# Select ONLY this object for export
bpy.ops.object.select_all(action='DESELECT')
sphere.select_set(True)

sphere_path = os.path.join(output_dir, "sphere.glb")
bpy.ops.export_scene.gltf(filepath=sphere_path, use_selection=True)
print(f"   ✓ Exported: {sphere_path}")

# Now delete it (it's still selected)
bpy.ops.object.delete(use_global=False)

# Now delete it (it's still selected)
bpy.ops.object.delete(use_global=False)

# ========== CUBE (RED) ==========
print("\n2. Creating RED cube...")
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
cube = bpy.context.active_object
cube.name = "Cube"
bpy.ops.object.shade_smooth()

red_mat = create_material("CubeMaterial", (1.0, 0.2, 0.2, 1.0))
cube.data.materials.append(red_mat)

# Select ONLY this object for export
bpy.ops.object.select_all(action='DESELECT')
cube.select_set(True)

cube_path = os.path.join(output_dir, "cube.glb")
bpy.ops.export_scene.gltf(filepath=cube_path, use_selection=True)
print(f"   ✓ Exported: {cube_path}")

# Now delete it (it's still selected)
bpy.ops.object.delete(use_global=False)

# ========== ICOSPHERE (BLUE) ==========
print("\n3. Creating BLUE icosphere...")
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5, location=(0, 0, 0))
ico = bpy.context.active_object
ico.name = "IcoSphere"
bpy.ops.object.shade_smooth()

blue_mat = create_material("IcoSphereMaterial", (0.2, 0.2, 1.0, 1.0))
ico.data.materials.append(blue_mat)

# Select ONLY this object for export
bpy.ops.object.select_all(action='DESELECT')
ico.select_set(True)

model_path = os.path.join(output_dir, "model.glb")
bpy.ops.export_scene.gltf(filepath=model_path, use_selection=True)
print(f"   ✓ Exported: {model_path}")

print("\n" + "="*60)
print("✓ ALL EXPORTS COMPLETE!")
print("="*60)
print(f"Files created in: {output_dir}")
print("  - sphere.glb (GREEN)")
print("  - cube.glb (RED)")
print("  - model.glb (BLUE)")
print("="*60)
