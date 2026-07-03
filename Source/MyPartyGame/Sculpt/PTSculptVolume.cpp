#include "PTSculptVolume.h"
#include "Async/Async.h"

// ─── Marching Cubes lookup tables (Bourke / Lorensen & Cline) ──────────────
// EdgeTable[i]: bitmask of the 12 edges intersected when corners have sign pattern i.
// Bit j is set when edge j crosses the iso-surface (density == 0).
const int32 APTSculptVolume::EdgeTable[256] = {
0x000,0x109,0x203,0x30a,0x406,0x50f,0x605,0x70c,
0x80c,0x905,0xa0f,0xb06,0xc0a,0xd03,0xe09,0xf00,
0x190,0x099,0x393,0x29a,0x596,0x49f,0x795,0x69c,
0x99c,0x895,0xb9f,0xa96,0xd9a,0xc93,0xf99,0xe90,
0x230,0x339,0x033,0x13a,0x636,0x73f,0x435,0x53c,
0xa3c,0xb35,0x83f,0x936,0xe3a,0xf33,0xc39,0xd30,
0x3a0,0x2a9,0x1a3,0x0aa,0x7a6,0x6af,0x5a5,0x4ac,
0xbac,0xaa5,0x9af,0x8a6,0xfaa,0xea3,0xda9,0xca0,
0x460,0x569,0x663,0x76a,0x066,0x16f,0x265,0x36c,
0xc6c,0xd65,0xe6f,0xf66,0x86a,0x963,0xa69,0xb60,
0x5f0,0x4f9,0x7f3,0x6fa,0x1f6,0x0ff,0x3f5,0x2fc,
0xdfc,0xcf5,0xfff,0xef6,0x9fa,0x8f3,0xbf9,0xaf0,
0x650,0x759,0x453,0x55a,0x256,0x35f,0x055,0x15c,
0xe5c,0xf55,0xc5f,0xd56,0xa5a,0xb53,0x859,0x950,
0x7c0,0x6c9,0x5c3,0x4ca,0x3c6,0x2cf,0x1c5,0x0cc,
0xfcc,0xec5,0xdcf,0xcc6,0xbca,0xac3,0x9c9,0x8c0,
0x8c0,0x9c9,0xac3,0xbca,0xcc6,0xdcf,0xec5,0xfcc,
0x0cc,0x1c5,0x2cf,0x3c6,0x4ca,0x5c3,0x6c9,0x7c0,
0x950,0x859,0xb53,0xa5a,0xd56,0xc5f,0xf55,0xe5c,
0x15c,0x055,0x35f,0x256,0x55a,0x453,0x759,0x650,
0xaf0,0xbf9,0x8f3,0x9fa,0xef6,0xfff,0xcf5,0xdfc,
0x2fc,0x3f5,0x0ff,0x1f6,0x6fa,0x7f3,0x4f9,0x5f0,
0xb60,0xa69,0x963,0x86a,0xf66,0xe6f,0xd65,0xc6c,
0x36c,0x265,0x16f,0x066,0x76a,0x663,0x569,0x460,
0xca0,0xda9,0xea3,0xfaa,0x8a6,0x9af,0xaa5,0xbac,
0x4ac,0x5a5,0x6af,0x7a6,0x0aa,0x1a3,0x2a9,0x3a0,
0xd30,0xc39,0xf33,0xe3a,0x936,0x83f,0xb35,0xa3c,
0x53c,0x435,0x73f,0x636,0x13a,0x033,0x339,0x230,
0xe90,0xf99,0xc93,0xd9a,0xa96,0xb9f,0x895,0x99c,
0x69c,0x795,0x49f,0x596,0x29a,0x393,0x099,0x190,
0xf00,0xe09,0xd03,0xc0a,0xb06,0xa0f,0x905,0x80c,
0x70c,0x605,0x50f,0x406,0x30a,0x203,0x109,0x000
};

