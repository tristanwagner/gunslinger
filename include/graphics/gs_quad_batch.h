#ifndef __GS_QUAD_BATCH_H__
#define __GS_QUAD_BATCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "base/gs_engine.h"
#include "graphics/gs_graphics.h"
#include "graphics/gs_mesh.h"
#include "math/gs_math.h"

_force_inline
 const char* __gs_default_quad_batch_vertex_src()
 {
 	return "#version 110\n"
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_uv;\n"
	"attribute vec4 a_color;\n"
	"uniform mat4 u_model;\n"
	"uniform mat4 u_view;\n"
	"uniform mat4 u_proj;\n"
	"varying vec2 uv;\n"
	"varying vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);\n"
	"	uv = a_uv;\n"
	"	color = a_color;\n"
	"}";
 }

 #define gs_quad_batch_default_vertex_src\
 	__gs_default_quad_batch_vertex_src()

_force_inline
const char* __gs_default_quad_batch_frag_src()
{
	return "#version 110\n"
	"uniform sampler2D u_tex;\n"
	"varying vec2 uv;\n"
	"varying vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = color * texture2D(u_tex, uv);\n"
	"}";
}

 #define gs_quad_batch_default_frag_src\
 	__gs_default_quad_batch_frag_src()

typedef struct gs_quad_batch_t
{
	gs_byte_buffer raw_vertex_data;
	gs_mesh_t mesh;
	gs_resource( gs_material ) material;
} gs_quad_batch_t;

// Default quad vertex structure
// Define your own to match custom materials
typedef struct gs_quad_batch_default_vert_t
{
	gs_vec3 position;
	gs_vec2 uv;
	gs_vec4 color;
} gs_quad_batch_default_vert_t;

typedef struct gs_quad_batch_vert_into_t
{
	gs_dyn_array( gs_vertex_attribute_type ) layout;
} gs_quad_batch_vert_info_t;

// Generic api for submitting custom data for a quad batch
/*
	Quad Batch API

	* Common functionality for quad batch
	* To define CUSTOM functionality, override specific function and information for API: 

		- gs_quad_batch_i.shader:	gs_resource( gs_shader )
			* Default shader used for quad batch material
			* Define custom vertex/fragment source and then set this shader to construct materials for batches

		- gs_quad_batch_i.vert_info: gs_quad_batch_vert_info_t
			* Holds a gs_dyn_array( gs_vertex_attribute_type ) for the vertex layout
			* Initialized by default
			* Reset this layout and then pass in custom vertex information for your custom shader and mesh layout

		- gs_quad_batch_i.add: func
			* This function requires certain parameters, and you can override this functionality with your specific data
				for adding information into the batch
					* vertex_data: void* 
						- all the data used for your add function
					* data_size: usize 
						- total size of data
			* I hope to make this part of the interface nicer in the future, but for now, this'll have to do.
*/
typedef struct gs_quad_batch_i
{
	gs_quad_batch_t ( * new )( gs_resource( gs_material ) );
	void ( * begin )( gs_quad_batch_t* );
	void ( * end )( gs_quad_batch_t* );
	void ( * add )( gs_quad_batch_t*, void* quad_data, usize data_size );
	void ( * submit )( gs_resource( gs_command_buffer ), gs_quad_batch_t* );
	void ( * free )( gs_quad_batch_t* );
	void ( * set_layout )( struct gs_quad_batch_i* api, void* layout, usize layout_size );
	void ( * set_shader )( struct gs_quad_batch_i* api, gs_resource( gs_shader ) );
	gs_resource( gs_shader ) shader;
	gs_quad_batch_vert_info_t vert_info;
} gs_quad_batch_i;

_force_inline
gs_quad_batch_t __gs_quad_batch_default_new( gs_resource( gs_material ) mat )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;

	gs_quad_batch_t qb = {0};
	qb.raw_vertex_data = gs_byte_buffer_new();
	qb.material = mat;

	// Calculate layout size from layout
	void* layout = (void*)gfx->quad_batch_i->vert_info.layout;
	u32 layout_count = gs_dyn_array_size( gfx->quad_batch_i->vert_info.layout );

	// The data for will be empty for now
	qb.mesh.vbo = gfx->construct_vertex_buffer( layout, layout_count, NULL, 0 );

	return qb;
}

_force_inline
void __gs_quad_batch_default_begin( gs_quad_batch_t* batch )
{
	// Reset position of vertex data
	gs_byte_buffer_clear( &batch->raw_vertex_data );	
	batch->mesh.vertex_count = 0;
}

typedef struct gs_default_quad_info_t
{
	gs_vqs transform;
	gs_vec4 uv;
	gs_vec4 color;
} gs_default_quad_info_t;

_force_inline
void __gs_quad_batch_add_raw_vert_data( gs_quad_batch_t* qb, void* data, usize data_size )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;
	gs_quad_batch_i* qbi = gfx->quad_batch_i;

	// Calculate layout size from layout
	void* layout = (void*)qbi->vert_info.layout;
	u32 layout_count = gs_dyn_array_size( qbi->vert_info.layout );

	// Calculate vert size
	usize vert_size = gfx->calculate_vertex_size_in_bytes( layout, layout_count );

	// In here, you need to know how to read/structure your data to parse the package
	u32 vert_count = data_size / vert_size;
	gs_byte_buffer_bulk_write( &qb->raw_vertex_data, data, data_size );
	qb->mesh.vertex_count += vert_count;
}

