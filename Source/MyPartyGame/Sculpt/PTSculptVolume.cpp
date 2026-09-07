#include "PTSculptVolume.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureResource.h"
#include "RenderUtils.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "PTSculptGameState.h"          // CurrentSculptor / TurnPhase para el bloqueo del área
#include "../Lobby/PTPlayerState.h"     // tipo completo de CurrentSculptor (APTPlayerState)
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Components/PrimitiveComponent.h"
#include "../Lobby/PTLobbyCharacter.h" // BuildEyesSection (esferas/mesh de ojos)

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
    Mesh->bUseAsyncCooking = true; // cocinar colisión fuera del hilo del juego (barato)

    // Caja que define el lienzo de esculpido. Visible en editor, oculta en juego.
    BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
    BoundsBox->SetupAttachment(Mesh);
    BoundsBox->SetRelativeLocation(FVector::ZeroVector); // centrado en el origen
    BoundsBox->SetBoxExtent(FVector(480.f));             // lienzo por defecto (960 UU/lado)
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsBox->ShapeColor = FColor(0, 200, 255);
    BoundsBox->bDrawOnlyIfSelected = false; // siempre visible en editor
    BoundsBox->SetHiddenInGame(true);

    // Malla de ojos (aparte de la arcilla, mismo espacio local que el volumen).
    EyesMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EyesMesh"));
    EyesMesh->SetupAttachment(Mesh);
    EyesMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EyesMesh->SetCastShadow(false);
}

void APTSculptVolume::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTSculptVolume, Eyes);
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
    Field.VoxelSize        = VoxelSize;
    Field.DisplaySmoothing = DisplaySmoothing;
    InitColorField();
    SetupClayMID();
    if (UMaterialInterface* M = ClayMaterialOverride ? ClayMaterialOverride
                                : (ClayMID ? (UMaterialInterface*)ClayMID : ClayMaterial))
        Mesh->SetMaterial(0, M);
}

void APTSculptVolume::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Apagar la fuente de partículas cuando pasa un ratito sin sellar (soltaste el click). No se
    // desactiva de golpe al soltar para que la "fuente" no corte seca; se deja terminar el chorro.
    if (SculptFX && SculptFX->IsActive() && GetWorld()
        && GetWorld()->GetTimeSeconds() - SculptFXLastTime > SculptFXIdleTimeout)
    {
        SculptFX->Deactivate(); // deja de emitir; las partículas vivas terminan su vida solas
    }

    // Reloj del material del clay: el brillo de la arcilla nueva se desvanece comparando este
    // "NowTime" con el tiempo de agregado horneado en la UV0 de cada vértice.
    if (ClayMID && GetWorld())
        ClayMID->SetScalarParameterValue(TEXT("NowTime"), GetWorld()->GetTimeSeconds());

    // Subida throttled de la textura de pintura.
    if (bPaintDirty)
    {
        TimeSincePaintUpload += DeltaTime;
        if (TimeSincePaintUpload >= PaintUploadInterval)
        {
            TimeSincePaintUpload = 0.f;
            bPaintDirty = false;
            UploadColorField();
        }
    }

    // Bloqueo del ÁREA de esculpido: solo el escultor del turno puede entrar al cubo; los demás rebotan.
    // (Solo aplica en gameplay: en el lobby no hay APTSculptGameState y el cubo nunca bloquea.)
    BoundaryAccum += DeltaTime;
    if (BoundaryAccum >= 0.2f) { BoundaryAccum = 0.f; UpdateSculptBoundaryCollision(); }

    // ¿Hay algo que re-mallar? La base, alguna capa de detalle, o el octree (modo SVO).
    bool bAnyDirty = bUseSVO ? bSVODirty : Field.HasDirty();
    for (const TSharedPtr<FPTSculptField>& L : DetailFields)
        if (L.IsValid() && L->HasDirty()) { bAnyDirty = true; break; }

    if (!bAnyDirty || bRebuildInProgress) return;
    TimeSinceRebuild += DeltaTime;
    if (TimeSinceRebuild >= RebuildInterval)
    {
        TimeSinceRebuild = 0.f;
        RebuildDirty();
    }
}

void APTSculptVolume::UpdateSculptBoundaryCollision()
{
    UWorld* W = GetWorld();
    if (!W || !BoundsBox) return;

    const APTSculptGameState* GS = W->GetGameState<APTSculptGameState>();
    if (!GS)
    {
        // No es el mundo de gameplay (ej: la cabeza en el lobby): el cubo NUNCA bloquea.
        if (bBoundaryOn) { BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); bBoundaryOn = false; }
        return;
    }

    // Encender el bloqueo del cubo (una vez): bloquea Pawns; el escultor lo IGNORA (abajo, por pawn).
    if (!bBoundaryOn)
    {
        BoundsBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BoundsBox->SetCollisionObjectType(ECC_WorldStatic);
        BoundsBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        BoundsBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        bBoundaryOn = true;
    }

    const APlayerState* Sculptor = GS->CurrentSculptor;
    const bool bTurnActive = (GS->TurnPhase == EPTTurnPhase::Drawing || GS->TurnPhase == EPTTurnPhase::ChoosingWord);

    // Recorrer TODOS los jugadores (por PlayerArray, no por PlayerControllers): así en CADA máquina
    // —server y todos los clientes— la copia del escultor (aunque sea un proxy simulado que no controlás)
    // ignora el cubo. Antes solo se lo sacaba al pawn LOCAL, así que en los demás clientes el proxy del
    // escultor chocaba con el cubo y saltaba (jitter/"teletransporte") cuando entraba al área.
    for (APlayerState* PS : GS->PlayerArray)
    {
        APawn* P = PS ? PS->GetPawn() : nullptr;
        if (!P) continue;
        UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(P->GetRootComponent()); // capsule del Character
        if (!Root) continue;
        const bool bCanEnter = (!bTurnActive) || (PS == Sculptor);
        Root->IgnoreActorWhenMoving(this, bCanEnter);
    }
}

// ─── Pintura por campo de color 3D DISPERSO (bricks + page table + atlas) ─────
// El color vive en un campo 3D real (cero bleed) pero solo se allocan bricks donde
// se pinta. Layout GPU:
//   • Page table: textura 2D empaquetada (w=BrickDim.X, h=BrickDim.Y*BrickDim.Z),
//     cada texel R16 = slot+1 (0 = vacío). Point-sampled.
//   • Atlas: cada brick = tile de CB × (CB*CB); el voxel (lx,ly,lz) va al texel
//     (tileX*CB + lx, tileY*CB*CB + lz*CB + ly). RGBA8 bilinear.

void APTSculptVolume::InitColorField()
{
    // Lienzo en espacio local (del BoundsBox).
    FIntVector BMin, BMax; CellBounds(BMin, BMax);
    CanvasMinLocal  = FVector(BMin) * VoxelSize;
    CanvasSizeLocal = FVector(BMax - BMin) * VoxelSize;
    if (CanvasSizeLocal.GetMin() <= 0.f) CanvasSizeLocal = FVector(960.f);

    const float CV = FMath::Max(ColorVoxel, 0.5f);
    ColorVoxDim = FIntVector(
        FMath::Max(1, FMath::CeilToInt(CanvasSizeLocal.X / CV)),
        FMath::Max(1, FMath::CeilToInt(CanvasSizeLocal.Y / CV)),
        FMath::Max(1, FMath::CeilToInt(CanvasSizeLocal.Z / CV)));
    ColorBrickDim = FIntVector(
        FMath::DivideAndRoundUp(ColorVoxDim.X, CB),
        FMath::DivideAndRoundUp(ColorVoxDim.Y, CB),
        FMath::DivideAndRoundUp(ColorVoxDim.Z, CB));

    // Page table (empaquetada 2D).
    const int32 PGW = ColorBrickDim.X;
    const int32 PGH = ColorBrickDim.Y * ColorBrickDim.Z;
    PageBuf.Init(0.f, PGW * PGH);
    bPageDirty = true;

    // Atlas: capacidad = MaxColorBricks, tiles cuadriculados.
    const int32 Cap  = FMath::Max(MaxColorBricks, 16);
    AtlasTilesPerRow = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt((float)Cap)), 1, 256);
    const int32 Rows = FMath::DivideAndRoundUp(Cap, AtlasTilesPerRow);
    AtlasW = AtlasTilesPerRow * CB;
    AtlasH = Rows * CB * CB;
    AtlasCapacity = AtlasTilesPerRow * Rows;
    AtlasBuf.Init(FColor(0,0,0,0), AtlasW * AtlasH);
    BrickSlot.Empty();
    DirtyTiles.Empty();
    DirtyPageIdx.Reset();
    FreeSlots.Reset();
    SlotUsed.Init(0, AtlasCapacity);
    NextSlot = 0;
    bPaintDirty = true;

    // Texturas GPU.
    PageTex = UTexture2D::CreateTransient(PGW, PGH, PF_R32_FLOAT);
    PageTex->SRGB = false; PageTex->Filter = TF_Nearest; PageTex->AddressX = TA_Clamp; PageTex->AddressY = TA_Clamp;
    PageTex->UpdateResource();

    AtlasTex = UTexture2D::CreateTransient(AtlasW, AtlasH, PF_B8G8R8A8);
    AtlasTex->SRGB = true; AtlasTex->Filter = TF_Bilinear; AtlasTex->AddressX = TA_Clamp; AtlasTex->AddressY = TA_Clamp;
    AtlasTex->UpdateResource();
}

