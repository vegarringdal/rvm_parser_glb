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
#include "libtess2/Include/tesselator.h"
#include "Geometry.h"
#include "Tessellator.h"
#include "LinAlgOps.h"

namespace
{
  const float pi = float(M_PI);
  const float one_over_pi = float(1.0 / M_PI);
  const float twopi = float(2.0 * M_PI);
  const float tolerance = 0.01;

  struct Interface
  {
    enum struct Kind
    {
      Undefined,
      Square,
      Circular
    };
    Kind kind = Kind::Undefined;

    union
    {
      struct
      {
        Vec3f p[4];
      } square;
      struct
      {
        float radius;
      } circular;
    };
  };

  Interface getInterface(const Geometry *geo, unsigned o)
  {
    Interface interface;
    auto *connection = geo->connections[o];
    auto ix = connection->geo[0] == geo ? 1 : 0;
    auto scale = getScale(geo->M_3x4);
    switch (geo->kind)
    {
    case Geometry::Kind::Pyramid:
    {
      auto bx = 0.5f * geo->pyramid.bottom[0];
      auto by = 0.5f * geo->pyramid.bottom[1];
      auto tx = 0.5f * geo->pyramid.top[0];
      auto ty = 0.5f * geo->pyramid.top[1];
      auto ox = 0.5f * geo->pyramid.offset[0];
      auto oy = 0.5f * geo->pyramid.offset[1];
      auto h2 = 0.5f * geo->pyramid.height;
      Vec3f quad[2][4] =
          {
              {makeVec3f(-bx - ox, -by - oy, -h2),
               makeVec3f(bx - ox, -by - oy, -h2),
               makeVec3f(bx - ox, by - oy, -h2),
               makeVec3f(-bx - ox, by - oy, -h2)},
              {makeVec3f(-tx + ox, -ty + oy, h2),
               makeVec3f(tx + ox, -ty + oy, h2),
               makeVec3f(tx + ox, ty + oy, h2),
               makeVec3f(-tx + ox, ty + oy, h2)},
          };

      interface.kind = Interface::Kind::Square;
      if (o < 4)
      {
        unsigned oo = (o + 1) & 3;
        interface.square.p[0] = mul(geo->M_3x4, quad[0][o]);
        interface.square.p[1] = mul(geo->M_3x4, quad[0][oo]);
        interface.square.p[2] = mul(geo->M_3x4, quad[1][oo]);
        interface.square.p[3] = mul(geo->M_3x4, quad[1][o]);
      }
      else
      {
        for (unsigned k = 0; k < 4; k++)
          interface.square.p[k] = mul(geo->M_3x4, quad[o - 4][k]);
      }
      break;
    }
    case Geometry::Kind::Box:
    {
      auto &box = geo->box;
      auto xp = 0.5f * box.lengths[0];
      auto xm = -xp;
      auto yp = 0.5f * box.lengths[1];
      auto ym = -yp;
      auto zp = 0.5f * box.lengths[2];
      auto zm = -zp;
      Vec3f V[6][4] = {
          {makeVec3f(xm, ym, zp), makeVec3f(xm, yp, zp), makeVec3f(xm, yp, zm), makeVec3f(xm, ym, zm)},
          {makeVec3f(xp, ym, zm), makeVec3f(xp, yp, zm), makeVec3f(xp, yp, zp), makeVec3f(xp, ym, zp)},
          {makeVec3f(xp, ym, zm), makeVec3f(xp, ym, zp), makeVec3f(xm, ym, zp), makeVec3f(xm, ym, zm)},
          {makeVec3f(xm, yp, zm), makeVec3f(xm, yp, zp), makeVec3f(xp, yp, zp), makeVec3f(xp, yp, zm)},
          {makeVec3f(xm, yp, zm), makeVec3f(xp, yp, zm), makeVec3f(xp, ym, zm), makeVec3f(xm, ym, zm)},
          {makeVec3f(xm, ym, zp), makeVec3f(xp, ym, zp), makeVec3f(xp, yp, zp), makeVec3f(xm, yp, zp)}};
      for (unsigned k = 0; k < 4; k++)
        interface.square.p[k] = mul(geo->M_3x4, V[o][k]);
      break;
    }
    case Geometry::Kind::RectangularTorus:
    {
      auto &tor = geo->rectangularTorus;
      auto h2 = 0.5f * tor.height;
      float square[4][2] = {
          {tor.outer_radius, -h2},
          {tor.inner_radius, -h2},
          {tor.inner_radius, h2},
          {tor.outer_radius, h2},
      };
      if (o == 0)
      {
        for (unsigned k = 0; k < 4; k++)
        {
          interface.square.p[k] = mul(geo->M_3x4, makeVec3f(square[k][0], 0.f, square[k][1]));
        }
      }
      else
      {
        for (unsigned k = 0; k < 4; k++)
        {
          interface.square.p[k] = mul(geo->M_3x4, makeVec3f(square[k][0] * cos(tor.angle),
                                                            square[k][0] * sin(tor.angle),
                                                            square[k][1]));
        }
      }
      break;
    }
    case Geometry::Kind::CircularTorus:
      interface.kind = Interface::Kind::Circular;
      interface.circular.radius = scale * geo->circularTorus.radius;
      break;

    case Geometry::Kind::EllipticalDish:
      interface.kind = Interface::Kind::Circular;
      interface.circular.radius = scale * geo->ellipticalDish.baseRadius;
      break;

    case Geometry::Kind::SphericalDish:
    {
      float r_circ = geo->sphericalDish.baseRadius;
      auto h = geo->sphericalDish.height;
      float r_sphere = (r_circ * r_circ + h * h) / (2.f * h);
      interface.kind = Interface::Kind::Circular;
      interface.circular.radius = scale * r_sphere;
      break;
    }
    case Geometry::Kind::Snout:
      interface.kind = Interface::Kind::Circular;
      interface.circular.radius = scale * (connection->offset[ix] == 0 ? geo->snout.radius_b : geo->snout.radius_t);
      break;
    case Geometry::Kind::Cylinder:
      interface.kind = Interface::Kind::Circular;
      interface.circular.radius = scale * geo->cylinder.radius;
      break;
    case Geometry::Kind::Sphere:
    case Geometry::Kind::Line:
    case Geometry::Kind::FacetGroup:
      interface.kind = Interface::Kind::Undefined;
      break;

    default:
      assert(false && "Unhandled primitive type");
      break;
    }
    return interface;
  }

