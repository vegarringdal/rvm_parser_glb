/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#pragma once
#include <cstdint>
#include "LinAlg.h"
#include "Arena.h"

struct Node;
struct Geometry;

struct Contour
{
    float *vertices;
    float *normals;
    uint32_t vertices_n;
};

struct Polygon
{
    Contour *contours;
    uint32_t contours_n;
};

struct Triangulation
{
    // need to be cleared manually
    Arena *arena = nullptr;

    float *vertices = nullptr;
    float *normals = nullptr;
    uint32_t *indices = 0;
    uint32_t vertices_n = 0;
    uint32_t triangles_n = 0;
    uint32_t id = 0;
    uint32_t color = 0;
    float error = 0.f;
};

struct Connection
{
    enum struct Flags : uint8_t
    {
        None = 0,
        HasCircularSide = 1 << 0,
        HasRectangularSide = 1 << 1
    };

    Connection *next = nullptr;
    Geometry *geo[2] = {nullptr, nullptr};
    unsigned offset[2];
    Vec3f p;
    Vec3f d;
    unsigned temp;
    Flags flags = Flags::None;

    void setFlag(Flags flag) { flags = (Flags)((uint8_t)flags | (uint8_t)flag); }
    bool hasFlag(Flags flag) { return (uint8_t)flags & (uint8_t)flag; }
};

struct Geometry
{
    enum struct Kind
    {
        Pyramid,
        Box,
        RectangularTorus,
        CircularTorus,
        EllipticalDish,
        SphericalDish,
        Snout,
        Cylinder,
        Sphere,
        Line,
        FacetGroup
    };
    enum struct Type
    {
        Primitive,
        Obstruction,
        Insulation
    };

    Connection *connections[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    Kind kind;
    Type type = Geometry::Type::Primitive;
    uint32_t transparency = 0; ///< Transparency of primitive, a percentage in [0,100].

    Mat3x4f M_3x4;
    BBox3f bboxLocal;
    BBox3f bboxWorld;
    float sampleStartAngle = 0.f;
    union
    {
        struct
        {
            float bottom[2];
            float top[2];
            float offset[2];
            float height;
        } pyramid;
        struct
        {
            float lengths[3];
        } box;
        struct
        {
            float inner_radius;
            float outer_radius;
            float height;
            float angle;
        } rectangularTorus;
        struct
        {
            float offset;
            float radius;
            float angle;
        } circularTorus;
        struct
        {
            float baseRadius;
            float height;
        } ellipticalDish;
        struct
        {
            float baseRadius;
            float height;
        } sphericalDish;
        struct
        {
            float offset[2];
            float bshear[2];
            float tshear[2];
            float radius_b;
            float radius_t;
            float height;
        } snout;
        struct
        {
            float radius;
            float height;
        } cylinder;
        struct
        {
            float diameter;
        } sphere;
        struct
        {
            float a, b;
        } line;
        struct
        {
            struct Polygon *polygons;
            uint32_t polygons_n;
        } facetGroup;
    };
};