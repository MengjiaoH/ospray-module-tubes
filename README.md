OSPRay Generalized "Tubes" Geometry Module
=======================

What are tubes?
---------------

"Tubes" are basically a wide generalization of ospray's "StreamLines"
geometry type. Whereas ospray's original stream lines only support a
fixed radius for all control points, "tubes" allow a per-node radius, bifurcations 
and correct transparency.

Compile with OSPRay
-------------------
Clone the repo into module folder inside OSPRay. 
Change "OSPRAY_MODULE_TUBES" to "ON".
Re-compile.

Data Structure
-------------- 
In this module, the name of geometry is "tubes" and we need commit a "nodeData" and a "linkData" to OSPRay.
Users can also commit "colorData" to have color per node.
"nodeData" and "linkData" are both vector contains a list of node and a list of line. 
The basic node struct contains: position, radius.
The basic line struct contains: current node index, its predecessor index.

All struct you can find in 'Neuron.h' under ospray/geometry folder.