  bool doInterfacesMatch(const Geometry *geo, const Connection *con)
  {
    bool isFirst = geo == con->geo[0];

    auto *thisGeo = con->geo[isFirst ? 0 : 1];
    auto thisOffset = con->offset[isFirst ? 0 : 1];
    auto thisIFace = getInterface(thisGeo, thisOffset);

    auto *thatGeo = con->geo[isFirst ? 1 : 0];
    auto thatOffset = con->offset[isFirst ? 1 : 0];
    auto thatIFace = getInterface(thatGeo, thatOffset);

    if (thisIFace.kind != thatIFace.kind)
      return false;

    if (thisIFace.kind == Interface::Kind::Circular)
    {

      return thisIFace.circular.radius <= 1.05f * thatIFace.circular.radius;
    }
    else
    {
      for (unsigned j = 0; j < 4; j++)
      {
        bool found = false;
        for (unsigned i = 0; i < 4; i++)
        {
          if (distanceSquared(thisIFace.square.p[j], thatIFace.square.p[i]) < 0.001f * 0.001f)
          {
            found = true;
          }
        }
        if (!found)
          return false;
      }
      return true;
    }
  }

  // FIXME: replace use of these with stuff from linalg.
  inline void sub3(float *dst, float *a, float *b)
  {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
  }

  inline void cross3(float *dst, float *a, float *b)
  {
    dst[0] = a[1] * b[2] - a[2] * b[1];
    dst[1] = a[2] * b[0] - a[0] * b[2];
    dst[2] = a[0] * b[1] - a[1] * b[0];
  }

  inline float dot3(float *a, float *b)
  {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }

  unsigned triIndices(uint32_t *indices, unsigned l, unsigned o, unsigned v0, unsigned v1, unsigned v2)
  {
    indices[l++] = o + v0;
    indices[l++] = o + v1;
    indices[l++] = o + v2;
    return l;
  }

  unsigned quadIndices(uint32_t *indices, unsigned l, unsigned o, unsigned v0, unsigned v1, unsigned v2, unsigned v3)
  {
    indices[l++] = o + v0;
    indices[l++] = o + v1;
    indices[l++] = o + v2;

    indices[l++] = o + v2;
    indices[l++] = o + v3;
    indices[l++] = o + v0;
    return l;
  }

  unsigned vertex(float *vertices, unsigned l, float *p)
  {

    vertices[l++] = p[0];

    vertices[l++] = p[1];

    vertices[l++] = p[2];
    return l;
  }

  unsigned vertex(float *vertices, unsigned l, float px, float py, float pz)
  {

    vertices[l++] = px;

    vertices[l++] = py;

    vertices[l++] = pz;
    return l;
  }

  unsigned tessellateCircle(uint32_t *indices, unsigned l, uint32_t *t, uint32_t *src, unsigned N)
  {
    while (3 <= N)
    {
      unsigned m = 0;
      unsigned i;
      for (i = 0; i + 2 < N; i += 2)
      {
        indices[l++] = src[i];
        indices[l++] = src[i + 1];
        indices[l++] = src[i + 2];
        t[m++] = src[i];
      }
      for (; i < N; i++)
      {
        t[m++] = src[i];
      }
      N = m;
      std::swap(t, src);
    }
    return l;
  }

}

unsigned TriangulationFactory::sagittaBasedSegmentCount(float arc, float radius, float scale)
{
  float samples = arc / std::acos(std::max(-1.f, 1.f - tolerance / (scale * radius)));
  return std::min(maxSamples, unsigned(std::max(float(minSamples), std::ceil(samples))));
}

float TriangulationFactory::sagittaBasedError(float arc, float radius, float scale, unsigned segments)
{
  auto s = scale * radius * (1.f - std::cos(arc / segments)); // Length of sagitta
  return s;
}

