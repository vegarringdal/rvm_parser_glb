/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <vector>
#include "Arena.h"
#include "Geometry.h"
#include "Tessellator.h"
#include "LinAlgOps.h"
#include "../libs/libtess2/Include/tesselator.h"

namespace
{

  const float pi = float(M_PI);
  const float half_pi = float(0.5 * M_PI);

}

Tessellator::~Tessellator()
{
  delete factory;
}

Triangulation *Tessellator::geometry(Geometry *geo, Arena *arena, float tolerance)
{
  Triangulation *tri = nullptr;
  TriangulationFactory factory;
  factory.tolerance = tolerance;
  // No need to tessellate lines.
  if (geo->kind == Geometry::Kind::Line)
  {
    return tri;
  }

  auto scale = getScale(geo->M_3x4);

  switch (geo->kind)
  {
  case Geometry::Kind::Pyramid:
    tri = factory.pyramid(arena, geo, scale);
    break;

  case Geometry::Kind::Box:
    tri = factory.box(arena, geo, scale);
    break;

  case Geometry::Kind::RectangularTorus:
    tri = factory.rectangularTorus(arena, geo, scale);
    break;

  case Geometry::Kind::CircularTorus:
    tri = factory.circularTorus(arena, geo, scale);
    break;

  case Geometry::Kind::EllipticalDish:
    tri = factory.sphereBasedShape(arena, geo, geo->ellipticalDish.baseRadius, half_pi, 0.f, geo->ellipticalDish.height / geo->ellipticalDish.baseRadius, scale);
    break;

  case Geometry::Kind::SphericalDish:
  {
    float r_circ = geo->sphericalDish.baseRadius;
    auto h = geo->sphericalDish.height;
    float r_sphere = (r_circ * r_circ + h * h) / (2.f * h);
    float sinval = std::min(1.f, std::max(-1.f, r_circ / r_sphere));
    float arc = asin(sinval);
    if (r_circ < h)
    {
      arc = pi - arc;
    }
    tri = factory.sphereBasedShape(arena, geo, r_sphere, arc, h - r_sphere, 1.f, scale);
    break;
  }
  case Geometry::Kind::Snout:
    tri = factory.snout(arena, geo, scale);
    break;

  case Geometry::Kind::Cylinder:
    tri = factory.cylinder(arena, geo, scale);
    break;

  case Geometry::Kind::Sphere:
    tri = factory.sphereBasedShape(arena, geo, 0.5f * geo->sphere.diameter, pi, 0.f, 1.f, scale);
    break;

  case Geometry::Kind::FacetGroup:
    tri = factory.facetGroup(arena, geo, scale);
    break;

  case Geometry::Kind::Line: // Handled at start of function.
  default:
    assert(false && "Unhandled primitive type");
    break;
  }

  BBox3f box = createEmptyBBox3f();
  for (unsigned i = 0; i < tri->vertices_n; i++)
  {
    engulf(box, makeVec3f(tri->vertices + 3 * i));
  }

  Mat3x4d M = makeMat3x4d(geo->M_3x4.data);

  // todo
  /* M.m03 -= localOrigin.x;
  M.m13 -= localOrigin.y;
  M.m23 -= localOrigin.z; */

  size_t vertexCount = tri->vertices_n;
  auto copy = tri->vertices;
  for (size_t i = 0; i < vertexCount; i++)
  {
    auto u = 3 * i;
    auto a = makeVec3f(mul(M, makeVec3d(tri->vertices + u)));

    copy[u] = a.x;
    copy[u + 1] = a.y;
    copy[u + 2] = a.z;
  }

  return tri;
}