void APTSculptVolume::SetupClayMID()
{
    if (!ClayMaterial) return;
    ClayMID = UMaterialInstanceDynamic::Create(ClayMaterial, this);
    if (!ClayMID) return;
    ClayMID->SetTextureParameterValue(TEXT("PageTable"), PageTex);
    ClayMID->SetTextureParameterValue(TEXT("Atlas"),     AtlasTex);
    ClayMID->SetVectorParameterValue(TEXT("CanvasMin"),  CanvasMinLocal);
    ClayMID->SetScalarParameterValue(TEXT("ColorVoxel"), FMath::Max(ColorVoxel, 0.5f));
    ClayMID->SetVectorParameterValue(TEXT("VoxDim"),     FVector(ColorVoxDim));
    ClayMID->SetVectorParameterValue(TEXT("BrickDim"),   FVector(ColorBrickDim));
    ClayMID->SetScalarParameterValue(TEXT("PageW"),      ColorBrickDim.X);
    ClayMID->SetScalarParameterValue(TEXT("PageH"),      ColorBrickDim.Y * ColorBrickDim.Z);
    ClayMID->SetScalarParameterValue(TEXT("TilesPerRow"), AtlasTilesPerRow);
    ClayMID->SetScalarParameterValue(TEXT("AtlasW"),     AtlasW);
    ClayMID->SetScalarParameterValue(TEXT("AtlasH"),     AtlasH);
    ClayMID->SetScalarParameterValue(TEXT("CB"),         CB);
    // Brillo de la arcilla nueva: el material desvanece según (NowTime - UV0.x)/Seconds.
    ClayMID->SetScalarParameterValue(TEXT("NewClayGlowSeconds"),    NewClayGlowSeconds);
    ClayMID->SetScalarParameterValue(TEXT("NewClayGlowBrightness"), NewClayGlowBrightness);
    ClayMID->SetScalarParameterValue(TEXT("NowTime"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

UMaterialInstanceDynamic* APTSculptVolume::CreateBakedColorMID(UObject* Outer)
{
    if (!ClayMaterial || !PageTex || !AtlasTex) return nullptr;

    // Copiar la page table (R32F) a una textura persistente (el volumen se destruye al salir del modo G).
    const int32 PGW = ColorBrickDim.X;
    const int32 PGH = ColorBrickDim.Y * ColorBrickDim.Z;
    UTexture2D* PageCopy = UTexture2D::CreateTransient(PGW, PGH, PF_R32_FLOAT);
    if (!PageCopy) return nullptr;
    PageCopy->SRGB = false; PageCopy->Filter = TF_Nearest; PageCopy->AddressX = TA_Clamp; PageCopy->AddressY = TA_Clamp;
    {
        FTexture2DMipMap& Mip = PageCopy->GetPlatformData()->Mips[0];
        void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(D, PageBuf.GetData(),
            FMath::Min<int64>((int64)PageBuf.Num() * sizeof(float), Mip.BulkData.GetBulkDataSize()));
        Mip.BulkData.Unlock();
    }
    PageCopy->UpdateResource();

    // Copiar el atlas (BGRA8) a una textura persistente.
    UTexture2D* AtlasCopy = UTexture2D::CreateTransient(AtlasW, AtlasH, PF_B8G8R8A8);
    if (!AtlasCopy) return nullptr;
    AtlasCopy->SRGB = true; AtlasCopy->Filter = TF_Bilinear; AtlasCopy->AddressX = TA_Clamp; AtlasCopy->AddressY = TA_Clamp;
    {
        FTexture2DMipMap& Mip = AtlasCopy->GetPlatformData()->Mips[0];
        void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(D, AtlasBuf.GetData(),
            FMath::Min<int64>((int64)AtlasBuf.Num() * sizeof(FColor), Mip.BulkData.GetBulkDataSize()));
        Mip.BulkData.Unlock();
    }
    AtlasCopy->UpdateResource();

    // MID con los MISMOS parámetros que el clay en vivo, pero apuntando a las copias y con glow apagado.
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ClayMaterial, Outer);
    if (!MID) return nullptr;
    MID->SetTextureParameterValue(TEXT("PageTable"), PageCopy);
    MID->SetTextureParameterValue(TEXT("Atlas"),     AtlasCopy);
    MID->SetVectorParameterValue(TEXT("CanvasMin"),  CanvasMinLocal);
    MID->SetScalarParameterValue(TEXT("ColorVoxel"), FMath::Max(ColorVoxel, 0.5f));
    MID->SetVectorParameterValue(TEXT("VoxDim"),     FVector(ColorVoxDim));
    MID->SetVectorParameterValue(TEXT("BrickDim"),   FVector(ColorBrickDim));
    MID->SetScalarParameterValue(TEXT("PageW"),      ColorBrickDim.X);
    MID->SetScalarParameterValue(TEXT("PageH"),      ColorBrickDim.Y * ColorBrickDim.Z);
    MID->SetScalarParameterValue(TEXT("TilesPerRow"), AtlasTilesPerRow);
    MID->SetScalarParameterValue(TEXT("AtlasW"),     AtlasW);
    MID->SetScalarParameterValue(TEXT("AtlasH"),     AtlasH);
    MID->SetScalarParameterValue(TEXT("CB"),         CB);
    MID->SetScalarParameterValue(TEXT("NewClayGlowBrightness"), 0.f); // sin brillo de arcilla nueva
    MID->SetScalarParameterValue(TEXT("NewClayGlowSeconds"),    NewClayGlowSeconds);
    MID->SetScalarParameterValue(TEXT("NowTime"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
    return MID;
}

bool APTSculptVolume::WriteColorVoxel(int32 vx, int32 vy, int32 vz, const FColor& C)
{
    if (vx < 0 || vy < 0 || vz < 0 ||
        vx >= ColorVoxDim.X || vy >= ColorVoxDim.Y || vz >= ColorVoxDim.Z) return true;

    const FIntVector BC(vx / CB, vy / CB, vz / CB);
    int32 Slot;
    if (int32* Found = BrickSlot.Find(BC))
    {
        Slot = *Found;
    }
    else
    {
        if (FreeSlots.Num() > 0)      Slot = FreeSlots.Pop(EAllowShrinking::No); // reusar slot liberado
        else if (NextSlot < AtlasCapacity) Slot = NextSlot++;
        else return false;            // atlas lleno → dejar de allocar
        BrickSlot.Add(BC, Slot);
        if (SlotUsed.IsValidIndex(Slot)) SlotUsed[Slot] = 0;
        const int32 PgIdx = BC.X + (BC.Y + BC.Z * ColorBrickDim.Y) * ColorBrickDim.X;
        if (PageBuf.IsValidIndex(PgIdx)) { PageBuf[PgIdx] = (float)(Slot + 1); DirtyPageIdx.Add(PgIdx); bPaintDirty = true; }
    }

    const int32 TileX = Slot % AtlasTilesPerRow, TileY = Slot / AtlasTilesPerRow;
    const int32 lx = vx - BC.X * CB, ly = vy - BC.Y * CB, lz = vz - BC.Z * CB;
    const int32 ax = TileX * CB + lx;
    const int32 ay = TileY * (CB * CB) + lz * CB + ly;
    const int32 AIdx = ax + ay * AtlasW;
    if (AtlasBuf.IsValidIndex(AIdx) && C.A >= AtlasBuf[AIdx].A)
    {
        BackupAtlas(AIdx, Slot); // undo: guardar el color previo antes de pisarlo
        if (AtlasBuf[AIdx].A == 0 && C.A > 0 && SlotUsed.IsValidIndex(Slot)) ++SlotUsed[Slot];
        AtlasBuf[AIdx] = C;
        DirtyTiles.Add(Slot);
        bPaintDirty = true;
    }
    return true;
}

bool APTSculptVolume::WritePaintStamp(FVector WorldPos, EPTStampShape Shape, float Size, FLinearColor Color, bool bFull,
                                      FVector StampScale)
{
    if (!AtlasTex) return false;
    bool bPaintedAny = false; // true si al menos un vóxel cayó sobre la superficie (no en el aire)
    const float CV = FMath::Max(ColorVoxel, 0.5f);
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPos);
    const FColor  FCol  = Color.ToFColor(true);
    const float   HalfUU = Size * 0.5f;
    // Escala en el plano del splat (X→u, Y→v). Z no aplica a un splat 2D de superficie.
    const float SU = FMath::Max(StampScale.X, 0.05f);
    const float SV = FMath::Max(StampScale.Y, 0.05f);

    // Normal de la superficie (gradiente del SDF), en espacio local. Una sola vez por sello.
    const float E = VoxelSize;
    FVector N(
        SampleWorldDensity(WorldPos + FVector(E,0,0)) - SampleWorldDensity(WorldPos - FVector(E,0,0)),
        SampleWorldDensity(WorldPos + FVector(0,E,0)) - SampleWorldDensity(WorldPos - FVector(0,E,0)),
        SampleWorldDensity(WorldPos + FVector(0,0,E)) - SampleWorldDensity(WorldPos - FVector(0,0,E)));
    N = (-N).GetSafeNormal();
    if (N.IsNearlyZero()) N = FVector::UpVector;
    FVector LocalN = GetActorTransform().InverseTransformVectorNoScale(N).GetSafeNormal();
    FVector T, B;
    LocalN.FindBestAxisVectors(T, B);

    const float SoftUU  = FMath::Max((1.f - PaintHardness) * HalfUU, CV);       // borde suave (UU)
    const float ShellUU = FMath::Max(1.5f * CV, 0.75f * VoxelSize);             // grosor a cada lado
    const float SurfBand = 0.95f; // en celdas: solo pinta si hay superficie cerca (no aire)

    // Iteración O(radio²): disco en el plano tangente × grosor fino a lo largo de la normal.
    // La sección del sello se evalúa en el plano (u,v); la normal es el eje "Z" del stamp.
    for (float u = -HalfUU * SU; u <= HalfUU * SU; u += CV)
    for (float v = -HalfUU * SV; v <= HalfUU * SV; v += CV)
    {
        const float sdf2d = StampSDF(Shape, FVector(u / SU, v / SV, 0.f), HalfUU);
        if (sdf2d < 0.f) continue;
        const uint8 cov = bFull ? 255 : (uint8)FMath::Clamp(sdf2d / SoftUU * 255.f, 0.f, 255.f);
        if (cov == 0) continue;

        const FColor Out(FCol.R, FCol.G, FCol.B, cov);
        for (float n = -ShellUU; n <= ShellUU; n += CV)
        {
            const FVector P = Local + u * T + v * B + n * LocalN;
            // Solo pintar si hay geometría cerca (evita pintar el aire → sin fantasmas). Se considera
            // la superficie de CUALQUIER campo: base O alguna capa de detalle (si no, no se podrían
            // pintar los lentes/bigote de una capa).
            const FVector Cell = P / VoxelSize;
            bool bNearSurface = FMath::Abs(Field.SampleSDF(Cell.X, Cell.Y, Cell.Z)) < SurfBand;
            if (!bNearSurface)
                for (const TSharedPtr<FPTSculptField>& L : DetailFields)
                    if (L.IsValid() && FMath::Abs(L->SampleSDF(Cell.X, Cell.Y, Cell.Z)) < SurfBand) { bNearSurface = true; break; }
            if (!bNearSurface) continue;
            const int32 vx = FMath::FloorToInt((P.X - CanvasMinLocal.X) / CV);
            const int32 vy = FMath::FloorToInt((P.Y - CanvasMinLocal.Y) / CV);
            const int32 vz = FMath::FloorToInt((P.Z - CanvasMinLocal.Z) / CV);
            WriteColorVoxel(vx, vy, vz, Out);
            bPaintedAny = true; // pintó sobre superficie
        }
    }
    return bPaintedAny;
}

void APTSculptVolume::ClearColorVoxel(int32 vx, int32 vy, int32 vz)
{
    if (vx < 0 || vy < 0 || vz < 0 ||
        vx >= ColorVoxDim.X || vy >= ColorVoxDim.Y || vz >= ColorVoxDim.Z) return;

    const FIntVector BC(vx / CB, vy / CB, vz / CB);
    int32* Found = BrickSlot.Find(BC);
    if (!Found) return;
    const int32 Slot = *Found;

    const int32 TileX = Slot % AtlasTilesPerRow, TileY = Slot / AtlasTilesPerRow;
    const int32 lx = vx - BC.X * CB, ly = vy - BC.Y * CB, lz = vz - BC.Z * CB;
    const int32 AIdx = (TileX * CB + lx) + (TileY * (CB * CB) + lz * CB + ly) * AtlasW;
    if (!AtlasBuf.IsValidIndex(AIdx) || AtlasBuf[AIdx].A == 0) return; // ya vacío

    BackupAtlas(AIdx, Slot); // undo: guardar el color previo antes de borrarlo
    AtlasBuf[AIdx] = FColor(0, 0, 0, 0);
    DirtyTiles.Add(Slot);
    bPaintDirty = true;

    if (SlotUsed.IsValidIndex(Slot) && --SlotUsed[Slot] <= 0)
    {
        // Brick vacío → liberar el slot y su entrada de page table.
        BrickSlot.Remove(BC);
        const int32 PgIdx = BC.X + (BC.Y + BC.Z * ColorBrickDim.Y) * ColorBrickDim.X;
        if (PageBuf.IsValidIndex(PgIdx)) { PageBuf[PgIdx] = 0.f; DirtyPageIdx.Add(PgIdx); }
        FreeSlots.Add(Slot);
    }
}

void APTSculptVolume::ClearPaintStamp(FVector WorldPos, EPTStampShape Shape, float Size, FVector StampScale)
{
    if (!AtlasTex || BrickSlot.Num() == 0) return;
    const float CV = FMath::Max(ColorVoxel, 0.5f);
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPos);
    const float HalfUU = Size * 0.5f;
    const FVector SafeScale(FMath::Max(StampScale.X, 0.05f), FMath::Max(StampScale.Y, 0.05f), FMath::Max(StampScale.Z, 0.05f));
    const float MaxScale = FMath::Max3(SafeScale.X, SafeScale.Y, SafeScale.Z);

    const FVector Center = (Local - CanvasMinLocal) / CV;
    const int32 Rv  = FMath::CeilToInt(HalfUU * MaxScale / CV) + 1;
    const int32 vcx = FMath::RoundToInt(Center.X), vcy = FMath::RoundToInt(Center.Y), vcz = FMath::RoundToInt(Center.Z);
    const int32 x0 = FMath::Max(0, vcx - Rv), x1 = FMath::Min(ColorVoxDim.X - 1, vcx + Rv);
    const int32 y0 = FMath::Max(0, vcy - Rv), y1 = FMath::Min(ColorVoxDim.Y - 1, vcy + Rv);
    const int32 z0 = FMath::Max(0, vcz - Rv), z1 = FMath::Min(ColorVoxDim.Z - 1, vcz + Rv);
    if (x1 < x0 || y1 < y0 || z1 < z0) return;

    // Recorre solo los bricks EXISTENTES del bbox → barato aunque la brocha sea grande.
    for (int32 bz = z0 / CB; bz <= z1 / CB; ++bz)
    for (int32 by = y0 / CB; by <= y1 / CB; ++by)
    for (int32 bx = x0 / CB; bx <= x1 / CB; ++bx)
    {
        if (!BrickSlot.Contains(FIntVector(bx, by, bz))) continue;
        const int32 vxs = bx * CB, vys = by * CB, vzs = bz * CB;
        for (int32 lz = 0; lz < CB; ++lz)
        for (int32 ly = 0; ly < CB; ++ly)
        for (int32 lx = 0; lx < CB; ++lx)
        {
            const int32 vx = vxs + lx, vy = vys + ly, vz = vzs + lz;
            if (vx < x0 || vx > x1 || vy < y0 || vy > y1 || vz < z0 || vz > z1) continue;
            const FVector VoxLocal = CanvasMinLocal + FVector(vx + 0.5f, vy + 0.5f, vz + 0.5f) * CV;
            if (StampSDF(Shape, (VoxLocal - Local) / SafeScale, HalfUU) < 0.f) continue; // fuera del sello (con escala)
            ClearColorVoxel(vx, vy, vz);
        }
    }
}

void APTSculptVolume::UploadColorField()
{
    const int32 PGW = ColorBrickDim.X;
    if (bPageDirty && PageTex)
    {
        // Subida COMPLETA (una vez, para limpiar la textura GPU recién creada).
        bPageDirty = false;
        DirtyPageIdx.Reset();
        const int32 PGH = ColorBrickDim.Y * ColorBrickDim.Z;
        const int32 Bytes = PGW * PGH * sizeof(float);
        uint8* Copy = (uint8*)FMemory::Malloc(Bytes);
        FMemory::Memcpy(Copy, PageBuf.GetData(), Bytes);
        FUpdateTextureRegion2D* Reg = new FUpdateTextureRegion2D(0, 0, 0, 0, PGW, PGH);
        PageTex->UpdateTextureRegions(0, 1, Reg, PGW * sizeof(float), sizeof(float), Copy,
            [](uint8* D, const FUpdateTextureRegion2D* R) { FMemory::Free(D); delete R; });
    }
    else if (DirtyPageIdx.Num() && PageTex)
    {
        // Subida INCREMENTAL: solo los texels de bricks nuevos (1×1 cada uno).
        for (int32 PgIdx : DirtyPageIdx)
        {
            const int32 px = PgIdx % PGW, py = PgIdx / PGW;
            float* Copy = (float*)FMemory::Malloc(sizeof(float));
            *Copy = PageBuf[PgIdx];
            FUpdateTextureRegion2D* Reg = new FUpdateTextureRegion2D(px, py, 0, 0, 1, 1);
            PageTex->UpdateTextureRegions(0, 1, Reg, sizeof(float), sizeof(float), (uint8*)Copy,
                [](uint8* D, const FUpdateTextureRegion2D* R) { FMemory::Free(D); delete R; });
        }
        DirtyPageIdx.Reset();
    }

    // Tiles del atlas sucios (subida parcial).
    if (DirtyTiles.Num() && AtlasTex)
    {
        const int32 TW = CB, TH = CB * CB;
        for (int32 Slot : DirtyTiles)
        {
            const int32 ox = (Slot % AtlasTilesPerRow) * CB;
            const int32 oy = (Slot / AtlasTilesPerRow) * (CB * CB);
            const int32 Bytes = TW * TH * sizeof(FColor);
            uint8* Copy = (uint8*)FMemory::Malloc(Bytes);
            for (int32 row = 0; row < TH; ++row)
                FMemory::Memcpy(Copy + row * TW * sizeof(FColor),
                                AtlasBuf.GetData() + (oy + row) * AtlasW + ox, TW * sizeof(FColor));
            FUpdateTextureRegion2D* Reg = new FUpdateTextureRegion2D(ox, oy, 0, 0, TW, TH);
            AtlasTex->UpdateTextureRegions(0, 1, Reg, TW * sizeof(FColor), sizeof(FColor), Copy,
                [](uint8* D, const FUpdateTextureRegion2D* R) { FMemory::Free(D); delete R; });
        }
        DirtyTiles.Empty();
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
    // Modo SVO: densidad = SDF del octree en ACTOR-LOCAL (>0 dentro). Lo usan los ojos (raymarch a
    // la superficie) y el cursor.
    if (bUseSVO)
        return SVOField.Sample(GetActorTransform().InverseTransformPosition(WorldPos));

    const FVector C = WorldToCell(WorldPos);
    // Unión (máximo) de la base + TODAS las capas de detalle: así el raymarch del cursor (ALT) se pega
    // a la superficie más externa exista donde exista arcilla, no solo a la base. Convención: SDF
    // positivo = dentro del material → el máximo es la unión de todas las mallas.
    float d = Field.SampleSDF(C.X, C.Y, C.Z);
    for (const TSharedPtr<FPTSculptField>& L : DetailFields)
        if (L.IsValid())
            d = FMath::Max(d, L->SampleSDF(C.X, C.Y, C.Z));
    return d;
}

FLinearColor APTSculptVolume::SampleWorldColor(FVector WorldPos) const
{
    if (bUseSVO) return ClayBaseColor; // el color en SVO va por vértice; para FX alcanza el color base
    const FVector C = WorldToCell(WorldPos);
    return FLinearColor(Field.GetColor(FMath::RoundToInt(C.X), FMath::RoundToInt(C.Y), FMath::RoundToInt(C.Z)));
}

FLinearColor APTSculptVolume::SampleWorldPaintColor(FVector WorldPos, bool& bOutPainted) const
{
    bOutPainted = false;
    const float CV = FMath::Max(ColorVoxel, 0.5f);
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPos);
    const int32 cx = FMath::FloorToInt((Local.X - CanvasMinLocal.X) / CV);
    const int32 cy = FMath::FloorToInt((Local.Y - CanvasMinLocal.Y) / CV);
    const int32 cz = FMath::FloorToInt((Local.Z - CanvasMinLocal.Z) / CV);

    // Buscar el voxel pintado más cercano en un vecindario 3³ (la pintura es una cáscara fina
    // alrededor de la superficie; el vértice puede caer a 1 voxel del centro pintado).
    for (int32 dz = -1; dz <= 1; ++dz)
    for (int32 dy = -1; dy <= 1; ++dy)
    for (int32 dx = -1; dx <= 1; ++dx)
    {
        const int32 vx = cx + dx, vy = cy + dy, vz = cz + dz;
        if (vx < 0 || vy < 0 || vz < 0 ||
            vx >= ColorVoxDim.X || vy >= ColorVoxDim.Y || vz >= ColorVoxDim.Z) continue;

        const FIntVector BC(vx / CB, vy / CB, vz / CB);
        const int32* Found = BrickSlot.Find(BC);
        if (!Found) continue;
        const int32 Slot = *Found;

        const int32 TileX = Slot % AtlasTilesPerRow, TileY = Slot / AtlasTilesPerRow;
        const int32 lx = vx - BC.X * CB, ly = vy - BC.Y * CB, lz = vz - BC.Z * CB;
        const int32 AIdx = (TileX * CB + lx) + (TileY * (CB * CB) + lz * CB + ly) * AtlasW;
        if (!AtlasBuf.IsValidIndex(AIdx) || AtlasBuf[AIdx].A == 0) continue;

        const FColor C = AtlasBuf[AIdx];
        bOutPainted = true;
        return FLinearColor(FColor(C.R, C.G, C.B, 255)); // opaco (el alpha del atlas es cobertura)
    }
    return FLinearColor::White;
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

    case EPTStampShape::TriPrism: // CONO (eje Z, punta arriba en +Z, base en -Z)
    {
        const float h = HalfSize;
        // Radio permitido según la altura: máx en la base (z=-h), 0 en la punta (z=+h).
        const float rAllow = HalfSize * FMath::Clamp((h - P.Z) / (2.f * h), 0.f, 1.f);
        const float radial = rAllow - FVector2D(P.X, P.Y).Size();
        const float axial  = FMath::Min(P.Z + h, h - P.Z);
        return FMath::Min(radial, axial);
    }

    case EPTStampShape::Pyramid: // pirámide de base cuadrada (eje Z, punta +Z)
    {
        const float h = HalfSize;
        const float rAllow = HalfSize * FMath::Clamp((h - P.Z) / (2.f * h), 0.f, 1.f);
        const float radial = rAllow - FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y));
        const float axial  = FMath::Min(P.Z + h, h - P.Z);
        return FMath::Min(radial, axial);
    }

    case EPTStampShape::Torus: // dona en el plano XY
    {
        const float R = HalfSize * 0.62f; // radio mayor
        const float t = HalfSize * 0.36f; // radio del tubo
        const float q = FVector2D(FVector2D(P.X, P.Y).Size() - R, P.Z).Size();
        return t - q;
    }

    case EPTStampShape::Capsule: // cápsula (eje Z)
    {
        const float r  = HalfSize * 0.5f;
        const float zc = FMath::Clamp(P.Z, -(HalfSize - r), (HalfSize - r));
        return r - FVector(P.X, P.Y, P.Z - zc).Size();
    }

    case EPTStampShape::HexPrism: // prisma hexagonal (eje Z)
    {
        const float k = 0.8660254f; // sqrt(3)/2
        const float hx = FMath::Max(FMath::Abs(P.X) * k + FMath::Abs(P.Y) * 0.5f, FMath::Abs(P.Y));
        const float radial = HalfSize - hx;
        const float axial  = HalfSize - FMath::Abs(P.Z);
        return FMath::Min(radial, axial);
    }

    case EPTStampShape::Octahedron:
        return HalfSize - (FMath::Abs(P.X) + FMath::Abs(P.Y) + FMath::Abs(P.Z));

    default:
        return HalfSize - P.Size();
    }
}