Triangulation *TriangulationFactory::pyramid(Arena *arena, const Geometry *geo, float /*scale*/)
{
  auto bx = 0.5f * geo->pyramid.bottom[0];
  auto by = 0.5f * geo->pyramid.bottom[1];
  auto tx = 0.5f * geo->pyramid.top[0];
  auto ty = 0.5f * geo->pyramid.top[1];
  auto ox = 0.5f * geo->pyramid.offset[0];
  auto oy = 0.5f * geo->pyramid.offset[1];
  auto h2 = 0.5f * geo->pyramid.height;

  Vec3f quad[2][4] =
      {
          {makeVec3f(-bx - ox, -by - oy, -h2),
           makeVec3f(bx - ox, -by - oy, -h2),
           makeVec3f(bx - ox, by - oy, -h2),
           makeVec3f(-bx - ox, by - oy, -h2)},
          {makeVec3f(-tx + ox, -ty + oy, h2),
           makeVec3f(tx + ox, -ty + oy, h2),
           makeVec3f(tx + ox, ty + oy, h2),
           makeVec3f(-tx + ox, ty + oy, h2)},
      };

  bool cap[6] = {
      true,
      true,
      true,
      true,
      1e-7f <= std::min(std::abs(geo->pyramid.bottom[0]), std::abs(geo->pyramid.bottom[1])),
      1e-7f <= std::min(std::abs(geo->pyramid.top[0]), std::abs(geo->pyramid.top[1]))};

  for (unsigned i = 0; i < 6; i++)
  {
    auto *con = geo->connections[i];
    if (cap[i] == false || con == nullptr || con->flags != Connection::Flags::HasRectangularSide)
      continue;

    if (doInterfacesMatch(geo, con))
    {
      cap[i] = false;
      discardedCaps++;
      // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0xff0000);
    }
  }

  unsigned caps = 0;
  for (unsigned i = 0; i < 6; i++)
    if (cap[i])
      caps++;

  auto *tri = arena->alloc<Triangulation>();
  tri->error = 0.f;

  tri->vertices_n = 4 * caps;
  tri->triangles_n = 2 * caps;

  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  unsigned l = 0;
  for (unsigned i = 0; i < 4; i++)
  {
    if (cap[i] == false)
      continue;
    unsigned ii = (i + 1) & 3;
    l = vertex(tri->vertices, l, quad[0][i].data);
    l = vertex(tri->vertices, l, quad[0][ii].data);
    l = vertex(tri->vertices, l, quad[1][ii].data);
    l = vertex(tri->vertices, l, quad[1][i].data);
  }
  if (cap[4])
  {
    for (unsigned i = 0; i < 4; i++)
    {
      l = vertex(tri->vertices, l, quad[0][i].data);
    }
  }
  if (cap[5])
  {
    for (unsigned i = 0; i < 4; i++)
    {
      l = vertex(tri->vertices, l, quad[1][i].data);
    }
  }
  assert(l == 3 * tri->vertices_n);

  l = 0;
  unsigned o = 0;
  tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);
  for (unsigned i = 0; i < 4; i++)
  {
    if (cap[i] == false)
      continue;
    l = quadIndices(tri->indices, l, o /*4 * i*/, 0, 1, 2, 3);
    o += 4;
  }
  if (cap[4])
  {
    l = quadIndices(tri->indices, l, o, 3, 2, 1, 0);
    o += 4;
  }
  if (cap[5])
  {
    l = quadIndices(tri->indices, l, o, 0, 1, 2, 3);
    o += 4;
  }
  assert(l == 3 * tri->triangles_n);
  assert(o == tri->vertices_n);

  return tri;
}

Triangulation *TriangulationFactory::box(Arena *arena, const Geometry *geo, float /*scale*/)
{
  auto &box = geo->box;

  auto xp = 0.5f * box.lengths[0];
  auto xm = -xp;
  auto yp = 0.5f * box.lengths[1];
  auto ym = -yp;
  auto zp = 0.5f * box.lengths[2];
  auto zm = -zp;

  Vec3f V[6][4] = {
      {makeVec3f(xm, ym, zp), makeVec3f(xm, yp, zp), makeVec3f(xm, yp, zm), makeVec3f(xm, ym, zm)},
      {makeVec3f(xp, ym, zm), makeVec3f(xp, yp, zm), makeVec3f(xp, yp, zp), makeVec3f(xp, ym, zp)},
      {makeVec3f(xp, ym, zm), makeVec3f(xp, ym, zp), makeVec3f(xm, ym, zp), makeVec3f(xm, ym, zm)},
      {makeVec3f(xm, yp, zm), makeVec3f(xm, yp, zp), makeVec3f(xp, yp, zp), makeVec3f(xp, yp, zm)},
      {makeVec3f(xm, yp, zm), makeVec3f(xp, yp, zm), makeVec3f(xp, ym, zm), makeVec3f(xm, ym, zm)},
      {makeVec3f(xm, ym, zp), makeVec3f(xp, ym, zp), makeVec3f(xp, yp, zp), makeVec3f(xm, yp, zp)}};

  bool faces[6] = {
      1e-5 <= box.lengths[0],
      1e-5 <= box.lengths[0],
      1e-5 <= box.lengths[1],
      1e-5 <= box.lengths[1],
      1e-5 <= box.lengths[2],
      1e-5 <= box.lengths[2],
  };
  for (unsigned i = 0; i < 6; i++)
  {
    auto *con = geo->connections[i];
    if (faces[i] == false || con == nullptr || con->flags != Connection::Flags::HasRectangularSide)
      continue;

    if (doInterfacesMatch(geo, con))
    {
      faces[i] = false;
      discardedCaps++;
      // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0xff0000);
    }
  }

  unsigned faces_n = 0;
  for (unsigned i = 0; i < 6; i++)
  {
    if (faces[i])
      faces_n++;
  }

  Triangulation *tri = arena->alloc<Triangulation>();
  tri->error = 0.f;

  if (faces_n)
  {
    tri->vertices_n = 4 * faces_n;
    tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

    tri->triangles_n = 2 * faces_n;
    tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);

    unsigned o = 0;
    unsigned i_v = 0;
    unsigned i_p = 0;
    for (unsigned f = 0; f < 6; f++)
    {
      if (!faces[f])
        continue;

      for (unsigned i = 0; i < 4; i++)
      {
        i_v = vertex(tri->vertices, i_v, V[f][i].data);
      }
      i_p = quadIndices(tri->indices, i_p, o, 0, 1, 2, 3);

      o += 4;
    }
    assert(i_v == 3 * tri->vertices_n);
    assert(i_p == 3 * tri->triangles_n);
    assert(o == tri->vertices_n);
  }

  return tri;
}

