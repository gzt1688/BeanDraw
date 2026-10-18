//! Artkal 拼豆色卡（S 系列 5mm Midi，国内主流拼豆标准）。
//!
//! 拼豆与十字绣（DMC 绣线）是两套完全不同的颜色体系：品牌不同、色号格式不同、
//! 色值不同，因此拼豆模式量化时必须按本表匹配，不能复用 DMC 色卡。
//!
//! 数据来源：Artkal Beads 官方 S 系列色卡（159 色，色号 S01..S159）。
//! 色值来自爱拼豆（ipindou.net）整理的 Artkal S 系列速查表，字段与 DMC 表一致。

use super::DmcColor;

/// (code, name, rgb) —— Artkal S 系列 5mm 拼豆标准色卡。
pub const PERLER_COLORS: &[DmcColor] = &[
    // 白 / 灰 / 黑
    DmcColor {
        code: "S01",
        name: "White",
        rgb: [255, 255, 255],
    },
    DmcColor {
        code: "S77",
        name: "Ghost White",
        rgb: [239, 239, 239],
    },
    DmcColor {
        code: "S78",
        name: "Ash Gray",
        rgb: [209, 209, 209],
    },
    DmcColor {
        code: "S79",
        name: "Light Gray",
        rgb: [187, 188, 188],
    },
    DmcColor {
        code: "S07",
        name: "Gray",
        rgb: [155, 155, 155],
    },
    DmcColor {
        code: "S43",
        name: "Dark Gray",
        rgb: [118, 119, 119],
    },
    DmcColor {
        code: "S89",
        name: "Charcoal Gray",
        rgb: [72, 73, 73],
    },
    DmcColor {
        code: "S69",
        name: "Mine Shaft",
        rgb: [35, 40, 43],
    },
    DmcColor {
        code: "S13",
        name: "Black",
        rgb: [0, 0, 0],
    },
    DmcColor {
        code: "S42",
        name: "Silver",
        rgb: [160, 159, 157],
    },
    DmcColor {
        code: "S159",
        name: "Oslo Gray",
        rgb: [136, 139, 141],
    },
    DmcColor {
        code: "S156",
        name: "Steel Grey",
        rgb: [68, 80, 91],
    },
    DmcColor {
        code: "S157",
        name: "Iron Grey",
        rgb: [62, 73, 85],
    },
    DmcColor {
        code: "S158",
        name: "Pepper",
        rgb: [32, 40, 48],
    },
    DmcColor {
        code: "S41",
        name: "Copper",
        rgb: [154, 85, 22],
    },
    DmcColor {
        code: "S63",
        name: "Metallic Gold",
        rgb: [76, 89, 20],
    },
    // 黄
    DmcColor {
        code: "S52",
        name: "Picasso",
        rgb: [242, 240, 161],
    },
    DmcColor {
        code: "S51",
        name: "Spring Sun",
        rgb: [252, 251, 205],
    },
    DmcColor {
        code: "S29",
        name: "Pastel Yellow",
        rgb: [246, 235, 97],
    },
    DmcColor {
        code: "S14",
        name: "Sandstorm",
        rgb: [250, 224, 83],
    },
    DmcColor {
        code: "S65",
        name: "Canary",
        rgb: [243, 234, 93],
    },
    DmcColor {
        code: "S27",
        name: "Yellow",
        rgb: [255, 209, 0],
    },
    DmcColor {
        code: "S48",
        name: "Corn",
        rgb: [255, 199, 44],
    },
    DmcColor {
        code: "S32",
        name: "Beeswax",
        rgb: [255, 231, 128],
    },
    DmcColor {
        code: "S92",
        name: "Dandelion",
        rgb: [222, 185, 71],
    },
    DmcColor {
        code: "S86",
        name: "Goldenrod",
        rgb: [234, 170, 0],
    },
    DmcColor {
        code: "S145",
        name: "Pale Yellow Moss",
        rgb: [211, 201, 93],
    },
    // 橙
    DmcColor {
        code: "S90",
        name: "Pastel Orange",
        rgb: [255, 197, 110],
    },
    DmcColor {
        code: "S03",
        name: "Tangerine",
        rgb: [246, 173, 76],
    },
    DmcColor {
        code: "S39",
        name: "Yellow Orange",
        rgb: [237, 139, 0],
    },
    DmcColor {
        code: "S56",
        name: "Bright Carrot",
        rgb: [255, 106, 19],
    },
    DmcColor {
        code: "S04",
        name: "Orange",
        rgb: [255, 103, 31],
    },
    DmcColor {
        code: "S66",
        name: "Blaze Orange",
        rgb: [244, 99, 58],
    },
    DmcColor {
        code: "S97",
        name: "Papaya",
        rgb: [212, 114, 42],
    },
    DmcColor {
        code: "S47",
        name: "Marigold",
        rgb: [180, 126, 0],
    },
    // 红 / 粉 / 肤
    DmcColor {
        code: "S87",
        name: "Coral Red",
        rgb: [255, 109, 106],
    },
    DmcColor {
        code: "S02",
        name: "Burning Sand",
        rgb: [255, 163, 139],
    },
    DmcColor {
        code: "S50",
        name: "Mandys Pink",
        rgb: [250, 170, 141],
    },
    DmcColor {
        code: "S94",
        name: "Warm Blush",
        rgb: [244, 169, 153],
    },
    DmcColor {
        code: "S95",
        name: "Salmon",
        rgb: [238, 125, 103],
    },
    DmcColor {
        code: "S96",
        name: "Apricot",
        rgb: [240, 134, 97],
    },
    DmcColor {
        code: "S19",
        name: "Bubble Gum",
        rgb: [252, 191, 169],
    },
    DmcColor {
        code: "S35",
        name: "Mona Lisa",
        rgb: [247, 206, 215],
    },
    DmcColor {
        code: "S28",
        name: "Lily Pink",
        rgb: [234, 184, 228],
    },
    DmcColor {
        code: "S40",
        name: "Carnation Pink",
        rgb: [241, 167, 220],
    },
    DmcColor {
        code: "S06",
        name: "Raspberry Pink",
        rgb: [236, 134, 208],
    },
    DmcColor {
        code: "S129",
        name: "Rosebud Pink",
        rgb: [230, 140, 163],
    },
    DmcColor {
        code: "S36",
        name: "Old Pink",
        rgb: [201, 128, 158],
    },
    DmcColor {
        code: "S25",
        name: "Hot Pink",
        rgb: [255, 52, 179],
    },
    DmcColor {
        code: "S132",
        name: "Flamingo",
        rgb: [232, 65, 107],
    },
    DmcColor {
        code: "S26",
        name: "Magenta",
        rgb: [219, 33, 82],
    },
    DmcColor {
        code: "S130",
        name: "Fuschia",
        rgb: [222, 84, 121],
    },
    DmcColor {
        code: "S88",
        name: "Dark Pink",
        rgb: [218, 24, 132],
    },
    DmcColor {
        code: "S133",
        name: "Pink Plum",
        rgb: [183, 56, 143],
    },
    DmcColor {
        code: "S05",
        name: "Tall Poppy",
        rgb: [225, 6, 0],
    },
    DmcColor {
        code: "S34",
        name: "Red",
        rgb: [186, 12, 47],
    },
    DmcColor {
        code: "S58",
        name: "Paprika",
        rgb: [165, 0, 52],
    },
    DmcColor {
        code: "S38",
        name: "Burgundy",
        rgb: [171, 37, 86],
    },
    DmcColor {
        code: "S146",
        name: "Bloodrose Red",
        rgb: [81, 9, 24],
    },
    DmcColor {
        code: "S49",
        name: "Mulberry Wood",
        rgb: [114, 25, 95],
    },
    DmcColor {
        code: "S57",
        name: "Buccaneer",
        rgb: [164, 73, 61],
    },
    DmcColor {
        code: "S84",
        name: "Deep Chestnut",
        rgb: [170, 87, 97],
    },
    DmcColor {
        code: "S85",
        name: "Red Wine",
        rgb: [66, 3, 26],
    },
    // 紫
    DmcColor {
        code: "S127",
        name: "Marshmallow Rose",
        rgb: [217, 182, 214],
    },
    DmcColor {
        code: "S128",
        name: "Light Grape",
        rgb: [173, 98, 164],
    },
    DmcColor {
        code: "S131",
        name: "Candy Violet",
        rgb: [158, 130, 186],
    },
    DmcColor {
        code: "S33",
        name: "Maverick",
        rgb: [197, 180, 227],
    },
    DmcColor {
        code: "S60",
        name: "Lavender",
        rgb: [167, 123, 202],
    },
    DmcColor {
        code: "S12",
        name: "Pastel Lavender",
        rgb: [160, 94, 181],
    },
    DmcColor {
        code: "S152",
        name: "Lilac",
        rgb: [151, 145, 197],
    },
    DmcColor {
        code: "S153",
        name: "Pale Lilac",
        rgb: [184, 189, 224],
    },
    DmcColor {
        code: "S150",
        name: "Iris Violet",
        rgb: [105, 99, 171],
    },
    DmcColor {
        code: "S23",
        name: "Royal Purple",
        rgb: [100, 53, 155],
    },
    DmcColor {
        code: "S134",
        name: "Amethyst",
        rgb: [88, 31, 126],
    },
    DmcColor {
        code: "S59",
        name: "Butterfly Bush",
        rgb: [74, 31, 135],
    },
    DmcColor {
        code: "S22",
        name: "Purple",
        rgb: [51, 0, 114],
    },
    // 蓝
    DmcColor {
        code: "S64",
        name: "Black Rock",
        rgb: [5, 8, 73],
    },
    DmcColor {
        code: "S135",
        name: "Moonlight Blue",
        rgb: [140, 163, 212],
    },
    DmcColor {
        code: "S136",
        name: "Summer Rain",
        rgb: [154, 154, 204],
    },
    DmcColor {
        code: "S44",
        name: "Sky Blue",
        rgb: [141, 200, 232],
    },
    DmcColor {
        code: "S98",
        name: "Himalaya Blue",
        rgb: [100, 172, 223],
    },
    DmcColor {
        code: "S10",
        name: "Baby Blue",
        rgb: [65, 182, 230],
    },
    DmcColor {
        code: "S53",
        name: "Blue Enchantress",
        rgb: [105, 179, 231],
    },
    DmcColor {
        code: "S137",
        name: "Azur Blue",
        rgb: [89, 129, 193],
    },
    DmcColor {
        code: "S138",
        name: "Cornflower Blue",
        rgb: [65, 102, 176],
    },
    DmcColor {
        code: "S54",
        name: "Light Blue",
        rgb: [0, 144, 218],
    },
    DmcColor {
        code: "S101",
        name: "Electric Blue",
        rgb: [49, 150, 221],
    },
    DmcColor {
        code: "S24",
        name: "True Blue",
        rgb: [20, 123, 209],
    },
    DmcColor {
        code: "S139",
        name: "Forget Me Not",
        rgb: [71, 95, 171],
    },
    DmcColor {
        code: "S102",
        name: "Pool Blue",
        rgb: [27, 108, 182],
    },
    DmcColor {
        code: "S141",
        name: "Horizon Blue",
        rgb: [61, 86, 165],
    },
    DmcColor {
        code: "S140",
        name: "Indigo",
        rgb: [55, 69, 147],
    },
    DmcColor {
        code: "S142",
        name: "Cobalt",
        rgb: [41, 66, 153],
    },
    DmcColor {
        code: "S143",
        name: "Royal Blue",
        rgb: [37, 38, 138],
    },
    DmcColor {
        code: "S11",
        name: "Dark Blue",
        rgb: [0, 51, 153],
    },
    DmcColor {
        code: "S103",
        name: "Caribbean Blue",
        rgb: [8, 57, 128],
    },
    DmcColor {
        code: "S144",
        name: "Marine",
        rgb: [26, 47, 111],
    },
    DmcColor {
        code: "S99",
        name: "Waterfall",
        rgb: [100, 194, 220],
    },
    DmcColor {
        code: "S100",
        name: "Lagoon",
        rgb: [79, 159, 179],
    },
    DmcColor {
        code: "S104",
        name: "Deep Water",
        rgb: [10, 102, 139],
    },
    DmcColor {
        code: "S107",
        name: "Pond Blue",
        rgb: [0, 85, 116],
    },
    DmcColor {
        code: "S106",
        name: "Wedgewood Blue",
        rgb: [0, 78, 120],
    },
    DmcColor {
        code: "S105",
        name: "Petrol Blue",
        rgb: [8, 91, 110],
    },
    DmcColor {
        code: "S30",
        name: "Shadow Green",
        rgb: [153, 214, 234],
    },
    DmcColor {
        code: "S72",
        name: "Light Sea Blue",
        rgb: [89, 213, 216],
    },
    DmcColor {
        code: "S74",
        name: "Azure",
        rgb: [0, 174, 214],
    },
    DmcColor {
        code: "S76",
        name: "Sea Blue",
        rgb: [0, 174, 199],
    },
    DmcColor {
        code: "S73",
        name: "Steel Blue",
        rgb: [72, 169, 197],
    },
    DmcColor {
        code: "S75",
        name: "Dark Steel Blue",
        rgb: [0, 133, 173],
    },
    DmcColor {
        code: "S45",
        name: "Medium Turquoise",
        rgb: [0, 178, 169],
    },
    DmcColor {
        code: "S37",
        name: "Blue-Green",
        rgb: [113, 216, 191],
    },
    // 绿 / 青
    DmcColor {
        code: "S31",
        name: "Sea Mist",
        rgb: [158, 229, 176],
    },
    DmcColor {
        code: "S147",
        name: "Spearmint",
        rgb: [100, 179, 158],
    },
    DmcColor {
        code: "S120",
        name: "Polar Mint",
        rgb: [102, 204, 153],
    },
    DmcColor {
        code: "S121",
        name: "Celadon Green",
        rgb: [86, 154, 131],
    },
    DmcColor {
        code: "S122",
        name: "Eucalyptus",
        rgb: [20, 194, 91],
    },
    DmcColor {
        code: "S123",
        name: "Clover Field",
        rgb: [24, 168, 24],
    },
    DmcColor {
        code: "S46",
        name: "Bright Green",
        rgb: [115, 211, 60],
    },
    DmcColor {
        code: "S08",
        name: "Emerald",
        rgb: [36, 222, 91],
    },
    DmcColor {
        code: "S55",
        name: "Pistachio",
        rgb: [173, 220, 145],
    },
    DmcColor {
        code: "S21",
        name: "Pastel Green",
        rgb: [135, 216, 57],
    },
    DmcColor {
        code: "S119",
        name: "Frosty Blue",
        rgb: [113, 196, 82],
    },
    DmcColor {
        code: "S20",
        name: "Green",
        rgb: [36, 158, 107],
    },
    DmcColor {
        code: "S71",
        name: "Jade Green",
        rgb: [0, 133, 43],
    },
    DmcColor {
        code: "S62",
        name: "Green Tea",
        rgb: [0, 124, 88],
    },
    DmcColor {
        code: "S125",
        name: "Snake Green",
        rgb: [19, 107, 90],
    },
    DmcColor {
        code: "S09",
        name: "Dark Green",
        rgb: [0, 104, 94],
    },
    DmcColor {
        code: "S124",
        name: "Pooltable Felt",
        rgb: [4, 85, 46],
    },
    DmcColor {
        code: "S126",
        name: "Dark Eucalyptus",
        rgb: [5, 70, 65],
    },
    DmcColor {
        code: "S91",
        name: "Brunswick Green",
        rgb: [24, 48, 40],
    },
    DmcColor {
        code: "S151",
        name: "Forrest Green",
        rgb: [43, 63, 31],
    },
    DmcColor {
        code: "S61",
        name: "Key Lemon Pie",
        rgb: [206, 220, 0],
    },
    DmcColor {
        code: "S70",
        name: "Dark Algae",
        rgb: [155, 188, 17],
    },
    DmcColor {
        code: "S80",
        name: "Dark Olive",
        rgb: [153, 155, 48],
    },
    DmcColor {
        code: "S114",
        name: "Light Greengray",
        rgb: [114, 140, 84],
    },
    DmcColor {
        code: "S115",
        name: "Mossy Green",
        rgb: [126, 124, 68],
    },
    DmcColor {
        code: "S116",
        name: "Earth Green",
        rgb: [100, 105, 46],
    },
    DmcColor {
        code: "S117",
        name: "Sage Green",
        rgb: [78, 88, 44],
    },
    DmcColor {
        code: "S118",
        name: "Pinetree Green",
        rgb: [74, 94, 45],
    },
    DmcColor {
        code: "S113",
        name: "Khaki",
        rgb: [186, 184, 162],
    },
    // 米 / 肤 / 棕
    DmcColor {
        code: "S149",
        name: "Creme",
        rgb: [237, 211, 158],
    },
    DmcColor {
        code: "S67",
        name: "Vanilla",
        rgb: [243, 207, 179],
    },
    DmcColor {
        code: "S154",
        name: "Sahara Sand",
        rgb: [249, 200, 152],
    },
    DmcColor {
        code: "S93",
        name: "Pale Skin",
        rgb: [218, 182, 152],
    },
    DmcColor {
        code: "S155",
        name: "Sunkissed Teint",
        rgb: [195, 144, 105],
    },
    DmcColor {
        code: "S18",
        name: "Sand",
        rgb: [204, 153, 102],
    },
    DmcColor {
        code: "S108",
        name: "Seashell Beige",
        rgb: [204, 190, 128],
    },
    DmcColor {
        code: "S109",
        name: "Beige",
        rgb: [164, 147, 80],
    },
    DmcColor {
        code: "S110",
        name: "Beach Beige",
        rgb: [158, 136, 60],
    },
    DmcColor {
        code: "S68",
        name: "Tan",
        rgb: [225, 192, 120],
    },
    DmcColor {
        code: "S81",
        name: "Deer",
        rgb: [205, 178, 119],
    },
    DmcColor {
        code: "S82",
        name: "Clay",
        rgb: [181, 129, 80],
    },
    DmcColor {
        code: "S83",
        name: "Sienna",
        rgb: [184, 97, 37],
    },
    DmcColor {
        code: "S111",
        name: "Caffe Latte",
        rgb: [118, 108, 43],
    },
    DmcColor {
        code: "S112",
        name: "Oaktree Brown",
        rgb: [121, 95, 38],
    },
    DmcColor {
        code: "S148",
        name: "Mocha",
        rgb: [99, 67, 56],
    },
    DmcColor {
        code: "S17",
        name: "Light Brown",
        rgb: [123, 77, 53],
    },
    DmcColor {
        code: "S16",
        name: "Brown",
        rgb: [92, 71, 56],
    },
    DmcColor {
        code: "S15",
        name: "Redwood",
        rgb: [121, 62, 44],
    },
];
