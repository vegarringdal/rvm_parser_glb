/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#pragma once
#include <vector>
#include "LinAlg.h"
#include "Arena.h"
#include "TriangulationFactory.h"

class Tessellator
{
public:
  Tessellator() = default;
  Tessellator(const Tessellator &) = delete;
  Tessellator &operator=(const Tessellator &) = delete;

  ~Tessellator();

  Triangulation *geometry(struct Geometry *geometry, Arena *arena, float tolerance);
  Triangulation *tri = nullptr;
  TriangulationFactory *factory = nullptr;
};