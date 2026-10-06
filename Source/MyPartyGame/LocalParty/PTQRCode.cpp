#include "PTQRCode.h"
#include "Engine/Texture2D.h"

namespace
{
    // Tablas para nivel de corrección M, índice = versión (0 sin usar).
    constexpr int32 MaxVersion = 10;
    const int32 EccCodewordsPerBlockM[MaxVersion + 1] = { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
    const int32 NumEccBlocksM[MaxVersion + 1]         = { -1,  1,  1,  1,  2,  2,  4,  4,  4,  5,  5 };
    constexpr int32 FormatBitsM = 0; // L=1, M=0, Q=3, H=2

    int32 NumRawDataModules(int32 Ver)
    {
        int32 Result = (16 * Ver + 128) * Ver + 64;
        if (Ver >= 2)
        {
            const int32 NumAlign = Ver / 7 + 2;
            Result -= (25 * NumAlign - 10) * NumAlign - 55;
            if (Ver >= 7) Result -= 36;
        }
        return Result;
    }

    int32 NumDataCodewords(int32 Ver)
    {
        return NumRawDataModules(Ver) / 8 - EccCodewordsPerBlockM[Ver] * NumEccBlocksM[Ver];
    }

    uint8 GfMul(uint8 X, uint8 Y)
    {
        int32 Z = 0;
        for (int32 i = 7; i >= 0; --i)
        {
            Z = (Z << 1) ^ ((Z >> 7) * 0x11D);
            Z ^= ((Y >> i) & 1) * X;
        }
        return (uint8)Z;
    }

    TArray<uint8> RsDivisor(int32 Degree)
    {
        TArray<uint8> Result;
        Result.Init(0, Degree);
        Result[Degree - 1] = 1;
        uint8 Root = 1;
        for (int32 i = 0; i < Degree; ++i)
        {
            for (int32 j = 0; j < Degree; ++j)
            {
                Result[j] = GfMul(Result[j], Root);
                if (j + 1 < Degree) Result[j] ^= Result[j + 1];
            }
            Root = GfMul(Root, 0x02);
        }
        return Result;
    }

    TArray<uint8> RsRemainder(const TArray<uint8>& Data, const TArray<uint8>& Divisor)
    {
        TArray<uint8> Result;
        Result.Init(0, Divisor.Num());
        for (uint8 B : Data)
        {
            const uint8 Factor = B ^ Result[0];
            Result.RemoveAt(0);
            Result.Add(0);
            for (int32 i = 0; i < Result.Num(); ++i) Result[i] ^= GfMul(Divisor[i], Factor);
        }
        return Result;
    }

    struct FQr
    {
        int32 Ver = 1;
        int32 Size = 21;
        TArray<bool> Mod;
        TArray<bool> Fn;

        bool  Get(int32 X, int32 Y) const { return Mod[Y * Size + X]; }
        void  SetFn(int32 X, int32 Y, bool bDark) { Mod[Y * Size + X] = bDark; Fn[Y * Size + X] = true; }

        void Init(int32 InVer)
        {
            Ver = InVer;
            Size = Ver * 4 + 17;
            Mod.Init(false, Size * Size);
            Fn.Init(false, Size * Size);
        }

        void DrawFinder(int32 X, int32 Y)
        {
            for (int32 Dy = -4; Dy <= 4; ++Dy)
                for (int32 Dx = -4; Dx <= 4; ++Dx)
                {
                    const int32 Dist = FMath::Max(FMath::Abs(Dx), FMath::Abs(Dy));
                    const int32 Xx = X + Dx, Yy = Y + Dy;
                    if (Xx >= 0 && Xx < Size && Yy >= 0 && Yy < Size)
                        SetFn(Xx, Yy, Dist != 2 && Dist != 4);
                }
        }

        void DrawAlignment(int32 X, int32 Y)
        {
            for (int32 Dy = -2; Dy <= 2; ++Dy)
                for (int32 Dx = -2; Dx <= 2; ++Dx)
                    SetFn(X + Dx, Y + Dy, FMath::Max(FMath::Abs(Dx), FMath::Abs(Dy)) != 1);
        }

        TArray<int32> AlignmentPositions() const
        {
            TArray<int32> Result;
            if (Ver == 1) return Result;
            const int32 NumAlign = Ver / 7 + 2;
            const int32 Step = (Ver * 4 + NumAlign * 2 + 1) / (NumAlign * 2 - 2) * 2;
            Result.Add(6);
            for (int32 i = 0, Pos = Size - 7; i < NumAlign - 1; ++i, Pos -= Step) Result.Insert(Pos, 1);
            return Result;
        }

        void DrawFormatBits(int32 Mask)
        {
            const int32 Data = (FormatBitsM << 3) | Mask;
            int32 Rem = Data;
            for (int32 i = 0; i < 10; ++i) Rem = (Rem << 1) ^ ((Rem >> 9) * 0x537);
            const int32 Bits = ((Data << 10) | Rem) ^ 0x5412;
            auto Bit = [Bits](int32 i) { return ((Bits >> i) & 1) != 0; };

            for (int32 i = 0; i <= 5; ++i) SetFn(8, i, Bit(i));
            SetFn(8, 7, Bit(6));
            SetFn(8, 8, Bit(7));
            SetFn(7, 8, Bit(8));
            for (int32 i = 9; i < 15; ++i) SetFn(14 - i, 8, Bit(i));

            for (int32 i = 0; i < 8; ++i) SetFn(Size - 1 - i, 8, Bit(i));
            for (int32 i = 8; i < 15; ++i) SetFn(8, Size - 15 + i, Bit(i));
            SetFn(8, Size - 8, true); // módulo oscuro fijo
        }

        void DrawVersion()
        {
            if (Ver < 7) return;
            int32 Rem = Ver;
            for (int32 i = 0; i < 12; ++i) Rem = (Rem << 1) ^ ((Rem >> 11) * 0x1F25);
            const int32 Bits = (Ver << 12) | Rem;
            for (int32 i = 0; i < 18; ++i)
            {
                const bool B = ((Bits >> i) & 1) != 0;
                const int32 A = Size - 11 + i % 3;
                const int32 C = i / 3;
                SetFn(A, C, B);
                SetFn(C, A, B);
            }
        }

        void DrawFunctionPatterns()
        {
            for (int32 i = 0; i < Size; ++i)
            {
                SetFn(6, i, i % 2 == 0);
                SetFn(i, 6, i % 2 == 0);
            }
            DrawFinder(3, 3);
            DrawFinder(Size - 4, 3);
            DrawFinder(3, Size - 4);

            const TArray<int32> Pos = AlignmentPositions();
            const int32 N = Pos.Num();
            for (int32 i = 0; i < N; ++i)
                for (int32 j = 0; j < N; ++j)
                {
                    if ((i == 0 && j == 0) || (i == 0 && j == N - 1) || (i == N - 1 && j == 0)) continue;
                    DrawAlignment(Pos[i], Pos[j]);
                }
            DrawFormatBits(0); // placeholder, se reescribe con la máscara elegida
            DrawVersion();
        }

        void DrawCodewords(const TArray<uint8>& Data)
        {
            int32 i = 0;
            for (int32 Right = Size - 1; Right >= 1; Right -= 2)
            {
                if (Right == 6) Right = 5;
                for (int32 Vert = 0; Vert < Size; ++Vert)
                    for (int32 j = 0; j < 2; ++j)
                    {
                        const int32 X = Right - j;
                        const bool bUpward = ((Right + 1) & 2) == 0;
                        const int32 Y = bUpward ? Size - 1 - Vert : Vert;
                        if (!Fn[Y * Size + X] && i < Data.Num() * 8)
                        {
                            Mod[Y * Size + X] = ((Data[i >> 3] >> (7 - (i & 7))) & 1) != 0;
                            ++i;
                        }
                    }
            }
        }

        void ApplyMask(int32 Mask)
        {
            for (int32 Y = 0; Y < Size; ++Y)
                for (int32 X = 0; X < Size; ++X)
                {
                    bool bInvert = false;
                    switch (Mask)
                    {
                        case 0: bInvert = (X + Y) % 2 == 0; break;
                        case 1: bInvert = Y % 2 == 0; break;
                        case 2: bInvert = X % 3 == 0; break;
                        case 3: bInvert = (X + Y) % 3 == 0; break;
                        case 4: bInvert = (X / 3 + Y / 2) % 2 == 0; break;
                        case 5: bInvert = X * Y % 2 + X * Y % 3 == 0; break;
                        case 6: bInvert = (X * Y % 2 + X * Y % 3) % 2 == 0; break;
                        case 7: bInvert = ((X + Y) % 2 + X * Y % 3) % 2 == 0; break;
                        default: break;
                    }
                    if (bInvert && !Fn[Y * Size + X]) Mod[Y * Size + X] = !Mod[Y * Size + X];
                }
        }

        // Penalización simplificada (reglas 1, 2 y 4 del estándar; la 3 —patrones tipo finder— se omite:
        // solo afecta qué máscara se elige, cualquier máscara produce un QR válido).
        int32 Penalty() const
        {
            int32 Result = 0;
            for (int32 Pass = 0; Pass < 2; ++Pass) // filas y columnas
                for (int32 A = 0; A < Size; ++A)
                {
                    int32 Run = 1;
                    for (int32 B = 1; B < Size; ++B)
                    {
                        const bool Cur  = Pass == 0 ? Get(B, A) : Get(A, B);
                        const bool Prev = Pass == 0 ? Get(B - 1, A) : Get(A, B - 1);
                        if (Cur == Prev)
                        {
                            ++Run;
                            if (Run == 5) Result += 3;
                            else if (Run > 5) Result += 1;
                        }
                        else Run = 1;
                    }
                }
            for (int32 Y = 0; Y < Size - 1; ++Y)
                for (int32 X = 0; X < Size - 1; ++X)
                {
                    const bool C = Get(X, Y);
                    if (C == Get(X + 1, Y) && C == Get(X, Y + 1) && C == Get(X + 1, Y + 1)) Result += 3;
                }
            int32 Dark = 0;
            for (bool B : Mod) if (B) ++Dark;
            const int32 Total = Size * Size;
            const int32 K = (FMath::Abs(Dark * 20 - Total * 10) + Total - 1) / Total - 1;
            Result += FMath::Max(0, K) * 10;
            return Result;
        }
    };
}

bool PTQR::Encode(const FString& Text, TArray<bool>& OutModules, int32& OutSize, int32 ForcedMask)
{
    const FTCHARToUTF8 Utf8(*Text);
    const int32 Len = Utf8.Length();
    const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());