Triangulation *TriangulationFactory::rectangularTorus(Arena *arena, const Geometry *geo, float scale)
{

  auto &tor = geo->rectangularTorus;
  auto segments = sagittaBasedSegmentCount(tor.angle, tor.outer_radius, scale);
  auto samples = segments + 1; // Assumed to be open, add extra sample.

  auto *tri = arena->alloc<Triangulation>();
  tri->error = sagittaBasedError(tor.angle, tor.outer_radius, scale, segments);

  bool shell = true;
  bool cap[2] = {
      true,
      true};

  for (unsigned i = 0; i < 2; i++)
  {
    auto *con = geo->connections[i];
    if (con && con->flags == Connection::Flags::HasRectangularSide)
    {
      if (doInterfacesMatch(geo, con))
      {
        cap[i] = false;
        discardedCaps++;
        // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0xff0000);
      }
    }
  }

  auto h2 = 0.5f * tor.height;
  float square[4][2] = {
      {tor.outer_radius, -h2},
      {tor.inner_radius, -h2},
      {tor.inner_radius, h2},
      {tor.outer_radius, h2},
  };

  // Not closed
  t0.resize(2 * samples + 1);
  for (unsigned i = 0; i < samples; i++)
  {
    t0[2 * i + 0] = std::cos((tor.angle / segments) * i);
    t0[2 * i + 1] = std::sin((tor.angle / segments) * i);
  }

  unsigned l = 0;

  tri->vertices_n = (shell ? 4 * 2 * samples : 0) + (cap[0] ? 4 : 0) + (cap[1] ? 4 : 0);
  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  if (shell)
  {
    for (unsigned i = 0; i < samples; i++)
    {
      float n[4][3] = {
          {0.f, 0.f, -1.f},
          {-t0[2 * i + 0], -t0[2 * i + 1], 0.f},
          {0.f, 0.f, 1.f},
          {t0[2 * i + 0], t0[2 * i + 1], 0.f},
      };

      for (unsigned k = 0; k < 4; k++)
      {
        unsigned kk = (k + 1) & 3;

        tri->vertices[l++] = square[k][0] * t0[2 * i + 0];

        tri->vertices[l++] = square[k][0] * t0[2 * i + 1];

        tri->vertices[l++] = square[k][1];

        tri->vertices[l++] = square[kk][0] * t0[2 * i + 0];

        tri->vertices[l++] = square[kk][0] * t0[2 * i + 1];

        tri->vertices[l++] = square[kk][1];
      }
    }
  }
  if (cap[0])
  {
    for (unsigned k = 0; k < 4; k++)
    {

      tri->vertices[l++] = square[k][0] * t0[0];

      tri->vertices[l++] = square[k][0] * t0[1];

      tri->vertices[l++] = square[k][1];
    }
  }
  if (cap[1])
  {
    for (unsigned k = 0; k < 4; k++)
    {

      tri->vertices[l++] = square[k][0] * t0[2 * (samples - 1) + 0];

      tri->vertices[l++] = square[k][0] * t0[2 * (samples - 1) + 1];

      tri->vertices[l++] = square[k][1];
    }
  }
  assert(l == 3 * tri->vertices_n);

  l = 0;
  unsigned o = 0;

  tri->triangles_n = (shell ? 4 * 2 * (samples - 1) : 0) + (cap[0] ? 2 : 0) + (cap[1] ? 2 : 0);
  tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);

  if (shell)
  {
    for (unsigned i = 0; i + 1 < samples; i++)
    {
      for (unsigned k = 0; k < 4; k++)
      {
        tri->indices[l++] = 4 * 2 * (i + 0) + 0 + 2 * k;
        tri->indices[l++] = 4 * 2 * (i + 0) + 1 + 2 * k;
        tri->indices[l++] = 4 * 2 * (i + 1) + 0 + 2 * k;

        tri->indices[l++] = 4 * 2 * (i + 1) + 0 + 2 * k;
        tri->indices[l++] = 4 * 2 * (i + 0) + 1 + 2 * k;
        tri->indices[l++] = 4 * 2 * (i + 1) + 1 + 2 * k;
      }
    }
    o += 4 * 2 * samples;
  }
  if (cap[0])
  {
    tri->indices[l++] = o + 0;
    tri->indices[l++] = o + 2;
    tri->indices[l++] = o + 1;
    tri->indices[l++] = o + 2;
    tri->indices[l++] = o + 0;
    tri->indices[l++] = o + 3;
    o += 4;
  }
  if (cap[1])
  {
    tri->indices[l++] = o + 0;
    tri->indices[l++] = o + 1;
    tri->indices[l++] = o + 2;
    tri->indices[l++] = o + 2;
    tri->indices[l++] = o + 3;
    tri->indices[l++] = o + 0;
    o += 4;
  }
  assert(o == tri->vertices_n);
  assert(l == 3 * tri->triangles_n);

  return tri;
}

