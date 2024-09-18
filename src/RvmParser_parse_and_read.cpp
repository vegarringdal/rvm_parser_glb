#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <array>
#include <set>
#include <fstream>
#include <iostream>
#include <chrono>
#define _FILE_OFFSET_BITS 64
#include <sys/stat.h>
#include "Arena.h"
#include "RvmParser.h"
#include "Geometry.h"
#include "LinAlgOps.h"
#include "Tessellator.h"
#include "TriangulationFactory.h"
#include "ColorStore.h"

uint8_t RvmParser::read_uint8()
{

    uint32_t index = p_index;
    p_index += 1;
    p_index_total += 1;


    // get byte before we might replace buffer
    uint8_t b = p_buffer[index];
    char* c = (char*)&b;

    p_md5.update(c, 1);

    if (p_index == p_buffer_size)
    {
        read_next_chunk();
        p_index = 0;
    }

    return b;
}

uint32_t RvmParser::read_uint32_be()
{

    union UintConverter
    {
        uint32_t u32;
        uint8_t b[sizeof(uint32_t)];
    } converter;

    converter.b[3] = read_uint8();
    converter.b[2] = read_uint8();
    converter.b[1] = read_uint8();
    converter.b[0] = read_uint8();

    return converter.u32;
}

float RvmParser::read_float32_be()
{

    union FloatConverter
    {
        float f;
        uint8_t b[sizeof(float)];
    } converter;

    converter.b[3] = read_uint8();
    converter.b[2] = read_uint8();
    converter.b[1] = read_uint8();
    converter.b[0] = read_uint8();

    return converter.f;
}

std::string RvmParser::read_string()
{

    uint32_t s_len = read_uint32_be();
    unsigned l = 4 * s_len;

    // just incase file is really messed up and give us really big number
    /* if (p_index + l > p_next_chunk)
    {
        
        return "";
    } */

    std::string temp_string;

    uint32_t x = 0;

    for (unsigned i = 0; i < l; i++)
    {
        if (p_buffer[p_index] == 0)
        {
            break;
        }
        temp_string += read_uint8();
        x += 1;
    }

    // read remaining buffer
    while (x < 4 * s_len)
    {
        read_uint8();
        x += 1;
    }

    return temp_string;
}

uint32_t RvmParser::parse_chunk(char *chunk_name)
{

    unsigned i = 0;
    for (i = 0; i < 4 && p_index + 4 <= p_buffer_total_length; i++)
    {
        read_uint8();
        read_uint8();
        read_uint8();
        chunk_name[i] = read_uint8();
    }
    for (; i < 4; i++)
    {
        chunk_name[i] = ' ';
    }

    if (p_index + 8 <= p_buffer_total_length)
    {
        auto next_chunk = read_uint32_be();

        read_uint32_be();

        return next_chunk;
    }
    return 0;
}

HeadBlock RvmParser::parse_head_block()
{
    HeadBlock block;

    block.version = read_uint32_be();
    block.info = read_string();
    block.note = read_string();
    block.date = read_string();
    block.user = read_string();
    if (block.version >= 2)
    {
        block.encoding = read_string();
    }

    return block;
}

ModlBlock RvmParser::parse_modl_block()
{
    ModlBlock block;

    block.version = read_uint32_be();
    block.project = read_string();
    block.name = read_string();

    return block;
}

ColrBlock RvmParser::parse_colr_block()
{
    ColrBlock block;

    block.version = read_uint32_be();
    block.index = read_uint32_be();
    for (unsigned i = 0; i < 3; i++)
    {
        block.color[i] = read_uint8();
    }
    read_uint8();

    return block;
}

CntbBlock RvmParser::parse_cntb_block()
{
    CntbBlock block;

    block.version = read_uint32_be();
    block.name = read_string();
    for (unsigned i = 0; i < 3; i++)
    {
        block.translation[i] = read_float32_be();
    }
    block.material = p_color_store.p_id_hex[read_uint32_be()];
    block.opacity = 100; // todo get from parent?

    // todo, see if we can get some files created having this..
    // if version > 2 then this has opacity ?
    if (block.version > 2)
    {
        // todo, need to improve this part, pass it to children...hint to my self, check DWE - LQ
        block.opacity = read_uint8();
        read_uint8();
        read_uint8();
        read_uint8();
    }

    return block;
}