    // Versión más chica donde entra: 4 bits de modo + contador (8 bits hasta v9, 16 desde v10) + datos.
    int32 Ver = 1;
    for (; Ver <= MaxVersion; ++Ver)
    {
        const int32 CcBits = Ver <= 9 ? 8 : 16;
        if (4 + CcBits + Len * 8 <= NumDataCodewords(Ver) * 8) break;
    }
    if (Ver > MaxVersion) return false;

    // ── Bits de datos ──
    TArray<bool> Bits;
    auto Append = [&Bits](uint32 Val, int32 N) { for (int32 i = N - 1; i >= 0; --i) Bits.Add(((Val >> i) & 1) != 0); };
    Append(0x4, 4);                     // modo byte
    Append(Len, Ver <= 9 ? 8 : 16);     // cantidad de bytes
    for (int32 i = 0; i < Len; ++i) Append(Bytes[i], 8);

    const int32 CapacityBits = NumDataCodewords(Ver) * 8;
    Append(0, FMath::Min(4, CapacityBits - Bits.Num())); // terminador
    Append(0, (8 - Bits.Num() % 8) % 8);                 // alinear a byte
    for (uint8 Pad = 0xEC; Bits.Num() < CapacityBits; Pad ^= 0xEC ^ 0x11) Append(Pad, 8);

