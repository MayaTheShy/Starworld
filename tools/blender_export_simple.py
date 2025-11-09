import bpy
import os

# Output directory
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)

# Log file
log_path = os.path.join(output_dir, "export_log.txt")
log = open(log_path, 'w')

def log_print(msg):
    print(msg)
    log.write(msg + "\n")
    log.flush()

log_print("="*60)
log_print("BLENDER EXPORT SCRIPT STARTING")
log_print("="*60)

# Clear scene
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
log_print("Cleared scene")

def create_colored_material(name, color):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    
    # Find or create Principled BSDF
    bsdf = None
    for node in mat.node_tree.nodes:
        if node.type == 'BSDF_PRINCIPLED':
            bsdf = node
            break
    
    if not bsdf:
        bsdf = mat.node_tree.nodes.new(type='ShaderNodeBsdfPrincipled')
    
    bsdf.inputs['Base Color'].default_value = color
    bsdf.inputs['Roughness'].default_value = 0.5
    bsdf.inputs['Metallic'].default_value = 0.1
    
    return mat

# SPHERE (GREEN)
log_print("\n1. Creating GREEN sphere...")
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.5)
sphere = bpy.context.active_object
sphere.name = "Sphere"
bpy.ops.object.shade_smooth()
log_print("   Created and smoothed")

mat = create_colored_material("GreenMat", (0.2, 1.0, 0.2, 1.0))
sphere.data.materials.append(mat)
log_print("   Applied green material")

bpy.ops.object.select_all(action='DESELECT')
sphere.select_set(True)

sphere_file = os.path.join(output_dir, "sphere.glb")
log_print(f"   Exporting to: {sphere_file}")

try:
    bpy.ops.export_scene.gltf(filepath=sphere_file, use_selection=True)
    log_print("   ✓ SUCCESS!")
except Exception as e:
    log_print(f"   ✗ FAILED: {e}")

bpy.ops.object.delete(use_global=False)

# CUBE (RED)
log_print("\n2. Creating RED cube...")
bpy.ops.mesh.primitive_cube_add(size=1.0)
cube = bpy.context.active_object  
cube.name = "Cube"
bpy.ops.object.shade_smooth()
log_print("   Created and smoothed")

mat = create_colored_material("RedMat", (1.0, 0.2, 0.2, 1.0))
cube.data.materials.append(mat)
log_print("   Applied red material")

bpy.ops.object.select_all(action='DESELECT')
cube.select_set(True)

cube_file = os.path.join(output_dir, "cube.glb")
log_print(f"   Exporting to: {cube_file}")

try:
    bpy.ops.export_scene.gltf(filepath=cube_file, use_selection=True)
    log_print("   ✓ SUCCESS!")
except Exception as e:
    log_print(f"   ✗ FAILED: {e}")

bpy.ops.object.delete(use_global=False)

# ICOSPHERE (BLUE)  
log_print("\n3. Creating BLUE icosphere...")
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5)
ico = bpy.context.active_object
ico.name = "IcoSphere"
bpy.ops.object.shade_smooth()
log_print("   Created and smoothed")

mat = create_colored_material("BlueMat", (0.2, 0.2, 1.0, 1.0))
ico.data.materials.append(mat)
log_print("   Applied blue material")

bpy.ops.object.select_all(action='DESELECT')
ico.select_set(True)

ico_file = os.path.join(output_dir, "model.glb")
log_print(f"   Exporting to: {ico_file}")

try:
    bpy.ops.export_scene.gltf(filepath=ico_file, use_selection=True)
    log_print("   ✓ SUCCESS!")
except Exception as e:
    log_print(f"   ✗ FAILED: {e}")

log_print("\n" + "="*60)
log_print("EXPORT COMPLETE!")
log_print("="*60)
log_print(f"Check files in: {output_dir}")
log_print(f"Log saved to: {log_path}")

log.close()

print(f"\n\n*** CHECK LOG FILE: {log_path} ***\n")
