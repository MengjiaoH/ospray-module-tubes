// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

// ospray
#include "ospray/math/vec.ih"
#include "ospray/math/AffineSpace.ih"
#include "ospray/common/Ray.ih"
#include "ospray/geometry/Geometry.ih"
#include "ospray/common/Model.ih"
#include "ospray/transferFunction/LinearTransferFunction.ih"
#include "ospray/common/safe_gather.ih"
// this module
#include "Neuron.ih"
// embree
#include "embree3/rtcore_scene.isph"

//#define NEURON_CSG 1

#define NEURON_CSG 0



//! max num component intersetoins we can store per ray
#define MAX_HIT_RECS 3000
//#define MAX_HIT_RECS 8120

//! flag we 'or' into the HitRec.primID to indicate that this hit is
//! the exit side of the hit point
#define CSG_COMP_EXIT_FLAG (1<<31)

/*! record that keeps track of ONE ray/comp intersection */
struct CSGHitRec {
  //! distance along the ray
  float t;
  //! ID of model component that generated that hit point. also see COMP_HIT_EXIT_FLAG
  uint32 compID;
};

extern "C" unmasked uniform RTCDevice ospray_getEmbreeDevice();

struct CSGHitList {
  //! number of ray-component hits in this list
  varying int numHits;
  //! valid ray interval: start
  varying float t0;
  //! valid ray interval: end
  varying float t1;
  //! list of all intersections with components
  varying CSGHitRec hitRec[MAX_HIT_RECS];
};

inline bool csgFindHit(varying Ray *uniform ray, uniform CSGHitList &hitList)
{
  if (hitList.numHits == 0) return false;
#if 1
  for (uniform int numSorted=1;numSorted < hitList.numHits;numSorted++) {
    for (uniform int i=numSorted;;--i)
      if (i > 0 && hitList.hitRec[i-1].t > hitList.hitRec[i].t) {
        CSGHitRec cur = hitList.hitRec[i];
        hitList.hitRec[i] = hitList.hitRec[i-1];
        hitList.hitRec[i-1] = cur;
      } else {
        break;
      }
  }

  varying int numIn = 0;
  for (uniform int i=0;i<hitList.numHits;i++) {
    CSGHitRec cur = hitList.hitRec[i];
    if (cur.t > hitList.t1) return false;
    if (cur.compID & CSG_COMP_EXIT_FLAG) {
      // this is an exit point
      --numIn;

      if (numIn == 0 && cur.t > hitList.t0) {
        // this is our true and valid hit point
        ray ->geomID = 0;
        ray ->primID = cur.compID - CSG_COMP_EXIT_FLAG;
        ray ->t      = cur.t;
        return true;
      }
    } else {
      // this is a entry point.
      if (numIn == 0 && cur.t > ray ->t0) {
        // this is our true and valid hit point
        ray ->geomID = 0;
        ray ->primID = cur.compID;
        ray ->t      = cur.t;
        return true;
      } else {
        ++numIn;
      }
    }
  }
  return false;
#else
  // print("num hits in hit list %\n",hitList.numHits);
  ray ->geomID = 0;
  ray ->primID = hitList.hitRec[0].compID;
  ray ->t      = hitList.hitRec[0].t;
  ray ->Ng     = make_vec3f(1,1,1);
  return true;
#endif
}

inline void csgAddRec(uniform CSGHitList &hitList,
                      float t, uniform uint32 compID)
{
  if (hitList.numHits >= MAX_HIT_RECS) {
    //print("#osp:neuron: out of entries in CSG hit list\n");
    return;
  }
  hitList.hitRec[hitList.numHits].t = t;
  hitList.hitRec[hitList.numHits].compID = compID;
  hitList.numHits++;
}