    TArray<uint8> Data;
    Data.Init(0, Bits.Num() / 8);
    for (int32 i = 0; i < Bits.Num(); ++i) if (Bits[i]) Data[i >> 3] |= 1 << (7 - (i & 7));

    // ── Corrección de errores + intercalado ──
    const int32 NumBlocks     = NumEccBlocksM[Ver];
    const int32 BlockEccLen   = EccCodewordsPerBlockM[Ver];
    const int32 RawCodewords  = NumRawDataModules(Ver) / 8;
    const int32 NumShort      = NumBlocks - RawCodewords % NumBlocks;
    const int32 ShortBlockLen = RawCodewords / NumBlocks;
    const TArray<uint8> Div   = RsDivisor(BlockEccLen);

    TArray<TArray<uint8>> Blocks;
    for (int32 i = 0, K = 0; i < NumBlocks; ++i)
    {
        const int32 DatLen = ShortBlockLen - BlockEccLen + (i < NumShort ? 0 : 1);
        TArray<uint8> Dat(Data.GetData() + K, DatLen);
        K += DatLen;
        const TArray<uint8> Ecc = RsRemainder(Dat, Div);
        if (i < NumShort) Dat.Add(0);
        Dat.Append(Ecc);
        Blocks.Add(MoveTemp(Dat));
    }
    TArray<uint8> All;
    for (int32 i = 0; i < Blocks[0].Num(); ++i)
        for (int32 j = 0; j < Blocks.Num(); ++j)
            if (i != ShortBlockLen - BlockEccLen || j >= NumShort)
                All.Add(Blocks[j][i]);

