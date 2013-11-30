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

#ifndef _FFT_1024_INPUT_REV_H_
#define _FFT_1024_INPUT_REV_H_

/* HACK: align to 8 for fft_1024_hack() */
short __attribute__((aligned(8))) fft_input_rev[1024*2] = {
  32384, 9776,
  12723, 2727,
  25330, 1528,
  20994, 10764,
  28923, 5111,
  17155, 6703,
  27752, 18741,
  25587, 26452,
  3786, 10118,
  0176, 1671,
  17666, 5252,
  28710, 4318,
  13863, 1591,
  24402, 2093,
  4905, 3714,
  23710, 14804,
  19896, 19276,
  4221, 19199,
  23251, 18892,
  22001, 29033,
  12087, 5186,
  28862, 23305,
  17685, 24147,
  1026, 19998,
  28784, 23142,
  9399, 23429,
  27178, 12477,
  32357, 25422,
  29624, 0736,
  1920,  207,
  18508, 28407,
  12972, 8000,
  30532, 9621,
  28439, 14125,
  10446, 7460,
  13636, 15601,
  5661, 18248,
  30417, 9557,
  20164, 0742,
  23480, 30912,
  6968, 2524,
  21086, 32475,
  28034, 1040,
  31154, 32396,
  18522, 4106,
  24626, 10724,
  24246, 16164,
  20914, 29509,
  1200, 4289,
  12893, 31333,
  29281, 7061,
  12160, 1215,
  14249, 29664,
  7964, 3299,
  22510, 14042,
  21735, 20679,
  6719, 20273,
  3202, 4678,
  4033, 28137,
  6225, 3060,
  4728, 9334,
  16847, 9842,
  29442, 1134,
  16358, 21474,
  31028, 19257,
  12057, 18118,
  31274, 32132,
  4490, 0462,
  16794, 30354,
  30696, 0036,
  32685, 12418,
  23920, 8779,
  9929, 16851,
  7447, 19645,
  10585, 15608,
  21428, 12845,
  30694, 5488,
  6786, 10724,
  14405, 2258,
  11187, 13363,
  9041, 4805,
  12100, 1258,
  4569, 26316,
  23899, 3187,
  20819, 19764,
  0061, 18331,
  4000, 4145,
  1017, 11142,
  16686, 10407,
  1652, 29174,
  31654, 23850,
  4363, 25769,
  31255, 6775,
  4602, 17524,
  29097, 1243,
  13412, 9080,
  3335, 13410,
  4273, 30824,
  22146, 7297,
  24960, 31927,
  16922, 7758,
  28383, 3914,
  8906, 9688,
  26637, 4555,
  15434, 4595,
  24523, 19418,
  24850, 9984,
  6720, 31233,
  15315, 19710,
  1786, 9773,
  24293, 19507,
  6309, 27977,
  32255, 17418,
  4842, 23561,
  12815, 26222,
  2418, 9568,
  29708, 20393,
  0436, 29395,
  5555, 16136,
  14012, 29492,
  15251, 4887,
  14236, 19748,
  29817, 5688,
  7865, 22989,
  14013, 23897,
  7633, 22443,
  27692, 9777,
  22585, 30582,
  2575, 3399,
  4424, 21059,
  26225, 30806,
  27386, 28265,
  18212, 15028,
  12055, 10428,
  5685, 23965,
  21954, 18261,
  15677, 29955,
  0545, 12240,
  1747, 14599,
  25801, 12816,
  20449, 9790,
  12941, 9642,
  15548, 30214,
  23868, 20742,
  18319, 19020,
  25987, 29346,
  1989, 28484,
  19874, 13411,
  30292, 2084,
  11199, 8839,
  27171, 12605,
  6443, 11481,
  26997, 23035,
  13820, 15699,
  26885, 23068,
  17595, 17955,
  31334, 5983,
  15865, 15338,
  31515, 9210,
  26393, 21616,
  30067, 23881,
  6917, 19047,
  29475, 30910,
  878, 10021,
  9388, 10357,
  2119, 2979,
  30351, 20910,
  12266, 2551,
  8475, 9559,
  20870, 8136,
  16304, 29158,
  4396, 2424,
  9500, 24493,
  5406, 7698,
  6250, 23967,
  17991, 25177,
  25227, 11278,
  25008, 25859,
  21058, 22810,
  20640, 14265,
  6861, 18160,
  18445, 23852,
  21153, 17038,
  22591, 10943,
  31968, 2925,
  2080, 10680,
  7271, 20256,
  30607, 15364,
  27466, 1035,
  28312, 2480,
  15712, 28222,
  19795, 2284,
  6626, 30216,
  11945, 9898,
  30361, 2346,
  27647, 11754,
  25173, 10112,
  24976, 9855,
  31612, 10939,
  22256, 18572,
  23337, 27640,
  17015, 26179,
  17786, 3074,
  22081, 25953,
  19078, 15984,
  31085, 12151,
  3243, 15825,
  19601, 13888,
  27335, 14425,
  21069, 25513,
  5263, 31635,
  2170, 6235,
  16085, 21392,
  6513, 31153,
  17259, 29243,
  24685, 25553,
  10153, 18327,
  20255, 28801,
  25472, 8205,
  13233, 31676,
  23314, 19251,
  4500, 6344,
  10560, 10080,
  13102, 22033,
  19590, 28358,
  29635, 6772,
  18369, 26328,
  29595, 8583,
  6928, 12512,
  32598, 6569,
  13246, 21364,
  16475, 21186,
  30322, 16533,
  2336, 0300,
  20081, 15771,
  8681, 21325,
  16871, 25035,
  27659, 12200,
  15937, 14475,
  11656, 23395,
  30142, 1415,
  30083, 24892,
  648, 22652,
  26238, 31186,
  26208, 4452,
  21598, 29535,
  15876, 17781,
  2982, 7535,
  30906, 14272,
  26984, 10396,
  28026, 9835,
  18340, 17025,
  1082, 15328,
  20820, 718,
  10781, 2933,
  14282, 14383,
  0524, 14551,
  14928, 4048,
  26486, 4468,
  26327, 12742,
  20404, 10776,
  11940, 31513,
  8819, 4391,
  14964, 23982,
  12652, 5642,
  13501, 4132,
  1621, 32449,
  14442, 16585,
  1893, 5815,
  21478, 19323,
  5751, 6257,
  4656, 17656,
  30384, 21112,
  19518, 5185,
  27841, 10623,
  13052, 29875,
  25055, 16063,
  2406, 12621,
  4795, 9498,
  7383, 2368,
  32100, 16092,
  8458, 31919,
  883, 31219,
  13789, 2950,
  16549, 28368,
  26894, 0536,
  7684, 0202,
  15986, 23801,
  15965, 7915,
  8260, 17124,
  32598, 25109,
  27079, 6311,
  28062, 23980,
  10308, 20050,
  3577, 30682,
  28741, 17246,
  4253, 21408,
  16311, 28619,
  11447, 8983,
  8312, 2422,
  4219, 23423,
  12319, 9116,
  2956, 26974,
  3228, 26315,
  22902, 27128,
  3388, 21454,
  9106, 4202,
  7804, 30862,
  9492, 24698,
  9697, 16268,
  4584, 2948,
  11029, 15466,
  16103, 30506,
  3531, 17824,
  6067, 31969,
  24580, 1275,
  2870, 29261,
  1847, 16804,
  2387, 1819,
  14250, 31116,
  9531, 30825,
  30570, 18378,
  30415, 9733,
  22600, 16735,
  20870, 20071,
  5127, 12962,
  11365, 7436,
  15516, 18268,
  12459, 7097,
  21867, 17173,
  32563, 17601,
  10159, 4070,
  1830, 21540,
  11596, 16167,
  8564, 23835,
  18395, 30069,
  969, 1439,
  23327, 22614,
  4511, 30238,
  9548, 20746,
  8207, 13224,
  24075, 1734,
  29539, 2668,
  23815, 9507,
  7795, 15740,
  9586, 18654,
  24862, 24579,
  909, 32760,
  5021, 17428,
  7909, 28603,
  32149, 17627,
  27218, 920,
  15501, 26237,
  10804, 20426,
  31380, 13575,
  7946, 30797,
  26286, 15576,
  191, 31525,
  15378, 9329,
  10811, 32423,
  24783, 4388,
  22148, 21636,
  10042, 6770,
  26753, 14920,
  30245, 31373,
  8333, 21138,
  17860, 30888,
  28168, 5683,
  5891, 10348,
  10853, 8986,
  2604, 18139,
  30594, 27570,
  23786, 0740,
  24921, 24534,
  9591, 20768,
  15850, 17241,
  32253, 3069,
  5885, 14401,
  23287, 32340,
  17108, 6255,
  4411, 21763,
  30102, 18682,
  17654, 13357,
  18789, 24630,
  31389, 26542,
  11431, 8490,
  2025, 14977,
  20826, 22655,
  19432, 32185,
  12216, 13974,
  27246, 19914,
  8954, 3262,
  30318, 9201,
  21551, 27931,
  4738, 7247,
  29621, 11137,
  15051, 4395,
  9590, 5685,
  2318, 23263,
  23457, 28916,
  27611, 1759,
  28689, 6571,
  3625, 26189,
  31190, 3461,
  10491, 21484,
  2667, 7305,
  26484, 5627,
  2152, 15283,
  3482, 6261,
  13840, 8280,
  32378, 31404,
  24185, 13310,
  18679, 58,
  19903, 5701,
  23054, 25765,
  27421, 5299,
  2005, 19969,
  30643, 18724,
  29459, 13718,
  1387, 26675,
  28036, 583,
  26289, 7113,
  30973, 23273,
  17314, 30279,
  3030, 28481,
  22274, 22964,
  30209, 24765,
  6932, 20640,
  9944, 14658,
  28141, 15361,
  3596, 23198,
  1439, 5637,
  24600, 21683,
  12021, 21060,
  3233, 17618,
  19114, 5571,
  17032, 32726,
  31043, 21772,
  4260, 22367,
  22870, 8618,
  30416, 11750,
  10137, 9299,
  9625, 17838,
  14807, 29242,
  32436, 25786,
  9788, 1681,
  26021, 30246,
  16531, 11782,
  1899, 27307,
  27136, 21146,
  28825, 29736,
  1379, 27387,
  20212, 14219,
  3108, 21165,
  16935, 25988,
  14699, 29874,
  32586, 12165,
  4499, 26639,
  26108, 17202,
  7446, 19060,
  11901, 5895,
  4067, 16666,
  9662, 11629,
  32003, 18876,
  15398, 16354,
  6457, 28320,
  17468, 12478,
  2332, 19168,
  21896, 14088,
  24407, 7610,
  19263, 31066,
  31326, 9754,
  18283, 25666,
  18445, 19054,
  16728, 15545,
  31457, 9684,
  6343, 16497,
  10356, 14646,
  1109, 24921,
  31136, 26709,
  1978, 22254,
  13437, 28566,
  23744, 27858,
  26533, 8202,
  15665, 14521,
  17812, 2568,
  11902, 2095,
  5060, 24571,
  15775, 32397,
  9631, 22849,
  17744, 18813,
  20002, 3655,
  31204, 25101,
  0704, 14675,
  30812, 22947,
  7405, 23744,
  11454, 7102,
  11923, 0443,
  32061, 3196,
  27232, 20622,
  32737, 30932,
  4427, 13816,
  20442, 23368,
  28014, 0435,
  10018, 4947,
  5361, 27063,
  8888, 3640,
  6156, 22242,
  29503, 17737,
  22429, 28659,
  7836, 18773,
  6196, 19615,
  22045, 8930,
  14370, 21279,
  20145, 30527,
  16158, 3222,
  0101, 0065,
  15401, 1440,
  21299, 11454,
  27525, 21165,
  17935, 32618,
  15154, 6547,
  8263, 11911,
  28184, 23557,
  26692, 0226,
  14044, 21562,
  6461, 9219,
  31675, 5358,
  15270, 18688,
  1273, 6029,
  29224, 27268,
  32716, 32687,
  0560, 10890,
  22272, 12168,
  28854, 21494,
  25508, 17470,
  24029, 17510,
  0001, 0310,
  28862, 26183,
  13208, 27231,
  13530, 31379,
  8524, 26755,
  24910, 8795,
  19831, 16804,
  15492, 24884,
  18456, 22960,
  26063, 3867,
  23159, 7375,
  19302, 29113,
  3602, 18144,
  30787, 21287,
  29566, 10875,
  7148, 10363,
  16961, 16853,
  30871, 27808,
  13009, 15945,
  4471, 910,
  15862, 9359,
  22757, 2159,
  7337, 24227,
  22007, 28289,
  13506, 21300,
  19150, 10665,
  16826, 15139,
  12353, 30517,
  13500, 27009,
  22783, 5664,
  7301, 13352,
  23289, 2532,
  32330, 22992,
  26660, 22264,
  14247, 18122,
  18686, 13245,
  28156, 22174,
  8308, 27505,
  26630, 24578,
  17620, 18414,
  8311, 20949,
  14881, 15437,
  26173, 16513,
  21596, 24652,
  12045, 22332,
  18115, 15817,
  838, 20127,
  21933, 19190,
  7357, 29776,
  14130, 11766,
  26500, 11206,
  921, 22537,
  26871, 23795,
  30717, 20765,
  26286, 14568,
  28520, 24432,
  7467, 23661,
  4113, 4587,
  30586, 20220,
  25550, 20239,
  27675, 23633,
  6131, 5752,
  8175, 10072,
  20973, 29272,
  20174, 3454,
  13841, 20875,
  2481, 11300,
  29559, 24791,
  11588, 21992,
  29668, 22214,
  12080, 13258,
  12716, 3348,
  28665, 25621,
  27349, 14908,
  14573, 22229,
  11207, 29108,
  6529, 18888,
  10426, 1618,
  26517, 32048,
  19123, 22717,
  12473, 15951,
  11928, 10710,
  30266, 23091,
  29859, 30741,
  9467, 29822,
  3736, 24468,
  16301, 25419,
  19795, 20410,
  3414, 24506,
  8861, 23745,
  6938, 14595,
  32463, 32656,
  3105, 21983,
  22807, 4947,
  3275, 0123,
  12426, 10398,
  27887, 16300,
  4603, 28492,
  4555, 27242,
  8988, 16402,
  26511, 2844,
  12980, 28897,
  4367, 13943,
  16332, 15269,
  23105, 19904,
  29754, 21319,
  17459, 30631,
  30478, 0652,
  3810, 11769,
  15786, 23011,
  17388, 23034,
  4570, 1117,
  8903, 9221,
  6971, 9843,
  13653, 31954,
  1400, 12609,
  16879, 9783,
  2212, 21447,
  16964, 1157,
  25990, 19520,
  31232, 5282,
  31845, 29150,
  14285, 8748,
  7812, 18618,
  0111, 13092,
  7718, 26901,
  27445, 4053,
  3960, 17008,
  31708, 29268,
  9197, 9685,
  7993, 23820,
  7976, 24693,
  28180, 11014,
  12803, 9700,
  11172, 24155,
  11399, 29285,
  13822, 9673,
  25903, 16086,
  18597, 25791,
  8392, 15614,
  15537, 28893,
  7453, 17522,
  8783, 10322,
  30703, 9139,
  21136, 25060,
  18986, 2442,
  30595, 23393,
  27866, 21197,
  21820, 20613,
  9383, 17322,
  32131, 27594,
  16278, 32427,
  20788, 9310,
  25255, 17308,
  8417, 30486,
  11786, 1091,
  11517, 30825,
  15048, 23099,
  27886, 24443,
  6672, 29563,
  5923, 4789,
  2768, 15522,
  22901, 27491,
  17084, 20590,
  19524, 11501,
  15223, 17021,
  11103, 5245,
  13926, 11292,
  9816, 17573,
  5842, 4277,
  1528, 27831,
  3486, 16166,
  3363, 24856,
  17368, 12819,
  22940, 10974,
  18003, 24288,
  3045, 26719,
  20774, 25511,
  17239, 21226,
  22909, 5093,
  10532, 3210,
  23286, 32374,
  17492, 21645,
  1155, 26956,
  19829, 25411,
  27025, 8039,
  20858, 2597,
  24041, 31293,
  22185, 10433,
  5943, 19261,
  12980, 15656,
  12830, 18209,
  20367, 22395,
  7272, 17353,
  9797, 29812,
  28482, 15069,
  30335, 13455,
  25256, 17357,
  16299, 11989,
  17519, 11353,
  22188, 17573,
  31632, 3212,
  9194, 14566,
  5257, 32104,
  10062, 31210,
  24367, 14882,
  17747, 16570,
  19122, 0257,
  31948, 26111,
  8770, 15415,
  21259, 22244,
  15161, 8887,
  21660, 4561,
  15463, 14853,
  21351, 11355,
  26396, 18662,
  179, 25192,
  25648, 20807,
  14363, 12412,
  20036, 8053,
  27277, 5815,
  19643, 5475,
  7023, 13659,
  29698, 6741,
  3973, 21378,
  23343, 32657,
  3186, 13640,
  25531, 12573,
  28922, 27158,
  6184, 25976,
  15701, 29241,
  4600, 19036,
  2941, 7760,
  5927, 27502,
  31396, 17413,
  10848, 9164,
  16188, 3082,
  15086, 32687,
  15765, 13378,
  27177, 7718,
  2962, 13188,
  16672, 0707,
  24526, 9019,
  13385, 22475,
  28315, 23326,
  25255, 12736,
  19450, 22949,
  16907, 13518,
  21944, 1999,
  23745, 24713,
  5537, 25398,
  9939, 4540,
  24673, 24961,
  0315, 7929,
  24659, 22547,
  7008, 10558,
  20042, 7144,
  17371, 16367,
  17005, 31107,
  26155, 24440,
  26829, 6550,
  14825, 27272,
  26600, 12900,
  16209, 3348,
  12042, 15021,
  19295, 22102,
  24298, 24513,
  8785, 23684,
  10767, 15599,
  16856, 0472,
  24550, 19264,
  1324, 17239,
  25573, 2541,
  13638, 28002,
  11107, 13058,
  17426, 284,
  5661, 24747,
  15101, 7235,
  12016, 1756,
  17687, 16571,
  4105, 2824,
  20053, 19933,
  21031, 13578,
  13085, 1024,
  15245, 9524,
  24805, 28931,
  5261, 2152,
  24228, 18904,
  0165, 22003,
  4271, 20032,
  11119, 31256,
  7489, 24743,
  25732, 32339,
  14994, 5349,
  789, 14747,
  6391, 6963,
  30202, 3897,
  12861, 1623,
  12262, 20283,
  0161, 31706,
  30751, 29130,
  24261, 27410,
  25411, 386,
  11852, 8100,
  22451, 4629,
  11179, 1551,
  26505, 19778,
  16414, 30704,
  19002, 15512,
  23797, 26889,
  7682, 21526,
  11035, 6419,
  28215, 17746,
  5428, 5970,
  26985, 8305,
  2067, 3233,
  16769, 24595,
  23609, 22529,
  26142, 10582,
  16082, 31743,
  28975, 24765,
  10440, 16362,
  13171, 24020,
  9799, 15284,
  24377, 5526,
  20666, 3778,
  14404, 20871,
  28115, 9768,
  28041, 24120,
  19500, 23620,
  25915, 25706,
  27859, 4799,
  30730, 25111,
  27101, 29624,
  13953, 10014,
  26568, 12940,
  14223, 31636,
  2107, 12000,
  15773, 16017,
  26386, 16931,
  0557, 7450,
  16422, 24509,
  32738, 31664,
  31295, 10591,
  13004, 2589,
  22326, 17071,
  12787, 17876,
  19761, 26358,
  7686, 19288,
  29110, 18729,
  11316, 16232,
  23725, 19747,
  12037, 23877,
  15627, 7196,
  21678, 28431,
  15990, 22707,
  11848, 17950,
  1526, 22453,
  10089, 12653,
  15938, 15893,
  17488, 8493,
  27333, 8027,
  689, 22787,
  26906, 3430,
  28809, 25908,
  18265, 27201,
  17110, 15973,
  6632, 22759,
  16311, 7772,
  24915, 1732,
  12103, 0063,
  12691, 29180,
  14659, 22242,
  13782, 22048,
  2060, 19762,
  17570, 16284,
  8099, 10602,
  13130, 17661,
  11050, 3032,
  27689, 11384,
  13860, 8050,
  7344, 9681,
  4767, 8421,
  9465, 16487,
  1243, 20469,
  31149, 8900,
  17231, 12219,
  16510, 10201,
  30434, 19624,
  1185, 2304,
  27572, 9499,
  8528, 16767,
  15692, 28078,
  5699, 14881,
  12974, 1314,
  14336, 23802,
  13410, 13435,
  6109, 5364,
  2809, 11432,
  2552, 29897,
  13868, 18926,
  4858, 29021,
  3347, 14328,
  6216, 20205,
  25200, 30984,
  10213, 2993,
  30960, 25763,
  26073, 12614,
  10019, 18282,
  5591, 28561,
  8827, 14980,
  23330, 12434,
  20488, 15002,
  20500, 19697,
  4294, 25803,
  6965, 28321,
  2300, 11461,
  28379, 49,
  28916, 29165,
  9921, 24844,
  12266, 16548,
  26935, 22548,
  9101, 23576,
  27497, 17067,
  31000, 32172,
  1539, 24060,
  11728, 28089,
  11798, 21094,
  6229, 9751,
  2952, 27505,
  6176, 2302,
  24014, 10696,
  1327, 16717,
  12759, 27761,
  27348, 17555,
  0526, 16172,
  5271, 28393,
  12225, 7843,
  8366, 29270,
  4391, 16685,
  9175, 13736,
  0443, 7679,
  27480, 24181,
  22635, 24369,
  18741, 25963,
  1971, 24067,
  2781, 1318,
  24963, 14511,
  17843, 9587,
  6536, 22598,
  23282, 2970,
  25897, 25571,
  16921, 23828,
  5435, 30326,
  3985, 3498,
  18059, 18668,
  22547, 4716,
  31264, 23014,
  32117, 28715,
  26612, 4802,
  10024, 25460,
  28911, 17881,
  28317, 15111,
  13717, 19397,
  25246, 8577,
  15461, 1030,
  7860, 15211,
  2978, 26905,
  1366, 27281,
  11253, 3535,
  4654 , 20425,
  32210, 12347,
  1994 , 24494,
  18122, 21632,
  1296 , 13772,
  17946, 18423,
  12222, 5923,
  9341 , 11101,
  9296 , 22096,
  16242, 23459,
  8347 , 26788,
  13479, 10374,
  5507 , 27319,
  25297, 20347,
  18743, 15593,
  18103, 30616,
  20019, 9589,
};

#endif