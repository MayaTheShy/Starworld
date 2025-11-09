#!/usr/bin/env python3
"""
Generate simple GLTF primitive models for Starworld entity rendering.
Requires: pip install pygltflib numpy
"""

import numpy as np
from pygltflib import *
import struct
import os
from pathlib import Path

def create_cube_gltf(output_path):
    """Create a 1x1x1 cube centered at origin"""
    
    # Cube vertices (8 vertices)
    vertices = np.array([
        [-0.5, -0.5, -0.5],  # 0
        [ 0.5, -0.5, -0.5],  # 1
        [ 0.5,  0.5, -0.5],  # 2
        [-0.5,  0.5, -0.5],  # 3
        [-0.5, -0.5,  0.5],  # 4
        [ 0.5, -0.5,  0.5],  # 5
        [ 0.5,  0.5,  0.5],  # 6
        [-0.5,  0.5,  0.5],  # 7
    ], dtype=np.float32)
    
    # Cube indices (12 triangles, 36 indices)
    indices = np.array([
        # Front face
        0, 1, 2, 0, 2, 3,
        # Back face
        5, 4, 7, 5, 7, 6,
        # Top face
        3, 2, 6, 3, 6, 7,
        # Bottom face
        4, 5, 1, 4, 1, 0,
        # Right face
        1, 5, 6, 1, 6, 2,
        # Left face
        4, 0, 3, 4, 3, 7,
    ], dtype=np.uint16)
    
    # Normals (per-face normals repeated for each vertex of triangle)
    normals = np.array([
        # Front
        [0, 0, -1], [0, 0, -1], [0, 0, -1], [0, 0, -1], [0, 0, -1], [0, 0, -1],
        # Back
        [0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1],
        # Top
        [0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0],
        # Bottom
        [0, -1, 0], [0, -1, 0], [0, -1, 0], [0, -1, 0], [0, -1, 0], [0, -1, 0],
        # Right
        [1, 0, 0], [1, 0, 0], [1, 0, 0], [1, 0, 0], [1, 0, 0], [1, 0, 0],
        # Left
        [-1, 0, 0], [-1, 0, 0], [-1, 0, 0], [-1, 0, 0], [-1, 0, 0], [-1, 0, 0],
    ], dtype=np.float32)
    
    # Create binary data
    vertices_binary = vertices.tobytes()
    indices_binary = indices.tobytes()
    
    # Build GLTF
    gltf = GLTF2(
        scene=0,
        scenes=[Scene(nodes=[0])],
        nodes=[Node(mesh=0)],
        meshes=[
            Mesh(primitives=[
                Primitive(
                    attributes=Attributes(POSITION=0),
                    indices=1,
                )
            ])
        ],
        accessors=[
            Accessor(
                bufferView=0,
                componentType=FLOAT,
                count=len(vertices),
                type=VEC3,
                max=vertices.max(axis=0).tolist(),
                min=vertices.min(axis=0).tolist(),
            ),
            Accessor(
                bufferView=1,
                componentType=UNSIGNED_SHORT,
                count=len(indices),
                type=SCALAR,
            ),
        ],
        bufferViews=[
            BufferView(
                buffer=0,
                byteOffset=0,
                byteLength=len(vertices_binary),
                target=ARRAY_BUFFER,
            ),
            BufferView(
                buffer=0,
                byteOffset=len(vertices_binary),
                byteLength=len(indices_binary),
                target=ELEMENT_ARRAY_BUFFER,
            ),
        ],
        buffers=[
            Buffer(byteLength=len(vertices_binary) + len(indices_binary))
        ],
    )
    
    gltf.set_binary_blob(vertices_binary + indices_binary)
    gltf.save(output_path)
    print(f"Created cube: {output_path}")

def create_sphere_gltf(output_path, segments=32, rings=16):
    """Create a UV sphere"""
    
    vertices = []
    normals = []
    indices = []
    
    # Generate vertices
    for ring in range(rings + 1):
        theta = ring * np.pi / rings
        sin_theta = np.sin(theta)
        cos_theta = np.cos(theta)
        
        for seg in range(segments + 1):
            phi = seg * 2 * np.pi / segments
            sin_phi = np.sin(phi)
            cos_phi = np.cos(phi)
            
            x = cos_phi * sin_theta
            y = cos_theta
            z = sin_phi * sin_theta
            
            vertices.append([x * 0.5, y * 0.5, z * 0.5])  # radius 0.5
            normals.append([x, y, z])
    
    # Generate indices
    for ring in range(rings):
        for seg in range(segments):
            first = ring * (segments + 1) + seg
            second = first + segments + 1
            
            indices.extend([first, second, first + 1])
            indices.extend([second, second + 1, first + 1])
    
    vertices = np.array(vertices, dtype=np.float32)
    normals = np.array(normals, dtype=np.float32)
    indices = np.array(indices, dtype=np.uint16)
    
    vertices_binary = vertices.tobytes()
    normals_binary = normals.tobytes()
    indices_binary = indices.tobytes()
    
    gltf = GLTF2(
        scene=0,
        scenes=[Scene(nodes=[0])],
        nodes=[Node(mesh=0)],
        meshes=[
            Mesh(primitives=[
                Primitive(
                    attributes=Attributes(POSITION=0, NORMAL=1),
                    indices=2,
                )
            ])
        ],
        accessors=[
            Accessor(
                bufferView=0,
                componentType=FLOAT,
                count=len(vertices),
                type=VEC3,
                max=vertices.max(axis=0).tolist(),
                min=vertices.min(axis=0).tolist(),
            ),
            Accessor(
                bufferView=1,
                componentType=FLOAT,
                count=len(normals),
                type=VEC3,
            ),
            Accessor(
                bufferView=2,
                componentType=UNSIGNED_SHORT,
                count=len(indices),
                type=SCALAR,
            ),
        ],
        bufferViews=[
            BufferView(
                buffer=0,
                byteOffset=0,
                byteLength=len(vertices_binary),
                target=ARRAY_BUFFER,
            ),
            BufferView(
                buffer=0,
                byteOffset=len(vertices_binary),
                byteLength=len(normals_binary),
                target=ARRAY_BUFFER,
            ),
            BufferView(
                buffer=0,
                byteOffset=len(vertices_binary) + len(normals_binary),
                byteLength=len(indices_binary),
                target=ELEMENT_ARRAY_BUFFER,
            ),
        ],
        buffers=[
            Buffer(byteLength=len(vertices_binary) + len(normals_binary) + len(indices_binary))
        ],
    )
    
    gltf.set_binary_blob(vertices_binary + normals_binary + indices_binary)
    gltf.save(output_path)
    print(f"Created sphere: {output_path}")

if __name__ == "__main__":
    # Create output directory
    cache_dir = Path.home() / ".cache" / "starworld" / "primitives"
    cache_dir.mkdir(parents=True, exist_ok=True)
    
    # Generate primitives
    create_cube_gltf(str(cache_dir / "cube.glb"))
    create_sphere_gltf(str(cache_dir / "sphere.glb"))
    
    print(f"\n✓ Primitive models generated in: {cache_dir}")
    print(f"  - cube.glb")
    print(f"  - sphere.glb")