inline void csgAddSeg(uniform CSGHitList &hitList,
                      float t0, float t1,
                      uniform uint32 primID)
{
  if (t1 < t0) print("INVALID SEG % %\n",t0,t1);
  csgAddRec(hitList,t0,primID);
  csgAddRec(hitList,t1,primID | CSG_COMP_EXIT_FLAG);
  // PRINT(hitList.numHits);
}


static void NeuronGeometry_postIntersect(uniform Geometry *uniform _self,
                                         uniform Model *uniform model,
                                         varying DifferentialGeometry &dg,
                                         const varying Ray &ray,
                                         uniform int64 flags)
{
  dg.Ng = dg.Ns = ray.Ng;
  if ((flags & DG_COLOR)) {
    //dg.color = make_vec4f(0.2, 0.3, 0.75, 1.0);
    uniform NeuronGeometry *uniform self = (uniform NeuronGeometry *uniform) _self;
    uniform vec4f *uniform color = self->colorArray; //colorArray
#if 0 /* enable this if you want to load one single large file */
    foreach_unique(p in ray.primID) { 
      NeuronComp * uniform comp = self->comp + p;
      //print("primID: %, comp: %\n", p, comp);
      const uint32 node00 = comp -> node0;
      const uint32 node11 = comp -> node1;
      dg.color = color[node00];
      //dg.color = ray.u * color[node00] + (1 - ray.u) * color[node11];
      //print("primID: % node00: %\n", p, node00);      
    }
#else
    NeuronComp * varying comp = self->comp + ray.primID;
    //print("primID: %, comp: %\n", ray.primID, comp);
    const uint32 node00 = comp -> node0;
    const uint32 node11 = comp -> node1;
    dg.color = ray.u * color[node00] + (1 - ray.u) * color[node11];
    //print("primID: % node00: %\n", ray.primID, node00);    
#endif
  }

  //if (flags & DG_MATERIALID) {    
      //dg.materialID = 0;
    ////if (round(dg.color.x * 255) == 228)
      ////dg.materialID = 0;
    ////else if (round(dg.color.x * 255) == 55)
      ////dg.materialID = 1;
    ////else if (round(dg.color.x * 255) == 255)
      ////dg.materialID = 2;
    ////else if (round(dg.color.x * 255) == 77)
      ////dg.materialID = 3;
    ////else if (round(dg.color.x * 255) == 152)
      ////dg.materialID = 4;    
  //}
}

inline void  csgComputeNormal(uniform NeuronGeometry *uniform self,
                              varying Ray *uniform ray)
{
  foreach_unique (primID in ray ->primID) {
    NeuronComp *uniform comp = self->comp + primID;

    const vec3f xf_org = xfmPoint(comp->unitXfm,ray ->org);
    const vec3f xf_dir = xfmVector(comp->unitXfm,ray ->dir);

    // compute normal: eventually move this to postintersect
    if (comp->type == COMP_TYPE_SPHERE) {
      // ------------------------------------------------------------------
      // sphere
      // ------------------------------------------------------------------
      vec3f P_hit = xf_org + ray ->t * xf_dir;
      ray ->u = 1;
      ray ->Ng = P_hit;
      vec3f N_local = P_hit;
      //ray.Ng = normalize(xfmVector(comp->backXfm,N_local));
    } else if (comp->type == COMP_TYPE_CONE) {
      // ------------------------------------------------------------------
      // cone
      // ------------------------------------------------------------------
      vec3f P_hit = xf_org + ray ->t * xf_dir;
       float z = xf_org.z + ray ->t * xf_dir.z;
      ray ->u = min(max( 1 - (z - comp -> z0) / (comp -> z1 - comp -> z0), 0.0f), 1.0f);
      // local normal x and y; z is same length as diagonal for (0,0)-(lnx,lny)
      float lnx = P_hit.x;
      float lny = P_hit.y;
      vec3f N_local = make_vec3f(lnx,lny,-sqrtf(lnx*lnx+lny*lny));
      ray ->Ng = normalize(xfmVector(comp->backXfm,N_local));
    } else {
      // ------------------------------------------------------------------
      // cylinder
      // ------------------------------------------------------------------
      vec3f P_hit = xf_org + ray ->t * xf_dir;
      float z = xf_org.z + ray ->t * xf_dir.z;
      ray ->u = min(max(1 - z, 0.f), 1.0f);
      vec3f N_local = make_vec3f(P_hit.x,P_hit.y,0);
      ray ->Ng = normalize(xfmVector(comp->backXfm,N_local));
    }
  }
}