bool APTSculptVolume::ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                                  EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot,
                                  FVector StampScale)
{
    // Modo SVO (experimental, detrás de flag): la geometría va por el octree adaptativo.
    if (bUseSVO)
    {
        ApplyStampSVO(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale);
        return true;
    }

    // Escala no-uniforme: se deforma el punto de muestreo local dividiéndolo por la escala (así una
    // escala 2 en Z estira la forma al doble en Z). Se clampea para no dividir por ~0.
    const FVector SafeScale(FMath::Max(StampScale.X, 0.05f),
                            FMath::Max(StampScale.Y, 0.05f),
                            FMath::Max(StampScale.Z, 0.05f));

    // Paint: escribe en el volumen 3D de pintura (per-pixel, no toca la geometría).
    // Devuelve si pintó sobre superficie: pintar en el aire no debe lanzar partículas.
    if (Mode == EPTEditMode::Paint)
        return WritePaintStamp(WorldPos, Shape, Size, PaintColor, false, SafeScale);

    // Erase también borra la pintura de esa zona (libera bricks, sin fantasmas).
    if (Mode == EPTEditMode::Erase)
        ClearPaintStamp(WorldPos, Shape, Size, SafeScale);

    // Rotación del sello: el SDF se evalúa siempre alineado a los ejes, así que en vez de rotar la
    // forma se ROTA AL REVÉS el punto de muestreo (a espacio local del sello). La rotación llega en
    // mundo → pasarla al espacio local del volumen.
    const FQuat LocalQ  = GetActorTransform().InverseTransformRotation(StampRot.Quaternion());
    const bool  bRotated = !LocalQ.IsIdentity(1e-4f);

    const FVector GC = WorldToCell(WorldPos);
    const float HalfSize = (Size * 0.5f) / VoxelSize; // radio en celdas
    // Con rotación (o escala >1) una forma barre más lejos que HalfSize → ampliar el rango de celdas a
    // revisar para no cortar la punta del sello. Se toma el eje más estirado.
    const float MaxScale = FMath::Max3(SafeScale.X, SafeScale.Y, SafeScale.Z);
    const int32 R = FMath::CeilToInt(HalfSize * MaxScale * (bRotated ? 1.75f : 1.f)) + 1;

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
    bool bRemovedSolid = false; // Borrar: true si se sacó arcilla sólida (para las partículas)
    float BestErasePrev = 0.f;  // el SDF más alto entre las celdas borradas (la más interior)
    // Instante de este sello: se hornea en las muestras que Agregar crea, para que la arcilla
    // nueva brille y se desvanezca (ver UV0 del mesher + material del clay).
    const float StampNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // Campo DESTINO de la geometría: base por defecto, o la capa de detalle activa (ALT). Así el
    // detalle no se fusiona con la arcilla base ni con otras capas (cada una es un campo aparte).
    FPTSculptField& F = *ActiveField;

    // ── Smooth: Laplaciano suave de dos pasos con falloff radial y leve empuje
    //    hacia afuera (SmoothBias) para que suavice sin encoger el modelo. ────
    if (Mode == EPTEditMode::Smooth)
    {
        const int32 nx = x1 - x0 + 1, ny = y1 - y0 + 1, nz = z1 - z0 + 1;
        TArray<float> NewV; NewV.SetNumUninitialized(nx * ny * nz);
        auto WI = [&](int32 x, int32 y, int32 z){ return (x-x0) + (y-y0)*nx + (z-z0)*nx*ny; };

        for (int32 z = z0; z <= z1; ++z)
        for (int32 y = y0; y <= y1; ++y)
        for (int32 x = x0; x <= x1; ++x)
        {
            const float prev = F.GetSDF(x, y, z);
            FVector LP(x - GC.X, y - GC.Y, z - GC.Z);
            if (bRotated) LP = LocalQ.UnrotateVector(LP);
            LP /= SafeScale; // escala no-uniforme: deforma el punto de muestreo
            const float sdf  = StampSDF(Shape, LP, HalfSize);
            const float fall = FMath::Clamp(sdf / FMath::Max(HalfSize, 1.f), 0.f, 1.f);
            if (fall <= 0.f) { NewV[WI(x,y,z)] = prev; continue; }

            const float avg = (F.GetSDF(x+1,y,z) + F.GetSDF(x-1,y,z)
                             + F.GetSDF(x,y+1,z) + F.GetSDF(x,y-1,z)
                             + F.GetSDF(x,y,z+1) + F.GetSDF(x,y,z-1)) / 6.f;
            const float target = avg + SmoothBias;
            float nv = FMath::Lerp(prev, target, fall * SmoothStrength);
            // Tope de cambio por celda por aplicación (no se dispara al mantener).
            nv = prev + FMath::Clamp(nv - prev, -SmoothMaxDelta, SmoothMaxDelta);
            NewV[WI(x,y,z)] = nv;
        }

        for (int32 z = z0; z <= z1; ++z)
        for (int32 y = y0; y <= y1; ++y)
        for (int32 x = x0; x <= x1; ++x)
        {
            const float nv = NewV[WI(x,y,z)];
            if (nv != F.GetSDF(x, y, z)) { F.SetSDF(x, y, z, nv); bAnyChange = true; }
        }

        if (!bAnyChange) return false;
        MarkStampDirty(x0, y0, z0, x1, y1, z1);
        return true;
    }

    for (int32 z = z0; z <= z1; ++z)
    for (int32 y = y0; y <= y1; ++y)
    for (int32 x = x0; x <= x1; ++x)
    {
        FVector LP(x - GC.X, y - GC.Y, z - GC.Z);
        if (bRotated) LP = LocalQ.UnrotateVector(LP);
        LP /= SafeScale; // escala no-uniforme
        const float sdf  = StampSDF(Shape, LP, HalfSize);
        const float prev = F.GetSDF(x, y, z);

        if (Mode == EPTEditMode::Add)
        {
            const float next = FMath::Max(prev, sdf);
            if (next != prev) { F.SetSDF(x, y, z, next); bAnyChange = true; }
            // Color pleno directo sobre la geometría, ambos lados de la superficie
            // (sdf > -1 = dentro del sello + 1 celda) → toda la malla nueva del color.
            if (sdf > -1.f)
            {
                F.SetColor(x, y, z, PaintColor.ToFColor(true));
                F.SetAddTime(x, y, z, StampNow); // marca la arcilla nueva como "fresca"
                bAnyChange = true;
            }
        }
        else // Erase
        {
            // Clampear ANTES de comparar (si no, borrar en aire "profundo" marcaba cambio falso).
            const float next = FMath::Clamp(FMath::Min(prev, -sdf), -1.f, 1.f);
            if (next != prev)
            {
                // ¿Se sacó geometría SÓLIDA? (SDF > 0 = dentro de la arcilla). Solo eso cuenta para
                // las partículas: la banda de transición alrededor de la superficie (prev en (-1,0])
                // también cambia al borrar cerca, pero ahí NO hay arcilla visible → no debe saltar.
                if (prev > 0.f)
                {
                    // Capturar el color de la arcilla que se borra (de una celda SÓLIDA, que sí tiene
                    // color) ANTES de sacarla. Así la partícula sale del color real, no del gris de
                    // una celda de borde. Se queda con la más interior (mayor SDF).
                    if (!bRemovedSolid || prev > BestErasePrev)
                    {
                        BestErasePrev  = prev;
                        LastErasedColor = FLinearColor::FromSRGBColor(F.GetColor(x, y, z));
                    }
                    bRemovedSolid = true;
                }
                F.SetSDF(x, y, z, next);
                bAnyChange = true;
            }
        }
    }

    if (!bAnyChange) return false;
    MarkStampDirty(x0, y0, z0, x1, y1, z1);
    // Para Borrar, "true" = se sacó arcilla REAL (para lanzar partículas). Add/Paint siempre true.
    return (Mode == EPTEditMode::Erase) ? bRemovedSolid : true;
}