Triangulation *TriangulationFactory::circularTorus(Arena *arena, const Geometry *geo, float scale)
{

  auto &ct = geo->circularTorus;
  unsigned segments_l = sagittaBasedSegmentCount(ct.angle, ct.offset + ct.radius, scale); // large radius, toroidal direction
  unsigned segments_s = sagittaBasedSegmentCount(twopi, ct.radius, scale);                // small radius, poloidal direction

  auto *tri = arena->alloc<Triangulation>();
  tri->error = std::max(sagittaBasedError(ct.angle, ct.offset + ct.radius, scale, segments_l),
                        sagittaBasedError(twopi, ct.radius, scale, segments_s));

  unsigned samples_l = segments_l + 1; // Assumed to be open, add extra sample
  unsigned samples_s = segments_s;     // Assumed to be closed

  bool shell = true;
  bool cap[2] = {true, true};
  for (unsigned i = 0; i < 2; i++)
  {
    auto *con = geo->connections[i];
    if (con && con->flags == Connection::Flags::HasCircularSide)
    {
      if (doInterfacesMatch(geo, con))
      {
        cap[i] = false;
        discardedCaps++;
      }
      else
      {
        // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0x00ffff);
      }
    }
  }

  t0.resize(2 * samples_l);
  for (unsigned i = 0; i < samples_l; i++)
  {
    t0[2 * i + 0] = std::cos((ct.angle / (samples_l - 1.f)) * i);
    t0[2 * i + 1] = std::sin((ct.angle / (samples_l - 1.f)) * i);
  }

  t1.resize(2 * samples_s);
  for (unsigned i = 0; i < samples_s; i++)
  {
    t1[2 * i + 0] = std::cos((twopi / samples_s) * i + geo->sampleStartAngle);
    t1[2 * i + 1] = std::sin((twopi / samples_s) * i + geo->sampleStartAngle);
  }

  tri->vertices_n = ((shell ? samples_l : 0) + (cap[0] ? 1 : 0) + (cap[1] ? 1 : 0)) * samples_s;
  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  tri->triangles_n = (shell ? 2 * (samples_l - 1) * samples_s : 0) + (samples_s - 2) * ((cap[0] ? 1 : 0) + (cap[1] ? 1 : 0));
  tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);

  // generate vertices
  unsigned l = 0;

  if (shell)
  {
    for (unsigned u = 0; u < samples_l; u++)
    {
      for (unsigned v = 0; v < samples_s; v++)
      {

        tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[2 * u + 0]);

        tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[2 * u + 1]);

        tri->vertices[l++] = ct.radius * t1[2 * v + 1];
      }
    }
  }
  if (cap[0])
  {
    for (unsigned v = 0; v < samples_s; v++)
    {

      tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[0]);

      tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[1]);

      tri->vertices[l++] = ct.radius * t1[2 * v + 1];
    }
  }
  if (cap[1])
  {
    unsigned m = 2 * (samples_l - 1);
    for (unsigned v = 0; v < samples_s; v++)
    {

      tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[m + 0]);

      tri->vertices[l++] = ((ct.radius * t1[2 * v + 0] + ct.offset) * t0[m + 1]);

      tri->vertices[l++] = ct.radius * t1[2 * v + 1];
    }
  }
  assert(l == 3 * tri->vertices_n);

  // generate indices
  l = 0;
  unsigned o = 0;
  if (shell)
  {
    for (unsigned u = 0; u + 1 < samples_l; u++)
    {
      for (unsigned v = 0; v + 1 < samples_s; v++)
      {
        tri->indices[l++] = samples_s * (u + 0) + (v + 0);
        tri->indices[l++] = samples_s * (u + 1) + (v + 0);
        tri->indices[l++] = samples_s * (u + 1) + (v + 1);

        tri->indices[l++] = samples_s * (u + 1) + (v + 1);
        tri->indices[l++] = samples_s * (u + 0) + (v + 1);
        tri->indices[l++] = samples_s * (u + 0) + (v + 0);
      }
      tri->indices[l++] = samples_s * (u + 0) + (samples_s - 1);
      tri->indices[l++] = samples_s * (u + 1) + (samples_s - 1);
      tri->indices[l++] = samples_s * (u + 1) + 0;
      tri->indices[l++] = samples_s * (u + 1) + 0;
      tri->indices[l++] = samples_s * (u + 0) + 0;
      tri->indices[l++] = samples_s * (u + 0) + (samples_s - 1);
    }
    o += samples_l * samples_s;
  }

  u1.resize(samples_s);
  u2.resize(samples_s);
  if (cap[0])
  {
    for (unsigned i = 0; i < samples_s; i++)
    {
      u1[i] = o + i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples_s);
    o += samples_s;
  }
  if (cap[1])
  {
    for (unsigned i = 0; i < samples_s; i++)
    {
      u1[i] = o + (samples_s - 1) - i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples_s);
    o += samples_s;
  }
  assert(l == 3 * tri->triangles_n);
  assert(o == tri->vertices_n);

  return tri;
}