void NeuronGeometry_bounds(const RTCBoundsFunctionArguments *uniform args)
{
    uniform NeuronGeometry *uniform self = (uniform NeuronGeometry *uniform) args ->geometryUserPtr;
    box3fa *uniform out = (box3fa *uniform)args ->bounds_o;
    *out = box_extend(box_extend(make_box3fa_empty(), make_vec3fa(self->bounds.lower)),
                                 make_vec3fa(self->bounds.upper));
    print("geometry bounds (%, %, %), (%, %, %)\n", out ->lower.x, out ->lower.y, out ->lower.z,
                                                    out ->upper.x, out ->upper.y, out ->upper.z);
}

/*! the 'virtual' traverse function for a pkd geometry */
void NeuronGeometry_intersect_kernel(const struct RTCIntersectFunctionNArguments *uniform args,
                                     const uniform bool isOcclusionTest) 
{
    if(!args ->valid[programIndex]) return;
   
    uniform NeuronGeometry *uniform self = (uniform NeuronGeometry *uniform) args ->geometryUserPtr;
    uniform unsigned int primID = args ->primID;

    varying Ray *uniform ray = (varying Ray *uniform)args ->rayhit;
    float t_in = ray ->t0, t_out = ray ->t;
  
    intersectBox(*ray,self->bounds,t_in,t_out);

    if (t_out < t_in)
        return;

    float t_in_old  = ray ->t0;
    float t_out_old = ray ->t;

#if NEURON_CSG
  uniform CSGHitList hitList;
  hitList.numHits = 0;
  uint32 oldPrimID = ray ->primID;
  ray ->primID = -1;
  ray ->userData = &hitList;
  hitList.t1 = t_out;
  hitList.t0 = t_in;
  ray ->t  = t_out; // inf;
  ray ->t0 = t_in;  //-inf;
  rtcIntersectV(self->compModel,args ->context, (varying RTCRayHit* uniform)ray);
  ray ->t0 = hitList.t0;
  ray ->t  = hitList.t1;
  if (csgFindHit(ray,hitList)) {
      if(isOcclusionTest){
          ray ->t = neg_inf;
      }else{
        csgComputeNormal(self,ray);
        ray ->geomID = self->super.geomID;
      }
  } else {
    ray ->primID = oldPrimID;
    ray ->t0 = t_in_old;
    ray ->t = t_out_old;
  }
#else
  uint32 oldPrimID = ray ->primID;
  // Ray oldRay = ray;
  ray ->primID = -1;
  ray ->t  = t_out; // inf;
  ray ->t0 = t_in;  //-inf;
  rtcIntersectV(self->compModel, args ->context, (varying RTCRayHit* uniform)ray);
  if (ray ->primID >= 0) {
      if(isOcclusionTest){
          ray ->t = neg_inf;
      }else{
        ray ->geomID = self->super.geomID;
      }
  } else {
    //ray = oldRay;
    ray ->primID = oldPrimID;
    ray ->t0 = t_in_old;
    ray ->t = t_out_old;
  }
#endif
}

unmasked void NeuronGeometry_intersect(const struct RTCIntersectFunctionNArguments *uniform args)
{
  //print("ng occ.org % % %\n",ray.org.x,ray.org.y,ray.org.z);
  NeuronGeometry_intersect_kernel(args, false);
}

