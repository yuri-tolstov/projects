//Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//  The source code contained or described herein and all documents
//  related to the source code ("Material") are owned by Tilera
//  Corporation or its suppliers or licensors.  Title to the Material
//  remains with Tilera Corporation or its suppliers and licensors. The
//  software is licensed under the Tilera MDE License.
//
//  Unless otherwise agreed by Tilera in writing, you may not remove or
//  alter this notice or any other notice embedded in Materials by Tilera
//  or Tilera's suppliers or licensors in any way.

// Constants shared between Tile and remote code.
#define MAP_LENGTH (64 << 10)

#define TILE_WINDOW_START 0xbadcafe0000ull
#define TILE_WINDOW_SIZE MAP_LENGTH

#define REMOTE_WINDOW_START 0xdecaf0000ull
#define REMOTE_WINDOW_SIZE MAP_LENGTH

#define FLAG_OFFSET (3 << 10)