Triangulation *TriangulationFactory::snout(Arena *arena, const Geometry *geo, float scale)
{

  auto &sn = geo->snout;
  auto radius_max = std::max(sn.radius_b, sn.radius_t);
  unsigned segments = sagittaBasedSegmentCount(twopi, radius_max, scale);
  unsigned samples = segments; // assumed to be closed

  auto *tri = arena->alloc<Triangulation>();
  tri->error = sagittaBasedError(twopi, radius_max, scale, segments);

  bool shell = true;
  bool cap[2] = {true, true};
  float radii[2] = {geo->snout.radius_b, geo->snout.radius_t};
  for (unsigned i = 0; i < 2; i++)
  {
    auto *con = geo->connections[i];
    if (con && con->flags == Connection::Flags::HasCircularSide)
    {
      if (doInterfacesMatch(geo, con))
      {
        cap[i] = false;
        discardedCaps++;
      }
      else
      {
        // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0x00ffff);
      }
    }
  }

  t0.resize(2 * samples);
  for (unsigned i = 0; i < samples; i++)
  {
    t0[2 * i + 0] = std::cos((twopi / samples) * i + geo->sampleStartAngle);
    t0[2 * i + 1] = std::sin((twopi / samples) * i + geo->sampleStartAngle);
  }
  t1.resize(2 * samples);
  for (unsigned i = 0; i < 2 * samples; i++)
  {
    t1[i] = sn.radius_b * t0[i];
  }
  t2.resize(2 * samples);
  for (unsigned i = 0; i < 2 * samples; i++)
  {
    t2[i] = sn.radius_t * t0[i];
  }

  float h2 = 0.5f * sn.height;
  unsigned l = 0;
  auto ox = 0.5f * sn.offset[0];
  auto oy = 0.5f * sn.offset[1];
  float mb[2] = {std::tan(sn.bshear[0]), std::tan(sn.bshear[1])};
  float mt[2] = {std::tan(sn.tshear[0]), std::tan(sn.tshear[1])};

  tri->vertices_n = (shell ? 2 * samples : 0) + (cap[0] ? samples : 0) + (cap[1] ? samples : 0);
  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  tri->triangles_n = (shell ? 2 * samples : 0) + (cap[0] ? samples - 2 : 0) + (cap[1] ? samples - 2 : 0);
  tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);

  if (shell)
  {
    for (unsigned i = 0; i < samples; i++)
    {
      float xb = t1[2 * i + 0] - ox;
      float yb = t1[2 * i + 1] - oy;
      float zb = -h2 + mb[0] * t1[2 * i + 0] + mb[1] * t1[2 * i + 1];

      float xt = t2[2 * i + 0] + ox;
      float yt = t2[2 * i + 1] + oy;
      float zt = h2 + mt[0] * t2[2 * i + 0] + mt[1] * t2[2 * i + 1];

      float s = (sn.offset[0] * t0[2 * i + 0] + sn.offset[1] * t0[2 * i + 1]);

      l = vertex(tri->vertices, l, xb, yb, zb);
      l = vertex(tri->vertices, l, xt, yt, zt);
    }
  }
  if (cap[0])
  {
    auto nx = std::sin(sn.bshear[0]) * std::cos(sn.bshear[1]);
    auto ny = std::sin(sn.bshear[1]);
    auto nz = -std::cos(sn.bshear[0]) * std::cos(sn.bshear[1]);
    for (unsigned i = 0; cap[0] && i < samples; i++)
    {
      l = vertex(tri->vertices, l,
                 t1[2 * i + 0] - ox,
                 t1[2 * i + 1] - oy,
                 -h2 + mb[0] * t1[2 * i + 0] + mb[1] * t1[2 * i + 1]);
    }
  }
  if (cap[1])
  {
    auto nx = -std::sin(sn.tshear[0]) * std::cos(sn.tshear[1]);
    auto ny = -std::sin(sn.tshear[1]);
    auto nz = std::cos(sn.tshear[0]) * std::cos(sn.tshear[1]);
    for (unsigned i = 0; i < samples; i++)
    {
      l = vertex(tri->vertices, l,
                 t2[2 * i + 0] + ox,
                 t2[2 * i + 1] + oy,
                 h2 + mt[0] * t2[2 * i + 0] + mt[1] * t2[2 * i + 1]);
    }
  }
  assert(l == 3 * tri->vertices_n);

  l = 0;
  unsigned o = 0;
  if (shell)
  {
    for (unsigned i = 0; i < samples; i++)
    {
      unsigned ii = (i + 1) % samples;
      l = quadIndices(tri->indices, l, 0, 2 * i, 2 * ii, 2 * ii + 1, 2 * i + 1);
    }
    o += 2 * samples;
  }

  u1.resize(samples);
  u2.resize(samples);
  if (cap[0])
  {
    for (unsigned i = 0; i < samples; i++)
    {
      u1[i] = o + (samples - 1) - i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples);
    o += samples;
  }
  if (cap[1])
  {
    for (unsigned i = 0; i < samples; i++)
    {
      u1[i] = o + i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples);
    o += samples;
  }
  assert(l == 3 * tri->triangles_n);
  assert(o == tri->vertices_n);

  return tri;
}

Triangulation *TriangulationFactory::cylinder(Arena *arena, const Geometry *geo, float scale)
{

  const auto &cy = geo->cylinder;
  unsigned segments = sagittaBasedSegmentCount(twopi, cy.radius, scale);
  unsigned samples = segments; // Assumed to be closed

  auto *tri = arena->alloc<Triangulation>();
  tri->error = sagittaBasedError(twopi, cy.radius, scale, segments);

  bool shell = true;
  bool cap[2] = {true, true};
  for (unsigned i = 0; i < 2; i++)
  {
    auto *con = geo->connections[i];
    if (con && con->flags == Connection::Flags::HasCircularSide)
    {
      if (doInterfacesMatch(geo, con))
      {
        cap[i] = false;
        discardedCaps++;
      }
      else
      {
        // store->addDebugLine(con->p.data, (con->p.data + 0.05f*con->d).data, 0x00ffff);
      }
    }
  }

  tri->vertices_n = (shell ? 2 * samples : 0) + (cap[0] ? samples : 0) + (cap[1] ? samples : 0);
  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  tri->triangles_n = (shell ? 2 * samples : 0) + (cap[0] ? samples - 2 : 0) + (cap[1] ? samples - 2 : 0);
  tri->indices = (uint32_t *)arena->alloc(3 * sizeof(uint32_t) * tri->triangles_n);

  t0.resize(2 * samples);
  for (unsigned i = 0; i < samples; i++)
  {
    t0[2 * i + 0] = std::cos((twopi / samples) * i + geo->sampleStartAngle);
    t0[2 * i + 1] = std::sin((twopi / samples) * i + geo->sampleStartAngle);
  }
  t1.resize(2 * samples);
  for (unsigned i = 0; i < 2 * samples; i++)
  {
    t1[i] = cy.radius * t0[i];
  }

  float h2 = 0.5f * cy.height;
  unsigned l = 0;

  if (shell)
  {
    for (unsigned i = 0; i < samples; i++)
    {
      l = vertex(tri->vertices, l, t1[2 * i + 0], t1[2 * i + 1], -h2);
      l = vertex(tri->vertices, l, t1[2 * i + 0], t1[2 * i + 1], h2);
    }
  }
  if (cap[0])
  {
    for (unsigned i = 0; cap[0] && i < samples; i++)
    {
      l = vertex(tri->vertices, l, t1[2 * i + 0], t1[2 * i + 1], -h2);
    }
  }
  if (cap[1])
  {
    for (unsigned i = 0; i < samples; i++)
    {
      l = vertex(tri->vertices, l, t1[2 * i + 0], t1[2 * i + 1], h2);
    }
  }
  assert(l == 3 * tri->vertices_n);

  l = 0;
  unsigned o = 0;
  if (shell)
  {
    for (unsigned i = 0; i < samples; i++)
    {
      unsigned ii = (i + 1) % samples;
      l = quadIndices(tri->indices, l, 0, 2 * i, 2 * ii, 2 * ii + 1, 2 * i + 1);
    }
    o += 2 * samples;
  }
  u1.resize(samples);
  u2.resize(samples);
  if (cap[0])
  {
    for (unsigned i = 0; i < samples; i++)
    {
      u1[i] = o + (samples - 1) - i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples);
    o += samples;
  }
  if (cap[1])
  {
    for (unsigned i = 0; i < samples; i++)
    {
      u1[i] = o + i;
    }
    l = tessellateCircle(tri->indices, l, u2.data(), u1.data(), samples);
    o += samples;
  }
  assert(l == 3 * tri->triangles_n);
  assert(o == tri->vertices_n);

  return tri;
}