// Marca todos los bricks que cubren el bbox de celdas (+1 de borde para que los
// bricks vecinos re-mallen su costura). Bricks vacíos hacen early-out al mallar.
void APTSculptVolume::MarkStampDirty(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1)
{
    const int32 BS = FPTBrick::BrickSize;
    auto FloorDivBS = [BS](int32 v){ return (v >= 0) ? v / BS : -(( -v + BS - 1) / BS); };
    const int32 bx0 = FloorDivBS(x0 - 1), bx1 = FloorDivBS(x1 + 1);
    const int32 by0 = FloorDivBS(y0 - 1), by1 = FloorDivBS(y1 + 1);
    const int32 bz0 = FloorDivBS(z0 - 1), bz1 = FloorDivBS(z1 + 1);
    for (int32 bz = bz0; bz <= bz1; ++bz)
    for (int32 by = by0; by <= by1; ++by)
    for (int32 bx = bx0; bx <= bx1; ++bx)
        ActiveField->MarkDirty(FPTBrickKey(bx, by, bz)); // marca el campo ACTIVO (base o capa de detalle)
}

// ─── Marching Cubes ───────────────────────────────────────────────────────────

FVector APTSculptVolume::Interp(FVector P1, FVector P2, float V1, float V2)
{
    if (FMath::Abs(V1 - V2) < 1e-5f) return (P1 + P2) * 0.5f;
    return P1 + (-V1 / (V2 - V1)) * (P2 - P1);
}