    // ── Matriz ──
    FQr Q;
    Q.Init(Ver);
    Q.DrawFunctionPatterns();
    Q.DrawCodewords(All);

    int32 BestMask = ForcedMask;
    if (BestMask < 0 || BestMask > 7)
    {
        int32 BestPenalty = MAX_int32;
        for (int32 M = 0; M < 8; ++M)
        {
            Q.ApplyMask(M);
            Q.DrawFormatBits(M);
            const int32 P = Q.Penalty();
            if (P < BestPenalty) { BestPenalty = P; BestMask = M; }
            Q.ApplyMask(M); // deshacer (XOR)
        }
    }
    Q.ApplyMask(BestMask);
    Q.DrawFormatBits(BestMask);

    OutModules = MoveTemp(Q.Mod);
    OutSize = Q.Size;
    return true;
}

UTexture2D* PTQR::MakeTexture(const FString& Text, int32 PixelsPerModule)
{
    TArray<bool> Modules;
    int32 Size = 0;
    if (!Encode(Text, Modules, Size)) return nullptr;

    const int32 Quiet = 4;
    const int32 Ppm = FMath::Clamp(PixelsPerModule, 1, 32);
    const int32 Dim = (Size + Quiet * 2) * Ppm;

    UTexture2D* Tex = UTexture2D::CreateTransient(Dim, Dim, PF_B8G8R8A8);
    if (!Tex) return nullptr;
    Tex->Filter = TF_Nearest;
    Tex->SRGB = true;
    Tex->NeverStream = true;

    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
    FColor* Px = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
    for (int32 Y = 0; Y < Dim; ++Y)
        for (int32 X = 0; X < Dim; ++X)
        {
            const int32 Mx = X / Ppm - Quiet, My = Y / Ppm - Quiet;
            const bool bDark = Mx >= 0 && My >= 0 && Mx < Size && My < Size && Modules[My * Size + Mx];
            Px[Y * Dim + X] = bDark ? FColor::Black : FColor::White;
        }
    Mip.BulkData.Unlock();
    Tex->UpdateResource();
    return Tex;
}