_force_inline
void __gs_quad_batch_default_add( gs_quad_batch_t* qb, void* quad_info_data, usize quad_info_data_size )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;

	gs_default_quad_info_t* quad_info = (gs_default_quad_info_t*)(quad_info_data);
	if ( !quad_info ) {
		gs_assert( false );
	}

	gs_vqs transform = quad_info->transform;
	gs_vec4 uv = quad_info->uv;
	gs_vec4 color = quad_info->color;

	// Add as many vertices as you want into the batch...should I perhaps just call this a triangle batch instead?
	// For now, no rotation (just position and scale)	
	gs_mat4 model = gs_vqs_to_mat4( &transform );

	gs_vec3 _tl = (gs_vec3){-0.5f, -0.5f, 0.f};
	gs_vec3 _tr = (gs_vec3){ 0.5f, -0.5f, 0.f};
	gs_vec3 _bl = (gs_vec3){-0.5f,  0.5f, 0.f};
	gs_vec3 _br = (gs_vec3){ 0.5f,  0.5f, 0.f};
	gs_vec4 position = {0};
	gs_quad_batch_default_vert_t tl = {0};
	gs_quad_batch_default_vert_t tr = {0};
	gs_quad_batch_default_vert_t bl = {0};
	gs_quad_batch_default_vert_t br = {0};

	// Top Left
	position = gs_mat4_mul_vec4( model, (gs_vec4){_tl.x, _tl.y, _tl.z, 1.0f} );
	position = gs_vec4_scale( position, 1.0f / position.w ); 
	tl.position = (gs_vec3){position.x, position.y, position.z};
	tl.uv = (gs_vec2){uv.x, uv.y};
	tl.color = color;

	// Top Right
	position = gs_mat4_mul_vec4( model, (gs_vec4){_tr.x, _tr.y, _tr.z, 1.0f} );
	position = gs_vec4_scale( position, 1.0f / position.w ); 
	tr.position = (gs_vec3){position.x, position.y, position.z};
	tr.uv = (gs_vec2){uv.z, uv.y};
	tr.color = color;

	// Bottom Left
	position = gs_mat4_mul_vec4( model, (gs_vec4){_bl.x, _bl.y, _bl.z, 1.0f} );
	position = gs_vec4_scale( position, 1.0f / position.w ); 
	bl.position = (gs_vec3){position.x, position.y, position.z};
	bl.uv = (gs_vec2){uv.x, uv.w};
	bl.color = color;

	// Bottom Right
	position = gs_mat4_mul_vec4( model, (gs_vec4){_br.x, _br.y, _br.z, 1.0f} );
	position = gs_vec4_scale( position, 1.0f / position.w ); 
	br.position = (gs_vec3){position.x, position.y, position.z};
	br.uv = (gs_vec2){uv.z, uv.w};
	br.color = color;

	gs_quad_batch_default_vert_t verts[] = {
		tl, br, bl, tl, tr, br
	};

	__gs_quad_batch_add_raw_vert_data( qb, verts, sizeof(verts) );
}

_force_inline
void __gs_quad_batch_default_end( gs_quad_batch_t* batch )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;
	gfx->update_vertex_buffer_data( batch->mesh.vbo, batch->raw_vertex_data.buffer, batch->raw_vertex_data.size );
}

_force_inline
void __gs_quad_batch_default_free( gs_quad_batch_t* batch )
{
	// Free byte buffer
	gs_byte_buffer_free( &batch->raw_vertex_data );	
}

_force_inline
void __gs_quad_batch_default_submit( gs_resource( gs_command_buffer ) cb, gs_quad_batch_t* batch )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;

	// Bind material shader
	gfx->bind_material_shader( cb, batch->material );

	// Not liking this too much...
	gfx->bind_material_uniforms( cb, batch->material );

	// Bind quad batch mesh vbo
	gfx->bind_vertex_buffer( cb, batch->mesh.vbo );

	// Draw
	gfx->draw( cb, 0, batch->mesh.vertex_count );
}

_force_inline
void __gs_quad_batch_i_set_layout( gs_quad_batch_i* api, void* layout, usize layout_size )
{
	gs_dyn_array_clear( api->vert_info.layout );
	u32 count = layout_size / sizeof(gs_vertex_attribute_type);
	gs_for_range_i( count )
	{
		gs_dyn_array_push( api->vert_info.layout, ((gs_vertex_attribute_type*)layout)[i] );
	}
}

_force_inline
void __gs_quad_batch_i_set_shader( gs_quad_batch_i* api, gs_resource( gs_shader ) shader )
{
	gs_graphics_i* gfx = gs_engine_instance()->ctx.graphics;

	// Free previous shader
	gfx->free_shader( api->shader );
	api->shader = shader;
}

_force_inline
gs_quad_batch_i __gs_quad_batch_i_new()
{
	gs_quad_batch_i api = {0};
	api.begin = &__gs_quad_batch_default_begin;
	api.end = &__gs_quad_batch_default_end;
	api.add = &__gs_quad_batch_default_add;
	api.begin = &__gs_quad_batch_default_begin;
	api.new = &__gs_quad_batch_default_new;
	api.free = &__gs_quad_batch_default_free;
	api.submit = &__gs_quad_batch_default_submit;
	api.set_layout = &__gs_quad_batch_i_set_layout;
	api.set_shader = &__gs_quad_batch_i_set_shader;

	api.vert_info.layout = gs_dyn_array_new( gs_vertex_attribute_type );
	gs_dyn_array_push( api.vert_info.layout, gs_vertex_attribute_float3 );		// Position
	gs_dyn_array_push( api.vert_info.layout, gs_vertex_attribute_float2 );		// UV
	gs_dyn_array_push( api.vert_info.layout, gs_vertex_attribute_float4 );		// Color

	return api;
}

#ifdef __cplusplus
}
#endif 	// c++

#endif // __GS_QUAD_BATCH_H__