/*! the 'virtual' occluded function for a pkd geometry */
unmasked void NeuronGeometry_occluded(const struct RTCIntersectFunctionNArguments *uniform args)
{
  //print("ng occ.org % % %\n",ray.org.x,ray.org.y,ray.org.z);
  NeuronGeometry_intersect_kernel(args, true);
}

export void *uniform NeuronGeometry_create(void *uniform cppEquivalent)
{
  NeuronGeometry *uniform self = uniform new uniform NeuronGeometry;
  Geometry_Constructor(&self->super,cppEquivalent,
                       NeuronGeometry_postIntersect,
                       NULL,0,NULL);
  return self;
};

inline vec3f sqrtf(vec3f v)
{ return make_vec3f(sqrtf(v.x),sqrtf(v.y),sqrtf(v.z)); }
inline uniform vec3f sqrtf(uniform vec3f v)
{ return make_vec3f(sqrtf(v.x),sqrtf(v.y),sqrtf(v.z)); }

void NeuronComp_bounds(const RTCBoundsFunctionArguments *uniform args)
{
  uniform NeuronGeometry * uniform self = (uniform NeuronGeometry *uniform)args ->geometryUserPtr;
  uniform int primID = args ->primID;
  NeuronComp *uniform comp = self->comp + primID;

  box3fa *uniform out = (box3fa *uniform)args ->bounds_o;
  if (comp->type == COMP_TYPE_SPHERE) {
    // NODE
    NeuronNode *uniform node = self->node+comp->node0;
    *out = box_extend(box_extend(make_box3fa_empty(), make_vec3fa(node->pos - make_vec3f(node->rad))),
                      make_vec3fa(node->pos + make_vec3f(node->rad)));
  } else {
    // CYLINDER, or CONE
    /*! \todo Use bbox of cap disks, not bbs of cap _spheres_, those
        are much larger than needed */
    //    NeuronLink *uniform link = self->link+comp
    NeuronNode *uniform nodeA = self->node + comp->node0;
    NeuronNode *uniform nodeB = self->node + comp->node1;

    uniform box3fa bbA;
    uniform box3fa bbB;
#if 0
    // iw: this code is broken for cones: the cones actually go all
    // the way to z0, NOT just to where the sphere center is.
    uniform vec3f scale = absf(normalize(nodeA->pos - nodeB->pos));
    scale = sqrtf(make_vec3f(1.f) - scale * scale);
    bbA.lower = nodeA->pos - scale * make_vec3f(nodeA->rad);
    bbA.upper = nodeA->pos + scale * make_vec3f(nodeA->rad);
    bbB.lower = nodeB->pos - scale * make_vec3f(nodeB->rad);
    bbB.upper = nodeB->pos + scale * make_vec3f(nodeB->rad);
#else
    bbA.lower = make_vec3fa(nodeA->pos - make_vec3f(nodeA->rad));
    bbA.upper = make_vec3fa(nodeA->pos + make_vec3f(nodeA->rad));
    bbB.lower = make_vec3fa(nodeB->pos - make_vec3f(nodeB->rad));
    bbB.upper = make_vec3fa(nodeB->pos + make_vec3f(nodeB->rad));
#endif
    *out = box_extend(box_extend(make_box3fa_empty(), make_vec3fa(min(bbA.lower,bbB.lower))),
                                 make_vec3fa(max(bbA.upper,bbB.upper)));
    // CULINDER or CONE
  }
}

inline float sqr(float f) { return f*f; }

