//! DMC 绣线 / 珠子色卡子集（开发数据集）。
//!
//! ⚠️ 这是**开发用子集**，非商业级全量色卡：
//! 完整数据须来源可追溯，并由手工从业者校验（开发大纲第 10 章风险 7）。
//! 珠绣色号复用绣线色号（Phase 1），独立珠色卡在 Phase 2 随素材库扩展。
//! 导出 JSON：`cargo run --example dump_dmc > ../resources/dmc_color.json`

use super::DmcColor;

/// (code, name, rgb)
pub const DMC_COLORS: &[DmcColor] = &[
    DmcColor {
        code: "White",
        name: "White",
        rgb: [255, 255, 255],
    },
    DmcColor {
        code: "B5200",
        name: "Snow White",
        rgb: [255, 255, 255],
    },
    DmcColor {
        code: "Ecru",
        name: "Ecru",
        rgb: [240, 234, 218],
    },
    DmcColor {
        code: "3865",
        name: "Winter White",
        rgb: [249, 248, 243],
    },
    DmcColor {
        code: "712",
        name: "Cream",
        rgb: [255, 251, 239],
    },
    DmcColor {
        code: "739",
        name: "Tan Ultra Very Light",
        rgb: [248, 228, 200],
    },
    DmcColor {
        code: "746",
        name: "Off White",
        rgb: [252, 252, 238],
    },
    DmcColor {
        code: "3822",
        name: "Straw Light",
        rgb: [246, 231, 176],
    },
    DmcColor {
        code: "3852",
        name: "Straw Very Light",
        rgb: [250, 243, 214],
    },
    // 黑 / 灰
    DmcColor {
        code: "310",
        name: "Black",
        rgb: [0, 0, 0],
    },
    DmcColor {
        code: "3371",
        name: "Black Very Dark",
        rgb: [22, 22, 22],
    },
    DmcColor {
        code: "3790",
        name: "Beaver Gray Very Dark",
        rgb: [92, 92, 92],
    },
    DmcColor {
        code: "414",
        name: "Steel Gray Dark",
        rgb: [140, 140, 140],
    },
    DmcColor {
        code: "415",
        name: "Pearl Gray",
        rgb: [211, 211, 214],
    },
    DmcColor {
        code: "762",
        name: "Pearl Gray Very Light",
        rgb: [227, 227, 230],
    },
    DmcColor {
        code: "844",
        name: "Beaver Gray Ultra Dark",
        rgb: [72, 72, 72],
    },
    // 棕 / 肤
    DmcColor {
        code: "433",
        name: "Brown Medium",
        rgb: [122, 69, 31],
    },
    DmcColor {
        code: "434",
        name: "Brown Light",
        rgb: [150, 93, 53],
    },
    DmcColor {
        code: "435",
        name: "Brown Very Light",
        rgb: [184, 119, 72],
    },
    DmcColor {
        code: "436",
        name: "Tan",
        rgb: [203, 145, 87],
    },
    DmcColor {
        code: "437",
        name: "Tan Light",
        rgb: [228, 187, 142],
    },
    DmcColor {
        code: "3781",
        name: "Mocha Brown Dark",
        rgb: [107, 69, 51],
    },
    DmcColor {
        code: "3782",
        name: "Mocha Brown Light",
        rgb: [210, 178, 152],
    },
    DmcColor {
        code: "801",
        name: "Coffee Brown Dark",
        rgb: [101, 57, 25],
    },
    DmcColor {
        code: "838",
        name: "Beige Brown Very Light",
        rgb: [226, 198, 165],
    },
    DmcColor {
        code: "839",
        name: "Beige Brown Dark",
        rgb: [103, 67, 42],
    },
    DmcColor {
        code: "840",
        name: "Beige Brown Medium",
        rgb: [154, 114, 84],
    },
    DmcColor {
        code: "869",
        name: "Hazelnut Brown Very Dark",
        rgb: [131, 94, 57],
    },
    DmcColor {
        code: "898",
        name: "Coffee Brown Very Dark",
        rgb: [73, 42, 19],
    },
    DmcColor {
        code: "938",
        name: "Coffee Brown Ultra Dark",
        rgb: [54, 31, 14],
    },
    // 红
    DmcColor {
        code: "150",
        name: "Red Bright",
        rgb: [206, 12, 38],
    },
    DmcColor {
        code: "151",
        name: "Red Light",
        rgb: [240, 133, 148],
    },
    DmcColor {
        code: "152",
        name: "Tangerine Medium Light",
        rgb: [235, 159, 165],
    },
    DmcColor {
        code: "304",
        name: "Christmas Red Medium",
        rgb: [183, 31, 51],
    },
    DmcColor {
        code: "321",
        name: "Red",
        rgb: [199, 15, 46],
    },
    DmcColor {
        code: "498",
        name: "Christmas Red Dark",
        rgb: [140, 20, 40],
    },
    DmcColor {
        code: "600",
        name: "Cranberry Very Dark",
        rgb: [167, 25, 66],
    },
    DmcColor {
        code: "601",
        name: "Cranberry Dark",
        rgb: [185, 32, 71],
    },
    DmcColor {
        code: "602",
        name: "Cranberry Medium",
        rgb: [202, 47, 82],
    },
    DmcColor {
        code: "603",
        name: "Cranberry",
        rgb: [224, 68, 96],
    },
    DmcColor {
        code: "605",
        name: "Cranberry Very Light",
        rgb: [252, 190, 199],
    },
    DmcColor {
        code: "606",
        name: "Orange Red Bright",
        rgb: [250, 14, 10],
    },
    DmcColor {
        code: "815",
        name: "Garnet Medium",
        rgb: [135, 7, 25],
    },
    DmcColor {
        code: "816",
        name: "Garnet",
        rgb: [151, 11, 27],
    },
    DmcColor {
        code: "817",
        name: "Coral Red Very Dark",
        rgb: [187, 5, 31],
    },
    // 粉
    DmcColor {
        code: "223",
        name: "Shell Pink Light",
        rgb: [240, 182, 187],
    },
    DmcColor {
        code: "224",
        name: "Shell Pink Very Light",
        rgb: [244, 208, 214],
    },
    DmcColor {
        code: "225",
        name: "Shell Pink Ultra Very Light",
        rgb: [255, 223, 217],
    },
    DmcColor {
        code: "309",
        name: "Rose Dark",
        rgb: [186, 74, 90],
    },
    DmcColor {
        code: "326",
        name: "Rose Very Dark",
        rgb: [179, 59, 75],
    },
    DmcColor {
        code: "776",
        name: "Pink Medium",
        rgb: [252, 113, 137],
    },
    DmcColor {
        code: "778",
        name: "Antique Mauve Very Light",
        rgb: [226, 187, 197],
    },
    DmcColor {
        code: "818",
        name: "Baby Pink",
        rgb: [255, 219, 219],
    },
    DmcColor {
        code: "819",
        name: "Baby Pink Light",
        rgb: [255, 238, 235],
    },
    DmcColor {
        code: "891",
        name: "Carnation Dark",
        rgb: [210, 33, 61],
    },
    DmcColor {
        code: "892",
        name: "Carnation Medium",
        rgb: [243, 71, 90],
    },
    DmcColor {
        code: "893",
        name: "Carnation Light",
        rgb: [242, 106, 122],
    },
    DmcColor {
        code: "894",
        name: "Carnation Very Light",
        rgb: [255, 178, 187],
    },
    DmcColor {
        code: "899",
        name: "Rose Medium",
        rgb: [241, 135, 145],
    },
    DmcColor {
        code: "948",
        name: "Peach Very Light",
        rgb: [250, 231, 218],
    },
    DmcColor {
        code: "957",
        name: "Geranium Pale",
        rgb: [253, 181, 181],
    },
    DmcColor {
        code: "961",
        name: "Dusty Rose Dark",
        rgb: [207, 110, 122],
    },
    DmcColor {
        code: "962",
        name: "Dusty Rose Medium",
        rgb: [230, 138, 148],
    },
    DmcColor {
        code: "963",
        name: "Dusty Rose Ultra Very Light",
        rgb: [255, 215, 217],
    },
    // 橙 / 黄
    DmcColor {
        code: "721",
        name: "Spice Dark",
        rgb: [212, 107, 48],
    },
    DmcColor {
        code: "722",
        name: "Spice Medium",
        rgb: [230, 120, 55],
    },
    DmcColor {
        code: "740",
        name: "Tangerine",
        rgb: [255, 138, 0],
    },
    DmcColor {
        code: "741",
        name: "Tangerine Medium",
        rgb: [255, 163, 43],
    },
    DmcColor {
        code: "742",
        name: "Tangerine Light",
        rgb: [255, 191, 87],
    },
    DmcColor {
        code: "743",
        name: "Yellow Medium",
        rgb: [254, 215, 120],
    },
    DmcColor {
        code: "744",
        name: "Yellow Pale",
        rgb: [254, 231, 158],
    },
    DmcColor {
        code: "745",
        name: "Yellow Pale Light",
        rgb: [254, 236, 179],
    },
    DmcColor {
        code: "900",
        name: "Burnt Orange Dark",
        rgb: [196, 78, 0],
    },
    DmcColor {
        code: "920",
        name: "Copper Medium",
        rgb: [172, 94, 51],
    },
    DmcColor {
        code: "921",
        name: "Copper",
        rgb: [198, 110, 61],
    },
    DmcColor {
        code: "922",
        name: "Copper Light",
        rgb: [226, 138, 88],
    },
    DmcColor {
        code: "947",
        name: "Burnt Orange",
        rgb: [235, 107, 0],
    },
    DmcColor {
        code: "970",
        name: "Pumpkin Light",
        rgb: [247, 135, 15],
    },
    DmcColor {
        code: "971",
        name: "Pumpkin",
        rgb: [252, 97, 0],
    },
    DmcColor {
        code: "972",
        name: "Canary Dark",
        rgb: [255, 199, 0],
    },
    DmcColor {
        code: "973",
        name: "Canary Bright",
        rgb: [255, 222, 51],
    },
    DmcColor {
        code: "3853",
        name: "Autumn Gold Dark",
        rgb: [242, 151, 23],
    },
    DmcColor {
        code: "3854",
        name: "Autumn Gold Medium",
        rgb: [246, 173, 59],
    },
    DmcColor {
        code: "3855",
        name: "Autumn Gold Light",
        rgb: [249, 199, 111],
    },
    DmcColor {
        code: "444",
        name: "Lemon Dark",
        rgb: [255, 214, 0],
    },
    DmcColor {
        code: "445",
        name: "Lemon Light",
        rgb: [255, 251, 139],
    },
    // 绿
    DmcColor {
        code: "319",
        name: "Pistachio Green Very Dark",
        rgb: [57, 88, 63],
    },
    DmcColor {
        code: "367",
        name: "Pistachio Green Dark",
        rgb: [101, 131, 84],
    },
    DmcColor {
        code: "368",
        name: "Pistachio Green Light",
        rgb: [141, 166, 106],
    },
    DmcColor {
        code: "369",
        name: "Pistachio Green Very Light",
        rgb: [196, 215, 167],
    },
    DmcColor {
        code: "469",
        name: "Avocado Green Dark",
        rgb: [82, 100, 54],
    },
    DmcColor {
        code: "470",
        name: "Avocado Green Light",
        rgb: [114, 140, 74],
    },
    DmcColor {
        code: "471",
        name: "Avocado Green Very Light",
        rgb: [148, 171, 94],
    },
    DmcColor {
        code: "700",
        name: "Christmas Green Bright",
        rgb: [4, 101, 37],
    },
    DmcColor {
        code: "701",
        name: "Christmas Green Light",
        rgb: [11, 123, 46],
    },
    DmcColor {
        code: "702",
        name: "Kelly Green",
        rgb: [22, 141, 55],
    },
    DmcColor {
        code: "703",
        name: "Chartreuse",
        rgb: [59, 155, 43],
    },
    DmcColor {
        code: "704",
        name: "Chartreuse Bright",
        rgb: [102, 176, 72],
    },
    DmcColor {
        code: "730",
        name: "Olive Green Very Dark",
        rgb: [94, 82, 26],
    },
    DmcColor {
        code: "731",
        name: "Olive Green Dark",
        rgb: [107, 94, 33],
    },
    DmcColor {
        code: "732",
        name: "Olive Green Medium",
        rgb: [120, 108, 42],
    },
    DmcColor {
        code: "733",
        name: "Olive Green Light",
        rgb: [148, 138, 66],
    },
    DmcColor {
        code: "734",
        name: "Olive Green Very Light",
        rgb: [167, 157, 87],
    },
    DmcColor {
        code: "890",
        name: "Pistachio Green Ultra Dark",
        rgb: [32, 71, 44],
    },
    DmcColor {
        code: "895",
        name: "Hunter Green Very Dark",
        rgb: [27, 83, 52],
    },
    DmcColor {
        code: "909",
        name: "Emerald Green Very Dark",
        rgb: [21, 111, 73],
    },
    DmcColor {
        code: "910",
        name: "Emerald Green Dark",
        rgb: [24, 126, 82],
    },
    DmcColor {
        code: "911",
        name: "Emerald Green Medium",
        rgb: [24, 144, 101],
    },
    DmcColor {
        code: "912",
        name: "Emerald Green Light",
        rgb: [27, 157, 107],
    },
    DmcColor {
        code: "924",
        name: "Gray Green Dark",
        rgb: [86, 110, 92],
    },
    DmcColor {
        code: "926",
        name: "Gray Green Medium",
        rgb: [107, 133, 113],
    },
    DmcColor {
        code: "927",
        name: "Gray Green Light",
        rgb: [129, 152, 128],
    },
    DmcColor {
        code: "928",
        name: "Gray Green Very Light",
        rgb: [153, 173, 149],
    },
    DmcColor {
        code: "934",
        name: "Avocado Green Black",
        rgb: [49, 57, 25],
    },
    DmcColor {
        code: "935",
        name: "Avocado Green Dark",
        rgb: [66, 77, 33],
    },
    DmcColor {
        code: "936",
        name: "Avocado Green Very Light",
        rgb: [156, 164, 114],
    },
    DmcColor {
        code: "937",
        name: "Avocado Green Medium Light",
        rgb: [110, 121, 54],
    },
    DmcColor {
        code: "954",
        name: "Nile Green",
        rgb: [136, 196, 172],
    },
    DmcColor {
        code: "955",
        name: "Nile Green Light",
        rgb: [162, 214, 192],
    },
    DmcColor {
        code: "986",
        name: "Forest Green Very Dark",
        rgb: [20, 84, 44],
    },
    DmcColor {
        code: "987",
        name: "Forest Green Dark",
        rgb: [26, 104, 58],
    },
    DmcColor {
        code: "988",
        name: "Forest Green Medium",
        rgb: [33, 124, 73],
    },
    DmcColor {
        code: "989",
        name: "Forest Green",
        rgb: [41, 141, 83],
    },
    DmcColor {
        code: "993",
        name: "Aquamarine Light",
        rgb: [144, 192, 180],
    },
    // 蓝
    DmcColor {
        code: "311",
        name: "Navy Blue Medium",
        rgb: [28, 57, 109],
    },
    DmcColor {
        code: "312",
        name: "Navy Blue Light",
        rgb: [53, 102, 164],
    },
    DmcColor {
        code: "336",
        name: "Navy Blue",
        rgb: [37, 59, 113],
    },
    DmcColor {
        code: "823",
        name: "Navy Blue Dark",
        rgb: [33, 48, 99],
    },
    DmcColor {
        code: "939",
        name: "Navy Blue Very Dark",
        rgb: [27, 40, 83],
    },
    DmcColor {
        code: "517",
        name: "Wedgewood Dark",
        rgb: [24, 98, 122],
    },
    DmcColor {
        code: "518",
        name: "Wedgewood Light",
        rgb: [40, 130, 160],
    },
    DmcColor {
        code: "519",
        name: "Sky Blue",
        rgb: [126, 177, 200],
    },
    DmcColor {
        code: "791",
        name: "Cornflower Blue Very Dark",
        rgb: [70, 76, 131],
    },
    DmcColor {
        code: "792",
        name: "Cornflower Blue Dark",
        rgb: [85, 91, 148],
    },
    DmcColor {
        code: "793",
        name: "Cornflower Blue Medium",
        rgb: [112, 125, 180],
    },
    DmcColor {
        code: "794",
        name: "Cornflower Blue Light",
        rgb: [143, 156, 209],
    },
    DmcColor {
        code: "796",
        name: "Royal Blue Dark",
        rgb: [17, 65, 134],
    },
    DmcColor {
        code: "797",
        name: "Royal Blue",
        rgb: [23, 73, 147],
    },
    DmcColor {
        code: "798",
        name: "Delft Blue Dark",
        rgb: [70, 106, 175],
    },
    DmcColor {
        code: "799",
        name: "Delft Blue Medium",
        rgb: [116, 152, 209],
    },
    DmcColor {
        code: "809",
        name: "Delft Blue",
        rgb: [136, 178, 224],
    },
    DmcColor {
        code: "813",
        name: "Blue Light",
        rgb: [157, 195, 230],
    },
    DmcColor {
        code: "820",
        name: "Royal Blue Very Light",
        rgb: [112, 152, 209],
    },
    DmcColor {
        code: "824",
        name: "Blue Very Dark",
        rgb: [29, 79, 136],
    },
    DmcColor {
        code: "825",
        name: "Blue Dark",
        rgb: [44, 98, 160],
    },
    DmcColor {
        code: "826",
        name: "Blue Medium",
        rgb: [57, 117, 189],
    },
    DmcColor {
        code: "827",
        name: "Blue Very Light",
        rgb: [160, 198, 232],
    },
    DmcColor {
        code: "828",
        name: "Blue Ultra Very Light",
        rgb: [197, 222, 242],
    },
    DmcColor {
        code: "930",
        name: "Antique Blue Dark",
        rgb: [70, 106, 127],
    },
    DmcColor {
        code: "931",
        name: "Antique Blue Medium",
        rgb: [98, 135, 157],
    },
    DmcColor {
        code: "932",
        name: "Antique Blue Light",
        rgb: [129, 164, 186],
    },
    DmcColor {
        code: "943",
        name: "Aquamarine Medium",
        rgb: [63, 166, 145],
    },
    DmcColor {
        code: "991",
        name: "Aquamarine Dark",
        rgb: [29, 111, 97],
    },
    DmcColor {
        code: "992",
        name: "Aquamarine",
        rgb: [49, 145, 127],
    },
    DmcColor {
        code: "3843",
        name: "Electric Blue",
        rgb: [20, 130, 190],
    },
    DmcColor {
        code: "3844",
        name: "Bright Turquoise Dark",
        rgb: [24, 158, 190],
    },
    DmcColor {
        code: "3845",
        name: "Bright Turquoise Medium",
        rgb: [56, 186, 213],
    },
    DmcColor {
        code: "3846",
        name: "Bright Turquoise Light",
        rgb: [111, 210, 231],
    },
    DmcColor {
        code: "3847",
        name: "Teal Green Dark",
        rgb: [13, 108, 104],
    },
    DmcColor {
        code: "3848",
        name: "Teal Green Medium",
        rgb: [16, 137, 132],
    },
    DmcColor {
        code: "3849",
        name: "Teal Green Light",
        rgb: [43, 167, 160],
    },
    DmcColor {
        code: "500",
        name: "Blue Green Very Dark",
        rgb: [19, 74, 54],
    },
    DmcColor {
        code: "501",
        name: "Blue Green Dark",
        rgb: [23, 91, 66],
    },
    DmcColor {
        code: "502",
        name: "Blue Green",
        rgb: [28, 109, 79],
    },
    DmcColor {
        code: "503",
        name: "Blue Green Medium",
        rgb: [39, 130, 92],
    },
    DmcColor {
        code: "597",
        name: "Turquoise",
        rgb: [103, 178, 178],
    },
    DmcColor {
        code: "598",
        name: "Turquoise Light",
        rgb: [140, 199, 199],
    },
    DmcColor {
        code: "640",
        name: "Blue Green Very Light",
        rgb: [157, 202, 183],
    },
    DmcColor {
        code: "747",
        name: "Peacock Blue Very Light",
        rgb: [153, 207, 209],
    },
    // 紫 / 罗兰
    DmcColor {
        code: "153",
        name: "Violet Very Light",
        rgb: [216, 196, 232],
    },
    DmcColor {
        code: "154",
        name: "Garnet Dark",
        rgb: [87, 33, 76],
    },
    DmcColor {
        code: "315",
        name: "Antique Mauve Medium Dark",
        rgb: [129, 74, 97],
    },
    DmcColor {
        code: "327",
        name: "Violet Dark",
        rgb: [99, 36, 110],
    },
    DmcColor {
        code: "550",
        name: "Violet Very Dark",
        rgb: [92, 24, 102],
    },
    DmcColor {
        code: "552",
        name: "Violet Medium",
        rgb: [128, 58, 138],
    },
    DmcColor {
        code: "553",
        name: "Violet",
        rgb: [147, 77, 155],
    },
    DmcColor {
        code: "554",
        name: "Violet Light",
        rgb: [173, 114, 180],
    },
    DmcColor {
        code: "561",
        name: "Jade Very Dark",
        rgb: [44, 105, 96],
    },
    DmcColor {
        code: "562",
        name: "Jade Medium",
        rgb: [83, 179, 170],
    },
    DmcColor {
        code: "563",
        name: "Jade Light",
        rgb: [116, 199, 191],
    },
    DmcColor {
        code: "718",
        name: "Plum Light",
        rgb: [156, 36, 92],
    },
    DmcColor {
        code: "915",
        name: "Plum Dark",
        rgb: [130, 24, 77],
    },
    DmcColor {
        code: "917",
        name: "Plum Medium",
        rgb: [155, 31, 91],
    },
    DmcColor {
        code: "3834",
        name: "Grape Dark",
        rgb: [114, 55, 106],
    },
    DmcColor {
        code: "3835",
        name: "Grape Medium",
        rgb: [148, 75, 137],
    },
    DmcColor {
        code: "3836",
        name: "Grape Light",
        rgb: [186, 145, 178],
    },
    DmcColor {
        code: "3837",
        name: "Lavender Ultra Dark",
        rgb: [108, 54, 136],
    },
    DmcColor {
        code: "3838",
        name: "Lavender Blue Dark",
        rgb: [104, 110, 172],
    },
    DmcColor {
        code: "3839",
        name: "Lavender Blue Medium",
        rgb: [126, 133, 197],
    },
    DmcColor {
        code: "3840",
        name: "Lavender Blue Light",
        rgb: [159, 166, 214],
    },
    DmcColor {
        code: "3841",
        name: "Pale Baby Blue",
        rgb: [205, 219, 242],
    },
    DmcColor {
        code: "3842",
        name: "Wedgewood Dark",
        rgb: [17, 91, 120],
    },
    DmcColor {
        code: "3856",
        name: "Mahogany Ultra Light",
        rgb: [183, 101, 47],
    },
    DmcColor {
        code: "3857",
        name: "Rosewood Medium",
        rgb: [103, 30, 22],
    },
    DmcColor {
        code: "3858",
        name: "Rosewood Dark",
        rgb: [130, 42, 32],
    },
    DmcColor {
        code: "3859",
        name: "Rosewood Light",
        rgb: [186, 139, 122],
    },
    DmcColor {
        code: "3860",
        name: "Cocoa",
        rgb: [125, 93, 87],
    },
    DmcColor {
        code: "3861",
        name: "Cocoa Light",
        rgb: [166, 137, 131],
    },
    DmcColor {
        code: "3862",
        name: "Mocha Brown Ultra Dark",
        rgb: [86, 55, 41],
    },
    DmcColor {
        code: "3863",
        name: "Mocha Brown Medium",
        rgb: [155, 121, 101],
    },
    DmcColor {
        code: "3864",
        name: "Mocha Brown Light",
        rgb: [195, 168, 148],
    },
    DmcColor {
        code: "3820",
        name: "Straw Dark",
        rgb: [218, 186, 105],
    },
    DmcColor {
        code: "3821",
        name: "Straw",
        rgb: [232, 202, 130],
    },
];