FColor APTSculptVolume::InterpColor(FLinearColor C1, FLinearColor C2, float V1, float V2)
{
    // El vertex color va al mesh y el shader lo lee como LINEAL (no decodifica sRGB), así
    // que se emite lineal (ToFColor(false)). El campo guarda sRGB y se decodifica bien al
    // leer; emitir sRGB acá hacía que el color se viera más claro que el elegido.
    if (FMath::Abs(V1 - V2) < 1e-5f) return ((C1 + C2) * 0.5f).ToFColor(false);
    float t = FMath::Clamp(-V1 / (V2 - V1), 0.f, 1.f);
    return FLinearColor::LerpUsingHSV(C1, C2, t).ToFColor(false);
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

// Aplica escala no-uniforme a una malla de preview ya construida (estira verts + corrige normales).
static void PT_ScalePreview(TArray<FVector>& Verts, TArray<FVector>& Normals, FVector Scale)
{
    if (Scale.Equals(FVector::OneVector)) return;
    const FVector S(FMath::Max(Scale.X,0.05f), FMath::Max(Scale.Y,0.05f), FMath::Max(Scale.Z,0.05f));
    for (FVector& V : Verts) V *= S;
    for (FVector& N : Normals) N = (N / S).GetSafeNormal(); // normal correcta al escalar no-uniforme
}

void APTSculptVolume::BuildStampPreview(EPTStampShape Shape, float Size, float VoxSz,
                                         TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                         TArray<FVector>& OutNormals, FVector StampScale)
{
    // Formas de CARAS PLANAS: malla explícita con caras lisas (el marching cubes las escalona → boxelart).
    if (Shape == EPTStampShape::Pyramid || Shape == EPTStampShape::Octahedron)
    {
        OutVerts.Reset(); OutTris.Reset(); OutNormals.Reset();
        const float HS = Size * 0.5f;
        auto AddTri = [&](FVector A, FVector B, FVector C)
        {
            FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
            const FVector Cen = (A + B + C) / 3.f;      // normal HACIA AFUERA (desde el origen)
            if (FVector::DotProduct(N, Cen) < 0.f) { Swap(B, C); N = -N; }
            const int32 i0 = OutVerts.Num();
            OutVerts.Add(A); OutVerts.Add(B); OutVerts.Add(C);
            OutNormals.Add(N); OutNormals.Add(N); OutNormals.Add(N); // flat: normal por cara (verts no compartidos)
            // Winding INVERTIDO respecto a la normal: UE culea la cara "front" con winding horario visto
            // de frente; con la normal hacia afuera hay que emitir el triángulo al revés o se ve por dentro.
            OutTris.Add(i0); OutTris.Add(i0 + 2); OutTris.Add(i0 + 1);
        };
        if (Shape == EPTStampShape::Pyramid)
        {
            const FVector Ap(0, 0, HS);
            const FVector B0(-HS, -HS, -HS), B1(HS, -HS, -HS), B2(HS, HS, -HS), B3(-HS, HS, -HS);
            AddTri(Ap, B0, B1); AddTri(Ap, B1, B2); AddTri(Ap, B2, B3); AddTri(Ap, B3, B0); // caras
            AddTri(B0, B2, B1); AddTri(B0, B3, B2);                                          // base
        }
        else // Octahedron
        {
            const FVector PX(HS,0,0), NX(-HS,0,0), PY(0,HS,0), NY(0,-HS,0), PZ(0,0,HS), NZ(0,0,-HS);
            AddTri(PZ, PX, PY); AddTri(PZ, PY, NX); AddTri(PZ, NX, NY); AddTri(PZ, NY, PX);
            AddTri(NZ, PX, PY); AddTri(NZ, PY, NX); AddTri(NZ, NX, NY); AddTri(NZ, NY, PX);
        }
        PT_ScalePreview(OutVerts, OutNormals, StampScale);
        return;
    }

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

    PT_ScalePreview(OutVerts, OutNormals, StampScale); // escala no-uniforme
}

void APTSculptVolume::RebuildDirty()
{
    // SVO: balance local y remallado de los componentes afectados.
    if (bUseSVO)
    {
        if (bSVODirty) RebuildSVOMesh(); // maneja bSVODirty y el mallado async internamente
        return;
    }

    bRebuildInProgress = true;

    UMaterialInterface* Mat = ClayMaterialOverride ? ClayMaterialOverride
                            : (ClayMID ? (UMaterialInterface*)ClayMID : ClayMaterial);

    // Snapshot de cada brick dirty (GameThread), etiquetado con su mesh destino: la base va a `Mesh`,
    // cada capa de detalle a su propio ProceduralMesh. Cada campo tiene su propio SectionIndex, y como
    // cada capa tiene su propio mesh, no hay colisión de índices entre capas.
    struct FSnapJob { FPTSculptField::FBrickSnapshot Snap; UProceduralMeshComponent* Target = nullptr; };
    TSharedPtr<TArray<FSnapJob>, ESPMode::ThreadSafe> Jobs = MakeShared<TArray<FSnapJob>, ESPMode::ThreadSafe>();

    auto Collect = [&](FPTSculptField& F, UProceduralMeshComponent* MComp)
    {
        if (!MComp || !F.HasDirty()) return;
        TArray<FPTBrickKey> Keys;
        F.TakeDirty(Keys);
        const int32 Base = Jobs->Num();
        for (const FPTBrickKey& K : Keys)
        {
            FSnapJob J; J.Target = MComp;
            J.Snap.Section = F.SectionIndex(K);
            F.SnapshotBrick(K, J.Snap); // actualiza flatness
            Jobs->Add(MoveTemp(J));
        }
        for (int32 i = Base; i < Jobs->Num(); ++i)
            (*Jobs)[i].Snap.Step = F.DecideStep((*Jobs)[i].Snap.Key);
    };

    Collect(Field, Mesh);
    for (int32 i = 0; i < DetailFields.Num(); ++i)
        if (DetailFields[i].IsValid() && DetailMeshes.IsValidIndex(i))
            Collect(*DetailFields[i], DetailMeshes[i]);

    if (Jobs->Num() == 0) { bRebuildInProgress = false; return; }

    Async(EAsyncExecution::ThreadPool, [this, Mat, Jobs]()
    {
        struct FMeshOut { FPTBrickMesh Mesh; UProceduralMeshComponent* Target = nullptr; };
        TSharedPtr<TArray<FMeshOut>, ESPMode::ThreadSafe> Results =
            MakeShared<TArray<FMeshOut>, ESPMode::ThreadSafe>();
        Results->Reserve(Jobs->Num());
        for (const FSnapJob& J : *Jobs)
        {
            FMeshOut O; O.Target = J.Target;
            FPTSculptField::MeshBrick(J.Snap, O.Mesh);
            Results->Add(MoveTemp(O));
        }

        AsyncTask(ENamedThreads::GameThread, [this, Mat, Results]()
        {
            for (FMeshOut& O : *Results)
            {
                if (!O.Target) continue;
                // Colisión OFF (el esculpido usa raymarch del SDF, no colisión física).
                O.Target->CreateMeshSection(O.Mesh.Section, O.Mesh.Verts, O.Mesh.Tris, O.Mesh.Normals,
                                            O.Mesh.UV0, O.Mesh.Colors, {}, /*collision=*/false);
                if (Mat) O.Target->SetMaterial(O.Mesh.Section, Mat);
            }
            bRebuildInProgress = false;
        });
    });
}

// ─── SVO (modo experimental detrás de flag) ────────────────────────────────────
void APTSculptVolume::InitSVOOctree(FPTVoxelOctree& F) const
{
    // Octree cúbico en ACTOR-LOCAL (UU) que cubre el BoundsBox del lienzo, con MARGEN: así las paredes
    // del box quedan estrictamente ADENTRO del octree (hay celdas de aire más allá) y el mallado SIEMPRE
    // cierra la arcilla contra el límite (no se ve el interior al esculpir pegado a la pared).
    const FVector Center = BoundsBox ? BoundsBox->GetRelativeLocation() : FVector::ZeroVector;
    const FVector Half   = BoundsBox ? BoundsBox->GetUnscaledBoxExtent() : FVector(480.f);
    const float Margin   = FMath::Max(VoxelSize * 4.f, 1.f);
    const float RootSize = 2.f * FMath::Max3(Half.X, Half.Y, Half.Z) + 2.f * Margin;
    // Profundidad tal que la celda mínima ≈ VoxelSize (mismo detalle que el campo clásico).
    const float Ratio = RootSize / FMath::Max(VoxelSize, 0.5f);
    const int32 MaxDepth = FMath::Clamp(FMath::CeilToInt(FMath::Log2(Ratio)), 0, 12);
    F.Init(Center - FVector(RootSize * 0.5f), RootSize, MaxDepth);
    // Recorte al lienzo (BoundsBox en local): la arcilla contra la pared cierra siempre.
    F.SetClampBox(FBox(Center - Half, Center + Half));
}

void APTSculptVolume::InitSVO()
{
    ClearSVOChunkMeshes();
    InitSVOOctree(SVOField);
    ActiveSVO = &SVOField;
    bSVOInit  = true;
    MarkAllSVODirty();
}

void APTSculptVolume::ApplyStampSVO(FVector WorldPos, EPTStampShape Shape, float Size, EPTEditMode Mode,
                                    FLinearColor PaintColor, FRotator StampRot, FVector StampScale)
{
    if (!bSVOInit) InitSVO();
    // Smooth no se usa en modo SVO (se ignora). Paint recolorea la superficie sin tocar geometría.
    if (Mode == EPTEditMode::Smooth) return;

    // Mapeo de forma clásica → forma del octree.
    EPTSVOShape S;
    switch (Shape)
    {
    case EPTStampShape::Cube:                              S = EPTSVOShape::Box;      break;
    case EPTStampShape::Cylinder: case EPTStampShape::HexPrism:
    case EPTStampShape::Capsule:                           S = EPTSVOShape::Cylinder; break;
    case EPTStampShape::TriPrism:  case EPTStampShape::Pyramid: S = EPTSVOShape::Cone; break;
    case EPTStampShape::Torus:                             S = EPTSVOShape::Torus;    break;
    case EPTStampShape::Sphere: case EPTStampShape::Octahedron:
    default:                                               S = EPTSVOShape::Sphere;   break;
    }

    // Transform del sello en ACTOR-LOCAL (posición + rotación). Tamaño por HalfExtent (no-uniforme).
    const FTransform& AX = GetActorTransform();
    const FVector LocalPos = AX.InverseTransformPosition(WorldPos);
    const FQuat   LocalQ   = AX.InverseTransformRotation(StampRot.Quaternion());
    const FVector SafeScale(FMath::Max(StampScale.X, 0.05f), FMath::Max(StampScale.Y, 0.05f), FMath::Max(StampScale.Z, 0.05f));
    const FVector HalfExtent = (Size * 0.5f) * SafeScale; // radio en UU por eje

    const FTransform Xf(LocalQ, LocalPos);
    FPTVoxelOctree& F = ActiveSVO ? *ActiveSVO : SVOField; // base o capa de detalle activa
    const FColor Col = PaintColor.ToFColor(false); // color por vértice (respaldo); byte lineal = picker

    // PINTAR: usa el MISMO sistema de color del clásico (atlas 3D por voxel), que el material samplea
    // por posición → crisp, con resolución/dureza propias, idéntico a antes. No toca geometría.
    if (Mode == EPTEditMode::Paint)
    {
        WritePaintStamp(WorldPos, Shape, Size, PaintColor, /*bFull=*/false, SafeScale);
        return;
    }

    F.EditShape(Xf, S, HalfExtent, /*bAdd=*/Mode == EPTEditMode::Add, Col);
    if (Mode == EPTEditMode::Add)
        WritePaintStamp(WorldPos, Shape, Size, PaintColor, /*bFull=*/false, SafeScale); // color al atlas también
    else if (Mode == EPTEditMode::Erase)
        ClearPaintStamp(WorldPos, Shape, Size, SafeScale); // borrar también limpia la pintura

    if (&F == &SVOField)
    {
        // Base: marcar solo los chunks tocados (re-mallado incremental).
        // Rotated boxes can extend farther than their largest unrotated half-axis.
        FBox Bounds(ForceInit);
        for (int32 c = 0; c < 8; ++c)
            Bounds += Xf.TransformPosition(FVector((c & 1) ? HalfExtent.X : -HalfExtent.X,
                (c & 2) ? HalfExtent.Y : -HalfExtent.Y, (c & 4) ? HalfExtent.Z : -HalfExtent.Z));
        if (F.GetPendingBalanceBounds().IsValid) Bounds += F.GetPendingBalanceBounds();
        MarkSVODirtyLocalBounds(Bounds.Min, Bounds.Max);
    }
    else
    {
        bSVODirty = true; // capa de detalle → se rehace entera (es chica)
        for (int32 i = 0; i < SVODetailFields.Num(); ++i)
            if (SVODetailFields[i].Get() == &F) { DirtySVODetailLayers.Add(i); break; }
    }
}

void APTSculptVolume::RebuildSVOInto(FPTVoxelOctree& F, UProceduralMeshComponent* M)
{
    if (!M) return;
    F.Balance(); // 2:1 → sin artefactos en saltos grandes de nivel
    TArray<FVector> V, N; TArray<int32> T; TArray<FColor> C;
    F.BuildMesh(V, T, N, C);

    UMaterialInterface* Mat = ClayMaterialOverride ? ClayMaterialOverride
                            : (ClayMID ? (UMaterialInterface*)ClayMID : ClayMaterial);
    TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
    M->CreateMeshSection(0, V, T, N, UV, C, Tan, /*collision=*/false);
    if (Mat) M->SetMaterial(0, Mat);
}

void APTSculptVolume::MarkAllSVODirty()
{
    ++SVOMeshGen; // invalida cualquier mallado async en vuelo (clear/load/init cambian todo)
    DirtySVODetailLayers.Reset();
    for (int32 i = 0; i < SVODetailFields.Num(); ++i) DirtySVODetailLayers.Add(i);
    bSVOCoarseDirty = true;
    DirtySVOChunks.Reset();
    const int32 N = SVOChunkDim * SVOChunkDim * SVOChunkDim;
    for (int32 i = 0; i < N; ++i) DirtySVOChunks.Add(i);
    bSVODirty = true;
}

void APTSculptVolume::MarkSVODirtyLocalBounds(const FVector& LMin, const FVector& LMax)
{
    bSVOCoarseDirty = true; // la sección gruesa es barata → siempre se rehace
    const FVector Origin = SVOField.GetOrigin();
    const float   CS = SVOChunkSize();
    if (CS <= KINDA_SMALL_NUMBER) return;
    // Halo = umbral fino (los vecinos de una hoja fina están a <= esa distancia) → cubre las costuras
    // sin re-mallar de más.
    const float Halo = SVOFineThreshold();
    const FVector Emin = LMin - FVector(Halo);
    const FVector Emax = LMax + FVector(Halo);
    auto CI = [&](float x, int32 axisMax) { return FMath::Clamp(FMath::FloorToInt(x), 0, axisMax); };
    const int32 x0 = CI((Emin.X - Origin.X) / CS, SVOChunkDim - 1), x1 = CI((Emax.X - Origin.X) / CS, SVOChunkDim - 1);
    const int32 y0 = CI((Emin.Y - Origin.Y) / CS, SVOChunkDim - 1), y1 = CI((Emax.Y - Origin.Y) / CS, SVOChunkDim - 1);
    const int32 z0 = CI((Emin.Z - Origin.Z) / CS, SVOChunkDim - 1), z1 = CI((Emax.Z - Origin.Z) / CS, SVOChunkDim - 1);
    for (int32 z = z0; z <= z1; ++z)
    for (int32 y = y0; y <= y1; ++y)
    for (int32 x = x0; x <= x1; ++x)
        DirtySVOChunks.Add(x + y * SVOChunkDim + z * SVOChunkDim * SVOChunkDim);
    bSVODirty = true;
}

void APTSculptVolume::RebuildSVOChunk(int32 ChunkIndex)
{
    if (!Mesh) return;
    const int32 D = SVOChunkDim;
    const int32 cx = ChunkIndex % D;
    const int32 cy = (ChunkIndex / D) % D;
    const int32 cz = ChunkIndex / (D * D);
    const FVector Origin = SVOField.GetOrigin();
    const float CS = SVOChunkSize();
    const FBox ChunkBox(Origin + FVector(cx, cy, cz) * CS, Origin + FVector(cx + 1, cy + 1, cz + 1) * CS);

    TArray<FVector> V, N; TArray<int32> T; TArray<FColor> C;
    SVOField.BuildMeshFiltered(ChunkBox, 0.f, SVOFineThreshold(), V, T, N, C);
    ApplySVOChunkMesh(ChunkIndex, V, T, N, C);
}

// Aplica una malla ya construida (posiblemente en un hilo de fondo) al componente del chunk. Game thread.
void APTSculptVolume::ApplySVOChunkMesh(int32 ChunkIndex, const TArray<FVector>& V, const TArray<int32>& T,
                                        const TArray<FVector>& N, const TArray<FColor>& C)
{
    if (!Mesh) return;
    UProceduralMeshComponent* ChunkMesh = SVOChunkMeshes.FindRef(ChunkIndex);
    if (T.Num() == 0)
    {
        if (ChunkMesh) { ChunkMesh->DestroyComponent(); SVOChunkMeshes.Remove(ChunkIndex); }
        return;
    }
    if (!ChunkMesh)
    {
        ChunkMesh = CreateDetailLayerMesh();
        if (!ChunkMesh) return;
        ChunkMesh->SetMobility(Mesh->Mobility);
        ChunkMesh->SetCastShadow(Mesh->CastShadow);
        ChunkMesh->SetVisibility(Mesh->IsVisible());
        SVOChunkMeshes.Add(ChunkIndex, ChunkMesh);
    }
    UMaterialInterface* Mat = ClayMaterialOverride ? ClayMaterialOverride
                            : (ClayMID ? (UMaterialInterface*)ClayMID : ClayMaterial);
    TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
    ChunkMesh->CreateMeshSection(0, V, T, N, UV, C, Tan, /*collision=*/false);
    if (Mat) ChunkMesh->SetMaterial(0, Mat);
}

void APTSculptVolume::ClearSVOChunkMeshes()
{
    for (const auto& Pair : SVOChunkMeshes)
        if (Pair.Value) Pair.Value->DestroyComponent();
    SVOChunkMeshes.Reset();
}

void APTSculptVolume::RebuildSVOMesh()
{
    if (!Mesh) return;
    if (bSVOMeshing) return; // ya hay un mallado async en vuelo; se reintenta cuando termine (bSVODirty sigue)

    TArray<FBox> RefinedBounds;
    SVOField.Balance(&RefinedBounds); // mantiene 2:1 (barato, solo superficie)

    // Capas de detalle: enteras (son chicas, sync).
    for (int32 i : DirtySVODetailLayers)
        if (SVODetailFields.IsValidIndex(i) && SVODetailFields[i].IsValid() && DetailMeshes.IsValidIndex(i))
            RebuildSVOInto(*SVODetailFields[i], DetailMeshes[i]);
    DirtySVODetailLayers.Reset();

    // Ya NO usamos chunks: se genera UNA sola malla watertight de todo el modelo con DC recursivo,
    // en un hilo de fondo (async) sobre un CLON completo → sin costuras entre secciones = CERO huecos.
    DirtySVOChunks.Reset();
    bSVOCoarseDirty = false;
    bSVODirty = false;

    const FBox Whole(SVOField.GetOrigin() - FVector(1.f),
                     SVOField.GetOrigin() + FVector(SVOField.GetRootSize() + 1.f));
    TSharedPtr<FPTVoxelOctree> Clone = SVOField.CloneRegion(Whole);

    bSVOMeshing = true;
    const uint32 Gen = SVOMeshGen;
    TWeakObjectPtr<APTSculptVolume> WeakThis(this);
    Async(EAsyncExecution::ThreadPool, [WeakThis, Clone, Gen]()
    {
        struct FRes { TArray<FVector> V, N; TArray<int32> T; TArray<FColor> C; };
        TSharedPtr<FRes, ESPMode::ThreadSafe> R = MakeShared<FRes, ESPMode::ThreadSafe>();
        Clone->BuildMeshMC(R->V, R->T, R->N, R->C); // Marching Cubes uniforme = watertight garantizado

        AsyncTask(ENamedThreads::GameThread, [WeakThis, R, Gen]()
        {
            APTSculptVolume* Self = WeakThis.Get();
            if (!Self) return;
            if (Gen == Self->SVOMeshGen && Self->Mesh)
            {
                Self->ClearSVOChunkMeshes(); // limpiar los componentes de chunk (ya no se usan)
                UMaterialInterface* Mat = Self->ClayMaterialOverride ? Self->ClayMaterialOverride
                                        : (Self->ClayMID ? (UMaterialInterface*)Self->ClayMID : Self->ClayMaterial);
                if (R->V.Num() == 0) Self->Mesh->ClearMeshSection(0);
                else
                {
                    TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
                    Self->Mesh->CreateMeshSection(0, R->V, R->T, R->N, UV, R->C, Tan, /*collision=*/false);
                    if (Mat) Self->Mesh->SetMaterial(0, Mat);
                }
            }
            Self->bSVOMeshing = false;
        });
    });
}

// ─── RPCs de replicación ──────────────────────────────────────────────────────

bool APTSculptVolume::Server_ApplyStamp_Validate(FVector, EPTStampShape, float, EPTEditMode, FLinearColor, FRotator, FVector)
{
    return true;
}

void APTSculptVolume::Server_ApplyStamp_Implementation(FVector WorldPos, EPTStampShape Shape,
                                                        float Size, EPTEditMode Mode, FLinearColor PaintColor,
                                                        FRotator StampRot, FVector StampScale)
{
    Multicast_ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor, StampRot, /*bDetail=*/false, StampScale);
}

