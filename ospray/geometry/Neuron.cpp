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

#include "Neuron.h"
// ospray
#include "ospray/common/Model.h"
#include "ospray/common/Data.h"
// ispc exports
#include "Neuron_ispc.h"
// stl
#include <set>
#include <map>

namespace ospray {
  namespace tubes {
    using std::endl;
    using std::cout;
    using std::ostream;
    using std::flush;

    // float sphereArea = 0.f;
    // float cylArea = 0.f;
    // float coneArea = 0.f;

    inline float sqr(float f) { return f*f; }

    inline bool operator<(const NeuronGeometry::Link &a, const NeuronGeometry::Link &b)
    {
      // return memcmp(&a,&b,sizeof(a)) < 0;
      if (a.first == b.first) return a.second < b.second;
      return a.first < b.first;
    }

    inline bool operator==(const NeuronGeometry::Link &a, const NeuronGeometry::Link &b)
    {
      return memcmp(&a,&b,sizeof(a)) == 0;
      return a.first == b.first && a.second == b.second;
    }


    inline bool operator<(const NeuronGeometry::Node &a, const NeuronGeometry::Node &b)
    {
      if (a.pos.x < b.pos.x) return true;
      if (a.pos.x > b.pos.x) return false;

      if (a.pos.y < b.pos.y) return true;
      if (a.pos.y > b.pos.y) return false;

      if (a.pos.z < b.pos.z) return true;
      if (a.pos.z > b.pos.z) return false;

      if (a.rad < b.rad) return true;
      if (a.rad > b.rad) return false;

      return false;
      // return memcmp(&a,&b,sizeof(a)) < 0;
    }
     inline bool operator==(const NeuronGeometry::Node &a, const NeuronGeometry::Node &b)
    { return a.pos == b.pos && a.rad == b.rad; }


    inline ostream &operator<<(ostream &o, const NeuronGeometry::Node &n)
    { o << n.pos << ":" << n.rad; return o; }
    inline ostream &operator<<(ostream &o, const NeuronGeometry::Link &n)
    { o << n.first << ":" << n.second; return o; }

    bool NeuronGeometry::Comp::initLink(const Node *nodeArray, const Link &_link)
    {
      Link link = _link;

      const Node *node0 = nodeArray+link.first; // current node
      const Node *node1 = nodeArray+link.second; // preIndex node
      if (node0->pos == node1->pos)
        return false;

      if (node0->rad == node1->rad) {
        affine3f unitXfm = frame(node1->pos - node0->pos);
        unitXfm.l.vx = node0->rad * normalize(unitXfm.l.vx);
        unitXfm.l.vy = node0->rad * normalize(unitXfm.l.vy);
        unitXfm.l.vz = node1->pos - node0->pos;
        unitXfm.p = node0->pos;
        this->unitXfm = rcp(unitXfm);
	      this->backXfm = rcp(unitXfm.l).transposed();
        this->type = CYLINDER;
        this->node0  = link.first;
        this->node1  = link.second;
        this->z0 = 0.f;
        this->z1 = 1.f;

        box3f bounds = empty;
        bounds.extend(node0->pos - vec3f(node0->rad));
        bounds.extend(node0->pos + vec3f(node0->rad));
        bounds.extend(node1->pos - vec3f(node1->rad));
        bounds.extend(node1->pos + vec3f(node1->rad));
        //cylArea += area(bounds);

      } else {
        if (node0->rad > node1->rad) {
          std::swap(node0,node1);
          std::swap(link.first,link.second);
        }

        box3f bounds = empty;
        bounds.extend(node0->pos - vec3f(node0->rad));
        bounds.extend(node0->pos + vec3f(node0->rad));
        bounds.extend(node1->pos - vec3f(node1->rad));
        bounds.extend(node1->pos + vec3f(node1->rad));
        //coneArea += area(bounds);

        // cout << "link " << link.first << ":" << link.second << endl;
        const float r1 = node0->rad;
        const float r2 = node1->rad;
        const vec3f P1 = node0->pos;
        const vec3f P2 = node1->pos;

        // cone orientation, normalized
        const vec3f C = normalize(P2-P1);
        const float p1 = r1/(r2-r1) * length(P2-P1);
        const float p2 = p1+length(P2-P1);

        // cone apex
        const vec3f A = P1 - p1*C;

        // z1 and z2
        //const float x1 = sqrtf(sqr(p1)-sqr(r1));
        const float z1 = p1-sqr(r1)/p1;
        const float x2 = sqrtf(sqr(p2)-sqr(r2));
        const float z2 = p2-sqr(r2)/p2;

        // cylinder width
        const float w2 = p2*r2/x2;

        affine3f unitXfm = frame(C);
        unitXfm.l.vx = (w2/p2) * normalize(unitXfm.l.vx);
        unitXfm.l.vy = (w2/p2) * normalize(unitXfm.l.vy);
        unitXfm.l.vz = normalize(unitXfm.l.vz);
        unitXfm.p = A;
       // write comp accel info
        this->type = CONE;
        this->node0  = link.first;
        this->node1  = link.second;
        this->unitXfm = rcp(unitXfm);
        this->backXfm = rcp(unitXfm.l).transposed();
        this->z0 = z1;
        this->z1 = z2;
      }
      return true;
    }

