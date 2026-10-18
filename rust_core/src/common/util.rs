//! 通用数学 / 颜色工具。

/// sRGB → CIE Lab（D65），用于 DMC 色卡最近匹配。
pub fn rgb_to_lab(rgb: [u8; 3]) -> [f64; 3] {
    fn srgb_to_linear(c: u8) -> f64 {
        let c = c as f64 / 255.0;
        if c <= 0.04045 {
            c / 12.92
        } else {
            ((c + 0.055) / 1.055).powf(2.4)
        }
    }
    let r = srgb_to_linear(rgb[0]);
    let g = srgb_to_linear(rgb[1]);
    let b = srgb_to_linear(rgb[2]);

    // sRGB D65 → XYZ
    let x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
    let y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
    let z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

    // D65 白点
    let (xn, yn, zn) = (0.95047, 1.0, 1.08883);
    let f = |t: f64| {
        if t > 0.008856 {
            t.cbrt()
        } else {
            7.787 * t + 16.0 / 116.0
        }
    };
    let fx = f(x / xn);
    let fy = f(y / yn);
    let fz = f(z / zn);
    [116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)]
}

/// Lab 空间欧氏距离的平方。
pub fn lab_dist_sq(a: [f64; 3], b: [f64; 3]) -> f64 {
    let dl = a[0] - b[0];
    let da = a[1] - b[1];
    let db = a[2] - b[2];
    dl * dl + da * da + db * db
}

/// RGB 空间欧氏距离的平方（KMeans 聚类用）。
pub fn rgb_dist_sq(a: [u8; 3], b: [u8; 3]) -> u32 {
    let dr = a[0] as i32 - b[0] as i32;
    let dg = a[1] as i32 - b[1] as i32;
    let db = a[2] as i32 - b[2] as i32;
    (dr * dr + dg * dg + db * db) as u32
}

/// 确定性线性同余 RNG（保证跨平台结果一致，KMeans 初始化用）。
pub struct Lcg(u64);

impl Lcg {
    pub fn new(seed: u64) -> Self {
        Self(
            seed.wrapping_mul(6364136223846793005)
                .wrapping_add(1442695040888963407)
                | 1,
        )
    }
    pub fn next_u64(&mut self) -> u64 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        self.0
    }
    pub fn below(&mut self, n: u64) -> u64 {
        self.next_u64() % n.max(1)
    }
}

/// 符号字符序列（ASCII，保证 PDF 内置字体与 Qt 画布可渲染）。
/// 注：Unicode 刺绣符号（▲●■ 等）需 TTF 字体嵌入后启用，见开发大纲第 7 章。
pub const SYMBOL_SEQUENCE: &str =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()+=<>?";

pub fn symbol_at(index: usize) -> char {
    SYMBOL_SEQUENCE
        .chars()
        .nth(index)
        .unwrap_or(SYMBOL_SEQUENCE.chars().next().unwrap())
}

// ------------------------------------------------------------
// 符号集（开发大纲第 7 章：导出文本可本地化；此处全部 ASCII，
// 保证 printpdf 内置 Helvetica 与 Qt 画布都能渲染，无需嵌入 TTF）
// ------------------------------------------------------------

/// 字母集：默认集，52 个，字形区分度最好。
pub const SYMBOL_SET_LETTERS: &str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/// 数字集：淡色底图纸常用。剔除易混的 0/O、1/I/l，共 32 个。
pub const SYMBOL_SET_DIGITS: &str = "123456789ABCDEFGHJKLMNPQRSTUVWXYZ";

/// 几何集：字母易与色块明暗混淆时使用。
pub const SYMBOL_SET_GEOMETRIC: &str = "+x*o#=^~v/<>%$@nu";

/// 混合集：字母优先（最清晰），用尽后接几何符号。
pub const SYMBOL_SET_MIXED: &str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ+x*o#=^~v/<>%$@";

/// 符号集 → 字符表。未知集回退到字母集。
pub fn symbol_set(set: u32) -> &'static str {
    match set {
        crate::common::ffi_types::symbol::DIGITS => SYMBOL_SET_DIGITS,
        crate::common::ffi_types::symbol::GEOMETRIC => SYMBOL_SET_GEOMETRIC,
        crate::common::ffi_types::symbol::MIXED => SYMBOL_SET_MIXED,
        _ => SYMBOL_SET_LETTERS,
    }
}

/// 符号集容量（换符号集时用于判断色数是否超限）。
pub fn symbol_set_len(set: u32) -> usize {
    symbol_set(set).chars().count()
}

/// 取符号集中第 index 个符号（越界则回绕，保证永不 panic）。
pub fn symbol_in_set(set: u32, index: usize) -> char {
    let chars: Vec<char> = symbol_set(set).chars().collect();
    chars[index % chars.len()]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lab_white_and_black() {
        let white = rgb_to_lab([255, 255, 255]);
        let black = rgb_to_lab([0, 0, 0]);
        assert!((white[0] - 100.0).abs() < 0.5);
        assert!(black[0].abs() < 0.5);
    }

    #[test]
    fn lab_primaries_have_expected_signs() {
        // Lab 感知均匀性：三原色应在 a*/b* 轴上正确分离
        let red = rgb_to_lab([255, 0, 0]);
        let green = rgb_to_lab([0, 255, 0]);
        let blue = rgb_to_lab([0, 0, 255]);
        assert!(red[1] > 50.0, "red a* should be strongly positive");
        assert!(red[2] > 50.0, "red b* should be positive");
        assert!(green[1] < -50.0, "green a* should be strongly negative");
        assert!(blue[2] < -50.0, "blue b* should be strongly negative");
        // 红绿距离与红蓝距离应为同一量级（感知均匀性粗验）
        let d_rg = lab_dist_sq(red, green);
        let d_rb = lab_dist_sq(red, blue);
        assert!((d_rg / d_rb - 1.0).abs() < 0.2, "d_rg={d_rg}, d_rb={d_rb}");
    }

    #[test]
    fn lcg_deterministic() {
        let mut a = Lcg::new(42);
        let mut b = Lcg::new(42);
        for _ in 0..10 {
            assert_eq!(a.next_u64(), b.next_u64());
        }
        assert!(Lcg::new(1).below(10) < 10);
    }
}
