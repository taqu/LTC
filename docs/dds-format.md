# DDS Format Reference

This document describes the subset of the DirectDraw Surface (DDS) format that `ltc_core` reads.

## File Layout

```
Offset  Size  Field
------  ----  -----
0       4     Magic number: 0x20534444 ('DDS ')
4       124   DDS_HEADER
128     20    DDS_HEADER_DXT10  (present only when ddspf.dwFourCC == 'DX10')
148+    n     Pixel data (mip 0, then mip 1, …)
```

## DDS_HEADER (124 bytes)

| Offset | Size | Field                | Notes                              |
|--------|------|----------------------|------------------------------------|
| 0      | 4    | dwSize               | Must equal 124                     |
| 4      | 4    | dwFlags              | Combination of DDSD_* flags        |
| 8      | 4    | dwHeight             | Height of mip 0 in pixels          |
| 12     | 4    | dwWidth              | Width of mip 0 in pixels           |
| 16     | 4    | dwPitchOrLinearSize  | Row pitch or compressed size       |
| 20     | 4    | dwDepth              | Depth (volume textures)            |
| 24     | 4    | dwMipMapCount        | Number of mip levels (0 or 1 means 1) |
| 28     | 44   | dwReserved1[11]      | Reserved, ignored                  |
| 72     | 32   | ddspf (DDSPixelFormat) | See below                        |
| 104    | 4    | dwCaps               | Surface complexity flags           |
| 108    | 4    | dwCaps2              |                                    |
| 112    | 4    | dwCaps3              |                                    |
| 116    | 4    | dwCaps4              |                                    |
| 120    | 4    | dwReserved2          | Reserved, ignored                  |

## DDSPixelFormat (32 bytes, embedded in DDS_HEADER at offset 72)

| Offset | Size | Field         | Notes                                    |
|--------|------|---------------|------------------------------------------|
| 0      | 4    | dwSize        | Must equal 32                            |
| 4      | 4    | dwFlags       | DDPF_FOURCC (0x4) = compressed/special   |
| 8      | 4    | dwFourCC      | Format code; 'DX10' triggers DX10 header |
| 12     | 4    | dwRGBBitCount | Bits per pixel (uncompressed formats)    |
| 16     | 4    | dwRBitMask    |                                          |
| 20     | 4    | dwGBitMask    |                                          |
| 24     | 4    | dwBBitMask    |                                          |
| 28     | 4    | dwABitMask    |                                          |

## DDS_HEADER_DXT10 (20 bytes, offset 128)

Present only when `ddspf.dwFlags & DDPF_FOURCC` and `ddspf.dwFourCC == 0x30315844` ('DX10').

| Offset | Size | Field             | Notes                            |
|--------|------|-------------------|----------------------------------|
| 0      | 4    | dxgiFormat        | DXGI format enum                 |
| 4      | 4    | resourceDimension | 2 = 1D, 3 = 2D, 4 = 3D          |
| 8      | 4    | miscFlag          | Cubemap flag                     |
| 12     | 4    | arraySize         | Number of textures in array      |
| 16     | 4    | miscFlags2        | Alpha mode                       |

## Supported Formats

### DX10 Path (dxgiFormat)

| dxgiFormat | Value | ltc DDSFormat |
|-----------|-------|---------------|
| R32G32B32A32_FLOAT | 2  | RGBA32F |
| R16G16B16A16_FLOAT | 10 | RGBA16F |
| R32G32_FLOAT       | 16 | RG32F   |
| R16G16_FLOAT       | 34 | RG16F   |

### Legacy Path (ddspf.dwFourCC)

| FourCC value | D3DFMT name        | ltc DDSFormat |
|--------------|--------------------|---------------|
| 112          | G16R16F            | RG16F         |
| 113          | A16B16G16R16F      | RGBA16F       |
| 115          | G32R32F            | RG32F         |
| 116          | A32B32G32R32F      | RGBA32F       |

## Pixel Encoding

For mip level 0 of a 2D non-array texture:

- Rows are stored top-to-bottom, pixels left-to-right (no row padding for these formats).
- Byte offset of pixel (x, y) = `dataOffset + (y * width + x) * bytesPerPixel`

| DDSFormat | Bytes/pixel | Channel layout (byte order) |
|-----------|-------------|------------------------------|
| RG16F     | 4           | R16, G16                    |
| RGBA16F   | 8           | R16, G16, B16, A16          |
| RG32F     | 8           | R32, G32                    |
| RGBA32F   | 16          | R32, G32, B32, A32          |

16-bit channels are IEEE 754 binary16 (fp16). 32-bit channels are IEEE 754 binary32 (float).

## fp16 Conversion

The fp16 bit layout (MSB to LSB):

```
bit 15    : sign
bits 14-10: exponent (5 bits, biased by 15)
bits 9-0  : mantissa (10 bits, implicit leading 1 for normals)
```

Conversion to fp32 (`DDSReader::fp16ToFloat`):

- Zero / negative zero: preserved exactly.
- Normal: fp32 exp = fp16 exp + 112.
- Subnormal: normalize by counting leading-zero bits in mantissa; fp32 exp = 113 - shift.
- Infinity / NaN: preserved (mantissa bits shifted to fp32 mantissa field).

## Validation

`DDSReader::readMetadata` checks:

1. File size ≥ 128 bytes (magic + header).
2. Magic == `0x20534444`.
3. `dwSize == 124`.
4. `ddspf.dwSize == 32`.
5. `dwWidth > 0` and `dwHeight > 0`.
6. If DX10: file size ≥ 148 bytes; `resourceDimension == 3` (TEXTURE2D); `arraySize == 1`.
7. Format is one of the supported values above.
8. `dataSize = width * bytesPerPixel * height` does not overflow `uint64_t`.
9. File size ≥ `dataOffset + dataSize`.