void RvmParser::parse_prim_block(uint32_t chunk_name_id)
{

    Arena *a = new Arena();

    Geometry *g = new Geometry();
    Tessellator *t = new Tessellator();

    uint32_t version = read_uint32_be();
    uint32_t kind = read_uint32_be();

    NodePrim node_prim;
    node_prim.type = Geometry::Type::Primitive;
    node_prim.opacity = 100;

    for (unsigned i = 0; i < 12; i++)
    {
        g->M_3x4.data[i] = read_float32_be();
    }

    for (unsigned i = 0; i < 6; i++)
    {
        g->bboxLocal.data[i] = read_float32_be();
    }

    g->bboxWorld = transform(g->M_3x4, g->bboxLocal);

    switch (chunk_name_id)
    {
    case chunk_id("PRIM"):
        g->type = Geometry::Type::Primitive;
        break;
    case chunk_id("OBST"):
        g->type = Geometry::Type::Obstruction;
        g->transparency = true;
        node_prim.type = Geometry::Type::Insulation;
        // not tested this, not found model with it
        node_prim.opacity = read_uint8();
        read_uint8();
        read_uint8();
        read_uint8();

        break;
    case chunk_id("INSU"):
        g->type = Geometry::Type::Insulation;
        g->transparency = true;
        node_prim.type = Geometry::Type::Insulation;
        // not tested this, not found model with it
        node_prim.opacity = read_uint8();
        read_uint8();
        read_uint8();
        read_uint8();
        break;
    }

    switch (kind)
    {
    case 1:
        g->kind = Geometry::Kind::Pyramid;

        g->pyramid.bottom[0] = read_float32_be();
        g->pyramid.bottom[1] = read_float32_be();
        g->pyramid.top[0] = read_float32_be();
        g->pyramid.top[1] = read_float32_be();
        g->pyramid.offset[0] = read_float32_be();
        g->pyramid.offset[1] = read_float32_be();
        g->pyramid.height = read_float32_be();
        break;

    case 2:
        g->kind = Geometry::Kind::Box;

        g->box.lengths[0] = read_float32_be();
        g->box.lengths[1] = read_float32_be();
        g->box.lengths[2] = read_float32_be();

        break;

    case 3:
        g->kind = Geometry::Kind::RectangularTorus;
        g->rectangularTorus.inner_radius = read_float32_be();
        g->rectangularTorus.outer_radius = read_float32_be();
        g->rectangularTorus.height = read_float32_be();
        g->rectangularTorus.angle = read_float32_be();

        break;

    case 4:
        g->kind = Geometry::Kind::CircularTorus;

        g->circularTorus.offset = read_float32_be();
        g->circularTorus.radius = read_float32_be();
        g->circularTorus.angle = read_float32_be();

        break;

    case 5:
        g->kind = Geometry::Kind::EllipticalDish;

        g->ellipticalDish.baseRadius = read_float32_be();
        g->ellipticalDish.height = read_float32_be();

        break;

    case 6:
        g->kind = Geometry::Kind::SphericalDish;

        g->ellipticalDish.baseRadius = read_float32_be();
        g->ellipticalDish.height = read_float32_be();

        break;

    case 7:
        g->kind = Geometry::Kind::Snout;

        g->snout.radius_b = read_float32_be();
        g->snout.radius_t = read_float32_be();
        g->snout.height = read_float32_be();
        g->snout.offset[0] = read_float32_be();
        g->snout.offset[1] = read_float32_be();
        g->snout.bshear[0] = read_float32_be();
        g->snout.bshear[1] = read_float32_be();
        g->snout.tshear[0] = read_float32_be();
        g->snout.tshear[1] = read_float32_be();

        break;

    case 8:
        g->kind = Geometry::Kind::Cylinder;

        g->cylinder.radius = read_float32_be();
        g->cylinder.height = read_float32_be();

        break;

    case 9:
        g->kind = Geometry::Kind::Sphere;

        g->sphere.diameter = read_float32_be();

        break;

    case 10:
        g->kind = Geometry::Kind::Line;

        g->line.a = read_float32_be();
        g->line.b = read_float32_be();

        break;

    case 11:

        g->kind = Geometry::Kind::FacetGroup;

        auto x = read_uint32_be();
        g->facetGroup.polygons_n = x;
        g->facetGroup.polygons = (Polygon *)a->alloc(sizeof(Polygon) * g->facetGroup.polygons_n);

        for (unsigned pi = 0; pi < g->facetGroup.polygons_n; pi++)
        {
            auto &poly = g->facetGroup.polygons[pi];

            poly.contours_n = read_uint32_be();
            poly.contours = (Contour *)a->alloc(sizeof(Contour) * poly.contours_n);
            for (unsigned gi = 0; gi < poly.contours_n; gi++)
            {
                auto &cont = poly.contours[gi];

                cont.vertices_n = read_uint32_be();
                cont.vertices = (float *)a->alloc(3 * sizeof(float) * cont.vertices_n);

                for (unsigned vi = 0; vi < cont.vertices_n; vi++)
                {
                    for (unsigned i = 0; i < 3; i++)
                    {
                        cont.vertices[3 * vi + i] = read_float32_be();
                    }
                    for (unsigned i = 0; i < 3; i++)
                    {
                        // dont need it but need to parse past it
                        read_float32_be();
                    }
                }
            }
        }

        break;
    }

    if (g->kind == Geometry::Kind::Line)
    {
        // we hide these for now
        // we dont support lines atm in the viewer, so no point in adding them to glb
        a->clear();
    }
    else
    {
        auto tri = t->geometry(g, arenaTriangulation, p_tolerance);
        tri->id = p_node_count_id;
        tri->color = p_node.material_id;
        a->clear();

        if (tri->vertices_n > 0)
        {
            tri->arena = arenaTriangulation;

            node_prim.triangulation = tri;

            p_node.primitives.push_back(std::move(node_prim));
        }
    }

    delete a;
    delete g;
    delete t;
}