Triangulation *TriangulationFactory::sphereBasedShape(Arena *arena, const Geometry *geo, float radius, float arc, float shift_z, float scale_z, float scale)
{

  if (!std::isfinite(scale_z))
  {
    scale_z = 0;
  }

  unsigned segments = sagittaBasedSegmentCount(twopi, radius, scale);
  unsigned samples = segments; // Assumed to be closed

  auto *tri = arena->alloc<Triangulation>();
  tri->error = sagittaBasedError(twopi, radius, scale, samples);

  bool is_sphere = false;
  if (pi - 1e-3 <= arc)
  {
    arc = pi;
    is_sphere = true;
  }

  unsigned min_rings = 3; // arc <= half_pi ? 2 : 3;
  unsigned rings = unsigned(std::max(float(min_rings), scale_z * samples * arc * (1.f / twopi)));

  u0.resize(rings);
  t0.resize(2 * rings);
  auto theta_scale = arc / (rings - 1);
  for (unsigned r = 0; r < rings; r++)
  {
    float theta = theta_scale * r;
    t0[2 * r + 0] = std::cos(theta);
    t0[2 * r + 1] = std::sin(theta);
    u0[r] = unsigned(std::max(3.f, t0[2 * r + 1] * samples)); // samples in this ring
  }
  u0[0] = 1;
  if (is_sphere)
  {
    u0[rings - 1] = 1;
  }

  unsigned s = 0;
  for (unsigned r = 0; r < rings; r++)
  {
    s += u0[r];
  }

  tri->vertices_n = s;
  tri->vertices = (float *)arena->alloc(3 * sizeof(float) * tri->vertices_n);

  unsigned l = 0;
  for (unsigned r = 0; r < rings; r++)
  {
    auto nz = t0[2 * r + 0];
    auto z = radius * scale_z * nz + shift_z;
    auto w = t0[2 * r + 1];
    auto n = u0[r];

    auto phi_scale = twopi / n;
    for (unsigned i = 0; i < n; i++)
    {
      auto phi = phi_scale * i + geo->sampleStartAngle;
      auto nx = w * std::cos(phi);
      auto ny = w * std::sin(phi);
      l = vertex(tri->vertices, l, radius * nx, radius * ny, z);
    }
  }
  assert(l == 3 * tri->vertices_n);

  unsigned o_c = 0;
  indices.clear();
  for (unsigned r = 0; r + 1 < rings; r++)
  {
    auto n_c = u0[r];
    auto n_n = u0[r + 1];
    auto o_n = o_c + n_c;

    if (n_c < n_n)
    {
      for (unsigned i_n = 0; i_n < n_n; i_n++)
      {
        unsigned ii_n = (i_n + 1);
        unsigned i_c = (n_c * (i_n + 1)) / n_n;
        unsigned ii_c = (n_c * (ii_n + 1)) / n_n;

        i_c %= n_c;
        ii_c %= n_c;
        ii_n %= n_n;

        if (i_c != ii_c)
        {
          indices.push_back(o_c + i_c);
          indices.push_back(o_n + ii_n);
          indices.push_back(o_c + ii_c);
        }
        assert(i_n != ii_n);
        indices.push_back(o_c + i_c);
        indices.push_back(o_n + i_n);
        indices.push_back(o_n + ii_n);
      }
    }
    else
    {
      for (unsigned i_c = 0; i_c < n_c; i_c++)
      {
        auto ii_c = (i_c + 1);
        unsigned i_n = (n_n * (i_c + 0)) / n_c;
        unsigned ii_n = (n_n * (ii_c + 0)) / n_c;

        i_n %= n_n;
        ii_n %= n_n;
        ii_c %= n_c;

        assert(i_c != ii_c);
        indices.push_back(o_c + i_c);
        indices.push_back(o_n + ii_n);
        indices.push_back(o_c + ii_c);

        if (i_n != ii_n)
        {
          indices.push_back(o_c + i_c);
          indices.push_back(o_n + i_n);
          indices.push_back(o_n + ii_n);
        }
      }
    }
    o_c = o_n;
  }

  tri->triangles_n = unsigned(indices.size() / 3);
  tri->indices = (uint32_t *)arena->dup(indices.data(), 3 * sizeof(uint32_t) * tri->triangles_n);

  return tri;
}