// TriTable[i]: list of edge-index triples forming triangles, terminated by -1.
const int32 APTSculptVolume::TriTable[256][16] = {
{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,1,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,8,3,9,8,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,2,10,0,2,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{2,8,3,2,10,8,10,9,8,-1,-1,-1,-1,-1,-1,-1},
{3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,11,2,8,11,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,9,0,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,11,2,1,9,11,9,8,11,-1,-1,-1,-1,-1,-1,-1},
{3,10,1,11,10,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,10,1,0,8,10,8,11,10,-1,-1,-1,-1,-1,-1,-1},
{3,9,0,3,11,9,11,10,9,-1,-1,-1,-1,-1,-1,-1},
{9,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,3,0,7,3,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,1,9,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,1,9,4,7,1,7,3,1,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,4,7,3,0,4,1,2,10,-1,-1,-1,-1,-1,-1,-1},
{9,2,10,9,0,2,8,4,7,-1,-1,-1,-1,-1,-1,-1},
{2,10,9,2,9,7,2,7,3,7,9,4,-1,-1,-1,-1},
{8,4,7,3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{11,4,7,11,2,4,2,0,4,-1,-1,-1,-1,-1,-1,-1},
{9,0,1,8,4,7,2,3,11,-1,-1,-1,-1,-1,-1,-1},
{4,7,11,9,4,11,9,11,2,9,2,1,-1,-1,-1,-1},
{3,10,1,3,11,10,7,8,4,-1,-1,-1,-1,-1,-1,-1},
{1,11,10,1,4,11,1,0,4,7,11,4,-1,-1,-1,-1},
{4,7,8,9,0,11,9,11,10,11,0,3,-1,-1,-1,-1},
{4,7,11,4,11,9,9,11,10,-1,-1,-1,-1,-1,-1,-1},
{9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,5,4,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,5,4,1,5,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{8,5,4,8,3,5,3,1,5,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,0,8,1,2,10,4,9,5,-1,-1,-1,-1,-1,-1,-1},
{5,2,10,5,4,2,4,0,2,-1,-1,-1,-1,-1,-1,-1},
{2,10,5,3,2,5,3,5,4,3,4,8,-1,-1,-1,-1},
{9,5,4,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,11,2,0,8,11,4,9,5,-1,-1,-1,-1,-1,-1,-1},
{0,5,4,0,1,5,2,3,11,-1,-1,-1,-1,-1,-1,-1},
{2,1,5,2,5,8,2,8,11,4,8,5,-1,-1,-1,-1},
{10,3,11,10,1,3,9,5,4,-1,-1,-1,-1,-1,-1,-1},
{4,9,5,0,8,1,8,10,1,8,11,10,-1,-1,-1,-1},
{5,4,0,5,0,11,5,11,10,11,0,3,-1,-1,-1,-1},
{5,4,8,5,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1},
{9,7,8,5,7,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,3,0,9,5,3,5,7,3,-1,-1,-1,-1,-1,-1,-1},
{0,7,8,0,1,7,1,5,7,-1,-1,-1,-1,-1,-1,-1},
{1,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,7,8,9,5,7,10,1,2,-1,-1,-1,-1,-1,-1,-1},
{10,1,2,9,5,0,5,3,0,5,7,3,-1,-1,-1,-1},
{8,0,2,8,2,5,8,5,7,10,5,2,-1,-1,-1,-1},
{2,10,5,2,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1},
{7,9,5,7,8,9,3,11,2,-1,-1,-1,-1,-1,-1,-1},
{9,5,7,9,7,2,9,2,0,2,7,11,-1,-1,-1,-1},
{2,3,11,0,1,8,1,7,8,1,5,7,-1,-1,-1,-1},
{11,2,1,11,1,7,7,1,5,-1,-1,-1,-1,-1,-1,-1},
{9,5,8,8,5,7,10,1,3,10,3,11,-1,-1,-1,-1},
{5,7,0,5,0,9,7,11,0,1,0,10,11,10,0,-1},
{11,10,0,11,0,3,10,5,0,8,0,7,5,7,0,-1},
{11,10,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,0,1,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,8,3,1,9,8,5,10,6,-1,-1,-1,-1,-1,-1,-1},
{1,6,5,2,6,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,6,5,1,2,6,3,0,8,-1,-1,-1,-1,-1,-1,-1},
{9,6,5,9,0,6,0,2,6,-1,-1,-1,-1,-1,-1,-1},
{5,9,8,5,8,2,5,2,6,3,2,8,-1,-1,-1,-1},
{2,3,11,10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{11,0,8,11,2,0,10,6,5,-1,-1,-1,-1,-1,-1,-1},
{0,1,9,2,3,11,5,10,6,-1,-1,-1,-1,-1,-1,-1},
{5,10,6,1,9,2,9,11,2,9,8,11,-1,-1,-1,-1},
{6,3,11,6,5,3,5,1,3,-1,-1,-1,-1,-1,-1,-1},
{0,8,11,0,11,5,0,5,1,5,11,6,-1,-1,-1,-1},
{3,11,6,0,3,6,0,6,5,0,5,9,-1,-1,-1,-1},
{6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1},
{5,10,6,4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,3,0,4,7,3,6,5,10,-1,-1,-1,-1,-1,-1,-1},
{1,9,0,5,10,6,8,4,7,-1,-1,-1,-1,-1,-1,-1},
{10,6,5,1,9,7,1,7,3,7,9,4,-1,-1,-1,-1},
{6,1,2,6,5,1,4,7,8,-1,-1,-1,-1,-1,-1,-1},
{1,2,5,5,2,6,3,0,4,3,4,7,-1,-1,-1,-1},
{8,4,7,9,0,5,0,6,5,0,2,6,-1,-1,-1,-1},
{7,3,9,7,9,4,3,2,9,5,9,6,2,6,9,-1},
{3,11,2,7,8,4,10,6,5,-1,-1,-1,-1,-1,-1,-1},
{5,10,6,4,7,2,4,2,0,2,7,11,-1,-1,-1,-1},
{0,1,9,4,7,8,2,3,11,5,10,6,-1,-1,-1,-1},
{9,2,1,9,11,2,9,4,11,7,11,4,5,10,6,-1},
{8,4,7,3,11,5,3,5,1,5,11,6,-1,-1,-1,-1},
{5,1,11,5,11,6,1,0,11,7,11,4,0,4,11,-1},
{0,5,9,0,6,5,0,3,6,11,6,3,8,4,7,-1},
{6,5,9,6,9,11,4,7,9,7,11,9,-1,-1,-1,-1},
{10,4,9,6,4,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,10,6,4,9,10,0,8,3,-1,-1,-1,-1,-1,-1,-1},
{10,0,1,10,6,0,6,4,0,-1,-1,-1,-1,-1,-1,-1},
{8,3,1,8,1,6,8,6,4,6,1,10,-1,-1,-1,-1},
{1,4,9,1,2,4,2,6,4,-1,-1,-1,-1,-1,-1,-1},
{3,0,8,1,2,9,2,4,9,2,6,4,-1,-1,-1,-1},
{0,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{8,3,2,8,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1},
{10,4,9,10,6,4,11,2,3,-1,-1,-1,-1,-1,-1,-1},
{0,8,2,2,8,11,4,9,10,4,10,6,-1,-1,-1,-1},
{3,11,2,0,1,6,0,6,4,6,1,10,-1,-1,-1,-1},
{6,4,1,6,1,10,4,8,1,2,1,11,8,11,1,-1},
{9,6,4,9,3,6,9,1,3,11,6,3,-1,-1,-1,-1},
{8,11,1,8,1,0,11,6,1,9,1,4,6,4,1,-1},
{3,11,6,3,6,0,0,6,4,-1,-1,-1,-1,-1,-1,-1},
{6,4,8,11,6,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{7,10,6,7,8,10,8,9,10,-1,-1,-1,-1,-1,-1,-1},
{0,7,3,0,10,7,0,9,10,6,7,10,-1,-1,-1,-1},
{10,6,7,1,10,7,1,7,8,1,8,0,-1,-1,-1,-1},
{10,6,7,10,7,1,1,7,3,-1,-1,-1,-1,-1,-1,-1},
{1,2,6,1,6,8,1,8,9,8,6,7,-1,-1,-1,-1},
{2,6,9,2,9,1,6,7,9,0,9,3,7,3,9,-1},
{7,8,0,7,0,6,6,0,2,-1,-1,-1,-1,-1,-1,-1},
{7,3,2,6,7,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{2,3,11,10,6,8,10,8,9,8,6,7,-1,-1,-1,-1},
{2,0,7,2,7,11,0,9,7,6,7,10,9,10,7,-1},
{1,8,0,1,7,8,1,10,7,6,7,10,2,3,11,-1},
{11,2,1,11,1,7,10,6,1,6,7,1,-1,-1,-1,-1},
{8,9,6,8,6,7,9,1,6,11,6,3,1,3,6,-1},
{0,9,1,11,6,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{7,8,0,7,0,6,3,11,0,11,6,0,-1,-1,-1,-1},
{7,11,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,0,8,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,1,9,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{8,1,9,8,3,1,11,7,6,-1,-1,-1,-1,-1,-1,-1},
{10,1,2,6,11,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,3,0,8,6,11,7,-1,-1,-1,-1,-1,-1,-1},
{2,9,0,2,10,9,6,11,7,-1,-1,-1,-1,-1,-1,-1},
{6,11,7,2,10,3,10,8,3,10,9,8,-1,-1,-1,-1},
{7,2,3,6,2,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{7,0,8,7,6,0,6,2,0,-1,-1,-1,-1,-1,-1,-1},
{2,7,6,2,3,7,0,1,9,-1,-1,-1,-1,-1,-1,-1},
{1,6,2,1,8,6,1,9,8,8,7,6,-1,-1,-1,-1},
{10,7,6,10,1,7,1,3,7,-1,-1,-1,-1,-1,-1,-1},
{10,7,6,1,7,10,1,8,7,1,0,8,-1,-1,-1,-1},
{0,3,7,0,7,10,0,10,9,6,10,7,-1,-1,-1,-1},
{7,6,10,7,10,8,8,10,9,-1,-1,-1,-1,-1,-1,-1},
{6,8,4,11,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,6,11,3,0,6,0,4,6,-1,-1,-1,-1,-1,-1,-1},
{8,6,11,8,4,6,9,0,1,-1,-1,-1,-1,-1,-1,-1},
{9,4,6,9,6,3,9,3,1,11,3,6,-1,-1,-1,-1},
{6,8,4,6,11,8,2,10,1,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,3,0,11,0,6,11,0,4,6,-1,-1,-1,-1},
{4,11,8,4,6,11,0,2,9,2,10,9,-1,-1,-1,-1},
{10,9,3,10,3,2,9,4,3,11,3,6,4,6,3,-1},
{8,2,3,8,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1},
{0,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,9,0,2,3,4,2,4,6,4,3,8,-1,-1,-1,-1},
{1,9,4,1,4,2,2,4,6,-1,-1,-1,-1,-1,-1,-1},
{8,1,3,8,6,1,8,4,6,6,10,1,-1,-1,-1,-1},
{10,1,0,10,0,6,6,0,4,-1,-1,-1,-1,-1,-1,-1},
{4,6,3,4,3,8,6,10,3,0,3,9,10,9,3,-1},
{10,9,4,6,10,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,9,5,7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,4,9,5,11,7,6,-1,-1,-1,-1,-1,-1,-1},
{5,0,1,5,4,0,7,6,11,-1,-1,-1,-1,-1,-1,-1},
{11,7,6,8,3,4,3,5,4,3,1,5,-1,-1,-1,-1},
{9,5,4,10,1,2,7,6,11,-1,-1,-1,-1,-1,-1,-1},
{6,11,7,1,2,10,0,8,3,4,9,5,-1,-1,-1,-1},
{7,6,11,5,4,10,4,2,10,4,0,2,-1,-1,-1,-1},
{3,4,8,3,5,4,3,2,5,10,5,2,11,7,6,-1},
{7,2,3,7,6,2,5,4,9,-1,-1,-1,-1,-1,-1,-1},
{9,5,4,0,8,6,0,6,2,6,8,7,-1,-1,-1,-1},
{3,6,2,3,7,6,1,5,0,5,4,0,-1,-1,-1,-1},
{6,2,8,6,8,7,2,1,8,4,8,5,1,5,8,-1},
{9,5,4,10,1,6,1,7,6,1,3,7,-1,-1,-1,-1},
{1,6,10,1,7,6,1,0,7,8,7,0,9,5,4,-1},
{4,0,10,4,10,5,0,3,10,6,10,7,3,7,10,-1},
{7,6,10,7,10,8,5,4,10,4,8,10,-1,-1,-1,-1},
{6,9,5,6,11,9,11,8,9,-1,-1,-1,-1,-1,-1,-1},
{3,6,11,0,6,3,0,5,6,0,9,5,-1,-1,-1,-1},
{0,11,8,0,5,11,0,1,5,5,6,11,-1,-1,-1,-1},
{6,11,3,6,3,5,5,3,1,-1,-1,-1,-1,-1,-1,-1},
{1,2,10,9,5,11,9,11,8,11,5,6,-1,-1,-1,-1},
{0,11,3,0,6,11,0,9,6,5,6,9,1,2,10,-1},
{11,8,5,11,5,6,8,0,5,10,5,2,0,2,5,-1},
{6,11,3,6,3,5,2,10,3,10,5,3,-1,-1,-1,-1},
{5,8,9,5,2,8,5,6,2,3,8,2,-1,-1,-1,-1},
{9,5,6,9,6,0,0,6,2,-1,-1,-1,-1,-1,-1,-1},
{1,5,8,1,8,0,5,6,8,3,8,2,6,2,8,-1},
{1,5,6,2,1,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,3,6,1,6,10,3,8,6,5,6,9,8,9,6,-1},
{10,1,0,10,0,6,9,5,0,5,6,0,-1,-1,-1,-1},
{0,3,8,5,6,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{10,5,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{11,5,10,7,5,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{11,5,10,11,7,5,8,3,0,-1,-1,-1,-1,-1,-1,-1},
{5,11,7,5,10,11,1,9,0,-1,-1,-1,-1,-1,-1,-1},
{10,7,5,10,11,7,9,8,1,8,3,1,-1,-1,-1,-1},
{11,1,2,11,7,1,7,5,1,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,1,2,7,1,7,5,7,2,11,-1,-1,-1,-1},
{9,7,5,9,2,7,9,0,2,2,11,7,-1,-1,-1,-1},
{7,5,2,7,2,11,5,9,2,3,2,8,9,8,2,-1},
{2,5,10,2,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1},
{8,2,0,8,5,2,8,7,5,10,2,5,-1,-1,-1,-1},
{9,0,1,5,10,3,5,3,7,3,10,2,-1,-1,-1,-1},
{9,8,2,9,2,1,8,7,2,10,2,5,7,5,2,-1},
{1,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,8,7,0,7,1,1,7,5,-1,-1,-1,-1,-1,-1,-1},
{9,0,3,9,3,5,5,3,7,-1,-1,-1,-1,-1,-1,-1},
{9,8,7,5,9,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{5,8,4,5,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1},
{5,0,4,5,11,0,5,10,11,11,3,0,-1,-1,-1,-1},
{0,1,9,8,4,10,8,10,11,10,4,5,-1,-1,-1,-1},
{10,11,4,10,4,5,11,3,4,9,4,1,3,1,4,-1},
{2,5,1,2,8,5,2,11,8,4,5,8,-1,-1,-1,-1},
{0,4,11,0,11,3,4,5,11,2,11,1,5,1,11,-1},
{0,2,5,0,5,9,2,11,5,4,5,8,11,8,5,-1},
{9,4,5,2,11,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{2,5,10,3,5,2,3,4,5,3,8,4,-1,-1,-1,-1},
{5,10,2,5,2,4,4,2,0,-1,-1,-1,-1,-1,-1,-1},
{3,10,2,3,5,10,3,8,5,4,5,8,0,1,9,-1},
{5,10,2,5,2,4,1,9,2,9,4,2,-1,-1,-1,-1},
{8,4,5,8,5,3,3,5,1,-1,-1,-1,-1,-1,-1,-1},
{0,4,5,1,0,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{8,4,5,8,5,3,9,0,5,0,3,5,-1,-1,-1,-1},
{9,4,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,11,7,4,9,11,9,10,11,-1,-1,-1,-1,-1,-1,-1},
{0,8,3,4,9,7,9,11,7,9,10,11,-1,-1,-1,-1},
{1,10,11,1,11,4,1,4,0,7,4,11,-1,-1,-1,-1},
{3,1,4,3,4,8,1,10,4,7,4,11,10,11,4,-1},
{4,11,7,9,11,4,9,2,11,9,1,2,-1,-1,-1,-1},
{9,7,4,9,11,7,9,1,11,2,11,1,0,8,3,-1},
{11,7,4,11,4,2,2,4,0,-1,-1,-1,-1,-1,-1,-1},
{11,7,4,11,4,2,8,3,4,3,2,4,-1,-1,-1,-1},
{2,9,10,2,7,9,2,3,7,7,4,9,-1,-1,-1,-1},
{9,10,7,9,7,4,10,2,7,8,7,0,2,0,7,-1},
{3,7,10,3,10,2,7,4,10,1,10,0,4,0,10,-1},
{1,10,2,8,7,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,9,1,4,1,7,7,1,3,-1,-1,-1,-1,-1,-1,-1},
{4,9,1,4,1,7,0,8,1,8,7,1,-1,-1,-1,-1},
{4,0,3,7,4,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{4,8,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{9,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,0,9,3,9,11,11,9,10,-1,-1,-1,-1,-1,-1,-1},
{0,1,10,0,10,8,8,10,11,-1,-1,-1,-1,-1,-1,-1},
{3,1,10,11,3,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,2,11,1,11,9,9,11,8,-1,-1,-1,-1,-1,-1,-1},
{3,0,9,3,9,11,1,2,9,2,11,9,-1,-1,-1,-1},
{0,2,11,8,0,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{3,2,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{2,3,8,2,8,10,10,8,9,-1,-1,-1,-1,-1,-1,-1},
{9,10,2,0,9,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{2,3,8,2,8,10,0,1,8,1,10,8,-1,-1,-1,-1},
{1,10,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{1,3,8,9,1,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,9,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{0,3,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
};

// ─── Constructor ─────────────────────────────────────────────────────────────

APTSculptVolume::APTSculptVolume()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCastShadow(true);
    Mesh->bCastDynamicShadow = true;

    // Caja que define el lienzo de esculpido. Visible en editor, oculta en juego.
    BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
    BoundsBox->SetupAttachment(Mesh);
    BoundsBox->SetRelativeLocation(FVector::ZeroVector); // centrado en el origen
    BoundsBox->SetBoxExtent(FVector(480.f));             // lienzo por defecto (960 UU/lado)
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsBox->ShapeColor = FColor(0, 200, 255);
    BoundsBox->bDrawOnlyIfSelected = false; // siempre visible en editor
    BoundsBox->SetHiddenInGame(true);
}

void APTSculptVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // La caja define el lienzo. La resolución (VoxelSize) es independiente del
    // tamaño de la caja: agrandarla da más volumen esculpible, no menos detalle.
}

void APTSculptVolume::BeginPlay()
{
    Super::BeginPlay();
    Field.VoxelSize = VoxelSize;
    if (ClayMaterial)
        Mesh->SetMaterial(0, ClayMaterial);
}

void APTSculptVolume::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!Field.HasDirty() || bRebuildInProgress) return;
    TimeSinceRebuild += DeltaTime;
    if (TimeSinceRebuild >= RebuildInterval)
    {
        TimeSinceRebuild = 0.f;
        RebuildDirty();
    }
}

// ─── Coordenadas ──────────────────────────────────────────────────────────────

FVector APTSculptVolume::WorldToCell(FVector W) const
{
    return GetActorTransform().InverseTransformPosition(W) / VoxelSize;
}

void APTSculptVolume::CellBounds(FIntVector& OutMin, FIntVector& OutMax) const
{
    const FVector Center = BoundsBox ? BoundsBox->GetRelativeLocation() : FVector::ZeroVector;
    const FVector Half   = BoundsBox ? BoundsBox->GetUnscaledBoxExtent() : FVector(480.f);
    const FVector LMin = (Center - Half) / VoxelSize;
    const FVector LMax = (Center + Half) / VoxelSize;
    OutMin = FIntVector(FMath::FloorToInt(LMin.X), FMath::FloorToInt(LMin.Y), FMath::FloorToInt(LMin.Z));
    OutMax = FIntVector(FMath::CeilToInt(LMax.X),  FMath::CeilToInt(LMax.Y),  FMath::CeilToInt(LMax.Z));
}

float APTSculptVolume::SampleWorldDensity(FVector WorldPos) const
{
    const FVector C = WorldToCell(WorldPos);
    return Field.SampleSDF(C.X, C.Y, C.Z);
}

FLinearColor APTSculptVolume::SampleWorldColor(FVector WorldPos) const
{
    const FVector C = WorldToCell(WorldPos);
    return FLinearColor(Field.GetColor(FMath::RoundToInt(C.X), FMath::RoundToInt(C.Y), FMath::RoundToInt(C.Z)));
}

// ─── Stamp operations ─────────────────────────────────────────────────────────

float APTSculptVolume::StampSDF(EPTStampShape Shape, FVector P, float HalfSize)
{
    switch (Shape)
    {
    case EPTStampShape::Sphere:
        return HalfSize - P.Size();

    case EPTStampShape::Cube:
        return FMath::Min3(HalfSize - FMath::Abs(P.X),
                           HalfSize - FMath::Abs(P.Y),
                           HalfSize - FMath::Abs(P.Z));

    case EPTStampShape::Cylinder: // eje Z
    {
        float radial = HalfSize - FVector2D(P.X, P.Y).Size();
        float axial  = HalfSize - FMath::Abs(P.Z);
        return FMath::Min(radial, axial);
    }

    case EPTStampShape::TriPrism: // sección triangular equilátera en XY, extruida en Z
    {
        const float k = 1.7320508f; // sqrt(3)
        float px = FMath::Abs(P.X) - HalfSize;
        float py = P.Y + HalfSize / k;
        if (px + k * py > 0.f)
        {
            float tx = px; float ty = py;
            px = (tx - k * ty) * 0.5f;
            py = (-k * tx - ty) * 0.5f;
        }
        px -= FMath::Clamp(px, -2.f * HalfSize, 0.f);
        float triSDF = -FMath::Sqrt(px*px + py*py) * FMath::Sign(py);
        float axial  = HalfSize - FMath::Abs(P.Z);
        return FMath::Min(triSDF, axial);
    }

    default:
        return HalfSize - P.Size();
    }
}

void APTSculptVolume::ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                                  EPTEditMode Mode, FLinearColor PaintColor)
{
    const FVector GC = WorldToCell(WorldPos);
    const float HalfSize = (Size * 0.5f) / VoxelSize; // radio en celdas
    const int32 R = FMath::CeilToInt(HalfSize) + 1;

    // Clamp al lienzo (BoundsBox).
    FIntVector BMin, BMax;
    CellBounds(BMin, BMax);
    const int32 x0 = FMath::Max(BMin.X, FMath::FloorToInt(GC.X) - R);
    const int32 y0 = FMath::Max(BMin.Y, FMath::FloorToInt(GC.Y) - R);
    const int32 z0 = FMath::Max(BMin.Z, FMath::FloorToInt(GC.Z) - R);
    const int32 x1 = FMath::Min(BMax.X, FMath::CeilToInt(GC.X) + R);
    const int32 y1 = FMath::Min(BMax.Y, FMath::CeilToInt(GC.Y) + R);
    const int32 z1 = FMath::Min(BMax.Z, FMath::CeilToInt(GC.Z) + R);

    bool bAnyChange = false;
    for (int32 z = z0; z <= z1; ++z)
    for (int32 y = y0; y <= y1; ++y)
    for (int32 x = x0; x <= x1; ++x)
    {
        const FVector LP(x - GC.X, y - GC.Y, z - GC.Z);
        const float sdf  = StampSDF(Shape, LP, HalfSize);
        const float prev = Field.GetSDF(x, y, z);

        if (Mode == EPTEditMode::Add)
        {
            const float next = FMath::Max(prev, sdf);
            if (next != prev) { Field.SetSDF(x, y, z, next); bAnyChange = true; }
            if (sdf > -1.f)
            {
                const float blend = FMath::Clamp(sdf + 1.f, 0.f, 1.f) * 0.8f;
                const FColor cur = Field.GetColor(x, y, z);
                Field.SetColor(x, y, z, FLinearColor::LerpUsingHSV(FLinearColor(cur), PaintColor, blend).ToFColor(true));
                bAnyChange = true;
            }
        }
        else if (Mode == EPTEditMode::Erase)
        {
            const float next = FMath::Min(prev, -sdf);
            if (next != prev) { Field.SetSDF(x, y, z, next); bAnyChange = true; }
        }
        else // Paint
        {
            if (prev > 0.f && sdf > -1.f)
            {
                const float blend = FMath::Clamp(sdf + 1.f, 0.f, 1.f) * 0.5f;
                const FColor cur = Field.GetColor(x, y, z);
                Field.SetColor(x, y, z, FLinearColor::LerpUsingHSV(FLinearColor(cur), PaintColor, blend).ToFColor(true));
                bAnyChange = true;
            }
        }
    }

    if (!bAnyChange) return;

    // Marca todos los bricks que cubren el bbox del sello (+1 celda de borde para
    // que los bricks vecinos re-mallen su costura). Bricks vacíos hacen early-out.
    const int32 BS = FPTBrick::BrickSize;
    auto FloorDivBS = [BS](int32 v){ return (v >= 0) ? v / BS : -(( -v + BS - 1) / BS); };
    const int32 bx0 = FloorDivBS(x0 - 1), bx1 = FloorDivBS(x1 + 1);
    const int32 by0 = FloorDivBS(y0 - 1), by1 = FloorDivBS(y1 + 1);
    const int32 bz0 = FloorDivBS(z0 - 1), bz1 = FloorDivBS(z1 + 1);
    for (int32 bz = bz0; bz <= bz1; ++bz)
    for (int32 by = by0; by <= by1; ++by)
    for (int32 bx = bx0; bx <= bx1; ++bx)
        Field.MarkDirty(FPTBrickKey(bx, by, bz));
}

// ─── Marching Cubes ───────────────────────────────────────────────────────────

FVector APTSculptVolume::Interp(FVector P1, FVector P2, float V1, float V2)
{
    if (FMath::Abs(V1 - V2) < 1e-5f) return (P1 + P2) * 0.5f;
    return P1 + (-V1 / (V2 - V1)) * (P2 - P1);
}

FColor APTSculptVolume::InterpColor(FLinearColor C1, FLinearColor C2, float V1, float V2)
{
    if (FMath::Abs(V1 - V2) < 1e-5f) return ((C1 + C2) * 0.5f).ToFColor(true);
    float t = FMath::Clamp(-V1 / (V2 - V1), 0.f, 1.f);
    return FLinearColor::LerpUsingHSV(C1, C2, t).ToFColor(true);
}

void APTSculptVolume::RunMarchingCubes(const TArray<float>& G, const TArray<FLinearColor>& CG,
                                        int32 GS, float VoxSz,
                                        int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1,
                                        TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                        TArray<FVector>& OutNormals, TArray<FColor>& OutColors)
{
    OutVerts.Reset(); OutTris.Reset(); OutNormals.Reset(); OutColors.Reset();

    static const int32 CX[8]   = {0,1,1,0,0,1,1,0};
    static const int32 CY[8]   = {0,0,1,1,0,0,1,1};
    static const int32 CZ[8]   = {0,0,0,0,1,1,1,1};
    static const int32 EV0[12] = {0,1,2,3,4,5,6,7,0,1,2,3};
    static const int32 EV1[12] = {1,2,3,0,5,6,7,4,4,5,6,7};

    auto GV = [&](int32 x,int32 y,int32 z)->float {
        bool ok = x>=0&&x<GS&&y>=0&&y<GS&&z>=0&&z<GS;
        return ok ? G[x + y*GS + z*GS*GS] : -1.f;
    };
    auto GC = [&](int32 x,int32 y,int32 z)->FLinearColor {
        bool ok = x>=0&&x<GS&&y>=0&&y<GS&&z>=0&&z<GS;
        return (ok && CG.Num() > 0) ? CG[x + y*GS + z*GS*GS] : FLinearColor::White;
    };

    for (int32 z=Z0; z<Z1; ++z)
    for (int32 y=Y0; y<Y1; ++y)
    for (int32 x=X0; x<X1; ++x)
    {
        float val[8]; FVector pos[8]; int32 cubeIdx=0;
        for (int32 c=0;c<8;++c)
        {
            val[c] = GV(x+CX[c], y+CY[c], z+CZ[c]);
            pos[c] = FVector(x+CX[c], y+CY[c], z+CZ[c]) * VoxSz;
            if (val[c] > 0.f) cubeIdx |= (1<<c);
        }
        if (EdgeTable[cubeIdx] == 0) continue;

        FVector edgePos[12];
        FColor  edgeCol[12];
        for (int32 e=0;e<12;++e)
            if (EdgeTable[cubeIdx]&(1<<e))
            {
                int32 a=EV0[e], b=EV1[e];
                edgePos[e] = Interp(pos[a], pos[b], val[a], val[b]);
                edgeCol[e] = InterpColor(GC(x+CX[a],y+CY[a],z+CZ[a]),
                                         GC(x+CX[b],y+CY[b],z+CZ[b]),
                                         val[a], val[b]);
            }

        for (int32 t=0; TriTable[cubeIdx][t]!=-1; t+=3)
        {
            int32 e0=TriTable[cubeIdx][t], e1=TriTable[cubeIdx][t+1], e2=TriTable[cubeIdx][t+2];
            FVector v0=edgePos[e0], v1=edgePos[e1], v2=edgePos[e2];
            FVector N = FVector::CrossProduct(v1-v0, v2-v0).GetSafeNormal();
            int32 base=OutVerts.Num();
            OutVerts.Add(v0);  OutVerts.Add(v1);  OutVerts.Add(v2);
            OutNormals.Add(N); OutNormals.Add(N); OutNormals.Add(N);
            OutColors.Add(edgeCol[e0]); OutColors.Add(edgeCol[e1]); OutColors.Add(edgeCol[e2]);
            OutTris.Add(base); OutTris.Add(base+1); OutTris.Add(base+2);
        }
    }

    // Vertex welding: promediar normales y colores por posición compartida (smooth shading).
    const int32 NumVerts = OutVerts.Num();
    TMap<FIntVector, FVector>  AccumNormal;
    TMap<FIntVector, FVector4> AccumColor;
    TMap<FIntVector, int32>    WeldCount;
    AccumNormal.Reserve(NumVerts);
    AccumColor.Reserve(NumVerts);
    WeldCount.Reserve(NumVerts);
    for (int32 i = 0; i < NumVerts; ++i)
    {
        FIntVector key(FMath::RoundToInt(OutVerts[i].X),
                       FMath::RoundToInt(OutVerts[i].Y),
                       FMath::RoundToInt(OutVerts[i].Z));
        AccumNormal.FindOrAdd(key) += OutNormals[i];
        FColor& c = OutColors[i];
        AccumColor.FindOrAdd(key) += FVector4(c.R, c.G, c.B, c.A);
        WeldCount.FindOrAdd(key)++;
    }
    for (int32 i = 0; i < NumVerts; ++i)
    {
        FIntVector key(FMath::RoundToInt(OutVerts[i].X),
                       FMath::RoundToInt(OutVerts[i].Y),
                       FMath::RoundToInt(OutVerts[i].Z));
        OutNormals[i] = AccumNormal[key].GetSafeNormal();
        float cnt = WeldCount[key];
        FVector4 ac = AccumColor[key] / cnt;
        OutColors[i] = FColor((uint8)FMath::Clamp(ac.X,0.f,255.f),
                              (uint8)FMath::Clamp(ac.Y,0.f,255.f),
                              (uint8)FMath::Clamp(ac.Z,0.f,255.f),
                              (uint8)FMath::Clamp(ac.W,0.f,255.f));
    }
}

void APTSculptVolume::BuildStampPreview(EPTStampShape Shape, float Size, float VoxSz,
                                         TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                         TArray<FVector>& OutNormals)
{
    const float HalfSize = (Size * 0.5f) / VoxSz;
    const int32 PGS = FMath::Clamp(2 * (FMath::CeilToInt(HalfSize) + 2), 6, 48);
    const float Center = PGS * 0.5f;

    TArray<float> PGrid;
    TArray<FLinearColor> PColors;
    PGrid.SetNumZeroed(PGS * PGS * PGS);
    // PColors vacio → MC usará blanco por defecto

    for (int32 z=0; z<PGS; ++z)
    for (int32 y=0; y<PGS; ++y)
    for (int32 x=0; x<PGS; ++x)
    {
        FVector LP(x - Center, y - Center, z - Center);
        PGrid[x + y*PGS + z*PGS*PGS] = StampSDF(Shape, LP, HalfSize);
    }

    TArray<FColor> Colors;
    RunMarchingCubes(PGrid, PColors, PGS, VoxSz, 0, 0, 0, PGS-1, PGS-1, PGS-1,
                     OutVerts, OutTris, OutNormals, Colors);

    // Centrar la preview en el origen
    FVector Offset(Center * VoxSz);
    for (FVector& V : OutVerts) V -= Offset;
}

void APTSculptVolume::RebuildDirty()
{
    bRebuildInProgress = true;

    // Snapshot de cada brick dirty en el GameThread (sin tocar el mapa desde el thread).
    TArray<FPTBrickKey> Keys;
    Field.TakeDirty(Keys);

    TSharedPtr<TArray<FPTSculptField::FBrickSnapshot>, ESPMode::ThreadSafe> Snaps =
        MakeShared<TArray<FPTSculptField::FBrickSnapshot>, ESPMode::ThreadSafe>();
    Snaps->Reserve(Keys.Num());
    for (const FPTBrickKey& K : Keys)
    {
        FPTSculptField::FBrickSnapshot S;
        S.Section = Field.SectionIndex(K);
        Field.SnapshotBrick(K, S); // actualiza flatness del brick
        Snaps->Add(MoveTemp(S));
    }
    // Decidir el paso con flatness ya actualizada de todos los bricks dirty.
    for (FPTSculptField::FBrickSnapshot& S : *Snaps)
        S.Step = Field.DecideStep(S.Key);

    UProceduralMeshComponent* MeshPtr = Mesh;
    UMaterialInterface* Mat = ClayMaterial;

    Async(EAsyncExecution::ThreadPool, [this, MeshPtr, Mat, Snaps]()
    {
        TSharedPtr<TArray<FPTBrickMesh>, ESPMode::ThreadSafe> Results =
            MakeShared<TArray<FPTBrickMesh>, ESPMode::ThreadSafe>();
        Results->Reserve(Snaps->Num());
        for (const FPTSculptField::FBrickSnapshot& S : *Snaps)
        {
            FPTBrickMesh M;
            FPTSculptField::MeshBrick(S, M);
            Results->Add(MoveTemp(M));
        }

        AsyncTask(ENamedThreads::GameThread, [this, MeshPtr, Mat, Results]()
        {
            for (FPTBrickMesh& M : *Results)
            {
                // Colisión OFF durante el esculpido: el hit usa raymarching SDF,
                // y cocinar colisión por rebuild era el mayor costo de FPS.
                MeshPtr->CreateMeshSection(M.Section, M.Verts, M.Tris, M.Normals, {}, M.Colors, {}, /*collision=*/false);
                if (Mat) MeshPtr->SetMaterial(M.Section, Mat);
            }
            bRebuildInProgress = false;
        });
    });
}

// ─── RPCs de replicación ──────────────────────────────────────────────────────

bool APTSculptVolume::Server_ApplyStamp_Validate(FVector, EPTStampShape, float, EPTEditMode, FLinearColor)
{
    return true;
}

void APTSculptVolume::Server_ApplyStamp_Implementation(FVector WorldPos, EPTStampShape Shape,
                                                        float Size, EPTEditMode Mode, FLinearColor PaintColor)
{
    Multicast_ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor);
}

void APTSculptVolume::Multicast_ApplyStamp_Implementation(FVector WorldPos, EPTStampShape Shape,
                                                           float Size, EPTEditMode Mode, FLinearColor PaintColor)
{
    ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor);
}
