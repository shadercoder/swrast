#include "inputassembler.h"
#include "rasterizer.h"
#include "context.h"
#include "config.h"
#include "shader.h"
#include "vector.h"
#include <math.h>



static unsigned char* read_vertex( rs_vertex* v, unsigned char* ptr,
                                   int vertex_format )
{
    /* initialize vertex structure */
    vec4_set( v->attribs + ATTRIB_POS,    0.0f, 0.0f, 0.0f, 1.0f );
    vec4_set( v->attribs + ATTRIB_COLOR,  1.0f, 1.0f, 1.0f, 1.0f );
    vec4_set( v->attribs + ATTRIB_NORMAL, 0.0f, 0.0f, 0.0f, 0.0f );
    vec4_set( v->attribs + ATTRIB_TEX0,   0.0f, 0.0f, 0.0f, 1.0f );
    vec4_set( v->attribs + ATTRIB_TEX1,   0.0f, 0.0f, 0.0f, 1.0f );
    v->used = 0;

    /* decode position */
    if( vertex_format & (VF_POSITION_F2|VF_POSITION_F3|VF_POSITION_F4) )
        v->used |= ATTRIB_FLAG_POS;

    if( vertex_format & VF_POSITION_F2 )
    {
        vec4_set( v->attribs + ATTRIB_POS,
                  ((float*)ptr)[0], ((float*)ptr)[1], 0.0f, 1.0f );
        ptr += 2*sizeof(float);
    }
    else if( vertex_format & VF_POSITION_F3 )
    {
        vec4_set( v->attribs + ATTRIB_POS,
                  ((float*)ptr)[0], ((float*)ptr)[1], ((float*)ptr)[2], 1.0f );
        ptr += 3*sizeof(float);
    }
    else if( vertex_format & VF_POSITION_F4 )
    {
        vec4_set( v->attribs + ATTRIB_POS, ((float*)ptr)[0], ((float*)ptr)[1],
                  ((float*)ptr)[2], ((float*)ptr)[3] );
        ptr += 4*sizeof(float);
    }

    /* decode surface normal */
    if( vertex_format & VF_NORMAL_F3 )
    {
        v->used |= ATTRIB_FLAG_NORMAL;
        vec4_set( v->attribs + ATTRIB_NORMAL,
                  ((float*)ptr)[0],((float*)ptr)[1],((float*)ptr)[2],0.0f );
        ptr += 3*sizeof(float);
    }

    /* decode color */
    if( vertex_format & (VF_COLOR_F3|VF_COLOR_F4|VF_COLOR_UB3|VF_COLOR_UB4) )
        v->used |= ATTRIB_FLAG_COLOR;

    if( vertex_format & VF_COLOR_F3 )
    {
        vec4_set( v->attribs + ATTRIB_COLOR,
                  ((float*)ptr)[0],((float*)ptr)[1],((float*)ptr)[2],1.0f );
        ptr += 3*sizeof(float);
    }
    else if( vertex_format & VF_COLOR_F4 )
    {
        vec4_set( v->attribs + ATTRIB_COLOR,
                  ((float*)ptr)[0], ((float*)ptr)[1],
                  ((float*)ptr)[2], ((float*)ptr)[3] );
        ptr += 4*sizeof(float);
    }
    else if( vertex_format & VF_COLOR_UB3 )
    {
        vec4_set( v->attribs + ATTRIB_COLOR,
                  ((float)ptr[0])/255.0f, ((float)ptr[1])/255.0f,
                  ((float)ptr[2])/255.0f, 1.0f );
        ptr += 3;
    }
    else if( vertex_format & VF_COLOR_UB4 )
    {
        vec4_set( v->attribs + ATTRIB_COLOR,
                  ((float)ptr[0])/255.0f, ((float)ptr[1])/255.0f,
                  ((float)ptr[2])/255.0f, ((float)ptr[3])/255.0f );
        ptr += 4;
    }

    /* decode texture coordinates */
    if( vertex_format & VF_TEX0 )
    {
        v->used |= ATTRIB_FLAG_TEX0;
        vec4_set( v->attribs + ATTRIB_TEX0,
                  ((float*)ptr)[0], ((float*)ptr)[1], 0.0f, 0.0f );
        ptr += 2*sizeof(float);
    }

    return ptr;
}

static void draw_triangle( context* ctx, rs_vertex* v0, rs_vertex* v1,
                           rs_vertex* v2 )
{
    shader_process_vertex( ctx, v0 );
    shader_process_vertex( ctx, v1 );
    shader_process_vertex( ctx, v2 );
    shader_process_triangle( ctx, v0, v1, v2 );

    rasterizer_process_triangle( ctx, v0, v1, v2 );
}