/*! intersect a component of a neuron */
void NeuronComp_intersect_kernel(const struct RTCIntersectFunctionNArguments *uniform args,
                                 const uniform bool isOcclusionTest)
{
  uniform NeuronGeometry * uniform self = (uniform NeuronGeometry *uniform)args ->geometryUserPtr;
  uniform int primID = args ->primID;
  
  varying Ray *uniform ray = (varying Ray *uniform)args ->rayhit;

  NeuronComp *uniform comp = self->comp + primID;
  NeuronNode * uniform node0 = self -> node + comp -> node0;
  NeuronNode * uniform node1 = self -> node + comp -> node1;
  vec3f pos0 = node0 -> pos;
  vec3f pos1 = node0 -> pos;

  const vec3f xf_org = xfmPoint(comp->unitXfm,ray ->org);
  const vec3f xf_dir = xfmVector(comp->unitXfm,ray ->dir);

  // coefficient for quadratic eqn
  float a, b, c;

  if (comp->type == COMP_TYPE_SPHERE) {
    // compute coefficients for quadratic eqn
    a = dot(xf_dir,xf_dir);
    b = 2.f*dot(xf_dir,xf_org);
    c = dot(xf_org,xf_org)-1.f;

  } else if (comp->type == COMP_TYPE_CONE) {
    // compute coefficients for quadratic eqn
    a = sqr(xf_dir.x)+sqr(xf_dir.y)-sqr(xf_dir.z);
    b = 2.f*(xf_org.x*xf_dir.x+xf_org.y*xf_dir.y-xf_org.z*xf_dir.z);
    c = sqr(xf_org.x)+sqr(xf_org.y)-sqr(xf_org.z);

  } else if (comp->type == COMP_TYPE_CYLINDER) {
    // compute coefficients for quadratic eqn
    a = sqr(xf_dir.x)+sqr(xf_dir.y);
    b = 2.f*(xf_org.x*xf_dir.x+xf_org.y*xf_dir.y);
    c = sqr(xf_org.x)+sqr(xf_org.y)-1.f;
  }

  // solve quadratic eqn
  const float radical = b*b-4.f*a*c;
  if (radical < 0.f) return;

#if NEURON_CSG
  uniform CSGHitList *uniform hitList
    = (uniform CSGHitList *uniform)ray ->userData;
#endif

  // interval that comp overlaps INFINITE ray
  float ray_t0 = -inf;
  float ray_t1 = +inf;
  float t_cap00;
  float t_cap11;

  // clip to caps
  if (comp->type != COMP_TYPE_SPHERE) {
    const float t_cap_0  = (comp->z0 - xf_org.z) * rcp(xf_dir.z);
    const float t_cap_1  = (comp->z1 - xf_org.z) * rcp(xf_dir.z);

    ray_t0 = min(t_cap_0,t_cap_1);
    ray_t1 = max(t_cap_0,t_cap_1);

    t_cap00 = ray_t0;
    t_cap11 = ray_t1;

#if NEURON_CSG
    if (ray_t0 > hitList->t1) return;
    if (ray_t1 < hitList->t0) return;
#else
    if (ray_t0 > ray ->t) return;
    if (ray_t1 < ray ->t0) return;
#endif
  }

  const float srad = sqrt(radical);

  // [tt0,tt1] are interval that (infinite) cone/cyl/sphere overlaps
  // (infinite) xf_ray line
  const float tt0 = (- b - srad) *rcpf(2.f*a);
  const float tt1 = (- b + srad) *rcpf(2.f*a);

  const float nr_t = min(tt0,tt1);
  const float fr_t = max(tt0,tt1);

  if (comp->type == COMP_TYPE_CONE) {

    // compute (local-frame) z-coords of intersections, to detect if
    // we're on positive-side of cone
    const float nr_z = xf_org.z + nr_t * xf_dir.z;
    const float fr_z = xf_org.z + fr_t * xf_dir.z;
    // print("cyl % -> %, % -> %\n",nr_t,nr_z,fr_t,fr_z);

    if (max(nr_z,fr_z) < 0.f) {
      // both intersections are on wrong side - no intersection at all
      return;
    } else if (min(nr_z,fr_z) > 0.f) {
      // both intersections are on positive side - zz0..zz1 is the interval
      ray_t0 = max(ray_t0,nr_t);
      ray_t1 = min(ray_t1,fr_t);
    } else {
      // one isec on positive side, one on negative; we're inside the
      // infinite code, leaving it, and entering it again on other
      // code side.
      if (nr_z < 0.f)
        // NEAR side is on wrong side, meaning ray interval STARTS on far side
        ray_t0 = max(ray_t0,fr_t)
          ;
      if (fr_z < 0.f)
        // FAR side is on wrong side, meaning ray interval ENDS on near side
        ray_t1 = min(ray_t1,nr_t)
          ;
    }
  } else {
    ray_t0 = max(ray_t0,nr_t);
    ray_t1 = min(ray_t1,fr_t);
  }
  if (ray_t0 >= ray_t1)
    return;

#if NEURON_CSG
  if (ray_t0 > hitList->t1) return;
  if (ray_t1 < hitList->t0) return;
  csgAddSeg(*hitList,ray_t0,ray_t1,primID);
#else
  if (ray_t0 > ray ->t) return;
  if (ray_t1 < ray ->t0) return;

  // check the two hit points on whether they're in the valid interval
  float t_hit;
  if(isOcclusionTest){
      ray ->t = neg_inf;
  }else {
    if (ray_t0 >= ray ->t0 && ray_t0 < ray ->t)
        t_hit = ray_t0;
    else if (ray_t1 >= ray ->t0 && ray_t1 < ray ->t)
        t_hit = ray_t1;
    else
        return;
  }

  ray ->primID = primID;
  ray ->t = t_hit;

  // compute normal: eventually move this to postintersect
  if (comp->type == COMP_TYPE_SPHERE) {
    // ------------------------------------------------------------------
    // sphere
    // ------------------------------------------------------------------
    vec3f P_hit = xf_org + ray ->t * xf_dir;
    ray ->u = 1;
    //ray.Ng = P_hit;
    vec3f N_local = P_hit;
    ray ->Ng = normalize(xfmVector(comp->backXfm,N_local));

  }
   else if (comp->type == COMP_TYPE_CONE) {
    // ------------------------------------------------------------------
    // cone
    // ------------------------------------------------------------------
    vec3f P_hit = xf_org + ray ->t * xf_dir;
    float z = xf_org.z + ray ->t * xf_dir.z;
     ray ->u = min(max( 1 - (z - comp -> z0) / (comp -> z1 - comp -> z0), 0.0f), 1.0f);
    // local normal x and y; z is same length as diagonal for (0,0)-(lnx,lny)
    float lnx = P_hit.x;
    float lny = P_hit.y;
    // affine3f unitXfm = comp -> unitXfm;
    // vec3f BA = unitXfm.l.vz;
    // vec3f A = unitXfm.p;
    // vec3f N_local = A + ray.u * BA;
    // ray.Ng = normalize(xfmVector(comp->backXfm, N_local));
     vec3f N_local = make_vec3f(lnx,lny, -sqrtf(lnx*lnx+lny*lny));
    // vec3f N_local = P_hit;
    ray ->Ng = normalize(xfmVector(comp->backXfm,N_local));
 }
   else {
    // ------------------------------------------------------------------
    // cylinder
    // ------------------------------------------------------------------
    vec3f P_hit = xf_org + ray ->t * xf_dir;
    float z = xf_org.z + ray ->t * xf_dir.z;
    ray ->u = min(max(1 - z, 0.f), 1.0f);
    // affine3f unitXfm = comp -> unitXfm;
    // vec3f BA = pos1 - pos0;
    // vec3f A = pos0;
    // vec3f  PonAxis = A + ray.u * BA;
    // vec3f p = xfmVector(unitXfm, PonAxis);
    // ray.Ng = normalize(p - P_hit);
    vec3f N_local = make_vec3f(P_hit.x,P_hit.y, 0);
    // ray.Ng = N_local;
    ray ->Ng = normalize(xfmVector(comp->backXfm, N_local));
    // if(dot(ray.Ng, ray.dir) > 0){
    //   ray.Ng.x = -ray.Ng.x;       ray.Ng.y = -ray.Ng.y;       ray.Ng.z = -ray.Ng.z;
    // }

  }
#endif
}

