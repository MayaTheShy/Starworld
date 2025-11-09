import bpy
import os

# Clear existing objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Output directory
output_dir = os.path.expanduser("~/.cache/starworld/primitives")
os.makedirs(output_dir, exist_ok=True)

# Create and export UV Sphere
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.5, location=(0, 0, 0))
sphere = bpy.context.active_object
sphere.name = "Sphere"
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "sphere.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported sphere.glb to {output_dir}")

# Delete sphere and create cube
bpy.ops.object.delete()
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
cube = bpy.context.active_object
cube.name = "Cube"
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "cube.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported cube.glb to {output_dir}")

# Delete cube and create ico sphere for the "model" placeholder
bpy.ops.object.delete()
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.5, location=(0, 0, 0))
ico = bpy.context.active_object
ico.name = "IcoSphere"
bpy.ops.export_scene.gltf(
    filepath=os.path.join(output_dir, "model.glb"),
    export_format='GLB',
    use_selection=True
)
print(f"Exported model.glb to {output_dir}")

print("\n✓ All primitive models exported successfully!")
print(f"Location: {output_dir}")