void APTSculptVolume::Multicast_ApplyStamp_Implementation(FVector WorldPos, EPTStampShape Shape,
                                                           float Size, EPTEditMode Mode, FLinearColor PaintColor,
                                                           FRotator StampRot, bool bDetail, FVector StampScale)
{
    // Modo SVO: base + capas de detalle sobre octrees paralelos.
    if (bUseSVO)
    {
        if (Mode == EPTEditMode::Erase)
        {
            ActiveSVO = &SVOField;
            ApplyStampAndFX(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale); // base + FX
            for (const TSharedPtr<FPTVoxelOctree>& L : SVODetailFields)
                if (L.IsValid()) { ActiveSVO = L.Get(); ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale); }
            ActiveSVO = &SVOField;
            // También borrar los ojos que caen bajo la brocha.
            const float MaxSc = FMath::Max3(StampScale.X, StampScale.Y, StampScale.Z);
            EraseEyesNear(WorldPos, Size * 0.5f * MaxSc);
        }
        else
        {
            ActiveSVO = (bDetail && SVODetailFields.Num() > 0) ? SVODetailFields.Last().Get() : &SVOField;
            ApplyStampAndFX(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale);
            ActiveSVO = &SVOField;
        }
        return;
    }

    // BORRAR afecta la BASE y TODAS las capas de detalle: saca arcilla de todo lo que haya bajo la
    // brocha (si no, no se podrían borrar los lentes/bigote de una capa). La FX sale una sola vez.
    if (Mode == EPTEditMode::Erase)
    {
        ActiveField = &Field;
        ApplyStampAndFX(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale); // base + partículas
        for (const TSharedPtr<FPTSculptField>& L : DetailFields)
            if (L.IsValid())
            {
                ActiveField = L.Get();
                ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale); // capa (sin FX repetida)
            }
        ActiveField = &Field;
        return;
    }

    // Add/Paint/Smooth: campo destino = la última capa si bDetail, o la base. Se restaura al final.
    ActiveField = (bDetail && DetailFields.Num() > 0) ? DetailFields.Last().Get() : &Field;
    ApplyStampAndFX(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale);
    ActiveField = &Field;
}

UProceduralMeshComponent* APTSculptVolume::CreateDetailLayerMesh()
{
    UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this);
    if (!M) return nullptr;
    M->SetupAttachment(GetRootComponent()); // el root ES el Mesh base
    M->RegisterComponent();
    // Transform relativo IDENTIDAD: la malla base guarda sus verts en espacio del root (coords del
    // campo), sin transform extra. Si le pusiéramos el transform del root (que como root = transform
    // del actor) se aplicaría DOBLE y el detalle saldría desplazado en Z / fuera del área.
    M->SetRelativeTransform(FTransform::Identity);
    M->SetCollisionEnabled(ECollisionEnabled::NoCollision); // igual que la arcilla base
    return M;
}

void APTSculptVolume::Multicast_BeginDetailLayer_Implementation()
{
    // Modo SVO: nueva capa = su propio octree + su propio mesh.
    if (bUseSVO)
    {
        if (!bSVOInit) InitSVO();
        TSharedPtr<FPTVoxelOctree> Layer = MakeShared<FPTVoxelOctree>();
        InitSVOOctree(*Layer);
        SVODetailFields.Add(Layer);
        DirtySVODetailLayers.Add(SVODetailFields.Num() - 1);
        DetailMeshes.Add(CreateDetailLayerMesh());
        UndoOrder.Add(1);
        bSVODirty = true;
        return;
    }

    // Nueva capa: su propio campo SDF (con los parámetros del volumen) + su propio mesh.
    TSharedPtr<FPTSculptField> Layer = MakeShared<FPTSculptField>();
    Layer->VoxelSize        = VoxelSize;
    Layer->DisplaySmoothing = DisplaySmoothing;
    DetailFields.Add(Layer);
    DetailMeshes.Add(CreateDetailLayerMesh());
    UndoOrder.Add(1); // 1 = capa de detalle (para el undo LIFO)
}

void APTSculptVolume::ApplyStampAndFX(FVector WorldPos, EPTStampShape Shape, float Size,
                                       EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot,
                                       FVector StampScale)
{
    // ApplyStamp devuelve, según la herramienta:
    //  · Borrar: true si sacó arcilla sólida (y deja su color en LastErasedColor).
    //  · Pintar: true si pintó sobre superficie (no en el aire).
    //  · Agregar: true si cambió algo (no usa partículas: la arcilla nueva brilla en la malla).
    const bool bChanged = ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor, StampRot, StampScale);

    switch (Mode)
    {
    case EPTEditMode::Erase:
        // Solo si borró arcilla sólida: los cubitos "son" lo que sacaste, con su color.
        if (bChanged) PlaySculptFX(FXErase, Mode, WorldPos, LastErasedColor, Size);
        break;
    case EPTEditMode::Paint:
        // Solo si pintó sobre la malla (no en el aire).
        if (bChanged) PlaySculptFX(FXPaint, Mode, WorldPos, PaintColor, Size);
        break;
    default: break; // Add / Smooth: sin partículas
    }
}