unmasked void NeuronComp_intersect(const struct RTCIntersectFunctionNArguments *uniform args)
{
    NeuronComp_intersect_kernel(args, false);
}

unmasked void NeuronComp_occluded(const struct RTCIntersectFunctionNArguments *uniform args)
{
  //print("nc occ.org % % %\n",ray.org.x,ray.org.y,ray.org.z);
  NeuronComp_intersect_kernel(args, true);
}


export void NeuronGeometry_set(void *uniform _self,
                               void *uniform _model,
                               const uniform box3f &bounds,
                               NeuronNode *uniform nodeArray, uniform uint32 numNodes,
                               NeuronLink *uniform linkArray, uniform uint32 numLinks,
                               NeuronComp *uniform compArray, uniform uint32 numComps,
                               uniform vec4f *uniform colorArray
                               )
{
  NeuronGeometry *uniform self = (NeuronGeometry *uniform)_self;
  uniform Model *uniform model = (uniform Model *uniform)_model;

  //-------------------------------------------------------
  // create a geometry for (the neuron as a whole) in the parent model
  //-------------------------------------------------------
  
  self->bounds   = bounds;
  self->link     = linkArray;
  self->numLinks = numLinks;
  self->node     = nodeArray;
  self->numNodes = numNodes;
  self->comp     = compArray;
  self->numComps = numComps;
  self->colorArray = colorArray;
  //print("numNodes = %\n", numNodes);

  RTCGeometry geom = rtcNewGeometry(ispc_embreeDevice(), RTC_GEOMETRY_TYPE_USER);
  uniform uint32 geomID = rtcAttachGeometry(model->embreeSceneHandle, geom);
  
  self->super.model  = model;
  self->super.geomID = geomID;

  rtcSetGeometryUserPrimitiveCount(geom, 1);
  rtcSetGeometryUserData(geom,self);
  rtcSetGeometryBoundsFunction(geom,(uniform RTCBoundsFunction)&NeuronGeometry_bounds, self);
  rtcSetGeometryIntersectFunction(geom, (uniform RTCIntersectFunctionN)&NeuronGeometry_intersect);
  rtcSetGeometryOccludedFunction(geom, (uniform RTCOccludedFunctionN)&NeuronGeometry_occluded);
  rtcCommitGeometry(geom);
  rtcReleaseGeometry(geom);
  //-------------------------------------------------------
  // now, create a new model for all the components
  //-------------------------------------------------------
  print("ispc: numComps %\n", numComps);
  //self->compModel = rtcNewScene(ispc_embreeDevice());
  self ->compModel = rtcNewScene(ospray_getEmbreeDevice());
  RTCGeometry neuronGeom = rtcNewGeometry(ospray_getEmbreeDevice(), RTC_GEOMETRY_TYPE_USER);
  //RTCGeometry neuronGeom = rtcNewGeometry(ispc_embreeDevice(), RTC_GEOMETRY_TYPE_USER);
  self->compGeomID = rtcAttachGeometry(self->compModel, neuronGeom);

  rtcSetGeometryUserData(neuronGeom,self);
  rtcSetGeometryUserPrimitiveCount(neuronGeom, numComps);
  rtcSetGeometryBoundsFunction(neuronGeom, (uniform RTCBoundsFunction)&NeuronComp_bounds, self);
  rtcSetGeometryIntersectFunction(neuronGeom, (uniform RTCIntersectFunctionN)&NeuronComp_intersect);
  rtcSetGeometryOccludedFunction(neuronGeom,(uniform RTCOccludedFunctionN)&NeuronComp_occluded);
  rtcCommitGeometry(neuronGeom);
  rtcReleaseGeometry(neuronGeom);
  rtcCommitScene(self->compModel);
  print("#osp:neuron: Neuron model built\n");
}