    bool NeuronGeometry::Comp::initNode(const Node *nodeArray, const uint32 nodeID)
    {
      const Node *node = nodeArray+nodeID;
      if (node->rad < 1e-6f)
        return false;

      affine3f unitFrame = one;
      unitFrame.l.vx *= node->rad;
      unitFrame.l.vy *= node->rad;
      unitFrame.l.vz *= node->rad;
      unitFrame.p = node->pos;
      this->unitXfm = rcp(unitFrame);
      this->backXfm = rcp(unitFrame.l).transposed();
      this->type    = SPHERE;
      this->z0      = -1.f;
      this->z1      = 1.f;
      this->node0   = nodeID;
      this->node1   = nodeID;
      box3f bounds = empty;
      bounds.extend(node->pos - vec3f(node->rad));
      bounds.extend(node->pos + vec3f(node->rad));
      //sphereArea += area(bounds);

      return true;
    }

    NeuronGeometry::NeuronGeometry()
      : Geometry()
    {
      ispcEquivalent = ispc::NeuronGeometry_create(this);
    }
    void NeuronGeometry::finalize(Model *model)
    {
      Geometry::finalize(model);
      linkData = getParamData("linkData", nullptr);
      nodeData = getParamData("nodeData", nullptr);
      colorData = getParamData("colorData", nullptr);

      PRINT(linkData->numItems / sizeof(Link));
      PRINT(nodeData->numItems / sizeof(Node));
      PRINT(colorData->numItems / sizeof(vec4f));

      if (!linkData || !nodeData) {
        cout << "#osp:neuron: Neuron geometry with missing links and/or nodes" << endl;
        return;
      }

      /*! \todo find and remove duplicates, and empty links! */
      int maxNumComps = linkData->numItems / sizeof(Link) + nodeData->numItems / sizeof(Node);
	
      const Node *nodeArray = (const Node*)nodeData->data;
      const Link *linkArray = (const Link*)linkData->data;
      colorArray = colorData ? (vec4f*)colorData->data : nullptr;
      
      size_t lineNum = 0;
      size_t numCylinders = 0;
      size_t numSpheres = 0;
      size_t numCones = 0;
      numComps = 0;

      size_t numNodes = nodeData->numItems / sizeof(Node);
      size_t numLinks = linkData->numItems/ sizeof(Link);

      // go through links to find how many lines here
      for (size_t i = 0; i < numLinks; i++) {
        Link link = linkArray[i];
        if (link.first == link.second) {
          lineNum++;
          continue;
        }
        if (link.first < link.second) {
          std::swap(link.first,link.second);
        }
        if (link.second >= numNodes) {
          std::cerr << "something seems wrong with this pdb file ...." << std::endl;
          PRINT(link.second);
          PRINT(numNodes);
          continue;
        }
      }

      comp = new Comp[maxNumComps - lineNum];

      for (size_t i = 0; i < numNodes; i++) {
        if (comp[numComps].initNode(nodeArray,i)) {
          numComps++;
          numSpheres++;
        }
      }

      for (size_t i = 0; i < numLinks; i++) {
        Link link = linkArray[i];
        if (comp[numComps].initLink(nodeArray,link)) {
          if (comp[numComps].type == Comp::CONE)
            numCones++;
          else
            numCylinders++;
          numComps++;
        }
      }
      cout << "#osp:neuron: created all component accels for neuron, found " << numComps << " components" << endl;
      cout << "#osp:neuron: contained " <<  lineNum << " invalid links" << endl;
      cout << "#osp:neuron: generated " << numSpheres << " spheres, " << numCones << " cones, and " << numCylinders << " cylinders" << endl;

      box3f bounds = empty;
      for (size_t i = 0; i < numNodes; i++) {
        bounds.extend(nodeArray[i].pos-vec3f(nodeArray[i].rad));
        bounds.extend(nodeArray[i].pos+vec3f(nodeArray[i].rad));
      }

      // Will's hack
      //bounds.lower.z = (bounds.upper.z + bounds.lower.z) / 2;
      //bounds.lower.z = bounds.lower.z + nodeArray[0].rad;

      ispc::NeuronGeometry_set(getIE(),model->getIE(),
                               (const ispc::box3f&)bounds,
                               (ispc::NeuronNode*)nodeArray,numNodes,
                               (ispc::NeuronLink*)linkArray,numLinks,
                               (ispc::NeuronComp*)comp,numComps,
                               (ispc::vec4f*) colorArray
                              );
    }

    OSP_REGISTER_GEOMETRY(NeuronGeometry,tubes);
  }
} // ::ospray

//extern "C" void ospray_init_module_neuron()
//{
  //static bool initialized = false;
  //if (initialized) return;

  //std::cout << "#osp:neuron: loading 'neuron' module" << std::endl;
  //initialized = true;
//}
