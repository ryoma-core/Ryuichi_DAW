/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   Ryuichi_Crystal_R_ico;
    const int            Ryuichi_Crystal_R_icoSize = 31950;

    extern const char*   Ryuichi_Crystal_R_png;
    const int            Ryuichi_Crystal_R_pngSize = 104230;

    extern const char*   Ryuichi_Brutalist_ico;
    const int            Ryuichi_Brutalist_icoSize = 437;

    extern const char*   Ryuichi_Brutalist_1024_png;
    const int            Ryuichi_Brutalist_1024_pngSize = 38405;

    extern const char*   Ryuichi_Cyan_Kata_ico;
    const int            Ryuichi_Cyan_Kata_icoSize = 541;

    extern const char*   Ryuichi_Cyan_Kata_1024_png;
    const int            Ryuichi_Cyan_Kata_1024_pngSize = 78009;

    extern const char*   Ryuichi_DarkNeon_R_ico;
    const int            Ryuichi_DarkNeon_R_icoSize = 504;

    extern const char*   Ryuichi_DarkNeon_R_1024_png;
    const int            Ryuichi_DarkNeon_R_1024_pngSize = 26165;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 8;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
