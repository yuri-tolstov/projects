// Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//   The source code contained or described herein and all documents
//   related to the source code ("Material") are owned by Tilera
//   Corporation or its suppliers or licensors.  Title to the Material
//   remains with Tilera Corporation or its suppliers and licensors. The
//   software is licensed under the Tilera MDE License.
//
//   Unless otherwise agreed by Tilera in writing, you may not remove or
//   alter this notice or any other notice embedded in Materials by Tilera
//   or Tilera's suppliers or licensors in any way.
//
//
//

#ifndef _FFT_1024_TWIDDLES_H_
#define _FFT_1024_TWIDDLES_H_

int32_t twiddles[1024] = {
  0xcf057640, 0xa57f5a81, 0x89c030fb, 0xa57f5a81, 0x80010000, 0xa57fa57f, 0x89c030fb, 0xa57fa57f, 
  0x30fb89c0, 0x7fff, 0x7fff, 0x7fff, 0xf3757f61, 0xe7087d89, 0xdad97a7c, 0xe7087d89, 
  0xcf057640, 0xb8e46a6c, 0xdad97a7c, 0xb8e46a6c, 0x9d0f5133, 0xcf057640, 0xa57f5a81, 0x89c030fb, 
  0xc3aa70e1, 0x9594471c, 0x809f0c8b, 0xb8e46a6c, 0x89c030fb, 0x8277e708, 0xaecd62f1, 0x827718f8, 
  0x8f1fc3aa, 0xa57f5a81, 0x80010000, 0xa57fa57f, 0x9d0f5133, 0x8277e708, 0xc3aa8f1f, 0x9594471c, 
  0x89c0cf05, 0xe7088277, 0x8f1f3c56, 0x9594b8e4, 0xc8b809f, 0x89c030fb, 0xa57fa57f, 0x30fb89c0, 
  0x85842527, 0xb8e49594, 0x51339d0f, 0x827718f8, 0xcf0589c0, 0x6a6cb8e4, 0x809f0c8b, 0xe7088277, 
  0x7a7cdad9, 0x7fff, 0x7fff, 0x7fff, 0xfcdc7ff5, 0xf9b97fd7, 0xf6967fa6, 0xf9b97fd7, 
  0xf3757f61, 0xed397e9c, 0xf6967fa6, 0xed397e9c, 0xe3f57ce2, 0xf3757f61, 0xe7087d89, 0xdad97a7c, 
  0xf0557f08, 0xe0e77c29, 0xd1f0776b, 0xed397e9c, 0xdad97a7c, 0xc94773b5, 0xea1f7e1c, 0xd4e27883, 
  0xc0ea6f5e, 0xe7087d89, 0xcf057640, 0xb8e46a6c, 0xe3f57ce2, 0xc94773b5, 0xb14164e7, 0xe0e77c29, 
  0xc3aa70e1, 0xaa0c5ed6, 0xdddd7b5c, 0xbe336dc9, 0xa34d5842, 0xdad97a7c, 0xb8e46a6c, 0x9d0f5133, 
  0xd7da7989, 0xb3c166ce, 0x975b49b3, 0xd4e27883, 0xaecd62f1, 0x923741cd, 0xd1f0776b, 0xaa0c5ed6, 
  0x8dac398c, 0xcf057640, 0xa57f5a81, 0x89c030fb, 0xcc227503, 0xa12a55f4, 0x86772826, 0xc94773b5, 
  0x9d0f5133, 0x83d71f19, 0xc6747254, 0x99324c3f, 0x81e415e1, 0xc3aa70e1, 0x9594471c, 0x809f0c8b, 
  0xc0ea6f5e, 0x923741cd, 0x800b0324, 0xbe336dc9, 0x8f1f3c56, 0x8029f9b9, 0xbb866c23, 0x8c4b36b9, 
  0x80f8f055, 0xb8e46a6c, 0x89c030fb, 0x8277e708, 0xb64d68a5, 0x877d2b1e, 0x84a4dddd, 0xb3c166ce, 
  0x85842527, 0x877dd4e2, 0xb14164e7, 0x83d71f19, 0x8afdcc22, 0xaecd62f1, 0x827718f8, 0x8f1fc3aa, 
  0xac6660eb, 0x816412c7, 0x93ddbb86, 0xaa0c5ed6, 0x809f0c8b, 0x9932b3c1, 0xa7be5cb3, 0x80290647, 
  0x9f15ac66, 0xa57f5a81, 0x80010000, 0xa57fa57f, 0xa34d5842, 0x8029f9b9, 0xac669f15, 0xa12a55f4, 
  0x809ff375, 0xb3c19932, 0x9f15539a, 0x8164ed39, 0xbb8693dd, 0x9d0f5133, 0x8277e708, 0xc3aa8f1f, 
  0x9b194ebf, 0x83d7e0e7, 0xcc228afd, 0x99324c3f, 0x8584dad9, 0xd4e2877d, 0x975b49b3, 0x877dd4e2, 
  0xdddd84a4, 0x9594471c, 0x89c0cf05, 0xe7088277, 0x93dd447a, 0x8c4bc947, 0xf05580f8, 0x923741cd, 
  0x8f1fc3aa, 0xf9b98029, 0x90a23f16, 0x9237be33, 0x324800b, 0x8f1f3c56, 0x9594b8e4, 0xc8b809f, 
  0x8dac398c, 0x9932b3c1, 0x15e181e4, 0x8c4b36b9, 0x9d0faecd, 0x1f1983d7, 0x8afd33de, 0xa12aaa0c, 
  0x28268677, 0x89c030fb, 0xa57fa57f, 0x30fb89c0, 0x88952e10, 0xaa0ca12a, 0x398c8dac, 0x877d2b1e, 
  0xaecd9d0f, 0x41cd9237, 0x86772826, 0xb3c19932, 0x49b3975b, 0x85842527, 0xb8e49594, 0x51339d0f, 
  0x84a42223, 0xbe339237, 0x5842a34d, 0x83d71f19, 0xc3aa8f1f, 0x5ed6aa0c, 0x831e1c0b, 0xc9478c4b, 
  0x64e7b141, 0x827718f8, 0xcf0589c0, 0x6a6cb8e4, 0x81e415e1, 0xd4e2877d, 0x6f5ec0ea, 0x816412c7, 
  0xdad98584, 0x73b5c947, 0x80f80fab, 0xe0e783d7, 0x776bd1f0, 0x809f0c8b, 0xe7088277, 0x7a7cdad9, 
  0x805a096a, 0xed398164, 0x7ce2e3f5, 0x80290647, 0xf375809f, 0x7e9ced39, 0x800b0324, 0xf9b98029, 
  0x7fa6f696, 0x7fff, 0x7fff, 0x7fff, 0xff377ffe, 0xfe6e7ffc, 0xfda57ff9, 0xfe6e7ffc, 
  0xfcdc7ff5, 0xfb4a7fe8, 0xfda57ff9, 0xfb4a7fe8, 0xf8f07fcd, 0xfcdc7ff5, 0xf9b97fd7, 0xf6967fa6, 
  0xfc137fef, 0xf8277fc1, 0xf43d7f74, 0xfb4a7fe8, 0xf6967fa6, 0xf1e57f37, 0xfa827fe0, 0xf5057f86, 
  0xef8e7eef, 0xf9b97fd7, 0xf3757f61, 0xed397e9c, 0xf8f07fcd, 0xf1e57f37, 0xeae57e3e, 0xf8277fc1, 
  0xf0557f08, 0xe8937dd5, 0xf75f7fb4, 0xeec77ed4, 0xe6437d61, 0xf6967fa6, 0xed397e9c, 0xe3f57ce2, 
  0xf5ce7f96, 0xebab7e5e, 0xe1aa7c59, 0xf5057f86, 0xea1f7e1c, 0xdf617bc4, 0xf43d7f74, 0xe8937dd5, 
  0xdd1c7b25, 0xf3757f61, 0xe7087d89, 0xdad97a7c, 0xf2ad7f4c, 0xe57e7d38, 0xd89979c7, 0xf1e57f37, 
  0xe3f57ce2, 0xd65d7908, 0xf11d7f20, 0xe26d7c88, 0xd425783f, 0xf0557f08, 0xe0e77c29, 0xd1f0776b, 
  0xef8e7eef, 0xdf617bc4, 0xcfbf768d, 0xeec77ed4, 0xdddd7b5c, 0xcd9375a4, 0xee007eb9, 0xdc5a7aee, 
  0xcb6a74b1, 0xed397e9c, 0xdad97a7c, 0xc94773b5, 0xec727e7e, 0xd9597a04, 0xc72872ae, 0xebab7e5e, 
  0xd7da7989, 0xc50e719d, 0xeae57e3e, 0xd65d7908, 0xc2f97082, 0xea1f7e1c, 0xd4e27883, 0xc0ea6f5e, 
  0xe9597df9, 0xd36877f9, 0xbee06e30, 0xe8937dd5, 0xd1f0776b, 0xbcdb6cf8, 0xe7cd7db0, 0xd07a76d8, 
  0xbadd6bb7, 0xe7087d89, 0xcf057640, 0xb8e46a6c, 0xe6437d61, 0xcd9375a4, 0xb6f26919, 0xe57e7d38, 
  0xcc227503, 0xb50667bc, 0xe4ba7d0e, 0xcab3745e, 0xb3206656, 0xe3f57ce2, 0xc94773b5, 0xb14164e7, 
  0xe3317cb6, 0xc7dc7306, 0xaf696370, 0xe26d7c88, 0xc6747254, 0xad9861f0, 0xe1aa7c59, 0xc50e719d, 
  0xabce6067, 0xe0e77c29, 0xc3aa70e1, 0xaa0c5ed6, 0xe0247bf7, 0xc2497022, 0xa8505d3d, 0xdf617bc4, 
  0xc0ea6f5e, 0xa69d5b9c, 0xde9f7b91, 0xbf8d6e95, 0xa4f159f3, 0xdddd7b5c, 0xbe336dc9, 0xa34d5842, 
  0xdd1c7b25, 0xbcdb6cf8, 0xa1b15689, 0xdc5a7aee, 0xbb866c23, 0xa01e54c9, 0xdb997ab5, 0xba346b4a, 
  0x9e925301, 0xdad97a7c, 0xb8e46a6c, 0x9d0f5133, 0xda197a41, 0xb797698b, 0x9b954f5d, 0xd9597a04, 
  0xb64d68a5, 0x9a234d80, 0xd89979c7, 0xb50667bc, 0x98bb4b9d, 0xd7da7989, 0xb3c166ce, 0x975b49b3, 
  0xd71b7949, 0xb28065dd, 0x960447c3, 0xd65d7908, 0xb14164e7, 0x94b645cc, 0xd59f78c6, 0xb00663ee, 
  0x937243d0, 0xd4e27883, 0xaecd62f1, 0x923741cd, 0xd425783f, 0xad9861f0, 0x91063fc5, 0xd36877f9, 
  0xac6660eb, 0x8fde3db7, 0xd2ac77b3, 0xab375fe2, 0x8ec03ba4, 0xd1f0776b, 0xaa0c5ed6, 0x8dac398c, 
  0xd1347722, 0xa8e35dc6, 0x8ca2376f, 0xd07a76d8, 0xa7be5cb3, 0x8ba2354d, 0xcfbf768d, 0xa69d5b9c, 
  0x8aac3326, 0xcf057640, 0xa57f5a81, 0x89c030fb, 0xce4c75f3, 0xa4645963, 0x88de2ecc, 0xcd9375a4, 
  0xa34d5842, 0x88072c98, 0xccda7554, 0xa23a571d, 0x873a2a61, 0xcc227503, 0xa12a55f4, 0x86772826, 
  0xcb6a74b1, 0xa01e54c9, 0x85bf25e7, 0xcab3745e, 0x9f15539a, 0x851223a6, 0xc9fd740a, 0x9e105268, 
  0x846f2161, 0xc94773b5, 0x9d0f5133, 0x83d71f19, 0xc891735e, 0x9c124ffa, 0x834a1ccf, 0xc7dc7306, 
  0x9b194ebf, 0x82c81a82, 0xc72872ae, 0x9a234d80, 0x82501833, 0xc6747254, 0x99324c3f, 0x81e415e1, 
  0xc5c171f9, 0x98444afa, 0x8182138e, 0xc50e719d, 0x975b49b3, 0x812c1139, 0xc45c7140, 0x96754869, 
  0x80e00ee3, 0xc3aa70e1, 0x9594471c, 0x809f0c8b, 0xc2f97082, 0x94b645cc, 0x806a0a32, 0xc2497022, 
  0x93dd447a, 0x803f07d9, 0xc1996fc0, 0x93084325, 0x8020057e, 0xc0ea6f5e, 0x923741cd, 0x800b0324, 
  0xc03b6efa, 0x916b4073, 0x800200c9, 0xbf8d6e95, 0x90a23f16, 0x8004fe6e, 0xbee06e30, 0x8fde3db7, 
  0x8011fc13, 0xbe336dc9, 0x8f1f3c56, 0x8029f9b9, 0xbd876d61, 0x8e633af2, 0x804cf75f, 0xbcdb6cf8, 
  0x8dac398c, 0x807af505, 0xbc306c8e, 0x8cfa3824, 0x80b4f2ad, 0xbb866c23, 0x8c4b36b9, 0x80f8f055, 
  0xbadd6bb7, 0x8ba2354d, 0x8147ee00, 0xba346b4a, 0x8afd33de, 0x81a2ebab, 0xb98c6adb, 0x8a5c326d, 
  0x8207e959, 0xb8e46a6c, 0x89c030fb, 0x8277e708, 0xb83d69fc, 0x89282f86, 0x82f2e4ba, 0xb797698b, 
  0x88952e10, 0x8378e26d, 0xb6f26919, 0x88072c98, 0x8409e024, 0xb64d68a5, 0x877d2b1e, 0x84a4dddd, 
  0xb5a96831, 0x86f829a3, 0x854bdb99, 0xb50667bc, 0x86772826, 0x85fcd959, 0xb4636745, 0x85fc26a7, 
  0x86b7d71b, 0xb3c166ce, 0x85842527, 0x877dd4e2, 0xb3206656, 0x851223a6, 0x884dd2ac, 0xb28065dd, 
  0x84a42223, 0x8928d07a, 0xb1e06562, 0x843c209f, 0x8a0dce4c, 0xb14164e7, 0x83d71f19, 0x8afdcc22, 
  0xb0a3646b, 0x83781d93, 0x8bf6c9fd, 0xb00663ee, 0x831e1c0b, 0x8cfac7dc, 0xaf696370, 0x82c81a82, 
  0x8e07c5c1, 0xaecd62f1, 0x827718f8, 0x8f1fc3aa, 0xae326271, 0x822b176d, 0x9040c199, 0xad9861f0, 
  0x81e415e1, 0x916bbf8d, 0xacff616e, 0x81a21455, 0x929fbd87, 0xac6660eb, 0x816412c7, 0x93ddbb86, 
  0xabce6067, 0x812c1139, 0x9525b98c, 0xab375fe2, 0x80f80fab, 0x9675b797, 0xaaa15f5d, 0x80c90e1b, 
  0x97cfb5a9, 0xaa0c5ed6, 0x809f0c8b, 0x9932b3c1, 0xa9775e4f, 0x807a0afb, 0x9a9eb1e0, 0xa8e35dc6, 
  0x805a096a, 0x9c12b006, 0xa8505d3d, 0x803f07d9, 0x9d8fae32, 0xa7be5cb3, 0x80290647, 0x9f15ac66, 
  0xa72d5c28, 0x801804b6, 0xa0a3aaa1, 0xa69d5b9c, 0x800b0324, 0xa23aa8e3, 0xa60d5b0f, 0x80040192, 
  0xa3d8a72d, 0xa57f5a81, 0x80010000, 0xa57fa57f, 0xa4f159f3, 0x8004fe6e, 0xa72da3d8, 0xa4645963, 
  0x800bfcdc, 0xa8e3a23a, 0xa3d858d3, 0x8018fb4a, 0xaaa1a0a3, 0xa34d5842, 0x8029f9b9, 0xac669f15, 
  0xa2c357b0, 0x803ff827, 0xae329d8f, 0xa23a571d, 0x805af696, 0xb0069c12, 0xa1b15689, 0x807af505, 
  0xb1e09a9e, 0xa12a55f4, 0x809ff375, 0xb3c19932, 0xa0a3555f, 0x80c9f1e5, 0xb5a997cf, 0xa01e54c9, 
  0x80f8f055, 0xb7979675, 0x9f995432, 0x812ceec7, 0xb98c9525, 0x9f15539a, 0x8164ed39, 0xbb8693dd, 
  0x9e925301, 0x81a2ebab, 0xbd87929f, 0x9e105268, 0x81e4ea1f, 0xbf8d916b, 0x9d8f51ce, 0x822be893, 
  0xc1999040, 0x9d0f5133, 0x8277e708, 0xc3aa8f1f, 0x9c905097, 0x82c8e57e, 0xc5c18e07, 0x9c124ffa, 
  0x831ee3f5, 0xc7dc8cfa, 0x9b954f5d, 0x8378e26d, 0xc9fd8bf6, 0x9b194ebf, 0x83d7e0e7, 0xcc228afd, 
  0x9a9e4e20, 0x843cdf61, 0xce4c8a0d, 0x9a234d80, 0x84a4dddd, 0xd07a8928, 0x99aa4ce0, 0x8512dc5a, 
  0xd2ac884d, 0x99324c3f, 0x8584dad9, 0xd4e2877d, 0x98bb4b9d, 0x85fcd959, 0xd71b86b7, 0x98444afa, 
  0x8677d7da, 0xd95985fc, 0x97cf4a57, 0x86f8d65d, 0xdb99854b, 0x975b49b3, 0x877dd4e2, 0xdddd84a4, 
  0x96e7490e, 0x8807d368, 0xe0248409, 0x96754869, 0x8895d1f0, 0xe26d8378, 0x960447c3, 0x8928d07a, 
  0xe4ba82f2, 0x9594471c, 0x89c0cf05, 0xe7088277, 0x95254674, 0x8a5ccd93, 0xe9598207, 0x94b645cc, 
  0x8afdcc22, 0xebab81a2, 0x94494523, 0x8ba2cab3, 0xee008147, 0x93dd447a, 0x8c4bc947, 0xf05580f8, 
  0x937243d0, 0x8cfac7dc, 0xf2ad80b4, 0x93084325, 0x8dacc674, 0xf505807a, 0x929f4279, 0x8e63c50e, 
  0xf75f804c, 0x923741cd, 0x8f1fc3aa, 0xf9b98029, 0x91d04120, 0x8fdec249, 0xfc138011, 0x916b4073, 
  0x90a2c0ea, 0xfe6e8004, 0x91063fc5, 0x916bbf8d, 0xc98002, 0x90a23f16, 0x9237be33, 0x324800b, 
  0x90403e67, 0x9308bcdb, 0x57e8020, 0x8fde3db7, 0x93ddbb86, 0x7d9803f, 0x8f7e3d07, 0x94b6ba34, 
  0xa32806a, 0x8f1f3c56, 0x9594b8e4, 0xc8b809f, 0x8ec03ba4, 0x9675b797, 0xee380e0, 0x8e633af2, 
  0x975bb64d, 0x1139812c, 0x8e073a3f, 0x9844b506, 0x138e8182, 0x8dac398c, 0x9932b3c1, 0x15e181e4, 
  0x8d5238d8, 0x9a23b280, 0x18338250, 0x8cfa3824, 0x9b19b141, 0x1a8282c8, 0x8ca2376f, 0x9c12b006, 
  0x1ccf834a, 0x8c4b36b9, 0x9d0faecd, 0x1f1983d7, 0x8bf63603, 0x9e10ad98, 0x2161846f, 0x8ba2354d, 
  0x9f15ac66, 0x23a68512, 0x8b4f3496, 0xa01eab37, 0x25e785bf, 0x8afd33de, 0xa12aaa0c, 0x28268677, 
  0x8aac3326, 0xa23aa8e3, 0x2a61873a, 0x8a5c326d, 0xa34da7be, 0x2c988807, 0x8a0d31b4, 0xa464a69d, 
  0x2ecc88de, 0x89c030fb, 0xa57fa57f, 0x30fb89c0, 0x89733041, 0xa69da464, 0x33268aac, 0x89282f86, 
  0xa7bea34d, 0x354d8ba2, 0x88de2ecc, 0xa8e3a23a, 0x376f8ca2, 0x88952e10, 0xaa0ca12a, 0x398c8dac, 
  0x884d2d54, 0xab37a01e, 0x3ba48ec0, 0x88072c98, 0xac669f15, 0x3db78fde, 0x87c12bdb, 0xad989e10, 
  0x3fc59106, 0x877d2b1e, 0xaecd9d0f, 0x41cd9237, 0x873a2a61, 0xb0069c12, 0x43d09372, 0x86f829a3, 
  0xb1419b19, 0x45cc94b6, 0x86b728e5, 0xb2809a23, 0x47c39604, 0x86772826, 0xb3c19932, 0x49b3975b, 
  0x86392767, 0xb5069844, 0x4b9d98bb, 0x85fc26a7, 0xb64d975b, 0x4d809a23, 0x85bf25e7, 0xb7979675, 
  0x4f5d9b95, 0x85842527, 0xb8e49594, 0x51339d0f, 0x854b2467, 0xba3494b6, 0x53019e92, 0x851223a6, 
  0xbb8693dd, 0x54c9a01e, 0x84db22e4, 0xbcdb9308, 0x5689a1b1, 0x84a42223, 0xbe339237, 0x5842a34d, 
  0x846f2161, 0xbf8d916b, 0x59f3a4f1, 0x843c209f, 0xc0ea90a2, 0x5b9ca69d, 0x84091fdc, 0xc2498fde, 
  0x5d3da850, 0x83d71f19, 0xc3aa8f1f, 0x5ed6aa0c, 0x83a71e56, 0xc50e8e63, 0x6067abce, 0x83781d93, 
  0xc6748dac, 0x61f0ad98, 0x834a1ccf, 0xc7dc8cfa, 0x6370af69, 0x831e1c0b, 0xc9478c4b, 0x64e7b141, 
  0x82f21b46, 0xcab38ba2, 0x6656b320, 0x82c81a82, 0xcc228afd, 0x67bcb506, 0x829f19bd, 0xcd938a5c, 
  0x6919b6f2, 0x827718f8, 0xcf0589c0, 0x6a6cb8e4, 0x82501833, 0xd07a8928, 0x6bb7badd, 0x822b176d, 
  0xd1f08895, 0x6cf8bcdb, 0x820716a7, 0xd3688807, 0x6e30bee0, 0x81e415e1, 0xd4e2877d, 0x6f5ec0ea, 
  0x81c2151b, 0xd65d86f8, 0x7082c2f9, 0x81a21455, 0xd7da8677, 0x719dc50e, 0x8182138e, 0xd95985fc, 
  0x72aec728, 0x816412c7, 0xdad98584, 0x73b5c947, 0x81471200, 0xdc5a8512, 0x74b1cb6a, 0x812c1139, 
  0xdddd84a4, 0x75a4cd93, 0x81111072, 0xdf61843c, 0x768dcfbf, 0x80f80fab, 0xe0e783d7, 0x776bd1f0, 
  0x80e00ee3, 0xe26d8378, 0x783fd425, 0x80c90e1b, 0xe3f5831e, 0x7908d65d, 0x80b40d53, 0xe57e82c8, 
  0x79c7d899, 0x809f0c8b, 0xe7088277, 0x7a7cdad9, 0x808c0bc3, 0xe893822b, 0x7b25dd1c, 0x807a0afb, 
  0xea1f81e4, 0x7bc4df61, 0x806a0a32, 0xebab81a2, 0x7c59e1aa, 0x805a096a, 0xed398164, 0x7ce2e3f5, 
  0x804c08a1, 0xeec7812c, 0x7d61e643, 0x803f07d9, 0xf05580f8, 0x7dd5e893, 0x80330710, 0xf1e580c9, 
  0x7e3eeae5, 0x80290647, 0xf375809f, 0x7e9ced39, 0x8020057e, 0xf505807a, 0x7eefef8e, 0x801804b6, 
  0xf696805a, 0x7f37f1e5, 0x801103ed, 0xf827803f, 0x7f74f43d, 0x800b0324, 0xf9b98029, 0x7fa6f696, 
  0x8007025b, 0xfb4a8018, 0x7fcdf8f0, 0x80040192, 0xfcdc800b, 0x7fe8fb4a, 0x800200c9, 0xfe6e8004, 
  0x7ff9fda5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

#endif