void APTSculptVolume::PlaySculptFX(UNiagaraSystem* Sys, EPTEditMode Mode, const FVector& WorldPos, const FLinearColor& Color, float BrushSize)
{
    if (!Sys) return;

    // Crear la fuente una sola vez (dedicated server no renderiza → no la creamos ahí).
    if (!SculptFX)
    {
        if (GetNetMode() == NM_DedicatedServer) return;
        SculptFX = NewObject<UNiagaraComponent>(this, TEXT("SculptFX"));
        SculptFX->SetupAttachment(GetRootComponent());
        SculptFX->SetAutoActivate(false);
        SculptFX->SetAutoDestroy(false); // es persistente: se reusa entre trazos
        SculptFX->RegisterComponent();
    }

    // Cambiar el sistema solo si cambió la herramienta (reasignar por frame es caro).
    const bool bAssetChanged = (SculptFXMode != Mode || SculptFX->GetAsset() != Sys);
    if (bAssetChanged)
    {
        SculptFX->SetAsset(Sys);
        SculptFXMode = Mode;
    }

    SculptFX->SetWorldLocation(WorldPos);
    // Setear el color del User param bajo LOS DOS nombres ("Color" y "User.Color"): según la versión
    // de Unreal, SetVariableLinearColor espera el nombre con o sin el prefijo "User.". Poner el que
    // no existe es inofensivo (no-op), así funciona en cualquier caso.
    SculptFX->SetVariableLinearColor(SculptFXColorParam, Color);
    {
        const FString N = SculptFXColorParam.ToString();
        if (!N.StartsWith(TEXT("User.")))
            SculptFX->SetVariableLinearColor(FName(*(TEXT("User.") + N)), Color);
    }

    // Radio de la brocha (en UU): BrushSize es el diámetro del sello, así que el radio = mitad.
    // Se manda a un User param float para que el Shape Location (esfera) escale con el pincel.
    const float BrushRadius = BrushSize * 0.5f;
    SculptFX->SetVariableFloat(SculptFXRadiusParam, BrushRadius);
    {
        const FString N = SculptFXRadiusParam.ToString();
        if (!N.StartsWith(TEXT("User.")))
            SculptFX->SetVariableFloat(FName(*(TEXT("User.") + N)), BrushRadius);
    }

    // Borrar usa un MESH renderer: para teñir su material (parámetro "Color") se le pasa un material
    // dinámico como override (parámetro User de tipo Material). Así el color del cubito = color de
    // la arcilla borrada. El color del MID se actualiza en vivo cada sello; el binding del material
    // en cambio hay que (re)aplicarlo cuando se (re)asigna el sistema y forzar un reinit para que el
    // mesh renderer lo tome (si no, sigue usando su material fijo con el color por defecto).
    bool bNeedReinit = false;
    if (Mode == EPTEditMode::Erase && EraseParticleMaterial)
    {
        if (!EraseMID) EraseMID = UMaterialInstanceDynamic::Create(EraseParticleMaterial, this);
        if (EraseMID)
        {
            EraseMID->SetVectorParameterValue(EraseMaterialColorParam, Color); // en vivo
            if (bAssetChanged || !SculptFX->IsActive())
            {
                SculptFX->SetVariableMaterial(EraseMaterialUserParam, EraseMID);
                bNeedReinit = true;
            }
        }
    }

    if (!SculptFX->IsActive()) SculptFX->Activate();
    if (bNeedReinit) SculptFX->ReinitializeSystem(); // el renderer toma el material override

    SculptFXLastTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void APTSculptVolume::ClearAll()
{
    // ── Geometría: descartar el campo y vaciar todas las secciones del mesh. ──
    // (Un rebuild async en vuelo tiene su propio snapshot copiado, así que reasignar
    //  el campo es seguro; a lo sumo una sección tardía se recrea y se limpia al
    //  próximo trazo/clear — inofensivo dado que los turnos están a segundos.)
    Field = FPTSculptField();
    Field.VoxelSize        = VoxelSize;
    Field.DisplaySmoothing = DisplaySmoothing;
    if (Mesh) Mesh->ClearAllMeshSections();
    ClearSVOChunkMeshes();
    DirtySVODetailLayers.Reset();
    TimeSinceRebuild = 0.f;

    // Modo SVO: reiniciar el octree base y descartar capas de detalle (lienzo en blanco).
    if (bUseSVO) { SVODetailFields.Reset(); InitSVO(); }

    // Capas de detalle: destruir sus meshes y descartar sus campos (el lienzo queda en blanco).
    for (UProceduralMeshComponent* M : DetailMeshes) if (M) M->DestroyComponent();
    DetailMeshes.Reset();
    DetailFields.Reset();
    UndoOrder.Reset();
    ActiveField = &Field;

    // Ojos: limpiar visualmente en todos; resetear el array autoritativo sólo en el servidor.
    if (HasAuthority()) Eyes.Reset();
    if (EyesMesh) EyesMesh->ClearAllMeshSections();

    // Undo: con el lienzo en blanco ya no hay nada a qué volver (y los respaldos apuntarían a
    // bricks de un campo que se descartó).
    Field.ClearUndo();
    VolumeUndoStack.Reset();
    CurrentVolumeUndo = FPTVolumeUndo();
    bRecordingStroke  = false;

    // ── Color: vaciar los buffers y re-subir la page table en blanco. Con toda la
    //    page table a 0 (slot vacío) el material no lee ningún brick → sin pintura,
    //    sin necesidad de recrear texturas ni re-bindear el material. ──
    for (float&  P : PageBuf)  P = 0.f;
    for (FColor& C : AtlasBuf) C = FColor(0, 0, 0, 0);
    BrickSlot.Empty();
    DirtyTiles.Empty();
    DirtyPageIdx.Reset();
    FreeSlots.Reset();
    SlotUsed.Init(0, AtlasCapacity);
    NextSlot    = 0;
    bPageDirty  = true;  // fuerza subida COMPLETA de la page table vacía
    bPaintDirty = false;
    UploadColorField();
}

void APTSculptVolume::SaveFieldState(TArray<uint8>& Out)
{
    // Modo SVO: el estado es el octree serializado (geometría + color). El flag es igual en server y
    // clientes → no hay ambigüedad de formato. Cubre late-join / reconexión.
    if (bUseSVO)
    {
        if (!bSVOInit) InitSVO();
        Out.Reset();
        FMemoryWriter Ar(Out, /*bIsPersistent=*/true);
        TArray<uint8> Base; SVOField.Serialize(Base); Ar << Base;
        int32 NumLayers = SVODetailFields.Num(); Ar << NumLayers;
        for (const TSharedPtr<FPTVoxelOctree>& L : SVODetailFields)
        { TArray<uint8> B; if (L.IsValid()) L->Serialize(B); Ar << B; }
        return;
    }

    Out.Reset();
    FMemoryWriter Ar(Out, /*bIsPersistent=*/true);
    Field.SerializeState(Ar);

    // También las CAPAS de detalle (para re-editar la cabeza con sus lentes/bigote intactos).
    TArray<FPTSculptField*> Valid;
    for (const TSharedPtr<FPTSculptField>& L : DetailFields) if (L.IsValid()) Valid.Add(L.Get());
    int32 NumLayers = Valid.Num();
    Ar << NumLayers;
    for (FPTSculptField* L : Valid) L->SerializeState(Ar);
}

bool APTSculptVolume::LoadFieldState(const TArray<uint8>& In)
{
    if (In.Num() == 0) return false;

    // Modo SVO: cargar el octree base + capas de detalle, y forzar remallado.
    if (bUseSVO)
    {
        FMemoryReader Ar(In, /*bIsPersistent=*/true);
        ClearSVOChunkMeshes();
        TArray<uint8> Base; Ar << Base;
        const bool bOk = SVOField.LoadFromBytes(Base);
        bSVOInit = bOk;

        // Descartar capas actuales.
        for (UProceduralMeshComponent* M : DetailMeshes) if (M) M->DestroyComponent();
        DetailMeshes.Reset();
        SVODetailFields.Reset();
        UndoOrder.Reset();
        ActiveSVO = &SVOField;

        if (!Ar.AtEnd())
        {
            int32 NumLayers = 0; Ar << NumLayers;
            for (int32 i = 0; i < NumLayers; ++i)
            {
                TArray<uint8> B; Ar << B;
                TSharedPtr<FPTVoxelOctree> L = MakeShared<FPTVoxelOctree>();
                L->LoadFromBytes(B);
                SVODetailFields.Add(L);
                DetailMeshes.Add(CreateDetailLayerMesh());
                UndoOrder.Add(1);
            }
        }

        MarkAllSVODirty();
        TimeSinceRebuild = RebuildInterval; // remallar en el próximo tick
        return bOk;
    }

    // Cargar el campo (SerializeState limpia lo previo y marca todos los bricks dirty).
    FMemoryReader Ar(In, /*bIsPersistent=*/true);
    Field.SerializeState(Ar);
    Field.VoxelSize        = VoxelSize;         // asegurar los parámetros del volumen actual
    Field.DisplaySmoothing = DisplaySmoothing;

    // Limpiar capas actuales antes de cargar las guardadas.
    for (UProceduralMeshComponent* M : DetailMeshes) if (M) M->DestroyComponent();
    DetailMeshes.Reset();
    DetailFields.Reset();
    UndoOrder.Reset();
    ActiveField = &Field;

    // Capas de detalle guardadas (si el blob es viejo y no las trae, AtEnd() corta acá → 0 capas).
    if (!Ar.AtEnd())
    {
        int32 NumLayers = 0;
        Ar << NumLayers;
        for (int32 i = 0; i < NumLayers && !Ar.AtEnd(); ++i)
        {
            TSharedPtr<FPTSculptField> L = MakeShared<FPTSculptField>();
            L->SerializeState(Ar);
            L->VoxelSize        = VoxelSize;
            L->DisplaySmoothing = DisplaySmoothing;
            DetailFields.Add(L);
            DetailMeshes.Add(CreateDetailLayerMesh());
            UndoOrder.Add(1);
        }
    }

    // Undo del volumen (pintura/ojos) también en blanco: los respaldos apuntarían a un campo viejo.
    VolumeUndoStack.Reset();
    CurrentVolumeUndo = FPTVolumeUndo();
    bRecordingStroke  = false;

    // NO re-mallar acá: el que llama (EnterHeadSculpt) setea el material (ClayMaterialOverride =
    // HeadPaintMID) DESPUÉS de LoadFieldState. Si remalláramos ya, las secciones tomarían el material
    // equivocado. Solo limpiamos y dejamos los bricks dirty → el próximo Tick remalla con el material
    // correcto (base + capas).
    if (Mesh) Mesh->ClearAllMeshSections();
    TimeSinceRebuild = RebuildInterval; // forzar el remallado en el próximo tick
    return true;
}

// ─── Snapshot COMPLETO (geometría + pintura) para (re)conexiones tardías ───────
void APTSculptVolume::SavePaintState(TArray<uint8>& Out) const
{
    Out.Reset();
    FMemoryWriter Ar(Out, /*bIsPersistent=*/true);

    // Recolectar todos los voxeles de color pintados (alpha>0). Bounded por la superficie pintada,
    // no por el atlas entero → mucho más chico que mandar los 8MB del atlas completo.
    TArray<FIntVector> Vox;
    TArray<FColor>     Cols;
    for (const TPair<FIntVector, int32>& It : BrickSlot)
    {
        const FIntVector BC   = It.Key;
        const int32      Slot = It.Value;
        const int32 TileX = Slot % AtlasTilesPerRow;
        const int32 TileY = Slot / AtlasTilesPerRow;
        for (int32 lz = 0; lz < CB; ++lz)
        for (int32 ly = 0; ly < CB; ++ly)
        for (int32 lx = 0; lx < CB; ++lx)
        {
            const int32 ax   = TileX * CB + lx;
            const int32 ay   = TileY * (CB * CB) + lz * CB + ly;
            const int32 AIdx = ax + ay * AtlasW;
            if (!AtlasBuf.IsValidIndex(AIdx) || AtlasBuf[AIdx].A == 0) continue;
            Vox.Add(FIntVector(BC.X * CB + lx, BC.Y * CB + ly, BC.Z * CB + lz));
            Cols.Add(AtlasBuf[AIdx]);
        }
    }

    int32 Count = Vox.Num();
    Ar << Count;
    for (int32 i = 0; i < Count; ++i)
    {
        Ar << Vox[i];
        FColor C = Cols[i];
        Ar << C;
    }
}

void APTSculptVolume::LoadPaintState(const TArray<uint8>& In)
{
    // Vaciar el color field actual (igual que la sección de color de ClearAll).
    for (float&  P : PageBuf)  P = 0.f;
    for (FColor& C : AtlasBuf) C = FColor(0, 0, 0, 0);
    BrickSlot.Empty();
    DirtyTiles.Empty();
    DirtyPageIdx.Reset();
    FreeSlots.Reset();
    SlotUsed.Init(0, AtlasCapacity);
    NextSlot   = 0;
    bPageDirty = true;

    if (In.Num() > 0)
    {
        FMemoryReader Ar(In, /*bIsPersistent=*/true);
        int32 Count = 0;
        Ar << Count;
        for (int32 i = 0; i < Count && !Ar.AtEnd(); ++i)
        {
            FIntVector V; FColor C;
            Ar << V;
            Ar << C;
            WriteColorVoxel(V.X, V.Y, V.Z, C); // reasigna slots + page + atlas
        }
    }

    // Los respaldos de undo apuntarían a slots viejos → resetear.
    VolumeUndoStack.Reset();
    CurrentVolumeUndo = FPTVolumeUndo();
    UploadColorField();
}

void APTSculptVolume::SaveSnapshot(TArray<uint8>& Out)
{
    Out.Reset();
    FMemoryWriter Ar(Out, /*bIsPersistent=*/true);

    TArray<uint8> FieldBytes; SaveFieldState(FieldBytes);
    TArray<uint8> PaintBytes; SavePaintState(PaintBytes);
    Ar << FieldBytes;
    Ar << PaintBytes;
}

void APTSculptVolume::LoadSnapshot(const TArray<uint8>& In)
{
    if (In.Num() == 0) return;
    FMemoryReader Ar(In, /*bIsPersistent=*/true);

    TArray<uint8> FieldBytes; Ar << FieldBytes;
    LoadFieldState(FieldBytes);                 // geometría base + capas (defiere el remallado)
    if (!Ar.AtEnd())
    {
        TArray<uint8> PaintBytes; Ar << PaintBytes;
        LoadPaintState(PaintBytes);             // pintura del atlas 3D
    }
}

void APTSculptVolume::Multicast_ClearAll_Implementation()
{
    ClearAll();
}

// ─── Undo ─────────────────────────────────────────────────────────────────────
void APTSculptVolume::BackupAtlas(int32 AIdx, int32 Slot)
{
    if (!bRecordingStroke) return;
    if (!AtlasBuf.IsValidIndex(AIdx)) return;
    CurrentVolumeUndo.Slots.Add(Slot);
    if (CurrentVolumeUndo.AtlasOld.Contains(AIdx)) return; // ya respaldado en este trazo
    CurrentVolumeUndo.AtlasOld.Add(AIdx, AtlasBuf[AIdx]);
}

void APTSculptVolume::Multicast_BeginStroke_Implementation()
{
    if (bUseSVO) { if (!bSVOInit) InitSVO(); SVOField.PushUndoSnapshot(); return; } // snapshot de la base

    Field.BeginStroke();
    CurrentVolumeUndo = FPTVolumeUndo();
    CurrentVolumeUndo.EyesCount = Eyes.Num();
    bRecordingStroke = true;
}

void APTSculptVolume::Multicast_EndStroke_Implementation()
{
    if (bUseSVO) { UndoOrder.Add(0); return; }   // 0 = trazo de la BASE (el snapshot ya se guardó en BeginStroke)

    Field.PushStroke();                          // la pila del campo y esta van 1:1
    VolumeUndoStack.Add(MoveTemp(CurrentVolumeUndo));
    CurrentVolumeUndo = FPTVolumeUndo();
    bRecordingStroke = false;
    UndoOrder.Add(0);                            // 0 = trazo de la BASE (para el undo LIFO)
    while (VolumeUndoStack.Num() > MaxUndoSteps)
    {
        VolumeUndoStack.RemoveAt(0);
        // Sacar el '0' (base) más viejo de UndoOrder para que siga alineado con VolumeUndoStack.
        const int32 Idx = UndoOrder.IndexOfByKey((uint8)0);
        if (Idx != INDEX_NONE) UndoOrder.RemoveAt(Idx);
    }
}

void APTSculptVolume::Multicast_Undo_Implementation()
{
    // Modo SVO: LIFO igual que el clásico — última capa entera, o último trazo de la base (snapshot).
    if (bUseSVO)
    {
        if (UndoOrder.Num() > 0 && UndoOrder.Last() == 1)
        {
            UndoOrder.Pop();
            if (SVODetailFields.Num() > 0) SVODetailFields.Pop();
            if (DetailMeshes.Num() > 0)
            {
                if (UProceduralMeshComponent* M = DetailMeshes.Last()) M->DestroyComponent();
                DetailMeshes.Pop();
            }
            bSVODirty = true; TimeSinceRebuild = RebuildInterval;
            return;
        }
        if (UndoOrder.Num() > 0 && UndoOrder.Last() == 0) UndoOrder.Pop();
        if (SVOField.Undo()) { MarkAllSVODirty(); TimeSinceRebuild = RebuildInterval; }
        return;
    }

    // El undo es LIFO sobre TODAS las operaciones: si lo último fue una CAPA de detalle, se saca la
    // capa entera (su campo + su mesh); si fue un trazo de la base, se deshace ese trazo.
    if (UndoOrder.Num() > 0 && UndoOrder.Last() == 1)
    {
        UndoOrder.Pop();
        if (DetailFields.Num() > 0) DetailFields.Pop();
        if (DetailMeshes.Num() > 0)
        {
            if (UProceduralMeshComponent* M = DetailMeshes.Last()) M->DestroyComponent();
            DetailMeshes.Pop();
        }
        return;
    }
    if (UndoOrder.Num() > 0 && UndoOrder.Last() == 0) UndoOrder.Pop();

    if (VolumeUndoStack.Num() == 0) return;

    // 1) Geometría: el campo restaura los bricks del último trazo y los marca dirty (se remallan).
    Field.UndoStroke();

    // 2) Pintura: devolver los texels del atlas a su color previo.
    const FPTVolumeUndo U = MoveTemp(VolumeUndoStack.Last());
    VolumeUndoStack.Pop();
    for (const auto& It : U.AtlasOld)
    {
        if (!AtlasBuf.IsValidIndex(It.Key)) continue;
        AtlasBuf[It.Key] = It.Value;
    }
    if (U.AtlasOld.Num() > 0)
    {
        for (const int32 Slot : U.Slots) DirtyTiles.Add(Slot); // re-subir esos tiles
        bPaintDirty = true;
        UploadColorField();
    }

    // 3) Ojos: son autoritativos del servidor (se replican por la propiedad Eyes) → solo él los
    //    recorta; a los clientes les llega por OnRep_Eyes y reconstruyen su malla.
    if (HasAuthority() && Eyes.Num() > U.EyesCount)
    {
        Eyes.SetNum(U.EyesCount);
        RebuildEyesMesh();
    }

    TimeSinceRebuild = RebuildInterval; // forzar el remallado en el próximo tick
}

bool APTSculptVolume::IsInsideCanvas(FVector WorldPos) const
{
    if (!BoundsBox) return true;
    // A espacio local de la caja (InverseTransformPosition ya saca su escala) → comparar contra
    // el extent sin escalar.
    const FVector L = BoundsBox->GetComponentTransform().InverseTransformPosition(WorldPos);
    const FVector E = BoundsBox->GetUnscaledBoxExtent();
    return FMath::Abs(L.X) <= E.X && FMath::Abs(L.Y) <= E.Y && FMath::Abs(L.Z) <= E.Z;
}

FVector APTSculptVolume::ClampInsideCanvas(FVector WorldPos, float InsetRadius) const
{
    if (!BoundsBox) return WorldPos;
    const FTransform T = BoundsBox->GetComponentTransform();
    FVector L = T.InverseTransformPosition(WorldPos);         // a espacio local (extent sin escalar)
    const FVector E = BoundsBox->GetUnscaledBoxExtent();
    // Margen = radio de la brocha en cada cara, para que la ESFERA del sello no se pase de la pared.
    const FVector Lim(FMath::Max(0.f, E.X - InsetRadius),
                      FMath::Max(0.f, E.Y - InsetRadius),
                      FMath::Max(0.f, E.Z - InsetRadius));
    L.X = FMath::Clamp(L.X, -Lim.X, Lim.X);
    L.Y = FMath::Clamp(L.Y, -Lim.Y, Lim.Y);
    L.Z = FMath::Clamp(L.Z, -Lim.Z, Lim.Z);
    return T.TransformPosition(L);
}

// ─── Ojos (replicados) ─────────────────────────────────────────────────────────
void APTSculptVolume::AddEye(FVector WorldPos, float Radius)
{
    if (!HasAuthority()) return; // autoritativo: sólo el servidor modifica Eyes (se replica)
    if (!IsInsideCanvas(WorldPos)) return; // no dejar poner ojos fuera de la zona de modelado
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPos);
    Eyes.Add(FVector4(Local.X, Local.Y, Local.Z, FMath::Max(Radius, 1.f)));
    RebuildEyesMesh(); // el servidor no recibe OnRep; reconstruir acá
}

