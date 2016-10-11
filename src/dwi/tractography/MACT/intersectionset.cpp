/*
 * Copyright (c) 2008-2016 the MRtrix3 contributors
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 * 
 * MRtrix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * For more details, see www.mrtrix.org
 * 
 */


#include "dwi/tractography/MACT/intersectionset.h"


#define EPSILON std::numeric_limits< double >::epsilon()


namespace MR
{

namespace DWI
{

namespace Tractography
{

namespace MACT
{


//
// struct Intersection
//

Intersection::Intersection()
             : _arcLength( std::numeric_limits< double >::infinity() )
{
}


Intersection::Intersection( double arcLength,
                            const Eigen::Vector3d& point,
                            const Tissue_ptr& tissue,
                            Polygon_i polygonIndex )
             : _arcLength( arcLength ),
               _point( point ),
               _tissue( tissue ),
               _polygonIndex( polygonIndex )
{
}

Intersection::Intersection( const Intersection& other )
             : _arcLength( other._arcLength ),
               _point( other._point ),
               _tissue( other._tissue ),
               _polygonIndex( other._polygonIndex )
{
}


Intersection&
Intersection::operator=( const Intersection& other )
{
  _arcLength = other._arcLength;
  _point = other._point;
  _tissue = other._tissue;
  _polygonIndex = other._polygonIndex;
  return *this;
}


size_t Intersection::nearestVertex() const
{
  auto mesh = _tissue->mesh();
  auto triangle = mesh.tri( _polygonIndex );
  double distance0 = ( mesh.vert( triangle[ 0 ] ) - _point ).norm();
  double distance1 = ( mesh.vert( triangle[ 1 ] ) - _point ).norm();
  double distance2 = ( mesh.vert( triangle[ 2 ] ) - _point ).norm();
  if ( distance0 <= distance1 && distance0 <= distance2 )
  {
    return triangle[ 0 ];
  }
  else if ( distance1 <= distance2 )
  {
    return triangle[ 1 ];
  }
  else
  {
    return triangle[ 2 ];
  }
  /*
  / Did not handle equal distances between the point and the vertices
  */
}


//
// class IntersectionSet
//

IntersectionSet::IntersectionSet( const SceneModeller& sceneModeller,
                                  const Eigen::Vector3d& from,
                                  const Eigen::Vector3d& to )
{
  // Collecting the voxels where the ray is passing through
  std::set< Eigen::Vector3i, Vector3iCompare > voxels;
  sceneModeller.bresenhamLine().rayVoxels( from, to, voxels, false );

  // calculating from->to vector and its norm
  Eigen::Vector3d fromTo = to - from;
  double fromToLength = fromTo.norm();

  // looping over voxels and polygons to get interseting points & arc lengths
  std::set< double > arcLengths;
  Eigen::Vector3d v1, v2, v3;
  Eigen::Vector3d intersectionPoint;
  Eigen::Vector3d fromIntersection;
  double fromIntersectionLength;
  bool hasIntersection = false;
  auto v = voxels.begin(), ve = voxels.end();
  while ( v != ve )
  {
    // collecting the list of tissues crossing this voxel
    auto tissues = sceneModeller.tissueLut().getTissues( *v );
    if ( !tissues.empty() )
    {
      auto  t = tissues.begin(), te = tissues.end();
      while ( t != te )
      {
        auto polygonIndices = ( *t )->polygonLut().getTriangles( *v );
        auto p = polygonIndices.begin(), pe = polygonIndices.end();
        while ( p != pe )
        {
          // obtaining the vertices of this polygon
          auto mesh = ( *t )->mesh();
          auto triangle = ( *t )->mesh().tri( *p );
          auto v1 = mesh.vert( triangle[ 0 ] );
          auto v2 = mesh.vert( triangle[ 1 ] );
          auto v3 = mesh.vert( triangle[ 2 ] );
          // perform ray/triangle intersection to find intersecting point
          hasIntersection = rayTriangleIntersection( from, to, v1, v2, v3,
                                                     intersectionPoint );
          if ( hasIntersection )
          {
            fromIntersection = intersectionPoint - from;
            fromIntersectionLength = fromIntersection.norm();
            // check whether the intersecting point is in the segment of
            // [ from , to ]
            if ( ( fromIntersectionLength <= fromToLength ) &&
                 ( fromIntersection.dot( fromTo ) > 0.0 ) )
            {
              Intersection intersection( fromIntersectionLength,
                                         intersectionPoint,
                                         *t, *p );
              arcLengths.insert( fromIntersectionLength );
              _intersections[ fromIntersectionLength ] = intersection;
            }
          }
          ++ p;
        }
        ++ t;
      }
    }
    ++ v;
  }

  // copying the arclength set to a vector
  _arcLengths.resize( arcLengths.size() );
  auto a = arcLengths.begin(), ae = arcLengths.end();
  size_t index = 0;
  while ( a != ae )
  {
    _arcLengths[ index ] = *a;
    ++ a;
    ++ index;
  }
}


IntersectionSet::IntersectionSet( const SceneModeller& sceneModeller,
                                  const Eigen::Vector3d& from,
                                  const Eigen::Vector3d& to,
                                  const Tissue_ptr& targetTissue )
{
  // Collecting the voxels where the ray is passing through
  std::set< Eigen::Vector3i, Vector3iCompare > voxels;
  sceneModeller.bresenhamLine().rayVoxels( from, to, voxels, false );

  // calculating from->to vector and its norm
  Eigen::Vector3d fromTo = to - from;
  double fromToLength = fromTo.norm();

  // looping over voxels and polygons to get interseting points & arc lengths
  std::set< double > arcLengths;
  Eigen::Vector3d v1, v2, v3;
  Eigen::Vector3d intersectionPoint;
  Eigen::Vector3d fromIntersection;
  double fromIntersectionLength;
  bool hasIntersection = false;
  auto v = voxels.begin(), ve = voxels.end();
  while ( v != ve )
  {
    // collecting the list of tissues crossing this voxel
    auto tissues = sceneModeller.tissueLut().getTissues( *v );
    if ( !tissues.empty() )
    {
      auto  t = tissues.begin(), te = tissues.end();
      while ( t != te )
      {
        if ( ( *t )->name() == targetTissue->name() )
        {
          auto polygonIndices = ( *t )->polygonLut().getTriangles( *v );
          auto p = polygonIndices.begin(), pe = polygonIndices.end();
          while ( p != pe )
          {
            auto mesh = ( *t )->mesh();
            auto triangle = ( *t )->mesh().tri( *p );
            auto v1 = mesh.vert( triangle[ 0 ] );
            auto v2 = mesh.vert( triangle[ 1 ] );
            auto v3 = mesh.vert( triangle[ 2 ] );
            // perform ray/triangle intersection to find intersecting point
            hasIntersection = rayTriangleIntersection( from, to, v1, v2, v3,
                                                       intersectionPoint );
            if ( hasIntersection )
            {
              fromIntersection = intersectionPoint - from;
              fromIntersectionLength = fromIntersection.norm();  
              // check whether the intersecting point is in the segment of
              // [ from , to ]
              if ( ( fromIntersectionLength <= fromToLength ) &&
                   ( fromIntersection.dot( fromTo ) > 0.0 ) )
              {
                Intersection intersection( fromIntersectionLength,
                                           intersectionPoint,
                                           *t, *p );
                arcLengths.insert( fromIntersectionLength );
                _intersections[ fromIntersectionLength ] = intersection;
              }
            }
            ++ p;
          }
        }
        ++ t;
      }
    }
    ++ v;
  }

  // copying the arclength set to a vector
  _arcLengths.resize( arcLengths.size() );
  auto a = arcLengths.begin(), ae = arcLengths.end();
  size_t index = 0;
  while ( a != ae )
  {
    _arcLengths[ index ] = *a;
    ++ a;
    ++ index;
  }
}


IntersectionSet::~IntersectionSet()
{
}


size_t IntersectionSet::count() const
{
  return _arcLengths.size();
}


const struct Intersection& IntersectionSet::intersection( size_t index ) const
{
  std::map< double, Intersection >::const_iterator
    i = _intersections.find( _arcLengths[ index ] );
  if ( i == _intersections.end() )
  {
    throw Exception( "intersection not found" );
  }
  return i->second;
}


bool IntersectionSet::rayTriangleIntersection(
                                      const Eigen::Vector3d& from,
                                      const Eigen::Vector3d& to,
                                      const Eigen::Vector3d& vertex1,
                                      const Eigen::Vector3d& vertex2,
                                      const Eigen::Vector3d& vertex3, 
                                      Eigen::Vector3d& intersectionPoint ) const
{
  Eigen::Vector3d u = vertex2 - vertex1;
  Eigen::Vector3d v = vertex3 - vertex1;
  Eigen::Vector3d n = u.cross( v );
  if ( std::abs( n.dot( n ) ) <= EPSILON )
  {
    // cross product equals to zero --> invalid triangle
    throw Exception( "Invalid ray/triangle intersection: invalid triangle" );
  }
  n.normalize();
  Eigen::Vector3d rayDirection = to - from;
  Eigen::Vector3d w0 = from - vertex1;
  double a = -n.dot( w0 );
  double b = n.dot( rayDirection );

  // ray is parallel to triangle plane
  if ( b == 0.0 )
  {
    // ray lies in triangle plane
    if ( a == 0.0 )
    {
      return false;
    }
    // ray disjoint from plane
    else
    {
      return false;
    }
  }
  double r = a / b;
  if ( r < 0.0 )
  {
    return false;
  }

  // processing ray/triangle plane intersection
  intersectionPoint = from + rayDirection * r;

  // is intersection inside the triangle ?
  double uu = u.dot( u );
  double uv = u.dot( v );
  double vv = v.dot( v );
  Eigen::Vector3d w = intersectionPoint - vertex1;
  double wu = w.dot( u );
  double wv = w.dot( v );
  double D = uv * uv - uu * vv;
 
  // get and test parametric coords
  double s = ( uv * wv - vv * wu ) / D;

  // intersection is outside of the triangle
  if ( ( s < 0.0 ) || ( s > 1.0 ) )
  {
    return false;
  }
  double t = ( uv * wu - uu * wv ) / D;
  // intersection is outside of the triangle
  if ( ( t < 0.0 ) || ( ( s + t ) > 1.0 ) )
  {
    return false;
  }
  
  return true;
}


}

}

}

}


#undef EPSILON