void ia_draw_triangles( context* ctx, unsigned int vertexcount )
{
    void* ptr = ctx->vertexbuffer;
    rs_vertex v0, v1, v2;
    unsigned int i;

    if( ctx->immediate.active )
        return;

    vertexcount -= vertexcount % 3;

    for( i=0; i<vertexcount; i+=3 )
    {
        ptr = read_vertex( &v0, ptr, ctx->vertex_format );
        ptr = read_vertex( &v1, ptr, ctx->vertex_format );
        ptr = read_vertex( &v2, ptr, ctx->vertex_format );

        draw_triangle( ctx, &v0, &v1, &v2 );
    }
}

void ia_draw_triangles_indexed( context* ctx, unsigned int vertexcount,
                                unsigned int indexcount )
{
    unsigned int i0, i1, i2, i = 0, vsize = 0;
    rs_vertex v0, v1, v2;

    if( ctx->immediate.active )
        return;

    /* determine vertex size in bytes */
         if(ctx->vertex_format & VF_POSITION_F2) { vsize += 2*sizeof(float); }
    else if(ctx->vertex_format & VF_POSITION_F3) { vsize += 3*sizeof(float); }
    else if(ctx->vertex_format & VF_POSITION_F4) { vsize += 4*sizeof(float); }

         if( ctx->vertex_format & VF_NORMAL_F3 ) { vsize += 3*sizeof(float); }

         if( ctx->vertex_format & VF_COLOR_F3  ) { vsize += 3*sizeof(float); }
    else if( ctx->vertex_format & VF_COLOR_F4  ) { vsize += 4*sizeof(float); }
    else if( ctx->vertex_format & VF_COLOR_UB3 ) { vsize += 3;               }
    else if( ctx->vertex_format & VF_COLOR_UB4 ) { vsize += 4;               }

         if( ctx->vertex_format & VF_TEX0      ) { vsize += 2*sizeof(float); }

    /* for each triangle */
    indexcount -= indexcount % 3;

    while( i<indexcount )
    {
        i0 = ctx->indexbuffer[ i++ ];
        i1 = ctx->indexbuffer[ i++ ];
        i2 = ctx->indexbuffer[ i++ ];

        if( i0>=vertexcount || i1>=vertexcount || i2>=vertexcount )
            continue;

        read_vertex( &(v0), ((unsigned char*)ctx->vertexbuffer)+vsize*i0,
                     ctx->vertex_format );

        read_vertex( &(v1), ((unsigned char*)ctx->vertexbuffer)+vsize*i1,
                     ctx->vertex_format );

        read_vertex( &(v2), ((unsigned char*)ctx->vertexbuffer)+vsize*i2,
                     ctx->vertex_format );

        draw_triangle( ctx, &v0, &v1, &v2 );
    }
}

void ia_begin( context* ctx )
{
    ctx->immediate.next.used = 0;
    ctx->immediate.current = 0;
    ctx->immediate.active = 1;
}

void ia_vertex( context* ctx, float x, float y, float z, float w )
{
    if( !ctx->immediate.active )
        return;

    vec4_set( ctx->immediate.next.attribs+ATTRIB_POS, x, y, z, w );
    ctx->immediate.next.used |= ATTRIB_FLAG_POS;

    ctx->immediate.vertex[ ctx->immediate.current++ ] = ctx->immediate.next;

    if( ctx->immediate.current < 3 )
        return;

    ctx->immediate.current = 0;
    draw_triangle( ctx, ctx->immediate.vertex, ctx->immediate.vertex+1,
                        ctx->immediate.vertex+2 );
}

void ia_color( context* ctx, float r, float g, float b, float a )
{
    vec4_set( ctx->immediate.next.attribs+ATTRIB_COLOR, r, g, b, a );
    ctx->immediate.next.used |= ATTRIB_FLAG_COLOR;
}

void ia_normal( context* ctx, float x, float y, float z )
{
    vec4_set( ctx->immediate.next.attribs+ATTRIB_NORMAL, x, y, z, 0.0f );
    ctx->immediate.next.used |= ATTRIB_FLAG_NORMAL;
}

void ia_texcoord( context* ctx, int layer, float s, float t )
{
    vec4_set(ctx->immediate.next.attribs+ATTRIB_TEX0+layer, s, t, 0.0f, 1.0f);
    ctx->immediate.next.used |= (ATTRIB_FLAG_TEX0 << layer);
}

void ia_end( context* ctx )
{
    ctx->immediate.next.used = 0;
    ctx->immediate.current = 0;
    ctx->immediate.active = 0;
}

