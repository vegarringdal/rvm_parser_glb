/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#pragma once

#include "LinAlg.h"
#include "Arena.h"
#include <vector>

class TriangulationFactory
{
public:
  float tolerance = 0.01;
  unsigned sagittaBasedSegmentCount(float arc, float radius, float scale);

  float sagittaBasedError(float arc, float radius, float scale, unsigned samples);

  Triangulation *pyramid(Arena *arena, const Geometry *geo, float scale);

  Triangulation *box(Arena *arena, const Geometry *geo, float scale);

  Triangulation *rectangularTorus(Arena *arena, const Geometry *geo, float scale);

  Triangulation *circularTorus(Arena *arena, const Geometry *geo, float scale);

  Triangulation *snout(Arena *arena, const Geometry *geo, float scale);

  Triangulation *cylinder(Arena *arena, const Geometry *geo, float scale);

  Triangulation *facetGroup(Arena *arena, const Geometry *geo, float scale);

  Triangulation *sphereBasedShape(Arena *arena, const Geometry *geo, float radius, float arc, float shift_z, float scale_z, float scale);

  unsigned discardedCaps = 0;

private:
  unsigned minSamples = 3;
  unsigned maxSamples = 100;

  std::vector<float> vertices;
  std::vector<Vec3f> vec3;
  std::vector<uint32_t> indices;

  std::vector<unsigned> u0;
  std::vector<uint32_t> u1;
  std::vector<uint32_t> u2;
  std::vector<float> t0;
  std::vector<float> t1;
  std::vector<float> t2;
};