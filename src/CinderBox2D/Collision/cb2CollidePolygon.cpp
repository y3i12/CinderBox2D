/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include <CinderBox2D/Collision/cb2Collision.h>
#include <CinderBox2D/Collision/Shapes/cb2PolygonShape.h>

// Find the max separation between poly1 and poly2 using edge normals from poly1.
static float b2FindMaxSeparation(int* edgeIndex,
								 const b2PolygonShape* poly1, const b2Transform& xf1,
								 const b2PolygonShape* poly2, const b2Transform& xf2)
{
	int count1 = poly1->m_count;
	int count2 = poly2->m_count;
	const ci::Vec2f* n1s = poly1->m_normals;
	const ci::Vec2f* v1s = poly1->m_vertices;
	const ci::Vec2f* v2s = poly2->m_vertices;
	b2Transform xf = b2MulT(xf2, xf1);

	int bestIndex = 0;
	float maxSeparation = -b2_maxFloat;
	for (int i = 0; i < count1; ++i)
	{
		// Get poly1 normal in frame2.
		ci::Vec2f n = b2Mul(xf.q, n1s[i]);
		ci::Vec2f v1 = b2Mul(xf, v1s[i]);

		// Find deepest point for normal i.
		float si = b2_maxFloat;
		for (int j = 0; j < count2; ++j)
		{
			float sij = b2Dot(n, v2s[j] - v1);
			if (sij < si)
	{
				si = sij;
	}
	}

		if (si > maxSeparation)
		{
			maxSeparation = si;
			bestIndex = i;
		}
	}

	*edgeIndex = bestIndex;
	return maxSeparation;
}

static void b2FindIncidentEdge(b2ClipVertex c[2],
							 const b2PolygonShape* poly1, const b2Transform& xf1, int edge1,
							 const b2PolygonShape* poly2, const b2Transform& xf2)
{
	const ci::Vec2f* normals1 = poly1->m_normals;

	int count2 = poly2->m_count;
	const ci::Vec2f* vertices2 = poly2->m_vertices;
	const ci::Vec2f* normals2 = poly2->m_normals;

	b2Assert(0 <= edge1 && edge1 < poly1->m_count);

	// Get the normal of the reference edge in poly2's frame.
	ci::Vec2f normal1 = b2MulT(xf2.q, b2Mul(xf1.q, normals1[edge1]));

	// Find the incident edge on poly2.
	int index = 0;
	float minDot = b2_maxFloat;
	for (int i = 0; i < count2; ++i)
	{
		float dot = b2Dot(normal1, normals2[i]);
		if (dot < minDot)
		{
			minDot = dot;
			index = i;
		}
	}

	// Build the clip vertices for the incident edge.
	int i1 = index;
	int i2 = i1 + 1 < count2 ? i1 + 1 : 0;

	c[0].v = b2Mul(xf2, vertices2[i1]);
	c[0].id.cf.indexA = (unsigned char)edge1;
	c[0].id.cf.indexB = (unsigned char)i1;
	c[0].id.cf.typeA = b2ContactFeature::e_face;
	c[0].id.cf.typeB = b2ContactFeature::e_vertex;

	c[1].v = b2Mul(xf2, vertices2[i2]);
	c[1].id.cf.indexA = (unsigned char)edge1;
	c[1].id.cf.indexB = (unsigned char)i2;
	c[1].id.cf.typeA = b2ContactFeature::e_face;
	c[1].id.cf.typeB = b2ContactFeature::e_vertex;
}

// Find edge normal of max separation on A - return if separating axis is found
// Find edge normal of max separation on B - return if separation axis is found
// Choose reference edge as min(minA, minB)
// Find incident edge
// Clip

