import bpy
import bmesh
import array
import os

bl_info = {"name": "YesMeshExporter", "category": "Exporter"}

	
class ExportMeshBin(bpy.types.Operator):
	"ExportMeshBin"
	bl_idname = "denglx.export_mesh_bin"
	bl_label = "ExportMeshBin"
	bl_options = set()
	def execute(self, context):
		export_selected("C:/checkout/Yes/Resources/Mesh")
		return {"FINISHED"}
		
		

def get_vb_ib_from_mesh(mesh_data, world_matrix):
	normal_matrix = world_matrix.copy()
	normal_matrix.transpose()
	normal_matrix.invert()
	uv_layer = mesh_data.loops.layers.uv.active
	vb = array.array('f')
	ib = array.array('i')
	for vi, v in enumerate(mesh_data.verts):
		uv = v.link_loops[0][uv_layer].uv
		wpos = world_matrix * v.co
		wnormal = normal_matrix * v.normal
		vb.append(wpos.x)
		vb.append(wpos.z)
		vb.append(wpos.y)
		
		vb.append(wnormal.x)
		vb.append(wnormal.z)
		vb.append(wnormal.y)
		
		vb.append(uv.x)
		vb.append(uv.y)
	for fi, f in enumerate(mesh_data.faces):
		for i in range(3):
			ib.append(f.verts[i].index)
	return vb, ib
		

def export_selected(base_path):
	wm = bpy.context.object.matrix_world.copy()
	bpy.ops.object.mode_set(mode='EDIT')
	bpy.ops.mesh.remove_doubles(threshold=0.0001)
	bm = bmesh.from_edit_mesh(bpy.context.edit_object.data)
	vb, ib = get_vb_ib_from_mesh(bm, wm)
	vb_path = os.path.join(base_path, "exported_vb.bin")
	ib_path = os.path.join(base_path, "exported_ib.bin")
	with open(vb_path, "wb") as vbf:
		vbf.write(vb.tobytes())
	with open(ib_path, "wb") as ibf:
		ibf.write(ib.tobytes())
	bpy.ops.object.mode_set(mode='OBJECT')
	
	
def register():
    bpy.utils.register_class(ExportMeshBin)

def unregister():
    bpy.utils.unregister_class(ExportMeshBin)

if __name__ == "__main__":
	register()
	