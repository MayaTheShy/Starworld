import bpy
import os
import traceback

# Clear existing objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

# Output directory
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)

def create_material(name, base_color):
    """Create a PBR material with the specified base color (RGBA)"""
    try:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        
        # Find the Principled BSDF node (should be created by default)
        bsdf = None
        for node in nodes:
            if node.type == 'BSDF_PRINCIPLED':
                bsdf = node
                break
        
        # If not found, create it
        if not bsdf:
            bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
        
        # Set material properties
        bsdf.inputs['Base Color'].default_value = base_color
        bsdf.inputs['Roughness'].default_value = 0.5
        bsdf.inputs['Metallic'].default_value = 0.1
        
        print(f"  Created material '{name}' with color {base_color}")
        return mat
    except Exception as e:
        print(f"ERROR creating material: {e}")
        traceback.print_exc()
        return None

try:
    print("Creating GREEN sphere...")
    # Create and export UV Sphere with GREEN material
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.5, location=(0, 0, 0))
    sphere = bpy.context.active_object
    sphere.name = "Sphere"
    sphere.select_set(True)
    bpy.context.view_layer.objects.active = sphere
    bpy.ops.object.shade_smooth()
    
    # Apply green material
    green_mat = create_material("SphereMaterial", (0.2, 1.0, 0.2, 1.0))  # Green
    if green_mat:
        sphere.data.materials.append(green_mat)
    
    sphere_path = os.path.join(output_dir, "sphere.glb")
    print(f"  Exporting to {sphere_path}...")
    bpy.ops.export_scene.gltf(
        filepath=sphere_path,
        export_format='GLB',
        use_selection=True
    )
    print(f"✓ Exported sphere.glb (GREEN)")
except Exception as e:
    print(f"ERROR with sphere: {e}")
    traceback.print_exc()

try:
    print("\nCreating RED cube...")
    # Delete sphere and create cube with RED material
    bpy.ops.object.delete(use_global=False)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
    cube = bpy.context.active_object
    cube.name = "Cube"
    cube.select_set(True)
    bpy.context.view_layer.objects.active = cube
    bpy.ops.object.shade_smooth()
    
    # Apply red material
    red_mat = create_material("CubeMaterial", (1.0, 0.2, 0.2, 1.0))  # Red
    if red_mat:
        cube.data.materials.append(red_mat)
    
    cube_path = os.path.join(output_dir, "cube.glb")
    print(f"  Exporting to {cube_path}...")
    bpy.ops.export_scene.gltf(
        filepath=cube_path,
        export_format='GLB',
        use_selection=True
    )
    print(f"✓ Exported cube.glb (RED)")
except Exception as e:
    print(f"ERROR with cube: {e}")
    traceback.print_exc()

try:
    print("\nCreating BLUE icosphere...")
    # Delete cube and create ico sphere for the "model" placeholder with BLUE material
    bpy.ops.object.delete(use_global=False)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5, location=(0, 0, 0))
    ico = bpy.context.active_object
    ico.name = "IcoSphere"
    ico.select_set(True)
    bpy.context.view_layer.objects.active = ico
    bpy.ops.object.shade_smooth()
    
    # Apply blue material
    blue_mat = create_material("IcoSphereMaterial", (0.2, 0.2, 1.0, 1.0))  # Blue
    if blue_mat:
        ico.data.materials.append(blue_mat)
    
    model_path = os.path.join(output_dir, "model.glb")
    print(f"  Exporting to {model_path}...")
    bpy.ops.export_scene.gltf(
        filepath=model_path,
        export_format='GLB',
        use_selection=True
    )
    print(f"✓ Exported model.glb (BLUE)")
except Exception as e:
    print(f"ERROR with icosphere: {e}")
    traceback.print_exc()

print("\n" + "="*60)
print("✓ EXPORT COMPLETE!")
print("="*60)
print(f"Output directory: {output_dir}")
print("  - cube.glb (RED)")
print("  - sphere.glb (GREEN)")  
print("  - model.glb (BLUE)")
print("="*60)