// The normal points from 1 to 2
void b2CollidePolygons(b2Manifold* manifold,
					  const b2PolygonShape* polyA, const b2Transform& xfA,
					  const b2PolygonShape* polyB, const b2Transform& xfB)
{
	manifold->pointCount = 0;
	float totalRadius = polyA->m_radius + polyB->m_radius;

	int edgeA = 0;
	float separationA = b2FindMaxSeparation(&edgeA, polyA, xfA, polyB, xfB);
	if (separationA > totalRadius)
		return;

	int edgeB = 0;
	float separationB = b2FindMaxSeparation(&edgeB, polyB, xfB, polyA, xfA);
	if (separationB > totalRadius)
		return;

	const b2PolygonShape* poly1;	// reference polygon
	const b2PolygonShape* poly2;	// incident polygon
	b2Transform xf1, xf2;
	int edge1;		// reference edge
	unsigned char flip;
	const float k_tol = 0.1f * b2_linearSlop;

	if (separationB > separationA + k_tol)
	{
		poly1 = polyB;
		poly2 = polyA;
		xf1 = xfB;
		xf2 = xfA;
		edge1 = edgeB;
		manifold->type = b2Manifold::e_faceB;
		flip = 1;
	}
	else
	{
		poly1 = polyA;
		poly2 = polyB;
		xf1 = xfA;
		xf2 = xfB;
		edge1 = edgeA;
		manifold->type = b2Manifold::e_faceA;
		flip = 0;
	}

	b2ClipVertex incidentEdge[2];
	b2FindIncidentEdge(incidentEdge, poly1, xf1, edge1, poly2, xf2);

	int count1 = poly1->m_count;
	const ci::Vec2f* vertices1 = poly1->m_vertices;

	int iv1 = edge1;
	int iv2 = edge1 + 1 < count1 ? edge1 + 1 : 0;

	ci::Vec2f v11 = vertices1[iv1];
	ci::Vec2f v12 = vertices1[iv2];

	ci::Vec2f localTangent = v12 - v11;
	localTangent.normalize();
	
	ci::Vec2f localNormal = b2Cross(localTangent, 1.0f);
	ci::Vec2f planePoint = 0.5f * (v11 + v12);

	ci::Vec2f tangent = b2Mul(xf1.q, localTangent);
	ci::Vec2f normal = b2Cross(tangent, 1.0f);
	
	v11 = b2Mul(xf1, v11);
	v12 = b2Mul(xf1, v12);

	// Face offset.
	float frontOffset = b2Dot(normal, v11);

	// Side offsets, extended by polytope skin thickness.
	float sideOffset1 = -b2Dot(tangent, v11) + totalRadius;
	float sideOffset2 = b2Dot(tangent, v12) + totalRadius;

	// Clip incident edge against extruded edge1 side edges.
	b2ClipVertex clipPoints1[2];
	b2ClipVertex clipPoints2[2];
	int np;

	// Clip to box side 1
	np = b2ClipSegmentToLine(clipPoints1, incidentEdge, -tangent, sideOffset1, iv1);

	if (np < 2)
		return;

	// Clip to negative box side 1
	np = b2ClipSegmentToLine(clipPoints2, clipPoints1,  tangent, sideOffset2, iv2);

	if (np < 2)
	{
		return;
	}

	// Now clipPoints2 contains the clipped points.
	manifold->localNormal = localNormal;
	manifold->localPoint = planePoint;

	int pointCount = 0;
	for (int i = 0; i < b2_maxManifoldPoints; ++i)
	{
		float separation = b2Dot(normal, clipPoints2[i].v) - frontOffset;

		if (separation <= totalRadius)
		{
			b2ManifoldPoint* cp = manifold->points + pointCount;
			cp->localPoint = b2MulT(xf2, clipPoints2[i].v);
			cp->id = clipPoints2[i].id;
			if (flip)
			{
				// Swap features
				b2ContactFeature cf = cp->id.cf;
				cp->id.cf.indexA = cf.indexB;
				cp->id.cf.indexB = cf.indexA;
				cp->id.cf.typeA = cf.typeB;
				cp->id.cf.typeB = cf.typeA;
			}
			++pointCount;
		}
	}

	manifold->pointCount = pointCount;
}
