import bpy
import os

print("="*60)
print("TESTING BLENDER EXPORT CAPABILITY")
print("="*60)

# Check if GLTF export is available
try:
    print("\n1. Checking if GLTF export operator exists...")
    if hasattr(bpy.ops.export_scene, 'gltf'):
        print("   ✓ GLTF export operator found!")
    else:
        print("   ✗ GLTF export operator NOT FOUND!")
        print("   You need to enable the glTF 2.0 add-on:")
        print("   Edit → Preferences → Add-ons → search 'gltf' → enable it")
        raise Exception("GLTF exporter not available")
except AttributeError:
    print("   ✗ export_scene module issue")
    raise

# Test creating a simple cube
print("\n2. Creating test cube...")
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add(size=1.0)
cube = bpy.context.active_object
print(f"   ✓ Created cube: {cube.name}")

# Test creating a material
print("\n3. Creating test material...")
mat = bpy.data.materials.new(name="TestMat")
mat.use_nodes = True
nodes = mat.node_tree.nodes

# Find Principled BSDF
bsdf = None
for node in nodes:
    print(f"   Found node: {node.name} (type: {node.type})")
    if node.type == 'BSDF_PRINCIPLED':
        bsdf = node
        break

if bsdf:
    print(f"   ✓ Found Principled BSDF node")
    bsdf.inputs['Base Color'].default_value = (1.0, 0.0, 0.0, 1.0)
    print(f"   ✓ Set color to RED")
else:
    print(f"   ✗ No Principled BSDF found, creating one...")
    bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    bsdf.inputs['Base Color'].default_value = (1.0, 0.0, 0.0, 1.0)

# Apply material to cube
cube.data.materials.append(mat)
print(f"   ✓ Applied material to cube")

# Test export
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)
test_path = os.path.join(output_dir, "test_cube.glb")

print(f"\n4. Attempting export to: {test_path}")
try:
    bpy.ops.export_scene.gltf(
        filepath=test_path,
        export_format='GLB',
        use_selection=False  # Export all
    )
    print(f"   ✓ EXPORT SUCCESSFUL!")
    
    # Check if file exists
    if os.path.exists(test_path):
        size = os.path.getsize(test_path)
        print(f"   ✓ File created: {size} bytes")
    else:
        print(f"   ✗ File was not created!")
        
except Exception as e:
    print(f"   ✗ EXPORT FAILED: {e}")
    import traceback
    traceback.print_exc()

print("\n" + "="*60)
print("TEST COMPLETE")
print("="*60)