void APTSculptVolume::OnRep_Eyes()
{
    RebuildEyesMesh();
}

void APTSculptVolume::EraseEyesNear(const FVector& WorldPos, float Radius)
{
    if (!HasAuthority() || Eyes.Num() == 0) return; // Eyes es autoritativo del servidor (se replica)
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldPos);
    bool bChanged = false;
    for (int32 i = Eyes.Num() - 1; i >= 0; --i)
    {
        const FVector EC(Eyes[i].X, Eyes[i].Y, Eyes[i].Z);
        if (FVector::Dist(EC, Local) <= Radius + Eyes[i].W) { Eyes.RemoveAt(i); bChanged = true; }
    }
    if (bChanged) RebuildEyesMesh();
}

void APTSculptVolume::RebuildEyesMesh()
{
    if (!EyesMesh) return;
    EyesMesh->ClearAllMeshSections();
    if (Eyes.Num() == 0) return;

    const FPTHeadSection S = APTLobbyCharacter::BuildEyesSection(Eyes, EyeMesh, EyeBaseSize);
    if (S.Verts.Num() == 0) return;
    const TArray<FProcMeshTangent> NoTangents;
    EyesMesh->CreateMeshSection(0, S.Verts, S.Tris, S.Normals, S.UVs, S.Colors, NoTangents, false);
    UMaterialInterface* Mat = EyeMaterial ? EyeMaterial : (ClayMID ? (UMaterialInterface*)ClayMID : ClayMaterial);
    if (Mat) EyesMesh->SetMaterial(0, Mat);
}