Triangulation *TriangulationFactory::facetGroup(Arena *arena, const Geometry *geo, float /*scale*/)
{
  auto &fg = geo->facetGroup;

  vertices.clear();

  indices.clear();
  for (size_t p = 0; p < fg.polygons_n; p++)
  {
    const Polygon &poly = fg.polygons[p];

    // Verify that all vertices is composed of finite numbers, otherwise skip polygon.
    for (size_t c = 0; c < poly.contours_n; c++)
    {
      const Contour &cont = poly.contours[c];
      for (size_t v = 0; v < 3 * cont.vertices_n; v++)
      {
        if (!std::isfinite(cont.vertices[v]))
        {
          // logger(1, "Encountered malformed vertex data in facet group, skipping polygon");
          goto skip_polygon;
        }
      }
    }

    if (poly.contours_n == 1 && poly.contours[0].vertices_n == 3)
    {
      auto &cont = poly.contours[0];
      auto vo = uint32_t(vertices.size()) / 3;

      vertices.resize(vertices.size() + 3 * 3);

      std::memcpy(vertices.data() + 3 * vo, cont.vertices, 3 * 3 * sizeof(float));

      indices.push_back(vo + 0);
      indices.push_back(vo + 1);
      indices.push_back(vo + 2);
    }
    else if (poly.contours_n == 1 && poly.contours[0].vertices_n == 4)
    {
      auto &cont = poly.contours[0];
      auto &V = cont.vertices;
      auto vo = uint32_t(vertices.size()) / 3;

      vertices.resize(vertices.size() + 3 * 4);

      std::memcpy(vertices.data() + 3 * vo, cont.vertices, 3 * 4 * sizeof(float));

      // find least folding diagonal

      float v01[3], v12[3], v23[3], v30[3];
      sub3(v01, V + 3 * 1, V + 3 * 0);
      sub3(v12, V + 3 * 2, V + 3 * 1);
      sub3(v23, V + 3 * 3, V + 3 * 2);
      sub3(v30, V + 3 * 0, V + 3 * 3);

      float n0[3], n1[3], n2[3], n3[3];
      cross3(n0, v01, v30);
      cross3(n1, v12, v01);
      cross3(n2, v23, v12);
      cross3(n3, v30, v23);

      if (dot3(n0, n2) < dot3(n1, n3))
      {
        indices.push_back(vo + 0);
        indices.push_back(vo + 1);
        indices.push_back(vo + 2);

        indices.push_back(vo + 2);
        indices.push_back(vo + 3);
        indices.push_back(vo + 0);
      }
      else
      {
        indices.push_back(vo + 3);
        indices.push_back(vo + 0);
        indices.push_back(vo + 1);

        indices.push_back(vo + 1);
        indices.push_back(vo + 2);
        indices.push_back(vo + 3);
      }
    }
    else
    {

      bool anyData = false;

      BBox3f bbox = createEmptyBBox3f();
      for (unsigned c = 0; c < poly.contours_n; c++)
      {
        for (unsigned i = 0; i < poly.contours[c].vertices_n; i++)
        {
          const Vec3f pos = makeVec3f(poly.contours[c].vertices + 3 * i);
          engulf(bbox, pos);
        }
      }
      auto m = 0.5f * (Vec3f(bbox.min) + Vec3f(bbox.max));

      auto tess = tessNewTess(nullptr);
      for (unsigned c = 0; c < poly.contours_n; c++)
      {
        auto &cont = poly.contours[c];
        if (cont.vertices_n < 3)
        {
          // logger(1, "Ignoring degenerate contour with %d vertices.", cont.vertices_n);
          continue;
        }
        vec3.resize(cont.vertices_n);
        for (unsigned i = 0; i < cont.vertices_n; i++)
        {
          vec3[i] = makeVec3f(cont.vertices + 3 * i) - m;
        }
        tessAddContour(tess, 3, vec3.data(), 3 * sizeof(float), cont.vertices_n);
        // tessAddContour(tess, 3, cont.vertices, 3 * sizeof(float), cont.vertices_n);
        anyData = true;
      }

      if (anyData == false)
      {
        // logger(1, "Ignoring polygon with no valid contours.");
      }
      else
      {
        if (tessTesselate(tess, TESS_WINDING_ODD, TESS_POLYGONS, 3, 3, nullptr))
        {
          auto vo = uint32_t(vertices.size()) / 3;
          auto vn = unsigned(tessGetVertexCount(tess));

          vertices.resize(vertices.size() + 3 * vn);

          auto *src = tessGetVertices(tess);
          for (unsigned i = 0; i < vn; i++)
          {
            const Vec3f pos = makeVec3f((float *)(src + 3 * i)) + m;
            write(vertices.data() + 3 * (vo + i), pos);
          }

          // std::memcpy(vertices.data() + 3 * vo, tessGetVertices(tess), 3 * vn * sizeof(float));

          auto *elements = tessGetElements(tess);
          auto elements_n = unsigned(tessGetElementCount(tess));
          for (unsigned e = 0; e < elements_n; e++)
          {
            auto ix = elements + 3 * e;
            if ((ix[0] != TESS_UNDEF) && (ix[1] != TESS_UNDEF) && (ix[2] != TESS_UNDEF))
            {
              indices.push_back(ix[0] + vo);
              indices.push_back(ix[1] + vo);
              indices.push_back(ix[2] + vo);
            }
          }
        }
      }

      tessDeleteTess(tess);
    }

  skip_polygon:;
  }

  Triangulation *tri = arena->alloc<Triangulation>();
  tri->error = 0.f;

  if (!indices.empty())
  {
    tri->vertices_n = uint32_t(vertices.size() / 3);
    tri->triangles_n = uint32_t(indices.size() / 3);

    tri->vertices = (float *)arena->dup(vertices.data(), sizeof(float) * 3 * tri->vertices_n);

    tri->indices = (uint32_t *)arena->dup(indices.data(), sizeof(uint32_t) * 3 * tri->triangles_n);

    vertices.clear();

    /* indices.data(); */
  }

  return tri;
}
